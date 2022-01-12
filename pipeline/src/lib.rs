use aterms::base::Term;
use checker::{check, desugar};
use compiler::{compile, flatten, print};
use parser::parse_gale_string;

pub fn compile_gale_program(program: &str) -> Result<String, String> {
    let term = parse_gale_string(program).unwrap();
    let checked_term = check(&term);
    let printed_term = desugar(&checked_term)
        .and_then(flatten)
        .and_then(compile)
        .and_then(print);

    match printed_term {
        Ok(Term::STerm(s)) => {
            return Ok(s.value);
        }
        Ok(_) => panic!("Expected String term as output"),
        Err(message) => return Err(message),
    }
}
