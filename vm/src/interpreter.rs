use crate::bytecode::*;

struct ProgramCounter {
    instruction: u64
}

struct Frame {
    pc: ProgramCounter,
    name: String,
    variables: Vec<Value>
}

enum VMState {
    Running,
    Finished
}

struct VM {
    callstack: Vec<Frame>,
    code: Module,
    state: VMState,
    result: Value
}

impl VM {
    fn is_running(&self) -> bool {
        match self.state {
            VMState::Running => true,
            _ => false
        }
    }

    fn interp_expr(frame: &Frame, expr: &Expression) -> Value {
        match expr {
            Expression::Array(exprs) => {
                let vs: Vec<Value> = exprs.iter()
                    .map(|e| VM::interp_expr(frame, e))
                    .collect();

                Value::Array(vs)
            },
            Expression::Var(Location::Var(loc)) => {
               frame.variables.get(*loc as usize).unwrap().clone()
            },
            Expression::Value(v) => {
                v.clone()
            }
        }
    }

    fn step(&mut self) {
        if !self.is_running() { return; }

        let frame: &mut Frame = self.callstack.last_mut().unwrap();
        
        let current_instruction = self.code.functions
            .get(&frame.name).unwrap().instructions
            .get(frame.pc.instruction as usize).unwrap();

        match current_instruction {
            Instruction::Store(Location::Var(loc), expr) => {
                let val = VM::interp_expr(frame, expr);
                frame.variables[*loc as usize] = val;
                frame.pc.instruction += 1;
            },
            Instruction::Return(expr) => {
                let val = VM::interp_expr(frame, expr);
                self.callstack.pop();

                // Finished execution
                if self.callstack.len() == 0 {
                    self.state = VMState::Finished;
                    self.result = val;
                    return;
                }

                let mut inner_frame = self.callstack.last_mut().unwrap();
                let pc = inner_frame.pc.instruction;

                let call_instruction = self.code
                    .functions.get(&inner_frame.name).unwrap()
                    .instructions.get(pc as usize).unwrap();

                match call_instruction {
                    Instruction::Call(Location::Var(loc), _, _) => {
                        inner_frame.variables[*loc as usize] = val;
                        inner_frame.pc.instruction += 1;
                    }
                    _ => panic!("Expected Call")
                }
            },
            Instruction::Print(expr) => {
                let val = VM::interp_expr(frame, expr);
                println!("{}", val);
                frame.pc.instruction += 1;
            },
            Instruction::Call(_, name, exprs) => {
                let new_frame: Frame = Frame {
                    pc: ProgramCounter { instruction: 0 },
                    name: name.clone(),
                    variables: exprs.iter().map(|e| VM::interp_expr(frame, e)).collect()
                };

                self.callstack.push(new_frame);
            }
        }
    }
}

pub fn run(m: Module) -> Value {
    let initial_frame_size = m.functions.get("main").unwrap().meta.vars;

    let mut machine: VM = VM {
        code: m,
        callstack: vec![Frame {
            pc: ProgramCounter { instruction: 0 },
            name: String::from("main"),
            variables: vec![Value::U64(0); initial_frame_size as usize]
        }],
        state: VMState::Running,
        result: Value::U64(0)
    };

    while machine.is_running() {
        machine.step();
    }

    machine.result
}
