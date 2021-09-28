use crate::bytecode::{ControlFlowGraph, FnLbl, Module};
use crate::interpreter::{Interpreter, InterpreterState};
use crate::jit::{JITEngine, JITState};
use std::time;

pub struct VMState {
    pub interpreter_state: InterpreterState,
    pub jit_state: JITState,
    pub result: Option<u64>,
}

impl VMState {
    pub fn new(first_function: FnLbl, first_frame_size: u64, arg: u64) -> VMState {
        VMState {
            interpreter_state: InterpreterState::new(first_function, first_frame_size, arg),
            jit_state: JITState::default(),
            result: None,
        }
    }
}

pub struct VM<'a> {
    pub module: &'a Module,
    pub interpreter: Interpreter<'a>,
    pub jit: JITEngine,
    debug: bool,
}

impl<'a> VM<'a> {
    pub fn new(m: &'a Module) -> VM {
        VM {
            module: m,
            interpreter: Interpreter::new(m),
            jit: JITEngine::default(),
            debug: false,
        }
    }

    pub fn new_debug(m: &'a Module) -> VM {
        VM {
            module: m,
            interpreter: Interpreter::new(m),
            jit: JITEngine::default(),
            debug: true,
        }
    }

    pub fn run(&self, args: Vec<&str>, jit: bool) -> VMState {
        assert_eq!(args.len(), 1); // FIXME limited to 1 number argument for now
        let arg = args[0].parse::<u64>().expect("u64 arg");

        let main_idx = self.module.find_function_id("main").unwrap();
        let main_frame_size = self.module.get_function_by_id(main_idx).unwrap().varcount;

        let start = time::SystemTime::now();

        if self.debug {
            println!("Jit enabled: {}", jit);
        }

        let mut state = VMState::new(main_idx, main_frame_size.into(), arg);

        if jit {
            /* Compile the main function */
            let main_idx = self.module.find_function_id("main").unwrap();
            JITEngine::compile(
                &mut state.jit_state,
                &self.module,
                &self.module.functions[main_idx as usize],
            );

            // Compile fib
            if let Some(idx) = self.module.find_function_id("fib") {
                if self.debug {
                    println!("Compiling fib");
                }

                let fun = &self.module.functions[idx as usize];
                let cfg = ControlFlowGraph::from_function(fun);

                if self.debug {
                    cfg.print_dot(&self.module.functions[idx as usize]);
                }

                JITEngine::compile(&mut state.jit_state, &self.module, fun);

                if self.debug {
                    println!(
                        "JIT Dump:\n{}",
                        JITEngine::to_hex_string(
                            &mut state.jit_state,
                            &self.module.functions[idx as usize]
                        )
                    );
                }
            }

            /* Run the main function in compiled mode */
            let res = JITEngine::call_if_compiled(self, &mut state, "main", arg);

            state.result = res;

            if self.debug {
                println!("jit time: {}ms", start.elapsed().unwrap().as_millis());
            }
        } else {
            /* Run the program in interpreted mode */
            while self.interpreter.step(&mut state.interpreter_state) {}
            
            state.result = Some(state.interpreter_state.result);

            if self.debug {
                let end = start.elapsed().unwrap().as_millis();
                println!("interp time: {}ms", end);
                println!("instrs: {}", state.interpreter_state.instr_counter);
                println!(
                    "MOp/s: {:.2}",
                    (state.interpreter_state.instr_counter as f64 / end as f64) / 1000 as f64
                );
            }
        }

        state
    }
}
