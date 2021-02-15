extern crate terms_format;
extern crate itertools;
mod interp;
mod parser;

pub use crate::parser::rwfile::*;
pub use crate::parser::parser::parse_rw_file as parse_rewrite_file;
pub use crate::parser::parser::parse_rw_string as parse_rewrite_string;
pub use crate::interp::interp::Rewriter;
