use crate::bytecode::*;
use crate::memory::{Heap, Pointer};

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
    result_var: Var,
    prev_ip: isize,
    prev_fn: isize,
}

pub struct Interpreter<'a> {
    code: &'a Module,
}

impl<'a> Interpreter<'a> {
    pub fn new(m: &Module) -> Interpreter {
        Interpreter { code: m }
    }

    pub fn push_native_caller_frame(&self, state: &mut InterpreterState, func: FnLbl, arg: u64) {
        let func = &self.code.functions[func as usize];
        let stack_size = func.varcount;

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
            prev_fn: isize::MAX,
        };

        state.call_info.push(fun_ci);

        state.set_stack_var(0, arg);

        state.ip = 0;
        state.func = func.location as isize;
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

    fn get_instruction(&self, func: isize, ip: isize) -> &'a Instruction {
        &self.code.functions[func as usize].instructions[ip as usize]
    }

    // Executes until the current function is finished, including further calls
    pub fn finish_function(&self, state: &mut InterpreterState, heap: &mut Heap) {
        let start_stack_size = state.call_info.len();
        assert_ne!(start_stack_size, 0);
        while self.step(state, heap) {
            if state.call_info.len() == start_stack_size - 1 {
                return;
            }
        }
    }

    //#[inline(never)] // <- uncomment for profiling with better source <-> asm map
    #[inline]
    pub fn step(&self, state: &mut InterpreterState, heap: &mut Heap) -> bool {
        assert_eq!(state.state, InterpreterStatus::Running);
        let current_instruction = self.get_instruction(state.func, state.ip);

        state.instr_counter += 1;

        //println!("{:?}", current_instruction);
        match current_instruction {
            Instruction::ConstU32(loc, n) => {
                state.set_stack_var(*loc, *n as u64);
                state.ip += 1;
            }
            Instruction::Copy(loc, src) => {
                let val = state.get_stack_var(*src);
                state.set_stack_var(*loc, val);
                state.ip += 1;
            }
            Instruction::EqVarVar(dest, a, b) => {
                let val_a = state.get_stack_var(*a);
                let val_b = state.get_stack_var(*b);
                state.set_stack_var(*dest, (val_a == val_b) as u64);
                state.ip += 1;
            }
            Instruction::LtVarVar(dest, a, b) => {
                let val_a = state.get_stack_var(*a);
                let val_b = state.get_stack_var(*b);
                state.set_stack_var(*dest, (val_a < val_b) as u64);
                state.ip += 1;
            }
            Instruction::SubVarVar(dest, a, b) => {
                let val_a = state.get_stack_var(*a);
                let val_b = state.get_stack_var(*b);
                state.set_stack_var(*dest, val_a - val_b);
                state.ip += 1;
            }
            Instruction::AddVarVar(dest, a, b) => {
                let val_a = state.get_stack_var(*a);
                let val_b = state.get_stack_var(*b);
                state.set_stack_var(*dest, val_a + val_b);
                state.ip += 1;
            }
            Instruction::Print(src) => {
                println!("{}", state.get_stack_var(*src));
                state.ip += 1;
            }
            Instruction::Return(src) => {
                let val = state.get_stack_var(*src);
                let ci = state.call_info.pop().unwrap();

                // Finished execution
                if state.call_info.len() == 0 {
                    state.state = InterpreterStatus::Finished;
                    state.result = val;
                    return false;
                }

                state.ip = ci.prev_ip;
                state.func = ci.prev_fn;

                // Native function
                if ci.called_by_native {
                    state.stack.slots.truncate(ci.var_base as usize);

                    *state.stack.slots.last_mut().unwrap() = val;
                } else {
                    let new_ci = state.call_info.last().unwrap();
                    let new_func = &self.code.functions[state.func as usize];

                    state
                        .stack
                        .slots
                        .truncate(new_ci.var_base as usize + new_func.varcount as usize);

                    state.set_stack_var(ci.result_var, val);

                    state.ip += 1;
                }
            }
            Instruction::Call(r, loc, arg) => {
                let func = &self.code.functions[*loc as usize];
                state.increment_function_counter(func.location);
                let stack_size = func.varcount;

                let new_base = state.stack.slots.len() as isize;

                state
                    .stack
                    .slots
                    .resize(new_base as usize + stack_size as usize, u64::MAX);

                let new_ci = CallInfo {
                    called_by_native: false,
                    var_base: new_base,
                    result_var: *r,
                    prev_ip: state.ip,
                    prev_fn: state.func,
                };

                let val = state.get_stack_var(*arg);
                state.call_info.push(new_ci);
                state.set_stack_var(0, val);

                state.ip = 0;
                state.func = func.location as isize;
            }
            Instruction::Jmp(instr) => {
                state.ip += *instr as isize;
            }
            Instruction::JmpIf(instr, src) => match state.get_stack_var(*src) {
                0 => {
                    state.ip += 1;
                }
                _ => {
                    state.ip += *instr as isize;
                }
            },
            Instruction::JmpIfNot(instr, src) => match state.get_stack_var(*src) {
                0 => {
                    state.ip += *instr as isize;
                }
                _ => {
                    state.ip += 1;
                }
            },
            Instruction::Lbl => {
                state.ip += 1;
            }
            Instruction::Nop => {
                state.ip += 1;
            }
            Instruction::Alloc(loc, ty) => {
                match self.code.get_type_by_id(*ty).expect("type idx") {
                    Type::U64 => {
                        let ptr = heap.allocate(8);
                        state.set_stack_var(*loc, ptr.into());
                    }
                    Type::Str => {
                        todo!();
                    }
                    _ => panic!("Illegal alloc operand: {:?}", ty),
                }

                state.ip += 1;
            }
            Instruction::LoadVar(dest, src, ty_idx) => {
                let ptr: Pointer = state.get_stack_var(*src).into();

                match self.code.get_type_by_id(*ty_idx).expect("type idx") {
                    Type::U64 => {
                        let res = unsafe { *(heap.raw(ptr.into()) as *mut u64) };
                        state.set_stack_var(*dest, res);
                    }
                    _ => panic!("Illegal load operand: {:?}", ty_idx),
                }
                state.ip += 1;
            }
            Instruction::LoadConst(_, _) => {
                state.ip += 1;
                unimplemented!();
            }
            Instruction::StoreVar(dest, src, ty_idx) => {
                let ptr: Pointer = state.get_stack_var(*dest).into();
                let value = state.get_stack_var(*src);

                match self.code.get_type_by_id(*ty_idx).expect("type idx") {
                    Type::U64 => unsafe {
                        let raw = heap.raw(ptr) as *mut u64;
                        *raw = value;
                    },
                    _ => panic!("Illegal load operand: {:?}", ty_idx),
                }
                state.ip += 1;
            }
            Instruction::Index(dest, ptr, idx) => {
                let idx = state.get_stack_var(*idx);
                let ptr = state.get_stack_var(*ptr).into();
                let new_ptr = heap.index(ptr, idx as usize);
                state.stack.slots[*dest as usize] = new_ptr.into();
                state.ip += 1;
                //unimplemented!("Incorrect, needs type to determine offset multiplier")
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
    func: isize,
    state: InterpreterStatus,

    pub result: u64,
    pub instr_counter: u64,
    pub function_counters: HashMap<FnLbl, u64>,
}

impl InterpreterState {
    pub fn new(
        entry: FnLbl,
        entry_framesize: u64,
    ) -> InterpreterState {
        InterpreterState {
            stack: Stack::new_with_framesize(entry_framesize as usize),
            call_info: vec![CallInfo {
                called_by_native: true,
                var_base: 0,
                result_var: Var::MAX,
                prev_fn: isize::MAX,
                prev_ip: isize::MAX,
            }],
            ip: 0,
            func: entry as isize,
            state: InterpreterStatus::Created,
            result: 0,
            instr_counter: 0,
            function_counters: HashMap::new(),
        }
    }

    pub fn init(&mut self, args_ptr: Pointer) {
        self.state = InterpreterStatus::Running;
        unsafe {
            self.stack.slots[0] = std::mem::transmute(args_ptr);
        }
    }

    fn get_stack_var(&mut self, var: Var) -> u64 {
        let ci = self.call_info.last().unwrap();
        self.stack.slots[ci.var_base as usize + var as usize]
    }

    fn set_stack_var(&mut self, var: Var, val: u64) {
        let ci = self.call_info.last().unwrap();
        self.stack.slots[ci.var_base as usize + var as usize] = val;
    }

    fn increment_function_counter(&mut self, fun: FnLbl) {
        let ctr = self.function_counters.entry(fun).or_insert(0);
        *ctr += 1;
    }
}
