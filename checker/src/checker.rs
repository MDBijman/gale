use terms_format::*;
use rewriter::{parse_rewrite_string, Rewriter};

const INDEXING_RW: &str    = include_str!("./transform/indexing.rw");
const INFER_TYPES_RW: &str = include_str!("./transform/infer_types.rw");

pub fn check(term: &Term) -> Term {
    let indexing    = parse_rewrite_string(INDEXING_RW).unwrap();
    let infer_types = parse_rewrite_string(INFER_TYPES_RW).unwrap();
    let mut rw_indexing    = Rewriter::new_with_prelude(indexing);
    let mut rw_infer_types = Rewriter::new_with_prelude(infer_types);
    let r1 = rw_indexing.rewrite(term.clone());
    let r2 = rw_infer_types.rewrite(r1.clone());
    r2
}
