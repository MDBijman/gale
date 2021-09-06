use aterms::*;
use terslib::{parse_rewrite_string, Rewriter};

const INFER_TYPES_RW: &str = include_str!("./transform/infer_types.rw");

pub fn check(term: &Term) -> Term {
    let mut infer_types = parse_rewrite_string(INFER_TYPES_RW).unwrap();
    infer_types.set_filename("./transform/infer_types.rw");
    let mut rw_infer_types = Rewriter::new_with_prelude(infer_types);
    rw_infer_types.rewrite(term.clone())
}
