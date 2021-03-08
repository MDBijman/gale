#[macro_use]
extern crate nom;
extern crate clap;

mod parser;
mod pp;

use clap::{App, Arg};
use std::fs;
use std::io::prelude::*;
use terms_format::*;

use crate::parser::*;
use crate::pp::*;

fn main() {
   let matches = App::new("Gale Parser")
        .version("0.1")
        .author("Matthijs Bijman <matthijs@bijman.org>")
        .about("Parses .gale files into .term files")
        .subcommand(App::new("pp")
            .display_order(1)
            .about("Pretty-prints .term files into .gale files")
            .arg(Arg::with_name("input_file")
                .short("i")
                .long("input")
                .value_name("INPUT_FILE")
                .help("The input .term file")
                .required(true))
            .arg(Arg::with_name("output_file")
                .short("o")
                .long("output")
                .value_name("OUTPUT_FILE")
                .help("The output .gale file")
                .required(true)))
        .arg(Arg::with_name("input_file")
            .short("i")
            .long("input")
            .value_name("INPUT_FILE")
            .help("The input .gale file"))
        .arg(Arg::with_name("output_file")
            .short("o")
            .long("output")
            .value_name("OUTPUT_FILE")
            .help("The output .term file"))
        .get_matches();

    match matches.subcommand() {
        ("pp", Some(sub_m)) => {
            let in_file = sub_m.value_of("input_file").unwrap();
            let out_file = sub_m.value_of("output_file").unwrap();

            let term_file = parse_term_from_file(&in_file.to_string()).unwrap();
            let gale = pretty_print_gale_term(&term_file);

            let mut o = fs::File::create(out_file).unwrap();
            o.write(format!("{}", gale).as_bytes()).unwrap();
        },
        ("", None) => {
            let in_file = matches.value_of("input_file").unwrap();
            let out_file = matches.value_of("output_file").unwrap();

            let terms = parse_gale_file(&in_file.to_string()).unwrap();

            let mut o = fs::File::create(out_file).unwrap();
            o.write(format!("{}", terms).as_bytes()).unwrap();
        }
        _ => { panic!("Unknown subcommand"); },
    }
}