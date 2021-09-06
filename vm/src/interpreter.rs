use crate::bytecode::*;

#[derive(Debug)]
struct ProgramCounter {
    instruction: u64,
}

struct Frame {
    pc: ProgramCounter,
    fn_idx: usize,
    variables: Vec<Value>,
    stack: Vec<Value>,
}

enum VMState {
    Running,
    Finished,
}

struct VM {
    callstack: Vec<Frame>,
    code: Module,
    state: VMState,
    heap: Vec<Value>,
    result: Value,
}

impl VM {
    fn is_running(&self) -> bool {
        match self.state {
            VMState::Running => true,
            _ => false,
        }
    }

    fn interp_expr(frame: &Frame, expr: &Expression) -> Value {
        match expr {
            Expression::Array(exprs) => {
                let vs: Vec<Value> = exprs.iter().map(|e| VM::interp_expr(frame, e)).collect();

                Value::Array(vs)
            }
            Expression::Var(Location::Var(loc)) => {
                frame.variables.get(*loc as usize).unwrap().clone()
            }
            Expression::Value(v) => v.clone(),
            _ => panic!("Illegal expression"),
        }
    }

    fn step(&mut self) {
        if !self.is_running() {
            return;
        }

        let frame: &mut Frame = self.callstack.last_mut().unwrap();
        let current_function = &self.code.functions[frame.fn_idx];
        let current_instruction = current_function
            .instructions
            .get(frame.pc.instruction as usize)
            .unwrap();

        match current_instruction {
            Instruction::Set(Location::Var(loc), expr) => {
                let val = VM::interp_expr(frame, expr);
                frame.variables[*loc as usize] = val;
                frame.pc.instruction += 1;
            }
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

                let call_instruction = &self
                    .code
                    .functions[inner_frame.fn_idx]
                    .instructions[pc as usize];

                match call_instruction {
                    Instruction::DirectCall(Location::Var(loc), _, _) => {
                        inner_frame.variables[*loc as usize] = val;
                        inner_frame.pc.instruction += 1;
                    }
                    _ => panic!("Expected direct call"),
                }
            }
            Instruction::Print(expr) => {
                let val = VM::interp_expr(frame, expr);
                println!("{}", val);
                frame.pc.instruction += 1;
            }
            Instruction::DirectCall(_, loc, exprs) => {
                let loc_idx = match loc {
                    Location::FnLbl(l) => l,
                    _ => panic!("Illegal direct call location"),
                };

                let func = &self.code.functions[*loc_idx];
                let stack_size = func.meta.vars;

                let mut new_frame: Frame = Frame {
                    pc: ProgramCounter { instruction: 0 },
                    fn_idx: *loc_idx,
                    variables:  vec![Value::Null; stack_size as usize],
                    stack: vec![],
                };

                let mut i = 0;
                for e in exprs {
                    new_frame.variables[i] = VM::interp_expr(frame, e);
                    i += 1;
                }

                self.callstack.push(new_frame);
            }
            Instruction::Jmp(lbl) => match lbl {
                Location::Lbl(lbl) => {
                    let target = current_function.jump_table.get(lbl).unwrap();
                    frame.pc.instruction = *target;
                }
                _ => panic!("Illegal jump target"),
            },
            Instruction::JmpIf(lbl, cond) => match lbl {
                Location::Lbl(lbl) => match VM::interp_expr(frame, cond) {
                    Value::U64(0) => {
                        frame.pc.instruction += 1;
                    }
                    Value::U64(_) => {
                        let target = current_function.jump_table.get(lbl).unwrap();
                        frame.pc.instruction = *target;
                    }
                    _ => panic!("Invalid expression result"),
                },
                _ => panic!("Illegal jump target"),
            },
            Instruction::JmpIfNot(lbl, cond) => match lbl {
                Location::Lbl(lbl) => match VM::interp_expr(frame, cond) {
                    Value::U64(0) => {
                        let target = current_function.jump_table.get(lbl).unwrap();
                        frame.pc.instruction = *target;
                    }
                    Value::U64(_) => {
                        frame.pc.instruction += 1;
                    }
                    _ => panic!("Invalid expression result"),
                },
                _ => panic!("Illegal jump target"),
            },
            Instruction::Pop(Location::Var(location)) => {
                let v = frame.stack.pop().unwrap();
                *frame.variables.get_mut(*location as usize).unwrap() = v;
                frame.pc.instruction += 1;
            }
            Instruction::Push(expr) => {
                let v = VM::interp_expr(frame, expr);
                frame.stack.push(v);
                frame.pc.instruction += 1;
            }
            Instruction::UnOp(op) => {
                let operand = frame.stack.pop().unwrap();

                match op {
                    UnOp::Not => {
                        let res = match operand {
                            Value::U64(0) => Value::U64(1),
                            Value::U64(_) => Value::U64(0),
                            _ => panic!("Illegal operands"),
                        };
                        frame.stack.push(res);
                    }
                }

                frame.pc.instruction += 1;
            }
            Instruction::BinOp(op) => {
                let rhs = frame.stack.pop().unwrap();
                let lhs = frame.stack.pop().unwrap();

                match op {
                    BinOp::Add => {
                        let res = match (lhs, rhs) {
                            (Value::U64(l), Value::U64(r)) => Value::U64(l + r),
                            (lhs, rhs) => panic!("Illegal add operands: {} {}", lhs, rhs),
                        };
                        frame.stack.push(res);
                    }
                    BinOp::Sub => {
                        let res = match (lhs, rhs) {
                            (Value::U64(l), Value::U64(r)) => Value::U64(l - r),
                            (lhs, rhs) => panic!("Illegal sub operands: {} {}", lhs, rhs),
                        };
                        frame.stack.push(res);
                    }
                    BinOp::Eq => {
                        let res = match (lhs, rhs) {
                            (Value::U64(l), Value::U64(r)) if l == r => Value::U64(1),
                            (Value::U64(l), Value::U64(r)) if l != r => Value::U64(0),
                            (Value::Str(a), Value::Str(b)) if a == b => Value::U64(1),
                            (Value::Str(a), Value::Str(b)) if a != b => Value::U64(0),
                            (lhs, rhs) => panic!("Illegal eq operands: {} {}", lhs, rhs),
                        };
                        frame.stack.push(res);
                    }
                    BinOp::Lt => {
                        let res = match (lhs, rhs) {
                            (Value::U64(l), Value::U64(r)) if l < r => Value::U64(1),
                            (Value::U64(_), Value::U64(_)) => Value::U64(0),
                            (lhs, rhs) => panic!("Illegal lt operands: {} {}", lhs, rhs),
                        };
                        frame.stack.push(res);
                    }
                }

                frame.pc.instruction += 1;
            }
            Instruction::Lbl(_) => {
                frame.pc.instruction += 1;
            }
            Instruction::Alloc(Location::Var(loc), ty) => {
                frame.variables[*loc as usize] = Value::Pointer(self.heap.len() as u64);

                match ty {
                    Type::U64() => self.heap.push(Value::U64(0)),
                    _ => panic!("Illegal alloc operand: {:?}", ty),
                }

                frame.pc.instruction += 1;
            }
            Instruction::Load(Location::Var(dest), Location::Var(src)) => {
                let loaded_value = frame.variables[*src as usize].clone();
                match loaded_value {
                    Value::Pointer(idx) => {
                        frame.variables[*dest as usize] = self.heap[idx as usize].clone()
                    }
                    _ => panic!("Illegal load operand: {:?}", loaded_value),
                }
                frame.pc.instruction += 1;
            }
            Instruction::LoadConst(Location::Var(dest), idx) => {
                frame.variables[*dest as usize] = self.code.constants[*idx as usize].clone();
                frame.pc.instruction += 1;
            }
            Instruction::Store(Location::Var(dest), e) => {
                let value = VM::interp_expr(frame, e);
                match &frame.variables[*dest as usize] {
                    Value::Pointer(loc) => self.heap[*loc as usize] = value,
                    v => panic!("Illegal store operand: {:?}", v),
                };
                frame.pc.instruction += 1;
            }
            Instruction::Index(Location::Var(dest), Location::Var(val), Location::Var(off)) => {
                let idx = match &frame.variables[*off as usize] {
                    Value::U64(n) => n,
                    v => panic!("Illegal index {:?}", v),
                };

                let value = &frame.variables[*val as usize];

                let result = match value {
                    Value::Array(elems) => elems[*idx as usize].clone(),
                    Value::Str(str) => Value::U64(str.as_bytes()[*idx as usize] as u64),
                    _ => panic!("Illegal index into {:?}", value),
                };
                frame.variables[*dest as usize] = result;
                frame.pc.instruction += 1;
            }
            i => panic!("Illegal instruction: {:?}", i),
        }
    }
}

pub fn run(m: Module, arg: &str) -> Value {
    let initial_fn_idx = m.functions.iter().position(|f| f.name == "main").unwrap();
    let initial_frame_size = m.functions[initial_fn_idx].meta.vars;

    let mut machine: VM = VM {
        code: m,
        callstack: vec![Frame {
            pc: ProgramCounter { instruction: 0 },
            fn_idx: initial_fn_idx,
            variables: vec![Value::U64(u64::MAX); initial_frame_size as usize],
            stack: vec![],
        }],
        state: VMState::Running,
        heap: Vec::with_capacity(5),
        result: Value::U64(0),
    };

    let args = arg
        .split(" ")
        .map(|s| Value::Str(String::from(s)))
        .collect::<Vec<_>>();
    *machine
        .callstack
        .last_mut()
        .unwrap()
        .variables
        .first_mut()
        .unwrap() = Value::Array(args);

    while machine.is_running() {
        machine.step()
    }

    machine.result
}
