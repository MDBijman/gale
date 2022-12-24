use log::{log_enabled, trace};

use crate::bytecode::*;
use crate::dialect::{Instruction, Var};
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
    pub called_by_native: bool,
    pub framesize: usize,
    pub var_base: isize,
    pub result_var: Var,
    pub prev_ip: isize,
    pub prev_fn: FnId,
    pub prev_mod: ModuleId,
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

    pub unsafe fn value_pointer(&self) -> Pointer {
        match self {
            Value::UI64(v) => std::mem::transmute::<_, Pointer>(v),
            Value::Bool(b) => std::mem::transmute::<_, Pointer>(b),
            Value::Tuple(t) => std::mem::transmute::<_, Pointer>(t),
            Value::Pointer(p) => std::mem::transmute::<_, Pointer>(p),
            Value::CallTarget(_) => todo!(),
            Value::Uninit => Pointer::null(),
            Value::Unit => Pointer::null(),
        }
    }
}

impl Into<u64> for Value {
    fn into(self) -> u64 {
        match self {
            Value::UI64(v) => v,
            Value::Bool(b) => b as u64,
            Value::CallTarget(t) => (((t.function as u32) << 16) | t.module as u32) as u64,
            Value::Pointer(_) => panic!(),
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
            Value::Pointer(v) => write!(f, "{:#x}", v.raw as usize),
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
        arg: Pointer,
    ) {
        let func = vm
            .module_loader
            .get_module_by_id(target.module)
            .expect("missing module impl")
            .get_function_by_id(target.function)
            .expect("function id");

        let bytecode = func.ast_impl().expect("bytecode");

        let stack_size = bytecode.varcount;
        let new_base = state.stack.slots.len() as isize;

        state.stack.slots.resize(
            new_base as usize + stack_size as usize + 1, /* <- for nfi result */
            Value::Uninit,
        );

        let fun_ci = CallInfo {
            called_by_native: true,
            framesize: stack_size as usize,
            var_base: new_base + 1,
            result_var: Var(0),
            prev_ip: isize::MAX,
            prev_fn: FnId::MAX,
            prev_mod: ModuleId::MAX,
        };

        state.call_info.push(fun_ci);

        panic!("Need to deref this var");
        state.set_stack_var(Var(0), Value::Pointer(arg));

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
            state.status = InterpreterStatus::Finished;
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
    ) -> &'a (dyn Instruction) {
        module
            .get_function_by_id(state.func)
            .expect("function")
            .ast_impl()
            .expect("ast impl")
            .instructions[state.ip as usize]
            .as_ref()
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
        assert_eq!(
            vm_state.interpreter_state.status,
            InterpreterStatus::Running
        );

        let current_module_opt = vm
            .module_loader
            .get_module_by_id(vm_state.interpreter_state.module);

        let current_module = current_module_opt.unwrap();

        let current_instruction = self.get_instruction(current_module, &vm_state.interpreter_state);

        vm_state.interpreter_state.instr_counter += 1;

        // Extremely detailed and slow!
        if log_enabled!(target: "interpreter", log::Level::Trace) {
            let mut from_states = vec![];
            let mut to_states = vec![];

            let reads_from = current_instruction.reads();
            for v in reads_from {
                let var = vm_state.interpreter_state.get_stack_var(v);
                from_states.push(format!("{}: {}", v, var));
            }

            let r = current_instruction.interpret(vm, vm_state);

            let writes_to = current_instruction.writes();
            for v in writes_to {
                let var = vm_state.interpreter_state.get_stack_var(v);
                to_states.push(format!("{}: {}", v, var));
            }

            trace!(target: "interpreter", "{} [{}] -> [{}]", current_instruction, from_states.join(", "), to_states.join(","));

            r
        } else {
            current_instruction.interpret(vm, vm_state)
        }
    }
}

pub struct Stack {
    slots: Vec<Value>,
}

impl Stack {
    pub fn new_with_framesize(framesize: usize) -> Stack {
        Stack {
            slots: vec![Value::Uninit; framesize],
        }
    }

    pub fn alloc(&mut self, size: usize) {
        self.slots.resize(self.slots.len() + size, Value::Uninit)
    }

    pub fn dealloc(&mut self, size: usize) {
        self.slots.truncate(self.slots.len() - size)
    }

    pub fn len(&self) -> usize {
        return self.slots.len();
    }
}

pub struct InterpreterState {
    pub stack: Stack,
    pub call_info: Vec<CallInfo>,
    pub ip: isize,
    pub func: FnId,
    pub module: ModuleId,
    pub status: InterpreterStatus,

    pub result: Value,
    pub instr_counter: u64,
    pub function_counters: HashMap<FnId, u64>,
}

impl InterpreterState {
    fn get_current_frame_base(&self) -> usize {
        let ci = self.call_info.last().unwrap();
        self.get_stack_size() - ci.framesize
    }

    pub fn new(entry: CallTarget, entry_framesize: u64) -> InterpreterState {
        InterpreterState {
            stack: Stack::new_with_framesize(entry_framesize as usize),
            call_info: vec![CallInfo {
                called_by_native: true,
                var_base: 0,
                framesize: entry_framesize as usize,
                result_var: Var(u8::MAX),
                prev_fn: FnId::MAX,
                prev_ip: isize::MAX,
                prev_mod: ModuleId::MAX,
            }],
            ip: 0,
            status: InterpreterStatus::Created,
            result: Value::Uninit,
            instr_counter: 0,
            function_counters: HashMap::new(),
            module: entry.module,
            func: entry.function,
        }
    }

    pub fn init(&mut self, entry: CallTarget, args_ptr: Pointer) {
        self.status = InterpreterStatus::Running;
        self.module = entry.module;
        self.func = entry.function;
        self.stack.slots[0] = Value::Pointer(args_ptr);
    }

    pub fn init_empty(&mut self) {
        self.status = InterpreterStatus::Running;
    }

    pub fn get_stack_var_raw_index(&self, idx: usize) -> &Value {
        &self.stack.slots[idx]
    }

    pub fn set_stack_var_raw_index(&mut self, idx: usize, val: Value) {
        self.stack.slots[idx] = val;
    }

    pub fn get_stack_var(&self, var: Var) -> &Value {
        let ci = self.call_info.last().unwrap();
        &self.stack.slots[ci.var_base as usize + var.0 as usize]
    }

    pub fn get_stack_var_mut(&mut self, var: Var) -> &mut Value {
        let ci = self.call_info.last().unwrap();
        &mut self.stack.slots[ci.var_base as usize + var.0 as usize]
    }

    pub fn set_stack_var(&mut self, var: Var, val: Value) {
        let ci = self.call_info.last().unwrap();
        self.stack.slots[ci.var_base as usize + var.0 as usize] = val;
    }

    pub fn get_stack_size(&self) -> usize {
        self.stack.slots.len()
    }
}
