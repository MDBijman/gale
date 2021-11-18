use crate::bytecode::{Function, Module, Size, Type, TypeDecl};
use crate::memory::{Pointer, STRING_HEADER_SIZE};
use crate::vm::VMState;
use std::ffi;

const DEBUG_STDLIB: bool = false;

extern "C" fn parse_ui64(state: &mut VMState, ptr: Pointer) -> u64 {
    let str_ptr = unsafe {
        let char_ptr =
            state.heap.raw(ptr).offset(STRING_HEADER_SIZE as isize) as *const libc::c_char;
        ffi::CStr::from_ptr(char_ptr)
    };

    if DEBUG_STDLIB {
        println!("parse_ui64: {}", str_ptr.to_str().unwrap());
        println!(
            "parse_ui64: {}",
            str_ptr.to_str().unwrap().parse::<u64>().unwrap()
        );
    }

    str_ptr.to_str().unwrap().parse::<u64>().unwrap()
}

extern "C" fn print_ui64(_: &mut VMState, val: u64) -> u64 {
    print!("{}", val);
    0
}

extern "C" fn print_str(state: &mut VMState, ptr: Pointer) -> u64 {
    let str_ptr = unsafe {
        let char_ptr =
            state.heap.raw(ptr).offset(STRING_HEADER_SIZE as isize) as *const libc::c_char;
        ffi::CStr::from_ptr(char_ptr)
    };

    print!("{}", str_ptr.to_str().unwrap());
    use std::io::Write;
    std::io::stdout().flush().unwrap();
    0
}

extern "C" fn flush(state: &mut VMState, _: u64) -> u64 {
    use std::io::Write;
    std::io::stdout().flush().unwrap();
    0
}

pub fn std_module() -> Module {
    unsafe {
        Module::from(
            String::from("std"),
            vec![
                TypeDecl {
                    idx: 0,
                    type_: Type::Fn(
                        Box::from(Type::ptr(Type::Str(Size::Unsized))),
                        Box::from(Type::U64),
                    ),
                },
                TypeDecl {
                    idx: 1,
                    type_: Type::Fn(Box::from(Type::U64), Box::from(Type::U64)),
                },
                TypeDecl {
                    idx: 2,
                    type_: Type::Fn(
                        Box::from(Type::ptr(Type::Str(Size::Unsized))),
                        Box::from(Type::U64),
                    ),
                },
                TypeDecl {
                    idx: 3,
                    type_: Type::Fn(Box::from(Type::Unit), Box::from(Type::Unit)),
                },
            ],
            vec![],
            vec![
                Function::new_native(
                    String::from("parse_ui64"),
                    0,
                    std::mem::transmute::<_, extern "C" fn() -> ()>(
                        parse_ui64 as extern "C" fn(_, _) -> _,
                    ),
                ),
                Function::new_native(
                    String::from("print_ui64"),
                    1,
                    std::mem::transmute::<_, extern "C" fn() -> ()>(
                        print_ui64 as extern "C" fn(_, _) -> _,
                    ),
                ),
                Function::new_native(
                    String::from("print_str"),
                    2,
                    std::mem::transmute::<_, extern "C" fn() -> ()>(
                        print_str as extern "C" fn(_, _) -> _,
                    ),
                ),
                Function::new_native(
                    String::from("flush"),
                    3,
                    std::mem::transmute::<_, extern "C" fn() -> ()>(
                        flush as extern "C" fn(_, _) -> _,
                    ),
                ),
            ],
        )
    }
}
