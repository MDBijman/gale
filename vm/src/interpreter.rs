use crate::bytecode::*;
use crate::memory::Pointer;
use crate::vm::{VMState, VM};

use std::collections::HashMap;
use std::fmt::Display;

#[derive(PartialEq, Debug)]
pub enum InterpreterStatus {
    Created,
    Running,
    Finished,
}

pub struct CallInfo {
    called_by_native: bool,
    var_base: isize,
    result_var: VarId,
    prev_ip: isize,
    prev_fn: FnId,
    prev_mod: ModuleId,
}

#[derive(Debug, Clone)]
pub enum Value {
    Uninit,
    Unit,
    UI64(u64),
    Bool(bool),
    Tuple(Vec<Value>),
    Pointer(Pointer),
    CallTarget(CallTarget),
}

impl Value {
    pub fn as_ui64(&self) -> Option<&u64> {
        if let Self::UI64(v) = self {
            Some(v)
        } else {
            None
        }
    }

    pub fn as_bool(&self) -> Option<&bool> {
        if let Self::Bool(v) = self {
            Some(v)
        } else {
            None
        }
    }

    pub fn as_tuple(&self) -> Option<&Vec<Value>> {
        if let Self::Tuple(v) = self {
            Some(v)
        } else {
            None
        }
    }

    pub fn as_pointer(&self) -> Option<&Pointer> {
        if let Self::Pointer(v) = self {
            Some(v)
        } else {
            None
        }
    }

    pub fn as_call_target(&self) -> Option<&CallTarget> {
        if let Self::CallTarget(v) = self {
            Some(v)
        } else {
            None
        }
    }
}

impl Into<u64> for Value {
    fn into(self) -> u64 {
        match self {
            Value::UI64(v) => v,
            Value::Bool(b) => b as u64,
            Value::Pointer(p) => p.into(),
            Value::CallTarget(t) => (((t.function as u32) << 16) | t.module as u32) as u64,
            Value::Uninit => panic!(),
            Value::Unit => panic!(),
            Value::Tuple(_) => panic!(),
        }
    }
}

impl Display for Value {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Value::Uninit => write!(f, "<uninit>"),
            Value::Unit => write!(f, "()"),
            Value::UI64(v) => write!(f, "{}", v),
            Value::Bool(v) => write!(f, "{}", v),
            Value::Tuple(elems) => {
                write!(f, "(")?;
                let mut iter = elems.iter();
                if let Some(first) = iter.next() {
                    write!(f, "{}", first)?;
                    for e in iter {
                        write!(f, ", {}", e)?;
                    }
                }

                write!(f, ")")
            }
            Value::Pointer(v) => write!(f, "{:?}", v),
            Value::CallTarget(ct) => write!(f, "{:?}", ct),
        }
    }
}

pub struct Interpreter {}

impl Default for Interpreter {
    fn default() -> Self {
        Self {}
    }
}

impl Interpreter {
    pub fn push_native_caller_frame(
        &self,
        vm: &VM,
        state: &mut InterpreterState,
        target: CallTarget,
        arg: u64,
    ) {
        let func = vm
            .module_loader
            .get_by_id(target.module)
            .expect("missing module")
            .as_ref()
            .expect("missing impl")
            .get_function_by_id(target.function)
            .expect("function id");

        let bytecode = func.bytecode_impl().expect("bytecode");

        let stack_size = bytecode.varcount;
        let new_base = state.stack.slots.len() as isize;

        state.stack.slots.resize(
            new_base as usize + stack_size as usize + 1, /* <- for nfi result */
            Value::Uninit,
        );

        let fun_ci = CallInfo {
            called_by_native: true,
            var_base: new_base + 1,
            result_var: 0,
            prev_ip: isize::MAX,
            prev_fn: FnId::MAX,
            prev_mod: ModuleId::MAX,
        };

        state.call_info.push(fun_ci);

        state.set_stack_var(0, Value::UI64(arg));

        state.ip = 0;
        state.func = func.location;
        state.module = target.module;
    }

