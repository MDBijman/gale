use aterms::Term;
use checker::{check, desugar};
use compiler::{compile, lower, print};
use parser::parse_gale_string;

pub fn compile_gale_program(program: &str) -> String {
    let term = parse_gale_string(program).unwrap();
    let checked_term = check(&term);
    let desugared_term = desugar(&checked_term);
    let lowered_term = lower(desugared_term);
    let compiled_term = compile(lowered_term);
    let printed_term = print(compiled_term);
    match printed_term {
        Term::STerm(s) => {
            return s.value;
        }
        _ => panic!("Expected String term as output"),
    }
}
