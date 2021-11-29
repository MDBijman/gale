use aterms::base::*;
use ters::{parse_rewrite_string, Rewriter};

const DESUGAR_RW: &str = include_str!("./transform/desugar.rw");

pub fn desugar(term: &Term) -> Term {
    let mut desugar = parse_rewrite_string(DESUGAR_RW).unwrap();
    desugar.set_filename("./transform/desugar.rw");
    let mut rw_desugar = Rewriter::new_with_prelude(desugar);
    rw_desugar.rewrite(term.clone())
}
