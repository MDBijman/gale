extern crate clap;

use parser::parse_gale_file;
use checker::check;
use compiler::compile;
use terms_format::Term;
use std::fs;
use clap::{App, Arg};

fn main() {
   let matches = App::new("Gale")
        .version("0.1")
        .author("Matthijs Bijman <matthijs@bijman.org>")
        .about("Compiles .gale files into bytecode format")
        .arg(Arg::with_name("input_file")
            .short("i")
            .long("input")
            .value_name("INPUT_FILE")
            .help("The input .gale file")
            .required(true))
        .arg(Arg::with_name("output_file")
            .short("o")
            .long("output")
            .value_name("OUTPUT_FILE")
            .help("The output .gbc file")
            .required(true))
        .get_matches();

    let in_file  = matches.value_of("input_file").unwrap();
    let out_file = matches.value_of("output_file").unwrap();

    let term = parse_gale_file(&String::from(in_file));
    let checked_term = check(&term);
    let compiled_term = compile(&checked_term);
    
    fs::write(out_file, format!("{}", compiled_term)).unwrap();
}