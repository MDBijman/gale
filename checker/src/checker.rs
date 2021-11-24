use aterms::*;
use tecs::Interpreter;

const TECS_CHECKER: &str = include_str!("./transform/checker.tcs");

pub fn check(term: &Term) -> Term {
    let interp = Interpreter::new_from_string(TECS_CHECKER);
    match interp.run(term.clone(), "FileOk") {
        Err(e) => {
            println!("{}", e);
            panic!()
        }
        Ok(t) => t,
    }
}
