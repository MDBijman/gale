use galevm::std_module;
use galevm::Instruction;
use galevm::ModuleId;
use galevm::ModuleLoader;
use galevm::VM;
use std::fs;
use std::path::Path;

#[test]
fn test_instr_size() {
    use std::mem::size_of;
    assert_eq!(size_of::<Instruction>(), 8);
}

struct TestCase {
    module_loader: ModuleLoader,
    main_module_id: ModuleId,
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

    let mut ml = ModuleLoader::from_module(std_module());
    let main_module_id = ml.load_module(bytecode_path.to_str().unwrap());
    let stdin = fs::read_to_string(stdin_path);
    let stdout = fs::read_to_string(stdout_path);
    let ret = fs::read_to_string(ret_path);

    TestCase {
        module_loader: ml,
        main_module_id,
        input: stdin.ok(),
        output: stdout.ok(),
        ret: ret.ok(),
    }
}

fn run_testcase(name: &str, jit: bool) {
    let testcase = load_test_file(name);
    let module_loader = testcase.module_loader;
    let input = testcase.input.unwrap_or(String::from("0"));
    let args = input
        .split(" ")
        .map(|s| String::from(s))
        .collect::<Vec<String>>();
    let ret = testcase.ret.unwrap_or(String::from("0"));

    let vm = VM::new(module_loader);
    print!("Running {} ...", name);
    let state = vm.run(
        vm.module_loader
            .get_by_id(testcase.main_module_id)
            .expect("missing module")
            .expect("missing impl"),
        args,
        jit,
    );
    let out = state.result.unwrap().to_string();
    assert_eq!(ret, out);
    println!(" ok")
}

const TEST_NAMES: [&str; 4] = ["fib", "identity", "nested_calls", "heap"];

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
