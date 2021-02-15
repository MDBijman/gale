#[macro_use]
extern crate nom;

mod parser;
mod pp;

pub use crate::parser::parse_gale_file as parse_gale_file;
pub use crate::parser::parse_gale_string as parse_gale_string;
