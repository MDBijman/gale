extern crate clap;

use parser::parse_gale_file;
use checker::check;
use compiler::{ lower, compile, print };
use terms_format::Term;
use std::fs;
use std::path;
use std::io::prelude::*;
use clap::{App, Arg};
use std::thread;


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


    let builder = thread::Builder::new()
        .name("rewriter".into())
        .stack_size(32 * 1000 * 1000);

    let handler = builder.spawn(move || {
        let in_file  = path::Path::new(matches.value_of("input_file").unwrap());
        let out_file = path::Path::new(matches.value_of("output_file").unwrap());

        let should_debug = matches.is_present("debug");

        print!("Parsing...");
        let term = parse_gale_file(&in_file.to_str().unwrap()).unwrap();
        println!("Done");

        if should_debug {
            let input_out_path = out_file.with_extension("term");
            let mut o = fs::File::create(input_out_path).unwrap();
            o.write(format!("{}", term).as_bytes()).unwrap();
        }

        print!("Checking...");
        let checked_term = check(&term);
        println!("Done");

        if should_debug {
            let checked_out_path = out_file.with_extension("checked.term");
            let mut o = fs::File::create(checked_out_path).unwrap();
            o.write(format!("{}", checked_term).as_bytes()).unwrap();
        }

        print!("Lowering...");
        let lowered_term = lower(checked_term);
        println!("Done");

        if should_debug {
            let lowered_out_path = out_file.with_extension("lowered.term");
            let mut o = fs::File::create(lowered_out_path).unwrap();
            o.write(format!("{}", lowered_term).as_bytes()).unwrap();
        }

        print!("Compiling...");
        let compiled_term = compile(lowered_term);
        println!("Done");

        if should_debug {
            let compiled_out_path = out_file.with_extension("compiled.term");
            let mut o = fs::File::create(compiled_out_path).unwrap();
            o.write(format!("{}", compiled_term).as_bytes()).unwrap();
        }

        print!("Printing...");
        let printed_term  = print(compiled_term);
        println!("Done");
        
        // Write result to file
        match printed_term {
            Term::STerm(s, _) => {
                let mut o = fs::File::create(out_file).unwrap();
                o.write(format!("{}", s.value).as_bytes()).unwrap();
            },
            _ => panic!("Expected String term as output")
        }
    }).unwrap();


    handler.join().unwrap();
}