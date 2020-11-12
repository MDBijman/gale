use terms_format::*;
use crate::parser::rwfile::*;
use std::collections::HashMap;
use crate::interp::builtins::*;

pub struct Program {
    file: File
}

#[derive(Clone, Debug)]
pub struct Context {
    bound_variables: HashMap<String, Term>,
    bound_functions: HashMap<String, FRef>
}

impl Context {
    pub fn new() -> Context {
        Context {
            bound_variables: HashMap::new(),
            bound_functions: HashMap::new(),
        }
    }

    pub fn merge_variable_bindings(&mut self, b: HashMap<String, Term>) {
        for (k, v) in b {
            self.bound_variables.insert(k, v);
        }
    }
}

fn check_and_bind_match(m: &Match, t: &Term) -> Option<HashMap<String, Term>> {
    match (m, t) {
        (Match::AnyMatcher, _) => Some(HashMap::new()),
        (Match::TermMatcher(tm), Term::RTerm(rec_term)) => {
            if tm.constructor == rec_term.constructor && tm.terms.len() == rec_term.terms.len() {
                let mut h: HashMap<String, Term> = HashMap::new();
                let maps: Vec<Option<HashMap<String, Term>>> = tm.terms.iter().zip(&rec_term.terms).map(|(match_term, val_term)| check_and_bind_match(&match_term, &val_term)).collect();
                for map in maps {
                    match map {
                        None => { return None; },
                        Some(bindings) => { h.extend(bindings); }
                    }
                }

                Some(h)
            } else {
                None
            }
        },
        (Match::VariableMatcher(vm), _) => {
            let mut h: HashMap<String, Term> = HashMap::new();
            h.insert(vm.name.clone(), t.clone());
            Some(h)
        },
        (Match::StringMatcher(sm), Term::STerm(s)) => {
            if s.value == sm.value {
                Some(HashMap::new())
            } else {
                None
            }
        },
        (Match::NumberMatcher(nm), Term::NTerm(n)) => {
            if n.value == nm.value {
                Some(HashMap::new())
            } else {
                None
            }
        },
        _ => None
    }
}

fn interp_expr(c: Context, p: &Program, e: &Expr, t: Term) -> Option<Term> {
    match e {
        Expr::Tuple(tup) => {
            let mut res = TTerm { terms: Vec::new() };
            for e in &tup.values {
                res.terms.push(interp_expr(c.clone(), &p, &e, t.clone()).unwrap());
            }

            Some(Term::TTerm(res))
        },
        Expr::Invoke(inv) => {
            let arg = interp_expr(c.clone(), &p, &inv.arg, t)?;
            interp_function(c, p, &inv.name, arg)
        },
        Expr::Ref(r) => Some(c.bound_variables.get(&r.name).unwrap().clone()),
        //Expr::FRef(r) => Some(Term::STerm(STerm { value: c.bound_functions.get(&r.name).unwrap().clone() })),
        Expr::Number(n) => Some(Term::NTerm(NTerm { value: n.value })),
        Expr::Op(Op::Or(l, r)) => {
            match interp_expr(c.clone(), &p, &*l, t.clone()) {
                Some(t) => Some(t),
                None => {
                    let r = interp_expr(c.clone(), &p, &*r, t);
                    r
                }
            }
        },
        Expr::Op(Op::And(l, r)) => {
            let lr = interp_expr(c.clone(), &p, &*l, t)?;
            interp_expr(c.clone(), &p, &*r, lr)
        },
        Expr::This => Some(t),
        _ => panic!("Unsupported expression")
    }
}

fn try_interp_function_instance(c: Context, p: &Program, f: &Function, meta: &Vec<Expr>, t: Term) -> Option<Term> {
    // Check matcher and bind variables to terms
    let bindings = check_and_bind_match(&f.matcher, &t)?;

    let mut new_context: Context = Context::new();

    new_context.merge_variable_bindings(bindings);

    // Bind metas
    for (p, a) in f.meta.iter().zip(meta.iter()) {
       
        match (p, a) {
            (Expr::FRef(f), Expr::FRef(n)) => { 
                match c.bound_functions.get(&n.name) {
                    Some(resolved_n) => new_context.bound_functions.insert(f.name.clone(), resolved_n.clone()), 
                    None => new_context.bound_functions.insert(f.name.clone(), n.clone())
                };
            },
            (Expr::Ref(f),  Expr::Ref(n))  => { new_context.bound_variables.insert(f.name.clone(), c.bound_variables.get(&n.name).unwrap().clone()); },
            _ => panic!("Unsupported meta expression")
        }
    }

    // println!("Matched\n\t{:?}\n\twith {}\n\twith context {:?}", f, t, c);
    // Run expr with new context
    let r = interp_expr(new_context, p, &f.body, t);
    r
}

pub fn interp_function(c: Context, p: &Program, function: &FRef, t: Term) -> Option<Term> {
    // println!("{:?}", c.bound_functions);
    match c.bound_functions.get(&function.name) {
        Some(f) => { return interp_function(c.clone(), p, &f /* We are throwing away call site meta args */, t) },
        None => {}
    }

    for f in &p.file.functions {
        if f.name == *function.name {
            match try_interp_function_instance(c.clone(), &p, &f, &function.meta.clone(), t.clone()) {
                Some(t) => { return Some(t); },
                None => { }
            }
        }
    }

    match try_run_builtin_function(c.clone(), &p, function, t.clone()) {
        Some(t) => { return Some(t); },
        None => { }
    }

    None
}

impl Program {
    pub fn new(f: File) -> Program {
        Program { file: f }
    }

    pub fn rewrite_term(&self, t: Term) -> Term {

        let r = interp_function(
            Context::new(),
            &self,
            &FRef::from(&String::from("main"), &Vec::new()),
            t);

        // println!("{:?}", r.clone().unwrap());

        r.unwrap()
    }
}