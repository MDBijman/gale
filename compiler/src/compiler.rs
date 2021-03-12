use terms_format::*;
use rewriter::{parse_rewrite_string, Rewriter};

const LOWER_RW: &str = include_str!("./transform/lower.rw");
const COMPILE_RW: &str = include_str!("./transform/compile.rw");
const PRINT_RW: &str = include_str!("./transform/print.rw");

pub fn lower(term: Term) -> Term {
    let mut r = parse_rewrite_string(LOWER_RW).unwrap();
    r.set_filename("./transform/lower.rw");
    let mut rw = Rewriter::new_with_prelude(r);
    rw.rewrite(term)
}

pub fn compile(term: Term) -> Term {
    let mut c = parse_rewrite_string(COMPILE_RW).unwrap();
    c.set_filename("./transform/compile.rw");
    let mut cw = Rewriter::new_with_prelude(c);
    cw.rewrite(term)
}

pub fn print(term: Term) -> Term {
    let mut p = parse_rewrite_string(PRINT_RW).unwrap();
    p.set_filename("./transform/print.rw");
    let mut pw = Rewriter::new_with_prelude(p);
    pw.rewrite(term)
}