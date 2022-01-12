use aterms::base::*;
use ters::{parse_rewrite_string, Rewriter};

const FLATTEN_RW: &str = include_str!("./transform/flatten.rw");
const COMPILE_RW: &str = include_str!("./transform/compile.rw");
const PRINT_RW: &str = include_str!("./transform/print.rw");

pub fn flatten(term: Term) -> Result<Term, String> {
    let mut r = parse_rewrite_string(FLATTEN_RW).unwrap();
    r.set_filename("./transform/flatten.rw");
    let mut rw = Rewriter::new_with_prelude(r);
    rw.rewrite(term)
}

pub fn compile(term: Term) -> Result<Term, String> {
    let mut c = parse_rewrite_string(COMPILE_RW).unwrap();
    c.set_filename("./transform/compile.rw");
    let mut cw = Rewriter::new_with_prelude(c);
    cw.rewrite(term)
}

pub fn print(term: Term) -> Result<Term, String> {
    let mut p = parse_rewrite_string(PRINT_RW).unwrap();
    p.set_filename("./transform/print.rw");
    let mut pw = Rewriter::new_with_prelude(p);
    pw.rewrite(term)
}
