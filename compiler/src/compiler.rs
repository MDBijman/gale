use terms_format::*;
use rewriter::{parse_rewrite_file, Rewriter};

pub fn compile(term: &Term) -> Term {
    let r = parse_rewrite_file("./src/transform/compile.rw").unwrap();
    let mut rw = Rewriter::new_with_prelude(r);
    let result = rw.rewrite(term.clone());
    result
}
