use galevm::parse_bytecode_file;
use galevm::Instruction;
use galevm::Module;
use galevm::VM;
use std::fs;
use std::path::Path;

#[test]
fn test_instr_size() {
    use std::mem::size_of;
    assert_eq!(size_of::<Instruction>(), 8);
}

struct TestCase {
    module: Option<Module>,
    input: Option<String>,
    output: Option<String>, // TODO use once we can capture stdout
    ret: Option<String>,
}

fn load_test_file(name: &str) -> TestCase {
    let path = Path::new("tests");
    let bytecode_path = path.join(Path::new(name)).with_extension("gbc");
    let stdin_path = path.join(Path::new(name)).with_extension("stdin");
    let stdout_path = path.join(Path::new(name)).with_extension("stdout");
    let ret_path = path.join(Path::new(name)).with_extension("ret");

    let bc = parse_bytecode_file(bytecode_path.to_str().unwrap());
    let stdin = fs::read_to_string(stdin_path);
    let stdout = fs::read_to_string(stdout_path);
    let ret = fs::read_to_string(ret_path);

    TestCase {
        module: bc,
        input: stdin.ok(),
        output: stdout.ok(),
        ret: ret.ok(),
    }
}

fn run_testcase(name: &str, jit: bool) {
    let testcase = load_test_file(name);
    let module = testcase.module.unwrap();
    let input = testcase.input.unwrap_or(String::from("0"));
    let args = input.split(" ").collect::<Vec<&str>>();
    let ret = testcase.ret.unwrap_or(String::from("0"));

    let vm = VM::new(&module);
    print!("Running {} ...", name);
    let state = vm.run(args, jit);
    let out = state.result.unwrap().to_string();
    assert_eq!(ret, out);
    println!(" ok")   
}

const TEST_NAMES: [&str; 3] = ["fib_15", "identity", "nested_calls"];

#[test]
fn test_files_interp() {
    for &name in TEST_NAMES.iter() {
        run_testcase(name, false);
    }
}

#[test]
fn test_files_jit() {
    for &name in TEST_NAMES.iter() {
        run_testcase(name, true);
    }
}
