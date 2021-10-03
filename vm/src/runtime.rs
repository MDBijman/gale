use crate::bytecode::{Module, Function, Value};

fn parse_ui64(from: Value) -> Value {
    match from {
        Value::Str(from_str) => Value::U64(from_str.parse::<u64>().unwrap()),
        _ => panic!("type error")
    }
}

pub fn std_module() -> Module {
    Module::from(String::from("std"), vec![], vec![], vec![
        Function::new_native(String::from("parse_ui64"), 0, parse_ui64)
    ])
}
