use crate::bytecode::{ControlFlowGraph, FnLbl, Module};
use crate::interpreter::{Interpreter, InterpreterState};
use crate::jit::{JITEngine, JITState};
use crate::memory::{Heap, Pointer, POINTER_SIZE, ARRAY_HEADER_SIZE, STRING_HEADER_SIZE};
use std::time;

pub struct VMState {
    pub interpreter_state: InterpreterState,
    pub jit_state: JITState,
    pub result: Option<u64>,
    pub heap: Box<Heap>,
}

impl VMState {
    pub fn new(entry: FnLbl, entry_framesize: u64) -> VMState {
        VMState {
            interpreter_state: InterpreterState::new(entry, entry_framesize),
            jit_state: JITState::default(),
            result: None,
            heap: Box::new(Heap::default()),
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

    pub fn run(&self, args: Vec<String>, jit: bool) -> VMState {
        let main_idx = self.module.find_function_id("main").unwrap();
        let main_frame_size = self.module.get_function_by_id(main_idx).unwrap().varcount;

        if self.debug {
            println!("Jit enabled: {}", jit);
        }

        let mut state = VMState::new(main_idx, main_frame_size.into());

        let args_ptr = {
            // TODO abstract this kind of logic
            let arr_ptr = state.heap.allocate_array(args.len() * POINTER_SIZE);
            unsafe {
                let mut raw_arr_ptr = state.heap.raw(arr_ptr).offset(ARRAY_HEADER_SIZE as isize);

                for arg in args.iter() {
                    let str_ptr = state.heap.allocate_string(arg.len());
                    let mut raw_str_ptr: *mut u8 = state.heap.raw(str_ptr).offset(8);

                    for b in arg.as_bytes() {
                        *raw_str_ptr = *b;
                        raw_str_ptr = raw_str_ptr.add(1);
                    }

                    *raw_str_ptr = '\0' as u8;

                    *(raw_arr_ptr as *mut u64) = str_ptr.into();

                    raw_arr_ptr = raw_arr_ptr.add(1);
                }
            }

            arr_ptr
        };

        if jit {
            self.run_jit(&mut state, args_ptr);
        } else {
            self.run_interp(&mut state, args_ptr);
        }

        if self.debug {
            println!("{}", state.heap.heap_dump());
        }

        state
    }

    fn run_jit(&self, state: &mut VMState, args_ptr: Pointer) {
        let start = time::SystemTime::now();

        /* Compile the main function */
        let main_idx = self.module.find_function_id("main").unwrap();
        JITEngine::compile(
            &mut state.jit_state,
            &self.module,
            &self.module.functions[main_idx as usize],
        );

        // Compile fib
        // TODO remove
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
        let res = JITEngine::call_if_compiled(self, state, "main", args_ptr.into());

        state.result = res;

        if self.debug {
            println!("jit time: {}ms", start.elapsed().unwrap().as_millis());
        }
    }

    fn run_interp(&self, state: &mut VMState, args_ptr: Pointer) {
        let start = time::SystemTime::now();

        state.interpreter_state.init(args_ptr);

        /* Run the program in interpreted mode */
        while self.interpreter.step(&mut state.interpreter_state, &mut state.heap) {}

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
}
