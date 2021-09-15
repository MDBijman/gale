mod bytecode;
mod interpreter;
mod parser;
mod jit;
mod vm;

extern crate clap;

use clap::{App, Arg};
use std::{time};
use vm::{VM, VMState};

fn main() {
    let startup = time::SystemTime::now();

    let matches = App::new("Gale VM")
        .version("0.1")
        .author("Matthijs Bijman <matthijs@bijman.org>")
        .about("VM for Gale bytecode")
        .arg(
            Arg::with_name("input_file")
                .short("i")
                .long("input")
                .value_name("INPUT_FILE")
                .help("The input file")
                .required(true),
        )
        .arg(
            Arg::with_name("arguments")
                .short("a")
                .long("args")
                .value_name("ARGS")
                .help("Arguments passed to the interpreted program"),
        )
        .get_matches();

    let input_file_name = matches.value_of("input_file").unwrap();

    let module = parser::parse_bytecode_file(input_file_name);

    let argument = matches.value_of("arguments").unwrap();
    println!("--- input: {}", argument);

    let startup_end = startup.elapsed().unwrap().as_nanos();
    println!("--- startup time: {:.3}ms", startup_end as f64 / 1000000 as f64);

    let vm = VM::new(&module);
    vm.run(argument, true);
    //vm.run(argument, false);
}
