mod parser;
mod bytecode;
mod interpreter;

extern crate clap;
use clap::{App, Arg};
use std::time;

fn main() {
    let matches = App::new("Gale VM")
        .version("0.1")
        .author("Matthijs Bijman <matthijs@bijman.org>")
        .about("VM for Gale bytecode")
        .arg(Arg::with_name("input_file")
            .short("i")
            .long("input")
            .value_name("INPUT_FILE")
            .help("The input file")
            .required(true))
        .arg(Arg::with_name("arguments")
            .short("a")
            .long("args")
            .value_name("ARGS")
            .help("Arguments passed to the interpreted program")
            .multiple(true))
        .get_matches();

    let input_file_name = matches.value_of("input_file").unwrap();
    let argument = matches.value_of("arguments").unwrap();
    println!("input: {}", argument);

    let module = parser::parse_bytecode_file(input_file_name);

    let start = time::SystemTime::now();

    let result = interpreter::run(module, argument);

    println!("out: {} in {}ms", result, start.elapsed().unwrap().as_millis());
}
