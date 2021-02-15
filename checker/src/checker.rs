use terms_format::*;
use rewriter::{parse_rewrite_file, Rewriter};

pub fn check(term: &Term) -> Term {
    let indexing    = parse_rewrite_file("./src/transform/indexing.rw").unwrap();
    let infer_types = parse_rewrite_file("./src/transform/infer_types.rw").unwrap();
    let mut rw_indexing    = Rewriter::new_with_prelude(indexing);
    let mut rw_infer_types = Rewriter::new_with_prelude(infer_types);
    let r1 = rw_indexing.rewrite(term.clone());
    let r2 = rw_infer_types.rewrite(r1.clone());
    r2
}
