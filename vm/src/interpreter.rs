use crate::bytecode::*;

struct ProgramCounter {
    instruction: u64
}

struct Frame {
    pc: ProgramCounter,
    name: String,
    variables: Vec<Value>,
    stack: Vec<Value>
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

        // println!("{:?}", current_instruction);

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
                let func = self.code.functions.get(name).unwrap();
                let stack_size = func.meta.vars;
                let mut variables = vec![Value::Null; stack_size as usize];

                let mut i = 0;
                for e in exprs {
                    variables[i] = VM::interp_expr(frame, e);
                    i += 1;
                }

                let new_frame: Frame = Frame {
                    pc: ProgramCounter { instruction: 0 },
                    name: name.clone(),
                    variables: variables,
                    stack: vec![]
                };

                self.callstack.push(new_frame);
            },
            Instruction::Pop(Location::Var(location)) => {
                let v = frame.stack.pop().unwrap();
                *frame.variables.get_mut(*location as usize).unwrap() = v;
                frame.pc.instruction += 1;
            },
            Instruction::Push(expr) => {
                let v = VM::interp_expr(frame, expr);
                frame.stack.push(v);
                frame.pc.instruction += 1;
            },
            Instruction::Op(op) => {
                match op {
                    Op::Add => {
                        let rhs = frame.stack.pop().unwrap();
                        let lhs = frame.stack.pop().unwrap();
                        let res = match (lhs, rhs) {
                            (Value::U64(l), Value::U64(r)) => {
                                Value::U64(l + r)
                            },
                            _ => panic!("Invalid operands")
                        };
                        frame.stack.push(res);
                    },
                    Op::Sub => {
                        let rhs = frame.stack.pop().unwrap();
                        let lhs = frame.stack.pop().unwrap();
                        let res = match (lhs, rhs) {
                            (Value::U64(l), Value::U64(r)) => {
                                Value::U64(l - r)
                            },
                            _ => panic!("Invalid operands")
                        };
                        frame.stack.push(res);
                    }
                }
                frame.pc.instruction += 1;
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
            variables: vec![Value::U64(0); initial_frame_size as usize],
            stack: vec![]
        }],
        state: VMState::Running,
        result: Value::U64(0)
    };

    while machine.is_running() {
        machine.step();
    }

    machine.result
}
