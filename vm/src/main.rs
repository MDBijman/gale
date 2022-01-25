mod bytecode;
mod dataflow;
mod debugger;
mod interpreter;
mod jit;
mod memory;
mod parser;
mod standard_library;
mod vm;

extern crate clap;

use bytecode::ModuleLoader;
use clap::{App, Arg};
use std::time;
use vm::VM;

fn main() {
    let startup_time = time::SystemTime::now();

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
                .multiple(false)
                .min_values(1)
                .value_name("ARGS")
                .help("Arguments passed to the interpreted program"),
        )
        .arg(
            Arg::with_name("enable_jit")
                .short("j")
                .long("jit")
                .takes_value(false)
                .help("Enables jit if specified"),
        )
        .arg(
            Arg::with_name("enable_debugger")
                .long("debug")
                .takes_value(false)
                .help("Enables debugging mode if specified"),
        )
        .arg(
            Arg::with_name("measure_time")
                .long("time")
                .takes_value(false)
                .help("Enables and prints timing if specified"),
        )
        .arg(
            Arg::with_name("verbosity")
                .short("v")
                .takes_value(false)
                .multiple(true)
                .help("Increases VM verbosity"),
        )
        .get_matches();

    // Arguments
    let measure_time = matches.is_present("measure_time");
    let debug_mode = matches.is_present("enable_debugger");
    let input_file_name = matches.value_of("input_file").unwrap();
    let arguments = matches.values_of("arguments").map_or(Vec::new(), |f| {
        f.into_iter().map(|s| String::from(s)).collect()
    });
    let use_jit = matches.is_present("enable_jit");

    let mut module_loader = ModuleLoader::from_module(standard_library::std_module());
    let main_module_id = match module_loader.load_module(input_file_name) {
        Ok(module_id) => module_id,
        Err(pe) => {
            eprintln!("{}", pe);
            return;
        }
    };
    let verbosity_level = matches.occurrences_of("verbosity");

    let vm = if verbosity_level == 0 {
        VM::new(module_loader)
    } else {
        println!("verbosity: {} - enabling debug output", verbosity_level);
        VM::new_debug(module_loader)
    };

    let main_module = vm
        .module_loader
        .get_by_id(main_module_id)
        .expect("missing module")
        .expect("missing impl");

    let state = if debug_mode {
        let debugger = crate::debugger::MyDebugger {};
        vm.run_with_debugger(main_module, arguments, Some(&debugger), use_jit)
    } else {
        vm.run(main_module, arguments, use_jit)
    };

    println!("{}", state.result.unwrap());

    let finish_time = startup_time.elapsed().unwrap().as_nanos();
    if measure_time {
        println!("elapsed: {:.3}s", finish_time as f64 / 1_000_000_000 as f64);
    };
}
