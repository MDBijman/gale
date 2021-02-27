extern crate clap;

use parser::parse_gale_file;
use checker::check;
use compiler::compile;
use terms_format::Term;
use std::fs;
use std::path;
use std::io::prelude::*;
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
        .arg(Arg::with_name("debug")
            .short("d")
            .takes_value(false)
            .help("Output intermediate results of the compilation pipeline"))
        .get_matches();

    let in_file  = path::Path::new(matches.value_of("input_file").unwrap());
    let out_file = path::Path::new(matches.value_of("output_file").unwrap());

    let should_debug = matches.is_present("debug");

    let term = parse_gale_file(&in_file.to_str().unwrap());

    if should_debug {
        let input_out_path = out_file.with_extension("term");
        let mut o = fs::File::create(input_out_path).unwrap();
        o.write(format!("{}", term).as_bytes()).unwrap();
    }

    let checked_term = check(&term);

    if should_debug {
        let checked_out_path = out_file.with_extension("checked.term");
        let mut o = fs::File::create(checked_out_path).unwrap();
        o.write(format!("{}", checked_term).as_bytes()).unwrap();
    }

    let compiled_term = compile(&checked_term);
    
    // Write result to file
    match compiled_term {
        Term::STerm(s, _) => {
            let mut o = fs::File::create(out_file).unwrap();
            o.write(format!("{}", s.value).as_bytes()).unwrap();
        },
        _ => panic!("Expected String term as output")
    }
}