    pub fn pop_native_caller_frame(&self, state: &mut InterpreterState) -> Value {
        let ci = state
            .call_info
            .pop()
            .expect("Expected call info to be on stack");
        let result = state
            .stack
            .slots
            .pop()
            .expect("Expected result to be on stack");

        // Finished execution
        if state.call_info.len() == 0 {
            state.state = InterpreterStatus::Finished;
            return result;
        }

        state.ip = ci.prev_ip; // Maybe has to increment
        state.func = ci.prev_fn;

        result
    }

    pub fn get_instruction<'a>(
        &self,
        module: &'a Module,
        state: &InterpreterState,
    ) -> &'a Instruction {
        &module
            .get_function_by_id(state.func)
            .expect("function")
            .bytecode_impl()
            .expect("bytecode impl")
            .instructions[state.ip as usize]
    }

    // Executes until the current function is finished, including further calls
    pub fn finish_function(&self, vm: &VM, state: &mut VMState) {
        let start_stack_size = state.interpreter_state.call_info.len();
        assert_ne!(start_stack_size, 0);

        while self.step(vm, state) {
            if state.interpreter_state.call_info.len() == start_stack_size - 1 {
                return;
            }
        }
    }

    //#[inline(never)] // <- uncomment for profiling with better source <-> asm map
    #[inline]
    pub fn step(&self, vm: &VM, vm_state: &mut VMState) -> bool {
        assert_eq!(vm_state.interpreter_state.state, InterpreterStatus::Running);

        let current_module = &vm
            .module_loader
            .get_by_id(vm_state.interpreter_state.module)
            .expect("missing module")
            .expect("missing impl");

        let current_instruction = self.get_instruction(current_module, &vm_state.interpreter_state);

        vm_state.interpreter_state.instr_counter += 1;

        match current_instruction {
            Instruction::ConstU32(loc, n) => {
                vm_state
                    .interpreter_state
                    .set_stack_var(*loc, Value::UI64(*n as u64));
                vm_state.interpreter_state.ip += 1;
            }
            Instruction::ConstBool(loc, n) => {
                vm_state
                    .interpreter_state
                    .set_stack_var(*loc, Value::Bool(*n));
                vm_state.interpreter_state.ip += 1;
            }
            Instruction::Tuple(loc, len) => {
                vm_state
                    .interpreter_state
                    .set_stack_var(*loc, Value::Tuple(vec![Value::Uninit; *len as usize]));
                vm_state.interpreter_state.ip += 1;
            }
            Instruction::Copy(loc, src) => {
                let val = vm_state.interpreter_state.get_stack_var(*src).clone();
                vm_state.interpreter_state.set_stack_var(*loc, val);
                vm_state.interpreter_state.ip += 1;
            }
            Instruction::CopyIntoIndex(loc, idx, src) => {
                let val = vm_state.interpreter_state.get_stack_var(*src).clone();
                match vm_state.interpreter_state.get_stack_var_mut(*loc) {
                    Value::Tuple(elems) => {
                        *elems.get_mut(*idx as usize).expect("invalid index") = val;
                    }
                    _ => panic!("invalid type"),
                }

                vm_state.interpreter_state.ip += 1;
            }
            Instruction::CopyAddress(loc, calltarget) => {
                vm_state
                    .interpreter_state
                    .set_stack_var(*loc, Value::CallTarget(*calltarget));
                vm_state.interpreter_state.ip += 1;
            }
            Instruction::CopyAddressIntoIndex(loc, idx, calltarget) => {
                match vm_state.interpreter_state.get_stack_var_mut(*loc) {
                    Value::Tuple(elems) => {
                        *elems.get_mut(*idx as usize).expect("invalid index") =
                            Value::CallTarget(*calltarget);
                    }
                    _ => panic!("invalid type"),
                }

                vm_state.interpreter_state.ip += 1;
            }
            Instruction::EqVarVar(dest, a, b) => {
                let val_a = *vm_state
                    .interpreter_state
                    .get_stack_var(*a)
                    .as_ui64()
                    .expect("invalid type");
                let val_b = *vm_state
                    .interpreter_state
                    .get_stack_var(*b)
                    .as_ui64()
                    .expect("invalid type");
                vm_state
                    .interpreter_state
                    .set_stack_var(*dest, Value::Bool(val_a == val_b));
                vm_state.interpreter_state.ip += 1;
            }
            Instruction::LtVarVar(dest, a, b) => {
                let val_a = *vm_state
                    .interpreter_state
                    .get_stack_var(*a)
                    .as_ui64()
                    .expect("invalid type");
                let val_b = *vm_state
                    .interpreter_state
                    .get_stack_var(*b)
                    .as_ui64()
                    .expect("invalid type");
                vm_state
                    .interpreter_state
                    .set_stack_var(*dest, Value::Bool(val_a < val_b));
                vm_state.interpreter_state.ip += 1;
            }
            Instruction::SubVarVar(dest, a, b) => {
                let val_a = *vm_state
                    .interpreter_state
                    .get_stack_var(*a)
                    .as_ui64()
                    .expect("invalid type");
                let val_b = *vm_state
                    .interpreter_state
                    .get_stack_var(*b)
                    .as_ui64()
                    .expect("invalid type");
                vm_state
                    .interpreter_state
                    .set_stack_var(*dest, Value::UI64(val_a - val_b));
                vm_state.interpreter_state.ip += 1;
            }
            Instruction::AddVarVar(dest, a, b) => {
                let val_a = *vm_state
                    .interpreter_state
                    .get_stack_var(*a)
                    .as_ui64()
                    .expect("invalid type");
                let val_b = *vm_state
                    .interpreter_state
                    .get_stack_var(*b)
                    .as_ui64()
                    .expect("invalid type");
                vm_state
                    .interpreter_state
                    .set_stack_var(*dest, Value::UI64(val_a + val_b));
                vm_state.interpreter_state.ip += 1;
            }
            Instruction::MulVarVar(dest, a, b) => {
                let val_a = *vm_state
                    .interpreter_state
                    .get_stack_var(*a)
                    .as_ui64()
                    .expect("invalid type");
                let val_b = *vm_state
                    .interpreter_state
                    .get_stack_var(*b)
                    .as_ui64()
                    .expect("invalid type");
                vm_state
                    .interpreter_state
                    .set_stack_var(*dest, Value::UI64(val_a * val_b));
                vm_state.interpreter_state.ip += 1;
            }
            Instruction::NotVar(dest, src) => {
                let src_val = *vm_state
                    .interpreter_state
                    .get_stack_var(*src)
                    .as_bool()
                    .expect("invalid type");
                vm_state
                    .interpreter_state
                    .set_stack_var(*dest, Value::Bool(!src_val));
                vm_state.interpreter_state.ip += 1;
            }
            Instruction::Print(src) => {
                println!("{}", vm_state.interpreter_state.get_stack_var(*src));
                vm_state.interpreter_state.ip += 1;
            }
            Instruction::Return(src) => {
                let val = vm_state.interpreter_state.get_stack_var(*src).clone();
                let ci = vm_state.interpreter_state.call_info.pop().unwrap();

                // Finished execution
                if vm_state.interpreter_state.call_info.len() == 0 {
                    vm_state.interpreter_state.state = InterpreterStatus::Finished;
                    vm_state.interpreter_state.result = val;
                    return false;
                }

                vm_state.interpreter_state.ip = ci.prev_ip;
                vm_state.interpreter_state.func = ci.prev_fn;
                vm_state.interpreter_state.module = ci.prev_mod;

                // Native function
                if ci.called_by_native {
                    vm_state
                        .interpreter_state
                        .stack
                        .slots
                        .truncate(ci.var_base as usize);

                    *vm_state.interpreter_state.stack.slots.last_mut().unwrap() = val.clone();
                } else {
                    let new_ci = vm_state.interpreter_state.call_info.last().unwrap();
                    let new_func = current_module
                        .get_function_by_id(vm_state.interpreter_state.func)
                        .expect("function")
                        .bytecode_impl()
                        .expect("bytecode impl");

                    vm_state
                        .interpreter_state
                        .stack
                        .slots
                        .truncate(new_ci.var_base as usize + new_func.varcount as usize);

                    vm_state
                        .interpreter_state
                        .set_stack_var(ci.result_var, val.clone());

                    vm_state.interpreter_state.ip += 1;
                }
            }
            Instruction::ModuleCall(r, _, arg) => {
                let target = current_instruction
                    .get_call_target(&vm_state.interpreter_state, current_module.id)
                    .expect("call target");

                let target_module = if target.module == ModuleId::MAX {
                    current_module.id
                } else {
                    target.module
                };

                let func = if target_module != current_module.id {
                    vm.module_loader
                        .get_by_id(target.module)
                        .expect("module")
                        .expect("mod impl")
                        .get_function_by_id(target.function)
                        .expect("function")
                } else {
                    &current_module
                        .get_function_by_id(target.function)
                        .expect("function")
                };

                vm_state
                    .interpreter_state
                    .increment_function_counter(func.location);

                if func.has_native_impl() {
                    let implementation = func.native_impl().unwrap();

                    let arg_value = vm_state.interpreter_state.get_stack_var(*arg);
                    let arg_in_64_bits: u64 = arg_value.clone().into();

                    let res = unsafe {
                        let unsafe_fn_ptr = std::mem::transmute::<_, fn(&mut VMState, u64) -> u64>(
                            implementation.fn_ptr,
                        );
                        (unsafe_fn_ptr)(vm_state, arg_in_64_bits)
                    };

                    vm_state
                        .interpreter_state
                        .set_stack_var(*r, Value::UI64(res));

                    vm_state.interpreter_state.ip += 1;
                } else if func.has_bytecode_impl() {
                    let implementation = func.bytecode_impl().unwrap();
                    let stack_size = implementation.varcount;

                    let new_base = vm_state.interpreter_state.stack.slots.len() as isize;

                    vm_state
                        .interpreter_state
                        .stack
                        .slots
                        .resize(new_base as usize + stack_size as usize, Value::Uninit);

                    let new_ci = CallInfo {
                        called_by_native: false,
                        var_base: new_base,
                        result_var: *r,
                        prev_ip: vm_state.interpreter_state.ip,
                        prev_fn: vm_state.interpreter_state.func,
                        prev_mod: vm_state.interpreter_state.module,
                    };

                    let val = vm_state.interpreter_state.get_stack_var(*arg).clone();
                    vm_state.interpreter_state.call_info.push(new_ci);
                    vm_state.interpreter_state.set_stack_var(0, val);
                    vm_state.interpreter_state.ip = 0;
                    vm_state.interpreter_state.func = target.function;
                    vm_state.interpreter_state.module = target_module;
                } else {
                    panic!("Unknown function implementation");
                }
            }
            Instruction::CallN(r, _, first_arg, num_args)
            | Instruction::VarCallN(r, _, first_arg, num_args) => {
                let target = current_instruction
                    .get_call_target(&vm_state.interpreter_state, current_module.id)
                    .expect("call target");

                let target_module = if target.module == ModuleId::MAX {
                    current_module.id
                } else {
                    target.module
                };

                let func = if target_module != current_module.id {
                    vm.module_loader
                        .get_by_id(target_module)
                        .expect("module")
                        .expect("mod impl")
                        .get_function_by_id(target.function)
                        .expect("function")
                } else {
                    &current_module
                        .get_function_by_id(target.function)
                        .expect("function")
                };

                vm_state
                    .interpreter_state
                    .increment_function_counter(func.location);

                if func.has_native_impl() {
                    todo!("Cannot turn large values into native values yet");
                } else if func.has_bytecode_impl() {
                    let implementation = func.bytecode_impl().unwrap();
                    let stack_size = implementation.varcount;

                    let new_base = vm_state.interpreter_state.stack.slots.len() as isize;

                    vm_state
                        .interpreter_state
                        .stack
                        .slots
                        .resize(new_base as usize + stack_size as usize, Value::Uninit);

                    let new_ci = CallInfo {
                        called_by_native: false,
                        var_base: new_base,
                        result_var: *r,
                        prev_ip: vm_state.interpreter_state.ip,
                        prev_fn: vm_state.interpreter_state.func,
                        prev_mod: vm_state.interpreter_state.module,
                    };

                    for idx in 0..*num_args {
                        let val = vm_state
                            .interpreter_state
                            .get_stack_var(idx + first_arg)
                            .clone();
                        vm_state
                            .interpreter_state
                            .set_stack_var_raw_index(new_base as usize + idx as usize, val);
                    }
                    vm_state.interpreter_state.call_info.push(new_ci);
                    vm_state.interpreter_state.ip = 0;
                    vm_state.interpreter_state.func = target.function;
                    vm_state.interpreter_state.module = target_module;
                } else {
                    panic!("Unknown function implementation");
                }
            }
            Instruction::Jmp(instr) => {
                vm_state.interpreter_state.ip += *instr as isize;
            }
            Instruction::JmpIf(instr, src) => {
                if !*vm_state
                    .interpreter_state
                    .get_stack_var(*src)
                    .as_bool()
                    .expect("invalid type")
                {
                    vm_state.interpreter_state.ip += 1;
                } else {
                    vm_state.interpreter_state.ip += *instr as isize;
                }
            }
            Instruction::JmpIfNot(instr, src) => {
                if !*vm_state
                    .interpreter_state
                    .get_stack_var(*src)
                    .as_bool()
                    .expect("invalid type")
                {
                    vm_state.interpreter_state.ip += *instr as isize;
                } else {
                    vm_state.interpreter_state.ip += 1;
                }
            }
            Instruction::Lbl => {
                vm_state.interpreter_state.ip += 1;
            }
            Instruction::Nop => {
                vm_state.interpreter_state.ip += 1;
            }
            Instruction::Alloc(loc, ty) => {
                let ty = current_module.get_type_by_id(*ty).expect("type idx");
                let pointer = vm_state.heap.allocate_type(ty);
                vm_state
                    .interpreter_state
                    .set_stack_var(*loc, Value::Pointer(pointer));

                vm_state.interpreter_state.ip += 1;
            }
            Instruction::LoadVar(dest, src, ty_idx) => {
                let ptr: Pointer = *vm_state
                    .interpreter_state
                    .get_stack_var(*src)
                    .as_pointer()
                    .expect("invalid type");

                match current_module.get_type_by_id(*ty_idx).expect("type idx") {
                    Type::U64 => {
                        let value = unsafe { vm_state.heap.load::<u64>(ptr) };
                        vm_state
                            .interpreter_state
                            .set_stack_var(*dest, Value::UI64(value));
                    }
                    Type::Pointer(_) => {
                        let value = unsafe { vm_state.heap.load::<u64>(ptr) };
                        vm_state
                            .interpreter_state
                            .set_stack_var(*dest, Value::Pointer(Pointer::from(value)));
                    }
                    _ => {
                        panic!(
                            "Illegal load operand: {:?}",
                            current_module.get_type_by_id(*ty_idx)
                        );
                    }
                }
                vm_state.interpreter_state.ip += 1;
            }
            Instruction::LoadConst(res, idx) => {
                let const_decl = current_module.get_constant_by_id(*idx).unwrap();

                let ptr = Module::write_constant_to_heap(
                    &mut vm_state.heap,
                    &const_decl.type_,
                    &const_decl.value,
                );

                vm_state
                    .interpreter_state
                    .set_stack_var(*res, Value::Pointer(ptr));
                vm_state.interpreter_state.ip += 1;
            }
            Instruction::StoreVar(dest, src, ty_idx) => {
                let ptr: Pointer = *vm_state
                    .interpreter_state
                    .get_stack_var(*dest)
                    .as_pointer()
                    .expect("invalid type");
                let value = vm_state.interpreter_state.get_stack_var(*src);

                match current_module.get_type_by_id(*ty_idx).expect("type idx") {
                    Type::U64 => unsafe {
                        vm_state
                            .heap
                            .store::<u64>(ptr, *value.as_ui64().expect("invalid type"));
                    },
                    _ => panic!("Illegal load operand: {:?}", ty_idx),
                }
                vm_state.interpreter_state.ip += 1;
            }
            Instruction::Index(dest, subject, idx, ty_idx) => {
                let idx = *vm_state
                    .interpreter_state
                    .get_stack_var(*idx)
                    .as_ui64()
                    .expect("invalid type");

                match current_module.get_type_by_id(*ty_idx).expect("type idx") {
                    Type::U64 => {
                        let ptr = *vm_state
                            .interpreter_state
                            .get_stack_var(*subject)
                            .as_pointer()
                            .expect("invalid type");
                        let new_ptr = vm_state
                            .heap
                            .index::<u64>(ptr, idx as usize + 1 /* array header */);
                        let loaded_value = unsafe { vm_state.heap.load::<u64>(new_ptr) };
                        vm_state
                            .interpreter_state
                            .set_stack_var(*dest, Value::UI64(loaded_value));
                    }
                    Type::Pointer(_) => {
                        let ptr = *vm_state
                            .interpreter_state
                            .get_stack_var(*subject)
                            .as_pointer()
                            .expect("invalid type");
                        let new_ptr = vm_state
                            .heap
                            .index::<u64>(ptr, idx as usize + 1 /* array header */);
                        let loaded_value = unsafe { vm_state.heap.load::<u64>(new_ptr) };
                        vm_state
                            .interpreter_state
                            .set_stack_var(*dest, Value::Pointer(loaded_value.into()));
                    }
                    Type::Tuple(_) => {
                        let value = vm_state
                            .interpreter_state
                            .get_stack_var(*subject)
                            .as_tuple()
                            .expect("invalid type")
                            .get(idx as usize)
                            .expect("invalid type")
                            .clone();

                        vm_state.interpreter_state.set_stack_var(*dest, value);
                    }
                    ty => panic!("Illegal index operand: {:?}", ty),
                }

                vm_state.interpreter_state.ip += 1;
            }
            Instruction::Sizeof(dest, src) => {
                let src_ptr = *vm_state
                    .interpreter_state
                    .get_stack_var(*src)
                    .as_pointer()
                    .expect("invalid type");

                unsafe {
                    let size = vm_state.heap.load_u64(src_ptr);
                    vm_state
                        .interpreter_state
                        .set_stack_var(*dest, Value::UI64(size));
                }

                vm_state.interpreter_state.ip += 1;
            }
            i => panic!("Illegal instruction: {:?}", i),
        }

        true
    }
}

