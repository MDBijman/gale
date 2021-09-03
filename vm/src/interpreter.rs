use crate::bytecode::*;

#[derive(Debug)]
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
    heap: Vec<Value>,
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
            },
            _ => panic!("Invalid expression")
        }
    }

    fn step(&mut self) {
        if !self.is_running() { return; }

        let frame: &mut Frame = self.callstack.last_mut().unwrap();
        let current_function = self.code.functions.get(&frame.name).unwrap(); 
        let current_instruction = current_function
            .instructions
            .get(frame.pc.instruction as usize).unwrap();

        //println!("{:?}", current_instruction);

        match current_instruction {
            Instruction::Set(Location::Var(loc), expr) => {
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
            Instruction::Jmp(lbl) => {
                match lbl {
                    Location::Lbl(lbl) => {
                        let target = current_function.jump_table.get(lbl).unwrap();
                        frame.pc.instruction = *target;
                    },
                    _ => panic!("Invalid jump target")
                }
            },
            Instruction::JmpIf(lbl, cond) => {
                match lbl {
                    Location::Lbl(lbl) => {
                        match VM::interp_expr(frame, cond) {
                            Value::U64(0) => {
                                frame.pc.instruction += 1;
                            },
                            Value::U64(_) => {
                                let target = current_function.jump_table.get(lbl).unwrap();
                                frame.pc.instruction = *target;
                            },
                            _ => panic!("Inavlid expression result")
                        }
                    },
                    _ => panic!("Invalid jump target")
                }
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
                    },
                    Op::Eq => {
                        let rhs = frame.stack.pop().unwrap();
                        let lhs = frame.stack.pop().unwrap();
                        let res = match (lhs, rhs) {
                            (Value::U64(l), Value::U64(r)) if l == r => Value::U64(1),
                            (Value::U64(l), Value::U64(r)) if l != r => Value::U64(0),
                            (Value::Str(a), Value::Str(b)) if a == b => Value::U64(1),
                            (Value::Str(a), Value::Str(b)) if a != b => Value::U64(0),
                            ops => panic!("Invalid operands {:?}", ops)
                        };
                        frame.stack.push(res);
                    },
                    Op::Not => {
                        let operand = frame.stack.pop().unwrap();
                        let res = match operand {
                            Value::U64(0) => Value::U64(1),
                            Value::U64(n) => Value::U64(0),
                            _ => panic!("Invalid operands")
                        };
                        frame.stack.push(res);
                    }
                }
                frame.pc.instruction += 1;
            },
            Instruction::Lbl(_) => {
                frame.pc.instruction += 1;
            },
            Instruction::Alloc(Location::Var(loc), ty) => {
                frame.variables[*loc as usize] = Value::Pointer(self.heap.len() as u64);

                match ty {
                    Type::U64() => self.heap.push(Value::U64(0)),
                    _ => panic!("Invalid operand")
                }

                frame.pc.instruction += 1;
            },
            Instruction::Load(Location::Var(dest), Location::Var(src)) => {
                let loaded_value = frame.variables[*src as usize].clone();
                match loaded_value {
                    Value::Pointer(idx) => frame.variables[*dest as usize] = self.heap[idx as usize].clone(),
                    _ => panic!("Unexpected loaded value")
                }
                frame.pc.instruction += 1;
            },
            Instruction::LoadConst(Location::Var(dest), idx) => {
                frame.variables[*dest as usize] = self.code.constants[*idx as usize].clone();
                frame.pc.instruction += 1;
            },
            Instruction::Store(Location::Var(dest), e) => {
                let value = VM::interp_expr(frame, e);
                match frame.variables[*dest as usize] {
                    Value::Pointer(loc) => self.heap[loc as usize] = value,
                    _ => panic!("Invalid operand")
                };
                frame.pc.instruction += 1;
            },
            Instruction::Index(Location::Var(dest), Location::Var(val), Location::Var(off)) => {
                let idx = match &frame.variables[*off as usize] {
                    Value::U64(n) => n,
                    v => panic!("Illegal index {:?}", v)
                };

                let value = &frame.variables[*val as usize];

                let result = match value {
                    Value::Array(elems) => elems[*idx as usize].clone(),
                    Value::Str(str) => Value::U64(str.as_bytes()[*idx as usize] as u64),
                    _ => panic!("Illegal index into {:?}", value)
                };
                frame.variables[*dest as usize] = result;
                frame.pc.instruction += 1;
            }
            i => panic!("Invalid instruction: {:?}", i)
        }
    }
}

pub fn run(m: Module, arg: &str) -> Value {
    let initial_frame_size = m.functions.get("main").unwrap().meta.vars;

    let mut machine: VM = VM {
        code: m,
        callstack: vec![Frame {
            pc: ProgramCounter { instruction: 0 },
            name: String::from("main"),
            variables: vec![Value::U64(u64::MAX); initial_frame_size as usize],
            stack: vec![]
        }],
        state: VMState::Running,
        heap: Vec::new(),
        result: Value::U64(0)
    };

    *machine.callstack.last_mut().unwrap().variables.first_mut().unwrap() = Value::Array(vec![Value::Str(String::from(arg))]);

    while machine.is_running() {
        machine.step();
    }

    machine.result
}
