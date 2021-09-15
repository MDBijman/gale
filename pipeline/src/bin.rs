extern crate clap;

use aterms::Term;
use checker::{check, desugar};
use clap::{App, Arg};
use compiler::{compile, lower, print};
use parser::parse_gale_file;
use std::fs;
use std::io::prelude::*;
use std::path;
use std::thread;
use std::time;

fn main() {
    let matches = App::new("Gale")
        .version("0.1")
        .author("Matthijs Bijman <matthijs@bijman.org>")
        .about("Compiles .gale files into bytecode format")
        .arg(
            Arg::with_name("input_file")
                .short("i")
                .long("input")
                .value_name("INPUT_FILE")
                .help("The input .gale file")
                .required(true),
        )
        .arg(
            Arg::with_name("output_file")
                .short("o")
                .long("output")
                .value_name("OUTPUT_FILE")
                .help("The output .gbc file")
                .required(true),
        )
        .arg(
            Arg::with_name("debug")
                .short("d")
                .takes_value(false)
                .help("Output intermediate results of the compilation pipeline"),
        )
        .arg(
            Arg::with_name("time")
                .short("t")
                .takes_value(false)
                .help("Outputs the running time of the various pipeline stages"),
        )
        .get_matches();

    let builder = thread::Builder::new()
        .name("rewriter".into())
        .stack_size(32 * 1000 * 1000);

    let handler = builder
        .spawn(move || {
            let in_file = path::Path::new(matches.value_of("input_file").unwrap());
            let out_file = path::Path::new(matches.value_of("output_file").unwrap());

            let should_debug = matches.is_present("debug");
            let should_time = matches.is_present("time");

            //

            let mut time = time::SystemTime::now();
            let term = parse_gale_file(&in_file.to_str().unwrap()).unwrap();
            let parse_time = time.elapsed().unwrap().as_millis();

            if should_debug {
                let input_out_path = out_file.with_extension("term");
                let mut o = fs::File::create(input_out_path).unwrap();
                o.write(format!("{}", term).as_bytes()).unwrap();
            }

            //

            time = time::SystemTime::now();
            let checked_term = check(&term);
            let check_time = time.elapsed().unwrap().as_millis();

            if should_debug {
                let checked_out_path = out_file.with_extension("checked.term");
                let mut o = fs::File::create(checked_out_path).unwrap();
                o.write(format!("{}", checked_term).as_bytes()).unwrap();
            }

            //

            time = time::SystemTime::now();
            let desugared_term = desugar(&checked_term);
            let desugar_time = time.elapsed().unwrap().as_millis();

            if should_debug {
                let checked_out_path = out_file.with_extension("desugared.term");
                let mut o = fs::File::create(checked_out_path).unwrap();
                o.write(format!("{}", desugared_term).as_bytes()).unwrap();
            }

            //

            time = time::SystemTime::now();
            let lowered_term = lower(desugared_term);
            let lower_time = time.elapsed().unwrap().as_millis();

            if should_debug {
                let lowered_out_path = out_file.with_extension("lowered.term");
                let mut o = fs::File::create(lowered_out_path).unwrap();
                o.write(format!("{}", lowered_term).as_bytes()).unwrap();
            }

            //

            time = time::SystemTime::now();
            let compiled_term = compile(lowered_term);
            let compile_time = time.elapsed().unwrap().as_millis();

            if should_debug {
                let compiled_out_path = out_file.with_extension("compiled.term");
                let mut o = fs::File::create(compiled_out_path).unwrap();
                o.write(format!("{}", compiled_term).as_bytes()).unwrap();
            }

            //

            time = time::SystemTime::now();
            let printed_term = print(compiled_term);
            let print_time = time.elapsed().unwrap().as_millis();

            // Write result to file
            match printed_term {
                Term::STerm(s, _) => {
                    let mut o = fs::File::create(out_file).unwrap();
                    o.write(format!("{}", s.value).as_bytes()).unwrap();
                }
                _ => panic!("Expected String term as output"),
            }

            if should_time {
                let json_times = format!(
                    r#"
{{
    "parse": {},
    "check": {},
    "desugar": {},
    "lower": {},
    "compile": {},
    "print": {}
}}
"#,
                    parse_time, check_time, desugar_time, lower_time, compile_time, print_time
                );

                let mut o = fs::File::create("./snippets/bench/times.json").unwrap();
                o.write(json_times.as_bytes()).unwrap();
            }
        })
        .unwrap();

    handler.join().unwrap();
}
