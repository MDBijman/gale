use terms_format::*;
use rewriter::{parse_rewrite_string, Rewriter};

const COMPILE_RW: &str = include_str!("./transform/compile.rw");

pub fn compile(term: &Term) -> Term {
    let r = parse_rewrite_string(COMPILE_RW).unwrap();
    let mut rw = Rewriter::new_with_prelude(r);
    let result = rw.rewrite(term.clone());
    result
}
