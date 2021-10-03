// This is the WebAssembly-target lib entry point
use wasm_bindgen::prelude::*;
use galelib;
use galevm;

// When the `wee_alloc` feature is enabled, use `wee_alloc` as the global
// allocator.
#[cfg(feature = "wee_alloc")]
#[global_allocator]
static ALLOC: wee_alloc::WeeAlloc = wee_alloc::WeeAlloc::INIT;
    
#[wasm_bindgen]
extern {
    fn alert(s: &str);
}

#[wasm_bindgen]
pub fn init_panic_hook() {
    console_error_panic_hook::set_once();
}

#[wasm_bindgen]
pub fn run_gale(code: &str, arg: &str) -> String {
    let bytecode = galelib::compile_gale_program(code);
    let module   = galevm::parse_bytecode_string(bytecode.as_str());
    let vm = galevm::VM::new(&module);
    let state = vm.run(arg.split(" ").map(|s| String::from(s)).collect(), false);
    format!("{}", state.result.unwrap())
}
