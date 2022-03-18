use std::io::Write;
use vm_internal::dialect::Var;

use crate::interpreter::Value;
use crate::vm::{VMState, VM};

enum DebugCommand {
    Quit,
    Continue,
    DumpMemory,
    Expr(DebugExpression),
}

enum DebugExpression {
    Var(Var),
    Deref(Box<Self>),
}

pub struct MyDebugger {}

impl MyDebugger {
    fn parse_command(&self, command: String) -> DebugCommand {
        if command.len() == 0 || command == "q" {
            return DebugCommand::Quit;
        }

        if command == "memdump" {
            return DebugCommand::DumpMemory;
        }

        match self.parse_expression(command) {
            Some(e) => DebugCommand::Expr(e),
            None => DebugCommand::Continue,
        }
    }

    fn parse_expression(&self, command: String) -> Option<DebugExpression> {
        if command.chars().next().unwrap() == '*' {
            if let Some(inner) = self.parse_expression(command[1..].to_string()) {
                return Some(DebugExpression::Deref(Box::from(inner)));
            } else {
                None
            }
        } else if let Ok(v) = command.parse::<u8>() {
            return Some(DebugExpression::Var(Var(v)));
        } else {
            None
        }
    }

    fn interp_expression(&self, vm: &VM, vm_state: &mut VMState, expr: DebugExpression) -> Value {
        match expr {
            DebugExpression::Var(v) => vm_state.interpreter_state.get_stack_var(v).clone(),
            DebugExpression::Deref(d) => {
                let ptr = *self
                    .interp_expression(vm, vm_state, *d)
                    .as_pointer()
                    .expect("invalid type");
                let value = unsafe { vm_state.heap.load::<u64>(ptr) };
                Value::UI64(value)
            }
        }
    }
}

impl crate::vm::VMDebugger for MyDebugger {
    fn inspect(&self, vm: &VM, vm_state: &mut VMState) {
        let current_module = &vm
            .module_loader
            .get_module_by_id(vm_state.interpreter_state.module)
            .expect("missing module impl");

        let current_instruction = vm
            .interpreter
            .get_instruction(current_module, &vm_state.interpreter_state);

        /*
         * Small command prompt for inspecting vm state
         */

        use std::io::{stdin, stdout};

        println!("{:?}", current_instruction);

        loop {
            print!("> ");

            stdout().flush().unwrap();

            let mut s = String::new();
            stdin().read_line(&mut s).unwrap();

            // Remove end of line characters
            if let Some('\n') = s.chars().next_back() {
                s.pop();
            }
            if let Some('\r') = s.chars().next_back() {
                s.pop();
            }

            match self.parse_command(s) {
                DebugCommand::Quit => break,
                DebugCommand::Continue => continue,
                DebugCommand::Expr(e) => {
                    println!("{}", self.interp_expression(vm, vm_state, e))
                }
                DebugCommand::DumpMemory => {
                    println!("{}", vm_state.heap.heap_dump());
                }
            }
        }
    }
}