struct Stack {
    slots: Vec<Value>,
}

impl Stack {
    pub fn new_with_framesize(framesize: usize) -> Stack {
        Stack {
            slots: vec![Value::Uninit; framesize],
        }
    }
}

pub struct InterpreterState {
    stack: Stack,
    call_info: Vec<CallInfo>,
    ip: isize,
    pub func: FnId,
    pub module: ModuleId,
    state: InterpreterStatus,

    pub result: Value,
    pub instr_counter: u64,
    pub function_counters: HashMap<FnId, u64>,
}

impl InterpreterState {
    pub fn new(entry: CallTarget, entry_framesize: u64) -> InterpreterState {
        InterpreterState {
            stack: Stack::new_with_framesize(entry_framesize as usize),
            call_info: vec![CallInfo {
                called_by_native: true,
                var_base: 0,
                result_var: VarId::MAX,
                prev_fn: FnId::MAX,
                prev_ip: isize::MAX,
                prev_mod: ModuleId::MAX,
            }],
            ip: 0,
            state: InterpreterStatus::Created,
            result: Value::Uninit,
            instr_counter: 0,
            function_counters: HashMap::new(),
            module: entry.module,
            func: entry.function,
        }
    }

    pub fn init(&mut self, entry: CallTarget, args_ptr: Pointer) {
        self.state = InterpreterStatus::Running;
        self.module = entry.module;
        self.func = entry.function;
        self.stack.slots[0] = 
        Value::Tuple(vec![Value::Unit, Value::Pointer(args_ptr)]);
    }

    pub fn init_empty(&mut self) {
        self.state = InterpreterStatus::Running;
    }

    pub fn get_stack_var_raw_index(&self, idx: usize) -> &Value {
        &self.stack.slots[idx]
    }

    pub fn set_stack_var_raw_index(&mut self, idx: usize, val: Value) {
        self.stack.slots[idx] = val;
    }

    pub fn get_stack_var(&self, var: VarId) -> &Value {
        let ci = self.call_info.last().unwrap();
        &self.stack.slots[ci.var_base as usize + var as usize]
    }

    pub fn get_stack_var_mut(&mut self, var: VarId) -> &mut Value {
        let ci = self.call_info.last().unwrap();
        &mut self.stack.slots[ci.var_base as usize + var as usize]
    }

    pub fn set_stack_var(&mut self, var: VarId, val: Value) {
        let ci = self.call_info.last().unwrap();
        self.stack.slots[ci.var_base as usize + var as usize] = val;
    }

    fn increment_function_counter(&mut self, fun: FnId) {
        let ctr = self.function_counters.entry(fun).or_insert(0);
        *ctr += 1;
    }
}
