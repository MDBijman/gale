use clap::{App, Arg, ArgMatches, SubCommand};
use galevm::*;

fn main() {
    let matches = App::new("Gale Utils")
        .version("0.1")
        .author("Matthijs Bijman <matthijs@bijman.org>")
        .about("Utils for working with the Gale VM and Compiler")
        .subcommand(
            SubCommand::with_name("vm").about("VM utils").subcommands([
                SubCommand::with_name("print-bin")
                    .args(&[
                        Arg::with_name("bytecode")
                            .short("i")
                            .required(true)
                            .takes_value(true)
                            .help("Name of the file containing the function"),
                        Arg::with_name("function")
                            .short("f")
                            .required(true)
                            .takes_value(true)
                            .help("Name of the function to compile and print"),
                        Arg::with_name("architecture")
                            .short("a")
                            .required(false)
                            .takes_value(true)
                            .help("Name of the architecture to compile to"),
                    ])
                    .about("Prints the hex representation for the given function (compiled)"),
                SubCommand::with_name("print-asm").args(&[
                    Arg::with_name("bytecode")
                        .short("i")
                        .required(true)
                        .takes_value(true)
                        .help("Name of the file containing the function"),
                    Arg::with_name("function")
                        .short("f")
                        .required(true)
                        .takes_value(true)
                        .help("Name of the function to compile and print"),
                    Arg::with_name("architecture")
                        .short("a")
                        .required(false)
                        .takes_value(true)
                        .help("Name of the architecture to compile to"),
                ]).about("Prints the disassembled nasm representation for the given function (compiled)"),
                SubCommand::with_name("print-lifetimes").args(&[
                    Arg::with_name("bytecode")
                        .short("i")
                        .required(true)
                        .takes_value(true)
                        .help("Name of the file containing the function"),
                    Arg::with_name("function")
                        .short("f")
                        .required(true)
                        .takes_value(true)
                        .help("Name of the function to analyse"),
                ]).about("Prints the computed lifetime ranges for the given function"),
            ]),
        )
        .get_matches();

    if let Some(vm_match) = matches.subcommand_matches("vm") {
        if let Some(print_bin) = vm_match.subcommand_matches("print-bin") {
            vm_print_bin(print_bin);
        } else if let Some(print_asm) = vm_match.subcommand_matches("print-asm") {
            vm_print_asm(print_asm);
        } else if let Some(print_life) = vm_match.subcommand_matches("print-lifetimes") {
            vm_print_lifetimes(print_life);
        }
    }
}

/// Prints a compiled function in binary format.
///
/// The function is specified by the `function` command line argument.
/// The module is specified by the `bytecode` command line argument.
///
/// Arguments: 
/// * `args`: Command line arguments
fn vm_print_bin(args: &ArgMatches) {
    let function_name = args.value_of("function").unwrap();
    let file_name = args.value_of("bytecode").unwrap();
    if let Some(_) = args.value_of("architecture") {
        todo!("print-bin arch option")
    }

    let mut module_loader = ModuleLoader::from_module(std_module());
    let id = module_loader.load_module(file_name).unwrap();
    let module = module_loader
        .get_module_by_id(id)
        .expect("missing impl");
    let func = module.get_function_by_name(function_name).unwrap();
    let mut state = JITState::default();
    JITEngine::compile(&mut state, &module, func);
    let hex_string = JITEngine::to_hex_string(&mut state, func);
    print!("{}", hex_string);
}

/// Prints a compiled function in assembly format.
///
/// The function is specified by the `function` command line argument.
/// The module is specified by the `bytecode` command line argument.
///
/// Arguments: 
/// * `args`: Command line arguments
fn vm_print_asm(args: &ArgMatches) {
    let function_name = args.value_of("function").unwrap();
    let file_name = args.value_of("bytecode").unwrap();

    if let Some(_) = args.value_of("architecture") {
        todo!("print-asm arch option")
    }

    let mut module_loader = ModuleLoader::from_module(std_module());
    let id = module_loader.load_module(file_name).unwrap();
    let module = module_loader
        .get_module_by_id(id)
        .expect("missing impl");

    let func = match module.get_function_by_name(function_name) {
        Some(r) => r,
        None => panic!("No such function: {}", function_name)
    };

    let mut state = JITState::default();
    JITEngine::compile(&mut state, &module, func);
    let bytes = JITEngine::get_function_bytes(&mut state, func);

    // Use iced_x86 to decompile
    use iced_x86::{Decoder, Formatter, Instruction, NasmFormatter};

    let mut decoder = Decoder::new(64, bytes, 0);
    let mut formatter = NasmFormatter::new();
    let mut instruction = Instruction::default();
    let mut output = String::new();
    while decoder.can_decode() {
        decoder.decode_out(&mut instruction);
        output.clear();
        formatter.format(&instruction, &mut output);
        print!("{:08X} ", instruction.ip());
        println!("{}", output);
    }
}

/// Prints the computed register lifetimes of a function.
///
/// The function is specified by the `function` command line argument.
/// The module is specified by the `bytecode` command line argument.
///
/// Arguments: 
/// * `args`: Command line arguments
fn vm_print_lifetimes(args: &ArgMatches) {
    let function_name = args.value_of("function").unwrap();
    let file_name = args.value_of("bytecode").unwrap();

    let mut module_loader = ModuleLoader::from_module(std_module());
    let id = module_loader.load_module(file_name).unwrap();
    let module = module_loader
        .get_module_by_id(id)
        .expect("missing impl");
    let func = module.get_function_by_name(function_name).unwrap();
    let cfg = ControlFlowGraph::from_function_instructions(func);
    let intervals = &cfg.liveness_intervals(func);
    func.print_liveness(intervals);
}
