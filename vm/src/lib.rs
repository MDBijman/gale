mod bytecode;
mod interpreter;
mod parser;

extern crate nom;

pub use bytecode::*;
pub use interpreter::run;
pub use parser::parse_bytecode_file;
pub use parser::parse_bytecode_string;
