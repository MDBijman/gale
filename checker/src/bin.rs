extern crate clap;
mod checker;
mod desugarer;

use clap::{App, Arg};
use std::fs;
use std::io::prelude::*;
use aterms::base::*;

use crate::checker::*;
use crate::desugarer::*;

fn main() {
   let matches = App::new("Gale Checker")
        .version("0.1")
        .author("Matthijs Bijman <matthijs@bijman.org>")
        .about("Checks gale .term files and outputs annotated versions")
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
    let checked = check(&term);
    let desugared = desugar(&checked);

    let mut o = fs::File::create(out_file).unwrap();
    o.write(format!("{}", desugared).as_bytes()).unwrap();
}