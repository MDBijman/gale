use crate::bytecode::*;
use crate::memory::Pointer;
use crate::primitives;
use crate::vm::{VMState, VM};

use std::collections::HashMap;

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

        let bytecode = func.implementation.as_bytecode().expect("bytecode");

        let stack_size = bytecode.varcount;
        let new_base = state.stack.slots.len() as isize;

        state.stack.slots.resize(
            new_base as usize + stack_size as usize + 1, /* <- for nfi result */
            u64::MAX,
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

        state.set_stack_var(0, arg);

        state.ip = 0;
        state.func = func.location;
        state.module = target.module;
    }

    pub fn pop_native_caller_frame(&self, state: &mut InterpreterState) -> u64 {
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

    fn get_instruction<'a>(&self, module: &'a Module, state: &InterpreterState) -> &'a Instruction {
        println!("get {}", module.name);
        &module
            .get_function_by_id(state.func)
            .expect("function")
            .implementation
            .as_bytecode()
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

        //println!("{:?}", current_instruction);
        match current_instruction {
            Instruction::ConstU32(loc, n) => {
                vm_state.interpreter_state.set_stack_var(*loc, *n as u64);
                vm_state.interpreter_state.ip += 1;
            }
            Instruction::Copy(loc, src) => {
                let val = vm_state.interpreter_state.get_stack_var(*src);
                vm_state.interpreter_state.set_stack_var(*loc, val);
                vm_state.interpreter_state.ip += 1;
            }
            Instruction::EqVarVar(dest, a, b) => {
                let val_a = vm_state.interpreter_state.get_stack_var(*a);
                let val_b = vm_state.interpreter_state.get_stack_var(*b);
                vm_state
                    .interpreter_state
                    .set_stack_var(*dest, (val_a == val_b) as u64);
                vm_state.interpreter_state.ip += 1;
            }
            Instruction::LtVarVar(dest, a, b) => {
                let val_a = vm_state.interpreter_state.get_stack_var(*a);
                let val_b = vm_state.interpreter_state.get_stack_var(*b);
                vm_state
                    .interpreter_state
                    .set_stack_var(*dest, (val_a < val_b) as u64);
                vm_state.interpreter_state.ip += 1;
            }
            Instruction::SubVarVar(dest, a, b) => {
                let val_a = vm_state.interpreter_state.get_stack_var(*a);
                let val_b = vm_state.interpreter_state.get_stack_var(*b);
                vm_state
                    .interpreter_state
                    .set_stack_var(*dest, val_a - val_b);
                vm_state.interpreter_state.ip += 1;
            }
            Instruction::AddVarVar(dest, a, b) => {
                let val_a = vm_state.interpreter_state.get_stack_var(*a);
                let val_b = vm_state.interpreter_state.get_stack_var(*b);
                vm_state
                    .interpreter_state
                    .set_stack_var(*dest, val_a + val_b);
                vm_state.interpreter_state.ip += 1;
            }
            Instruction::NotVar(dest, src) => {
                let src_val = vm_state.interpreter_state.get_stack_var(*src);
                vm_state
                    .interpreter_state
                    .set_stack_var(*dest, if src_val == 0 { 1 } else { 0 });
                vm_state.interpreter_state.ip += 1;
            }
            Instruction::Print(src) => {
                println!("{}", vm_state.interpreter_state.get_stack_var(*src));
                vm_state.interpreter_state.ip += 1;
            }
            Instruction::Return(src) => {
                let val = vm_state.interpreter_state.get_stack_var(*src);
                let ci = vm_state.interpreter_state.call_info.pop().unwrap();

                // Finished execution
                if vm_state.interpreter_state.call_info.len() == 0 {
                    vm_state.interpreter_state.state = InterpreterStatus::Finished;
                    vm_state.interpreter_state.result = val;
                    return false;
                }

                vm_state.interpreter_state.ip = ci.prev_ip;
                vm_state.interpreter_state.func = ci.prev_fn;

                // Native function
                if ci.called_by_native {
                    vm_state
                        .interpreter_state
                        .stack
                        .slots
                        .truncate(ci.var_base as usize);

                    *vm_state.interpreter_state.stack.slots.last_mut().unwrap() = val;
                } else {
                    let new_ci = vm_state.interpreter_state.call_info.last().unwrap();
                    let new_func = current_module
                        .get_function_by_id(vm_state.interpreter_state.func)
                        .expect("function")
                        .implementation
                        .as_bytecode()
                        .expect("bytecode impl");

                    vm_state
                        .interpreter_state
                        .stack
                        .slots
                        .truncate(new_ci.var_base as usize + new_func.varcount as usize);

                    vm_state.interpreter_state.set_stack_var(ci.result_var, val);

                    vm_state.interpreter_state.ip += 1;
                }
            }
            Instruction::Call(r, _, arg) | Instruction::ModuleCall(r, _, arg) => {
                let target = current_instruction
                    .get_call_target(current_module.id)
                    .expect("call target");
                let func = if target.module != current_module.id {
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

                match &func.implementation {
                    FunctionImpl::Bytecode(implementation) => {
                        println!("{}", func.name);
                        let stack_size = implementation.varcount;

                        let new_base = vm_state.interpreter_state.stack.slots.len() as isize;

                        vm_state
                            .interpreter_state
                            .stack
                            .slots
                            .resize(new_base as usize + stack_size as usize, u64::MAX);

                        let new_ci = CallInfo {
                            called_by_native: false,
                            var_base: new_base,
                            result_var: *r,
                            prev_ip: vm_state.interpreter_state.ip,
                            prev_fn: vm_state.interpreter_state.func,
                            prev_mod: vm_state.interpreter_state.module,
                        };

                        let val = vm_state.interpreter_state.get_stack_var(*arg);
                        vm_state.interpreter_state.call_info.push(new_ci);
                        vm_state.interpreter_state.set_stack_var(0, val);
                        vm_state.interpreter_state.ip = 0;
                        vm_state.interpreter_state.func = target.function;
                        vm_state.interpreter_state.module = target.module;
                    }
                    FunctionImpl::Native(implementation) => {
                        let arg_value = Value::U64(vm_state.interpreter_state.get_stack_var(*arg));

                        match (implementation.fn_ptr)(arg_value) {
                            Value::U64(res) => vm_state.interpreter_state.set_stack_var(*r, res),
                            _ => panic!(),
                        }

                        vm_state.interpreter_state.ip += 1;
                    }
                }
            }
            Instruction::Jmp(instr) => {
                vm_state.interpreter_state.ip += *instr as isize;
            }
            Instruction::JmpIf(instr, src) => {
                match vm_state.interpreter_state.get_stack_var(*src) {
                    0 => {
                        vm_state.interpreter_state.ip += 1;
                    }
                    _ => {
                        vm_state.interpreter_state.ip += *instr as isize;
                    }
                }
            }
            Instruction::JmpIfNot(instr, src) => {
                match vm_state.interpreter_state.get_stack_var(*src) {
                    0 => {
                        vm_state.interpreter_state.ip += *instr as isize;
                    }
                    _ => {
                        vm_state.interpreter_state.ip += 1;
                    }
                }
            }
            Instruction::Lbl => {
                vm_state.interpreter_state.ip += 1;
            }
            Instruction::Nop => {
                vm_state.interpreter_state.ip += 1;
            }
            Instruction::Alloc(loc, ty) => {
                match current_module.get_type_by_id(*ty).expect("type idx") {
                    Type::U64 => {
                        let pointer = primitives::alloc(vm_state, 8);
                        vm_state
                            .interpreter_state
                            .set_stack_var(*loc, pointer.into());
                    }
                    Type::Str => {
                        todo!();
                    }
                    _ => panic!("Illegal alloc operand: {:?}", ty),
                }

                vm_state.interpreter_state.ip += 1;
            }
            Instruction::LoadVar(dest, src, ty_idx) => {
                let ptr: Pointer = vm_state.interpreter_state.get_stack_var(*src).into();

                match current_module.get_type_by_id(*ty_idx).expect("type idx") {
                    Type::U64 => {
                        let value = primitives::load::<u64>(vm_state, ptr);
                        vm_state.interpreter_state.set_stack_var(*dest, value);
                    }
                    _ => panic!("Illegal load operand: {:?}", ty_idx),
                }
                vm_state.interpreter_state.ip += 1;
            }
            Instruction::LoadConst(_, _) => {
                vm_state.interpreter_state.ip += 1;
                unimplemented!();
            }
            Instruction::StoreVar(dest, src, ty_idx) => {
                let ptr: Pointer = vm_state.interpreter_state.get_stack_var(*dest).into();
                let value = vm_state.interpreter_state.get_stack_var(*src);

                match current_module.get_type_by_id(*ty_idx).expect("type idx") {
                    Type::U64 => unsafe {
                        primitives::store::<u64>(vm_state, ptr, value);
                    },
                    _ => panic!("Illegal load operand: {:?}", ty_idx),
                }
                vm_state.interpreter_state.ip += 1;
            }
            Instruction::Index(dest, ptr, idx, ty_idx) => {
                let idx = vm_state.interpreter_state.get_stack_var(*idx);
                let ptr = vm_state.interpreter_state.get_stack_var(*ptr).into();
                match current_module.get_type_by_id(*ty_idx).expect("type idx") {
                    Type::U64 | Type::Pointer(_) => {
                        let new_ptr = primitives::index::<u64>(vm_state, ptr, idx as usize);
                        let loaded_value = primitives::load::<u64>(vm_state, new_ptr);
                        vm_state
                            .interpreter_state
                            .set_stack_var(*dest, loaded_value);
                    }
                    ty => panic!("Illegal index operand: {:?}", ty),
                }

                vm_state.interpreter_state.ip += 1;
            }
            i => panic!("Illegal instruction: {:?}", i),
        }

        true
    }
}

struct Stack {
    slots: Vec<u64>,
}

impl Stack {
    pub fn new_with_framesize(framesize: usize) -> Stack {
        Stack {
            slots: vec![u64::MAX; framesize],
        }
    }
}

pub struct InterpreterState {
    stack: Stack,
    call_info: Vec<CallInfo>,
    ip: isize,
    func: FnId,
    module: ModuleId,
    state: InterpreterStatus,

    pub result: u64,
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
            result: 0,
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
        unsafe {
            self.stack.slots[0] = std::mem::transmute(args_ptr);
        }
    }

    fn get_stack_var(&mut self, var: VarId) -> u64 {
        let ci = self.call_info.last().unwrap();
        self.stack.slots[ci.var_base as usize + var as usize]
    }

    fn set_stack_var(&mut self, var: VarId, val: u64) {
        let ci = self.call_info.last().unwrap();
        self.stack.slots[ci.var_base as usize + var as usize] = val;
    }

    fn increment_function_counter(&mut self, fun: FnId) {
        let ctr = self.function_counters.entry(fun).or_insert(0);
        *ctr += 1;
    }
}
