
use terms_format::*;
use crate::parser::rwfile::*;
use crate::interp::interp::*;

pub fn try_run_builtin_function(c: Context, p: &Program, function: &FRef, t: Term) -> Option<Term> {
    let meta = &function.meta;
    match (function.name.as_str(), t) {
        ("add", Term::TTerm(rt)) if rt.terms.len() == 2 => {
            match (rt.terms.get(0).unwrap(), rt.terms.get(1).unwrap())  {
                (Term::NTerm(n1), Term::NTerm(n2)) => Some(Term::NTerm(NTerm { value: n1.value + n2.value })),
                _ => None
            }
        },
        ("mul", Term::TTerm(rt)) if rt.terms.len() == 2 => {
            match (rt.terms.get(0).unwrap(), rt.terms.get(1).unwrap())  {
                (Term::NTerm(n1), Term::NTerm(n2)) => Some(Term::NTerm(NTerm { value: n1.value * n2.value })),
                _ => None
            }
        },
        ("div", Term::TTerm(rt)) if rt.terms.len() == 2 => {
            match (rt.terms.get(0).unwrap(), rt.terms.get(1).unwrap())  {
                (Term::NTerm(n1), Term::NTerm(n2)) => Some(Term::NTerm(NTerm { value: n1.value / n2.value })),
                _ => None
            }
        },
        ("all", Term::RTerm(rt)) if meta.len() == 1 => {
            match meta.get(0).unwrap() {
                Expr::FRef(n) => {
                    let r: Vec<Option<Term>> = rt.terms.iter().map(|t| { interp_function(c.clone(), &p, &n, t.clone()) }).collect();
                    let mut out = RTerm { constructor: rt.constructor, terms: Vec::new() };
                    for res in r {
                        match res {
                            None => { return None; },
                            Some(t) => { out.terms.push(t); }
                        }
                    }
                    Some(Term::RTerm(out))
                },
                _ => None
            }
        },
        ("all", t) => Some(t),
        _ => None
    }

}