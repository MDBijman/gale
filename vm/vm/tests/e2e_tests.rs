use std::path::Path;
use galevm::standard_dialect;

use vm_internal::{
    bytecode::{ModuleId, ModuleLoader},
    standard_library::std_module,
    vm::VM,
};

struct TestCase {
    name: String,
    module_loader: ModuleLoader,
    main_module_id: ModuleId,
    input: Option<String>,
    ret: Option<u64>,
}

fn make_test_case(name: &str, stdin: &str, out: u64) -> TestCase {
    let path = Path::new("tests");
    let bytecode_path = path.join(Path::new(name)).with_extension("gbc");

    let mut ml = ModuleLoader::from_module(std_module());
    ml.add_dialect(Box::from(standard_dialect::StandardDialect {}));
    ml.set_default_dialect("std");
    let main_module_id = ml.load_module(bytecode_path.to_str().unwrap()).unwrap();

    TestCase {
        name: String::from(name),
        module_loader: ml,
        main_module_id,
        input: Some(String::from(stdin)),
        ret: Some(out),
    }
}

fn run_testcase(testcase: TestCase, jit: bool) {
    let input = testcase.input.unwrap_or(String::from("0"));
    let args = input
        .split(" ")
        .map(|s| String::from(s))
        .collect::<Vec<String>>();
    let ret = testcase.ret.unwrap_or(0);

    let mut vm = VM::new(testcase.module_loader);
    print!("Running {} ...", testcase.name);
    let state = vm.run(
        vm.module_loader
            .get_module_by_id(testcase.main_module_id)
            .expect("missing module impl")
            .get_function_by_name("main")
            .unwrap()
            .location(),
        args,
        jit,
    );
    let out = state.result.unwrap();
    assert_eq!(ret, out);
    println!(" ok")
}

#[test]
fn test_fib() {
    run_testcase(make_test_case("fib", "15", 987), false);
    //run_testcase(make_test_case("fib", "15", 987), true);
}

#[test]
fn test_identity() {
    run_testcase(make_test_case("identity", "42", 42), false);
    //run_testcase(make_test_case("identity", "42", 42), true);
}

#[test]
fn test_nested_calls() {
    run_testcase(make_test_case("nested_calls", "42", 43), false);
    //run_testcase(make_test_case("nested_calls", "42", 43), true);
}

#[test]
fn test_heap() {
    run_testcase(make_test_case("heap", "42", 48), false);
    //run_testcase(make_test_case("heap", "42", 48), true);
}

#[test]
fn test_fib_until() {
    run_testcase(
        make_test_case("fib_until", "1 1 2 3 5 8 13 21 34 55 55", 1),
        false,
    );
    //run_testcase(make_test_case("fib_until", "1 1 2 3 5 8 13 21 34 55 55", 1), true);
}

#[test]
fn test_multi_param() {
    run_testcase(make_test_case("multi_param", "", 2), false);
}

#[test]
fn test_const_decl() {
    run_testcase(make_test_case("const_decl", "", 42), false);
}

#[test]
fn test_fn_as_param() {
    run_testcase(make_test_case("fn_as_param", "", 2), false);
}

#[test]
fn test_fn_as_result() {
    run_testcase(make_test_case("fn_as_result", "", 2), false);
}
