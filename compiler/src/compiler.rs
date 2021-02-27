use terms_format::*;
use rewriter::{parse_rewrite_string, Rewriter};

const LOWER_RW: &str = include_str!("./transform/lower.rw");
const COMPILE_RW: &str = include_str!("./transform/compile.rw");
const PRINT_RW: &str = include_str!("./transform/print.rw");

pub fn compile(term: &Term) -> Term {
    let r = parse_rewrite_string(LOWER_RW).unwrap();
    let mut rw = Rewriter::new_with_prelude(r);
    let result = rw.rewrite(term.clone());

    let c = parse_rewrite_string(COMPILE_RW).unwrap();
    let mut cw = Rewriter::new_with_prelude(c);
    let result = cw.rewrite(result);

    let p = parse_rewrite_string(PRINT_RW).unwrap();
    let mut pw = Rewriter::new_with_prelude(p);
    let result = pw.rewrite(result);

    result
}
