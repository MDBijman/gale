use crate::bytecode::{FnLbl, Module};
use crate::interpreter::{Interpreter, InterpreterState};
use crate::jit::{JITEngine, JITState};
use std::time;

pub struct VMState {
    pub interpreter_state: InterpreterState,
    pub jitter_state: JITState,
}

impl VMState {
    pub fn new(first_function: FnLbl, first_frame_size: u64, arg: u64) -> VMState {
        VMState {
            interpreter_state: InterpreterState::new(first_function, first_frame_size, arg),
            jitter_state: JITState::new(),
        }
    }
}

pub struct VM<'a> {
    pub module: &'a Module,
    pub interpreter: Interpreter<'a>,
    pub jitter: JITEngine,
}

impl<'a> VM<'a> {
    pub fn new(m: &'a Module) -> VM {
        VM {
            module: m,
            interpreter: Interpreter::new(m),
            jitter: JITEngine::default(),
        }
    }

    pub fn run(&self, arg: &str, jit: bool) {
        let args = arg
            .split(" ")
            .map(|s| s.parse::<u64>().unwrap())
            .collect::<Vec<_>>();
        assert_eq!(args.len(), 1); // Just limited to 1 number argument for now

        let main_idx = self.module.find_function_id("main").unwrap();
        let main_frame_size = self.module.get_function_by_id(main_idx).unwrap().meta.vars;

        let start = time::SystemTime::now();
        println!("Jit enabled: {}", jit);
        if jit {
            /* Compile the main function */
            let mut state = VMState::new(main_idx, main_frame_size.into(), args[0]);
            let main_idx = self.module.find_function_id("main").unwrap();
            JITEngine::compile(
                &mut state.jitter_state,
                &self.module.functions[main_idx as usize],
            );

            // Compile fib
            if let Some(fib_idx) = self.module.find_function_id("fib") {
                println!("--- Compiling fib");
                JITEngine::compile(
                    &mut state.jitter_state,
                    &self.module.functions[fib_idx as usize],
                );
            }

            /* Run the main function in compiled mode */
            let res = JITEngine::call_if_compiled(self, &mut state, "main", args[0]);
            println!("--- out: {:?}", res.unwrap());
            println!("--- time: {}ms", start.elapsed().unwrap().as_millis());
        } else {
            /* Run the program in interpreted mode */
            let mut state = VMState::new(main_idx, main_frame_size.into(), args[0]);
            while self.interpreter.step(&mut state.interpreter_state) {}
            println!("--- out: {}", state.interpreter_state.result);
            let end = start.elapsed().unwrap().as_millis();
            println!("--- time: {}ms", end);
            println!("--- instrs: {}", state.interpreter_state.instr_counter);
            println!(
                "--- MOp/s: {:.2}",
                (state.interpreter_state.instr_counter as f64 / end as f64) / 1000 as f64
            );
        }
    }
}
