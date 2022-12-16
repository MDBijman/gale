use crate::bytecode::{CallTarget, Function, Module, ModuleLoader};
use crate::interpreter::{Interpreter, InterpreterState};
use crate::jit::{self, JITState};
use crate::memory::{Heap, Pointer, ARRAY_HEADER_SIZE};
use crate::typechecker::{Size, Type};
use std::time;

pub trait VMDebugger {
    fn inspect(&self, vm: &VM, state: &mut VMState);
}

pub struct VMState {
    pub interpreter_state: InterpreterState,
    pub jit_state: JITState,
    pub result: Option<u64>,
    pub heap: Box<Heap>,
}

impl VMState {
    pub fn new(entry: CallTarget, entry_framesize: u64, debug: bool) -> VMState {
        VMState {
            interpreter_state: InterpreterState::new(entry, entry_framesize),
            jit_state: JITState::new(debug),
            result: None,
            heap: Box::new(Heap::default()),
        }
    }

    pub extern "C" fn heap_mut(&mut self) -> &mut Heap {
        &mut *self.heap
    }
}

pub struct VM {
    pub module_loader: ModuleLoader,
    pub interpreter: Interpreter,
    pub debug: bool,
}

impl VM {
    pub fn from_single_module(m: Module) -> VM {
        VM {
            module_loader: ModuleLoader::from_module(m),
            interpreter: Interpreter::default(),
            debug: false,
        }
    }

    pub fn new(m: ModuleLoader) -> VM {
        VM {
            module_loader: m,
            interpreter: Interpreter::default(),
            debug: false,
        }
    }

    pub fn new_debug(m: ModuleLoader) -> VM {
        VM {
            module_loader: m,
            interpreter: Interpreter::default(),
            debug: true,
        }
    }

    pub fn run(&self, main_module: &mut Module, args: Vec<String>, jit: bool) -> VMState {
        self.run_with_debugger(main_module, args, None, jit)
    }

    pub fn run_with_debugger(
        &self,
        main_module: &mut Module,
        args: Vec<String>,
        debug: Option<&dyn VMDebugger>,
        jit: bool,
    ) -> VMState {
        if debug.is_some() && jit {
            panic!("Cannot debug jitting VM instance yet");
        }

        let main_id = main_module.find_function_id("main").expect("function");
        let main_fn = main_module.get_function_by_name("main").expect("function");
        let ast = main_fn.ast_impl().expect("ast impl");
        let main_type = main_module
            .get_type_by_id(main_fn.type_id)
            .expect("main function type idx");

        let expected_main_type: Type = Type::fun(
            Type::tuple(vec![Type::ptr(Type::unsized_array(Type::ptr(Type::Str(
                Size::Unsized,
            ))))]),
            Type::U64,
        );

        if main_type != &expected_main_type {
            panic!(
                "main type was {}, expected {}",
                main_type, expected_main_type
            );
        }

        let main_frame_size = ast.varcount;

        log::info!(target: "config", "JIT: {}", jit);

        let mut state = VMState::new(
            CallTarget::new(main_module.id, main_id),
            main_frame_size.into(),
            self.debug,
        );

        state.interpreter_state.init_empty();

        // TODO abstract this kind of logic
        let args_ptr = {
            let arr_ptr = state.heap.allocate_type(&Type::sized_array(
                Type::ptr(Type::Str(Size::Unsized)),
                args.len(),
            ));
            unsafe {
                let mut raw_arr_ptr = state.heap.raw(arr_ptr).offset(ARRAY_HEADER_SIZE as isize);

                for arg in args.iter() {
                    let str_ptr = state.heap.allocate_type(&Type::Str(Size::Sized(arg.len())));
                    state.heap.store_string(str_ptr, arg.as_str());
                    *(raw_arr_ptr as *mut Pointer) = str_ptr;
                    raw_arr_ptr = raw_arr_ptr.add(std::mem::size_of::<Pointer>());
                }
            }

            arr_ptr
        };

        if jit {
            /* Compile the fib function */
            match main_module.get_function_by_name_mut("fib") {
                Some(fib_fn) => {
                    jit::compile(&self, &main_module, &mut fib_fn, &mut state.jit_state);
                }
                _ => {}
            }

            let main_fn = main_module
                .get_function_by_name_mut("main")
                .expect("function");

            self.run_jit(main_module, main_fn, &mut state, args_ptr);
        } else if debug.is_none() {
            self.run_interp(main_module, &mut state, args_ptr);
        } else if debug.is_some() {
            self.run_interp_with_debugger(main_module, &mut state, args_ptr, debug.unwrap());
        }

        log::debug!(target: "vm", "execution finished");
        log::debug!(target: "vm", "return code: {}", state.interpreter_state.result);
        log::debug!(target: "vm", "heap dump\n{}", state.heap.heap_dump());

        state
    }

    fn run_jit(
        &self,
        main_module: &Module,
        function: &mut Function,
        state: &mut VMState,
        args_ptr: Pointer,
    ) {
        let start = time::SystemTime::now();

        /* Compile the main function */
        jit::compile(&self, &main_module, function, &mut state.jit_state);

        /* Run the main function in compiled mode */
        let res = jit::call_if_compiled(
            self,
            state,
            CallTarget::new(main_module.id, function.location),
            args_ptr,
        );

        state.result = res;

        log::info!(target: "compiler", "Compiling {} took {}ms", "main", start.elapsed().unwrap().as_millis());
    }

    fn run_interp(&self, main_module: &Module, state: &mut VMState, args_ptr: Pointer) {
        let start = time::SystemTime::now();

        let main_id = main_module.find_function_id("main").expect("function");

        state
            .interpreter_state
            .init(CallTarget::new(main_module.id, main_id), args_ptr);

        while self.interpreter.step(self, state) {}

        state.result = Some(
            *state
                .interpreter_state
                .result
                .as_ui64()
                .expect("invalid result type"),
        );

        let end = start.elapsed().unwrap().as_millis();
        log::info!(target: "interpreter", "interp time: {}ms", end);
        log::info!(target: "interpreter", "instructions run: {}",  state.interpreter_state.instr_counter);
        log::info!(target: "interpreter", "MOp/s: {:.2}", (state.interpreter_state.instr_counter as f64 / end as f64) / 1000 as f64 );
    }

    fn run_interp_with_debugger(
        &self,
        main_module: &Module,
        state: &mut VMState,
        args_ptr: Pointer,
        debugger: &dyn VMDebugger,
    ) {
        let start = time::SystemTime::now();

        let main_id = main_module.find_function_id("main").expect("function");

        state
            .interpreter_state
            .init(CallTarget::new(main_module.id, main_id), args_ptr);

        /* Run the program in interpreted mode */
        while self.interpreter.step(self, state) {
            debugger.inspect(self, state);
        }

        state.result = Some(
            *state
                .interpreter_state
                .result
                .as_ui64()
                .expect("invalid result type"),
        );

        let end = start.elapsed().unwrap().as_millis();
        log::info!(target: "interpreter", "interp time: {}ms", end);
        log::info!(target: "interpreter", "instructions run: {}",  state.interpreter_state.instr_counter);
        log::info!(target: "interpreter", "MOp/s: {:.2}", (state.interpreter_state.instr_counter as f64 / end as f64) / 1000 as f64 );
    }
}