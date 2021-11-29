extern crate clap;
mod compiler;
use clap::{App, Arg};
use std::fs;
use std::io::prelude::*;
use aterms::base::*;
use crate::compiler::*;

fn main() {
   let matches = App::new("Gale Compiler")
        .version("0.1")
        .author("Matthijs Bijman <matthijs@bijman.org>")
        .about("Compiles annotated gale term files")
        .arg(Arg::with_name("input_file")
            .short("i")
            .long("input")
            .value_name("INPUT_FILE")
            .help("The input .term file"))
        .arg(Arg::with_name("output_file")
            .short("o")
            .long("output")
            .value_name("OUTPUT_FILE")
            .help("The output .term file"))
        .get_matches();

    let in_file = matches.value_of("input_file").unwrap();
    let out_file = matches.value_of("output_file").unwrap();

    let term = parse_term_from_file(&in_file.to_string()).unwrap();
    let lowered = lower(term);
    let compiled = compile(lowered);
    let printed = print(compiled);

    match printed {
        Term::STerm(s) => {
            let mut o = fs::File::create(out_file).unwrap();
            println!("{}", s.value);
            o.write(format!("{}", s.value).as_bytes()).unwrap();
        },
        _ => panic!("Expected String term as output")
    }
}