mod parser;
mod bytecode;
mod interpreter;

extern crate clap;
use clap::{App, Arg};
use std::fs;

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
        .get_matches();

    let input_file_name = matches.value_of("input_file").unwrap();

    let module = parser::parse_bytecode_file(input_file_name);

    let result = interpreter::run(module);

    println!("out: {}", result);
}
