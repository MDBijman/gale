use terms_format as tf;
use crate::parser::rwfile::*;
use std::collections::HashMap;
use crate::parser::parser::parse_rw_string;

pub struct Rewriter {
    rules: File,
    newname_counts: HashMap<String, u64>
}

#[derive(Clone, Debug)]
pub struct Context {
    bound_variables: HashMap<String, tf::Term>,
    bound_functions: HashMap<String, FRef>,
}

impl Context {
    pub fn new() -> Context {
        Context {
            bound_variables: HashMap::new(),
            bound_functions: HashMap::new(),
        }
    }

    pub fn merge_variable_bindings(&mut self, b: HashMap<String, tf::Term>) {
        for (k, v) in b {
            self.bound_variables.insert(k, v);
        }
    }
}

fn check_and_bind_match(m: &Match, t: &tf::Term) -> Option<HashMap<String, tf::Term>> {
    match (m, t) {
        (Match::AnyMatcher, _) => Some(HashMap::new()),
        (Match::TermMatcher(tm), tf::Term::RTerm(rec_term, term_annot)) => {
            if tm.constructor == rec_term.constructor && tm.terms.len() == rec_term.terms.len() {
                let mut h: HashMap<String, tf::Term> = HashMap::new();

                for annot_match in tm.annotations.iter() {
                    let mut found: bool = false;
                    for annot in term_annot.elems.iter() {
                        match check_and_bind_match(annot_match, annot) {
                            None => { },
                            Some(m) => {
                                h.extend(m);
                                found = true;
                                break;
                            }
                        }
                    }
                    if !found {
                        return None;
                    }
                }

                let maps: Vec<Option<HashMap<String, tf::Term>>> = tm.terms.iter().zip(&rec_term.terms).map(|(match_term, val_term)| check_and_bind_match(&match_term, &val_term)).collect();
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
        (Match::VariableMatcher(vm), t) => {
            let mut h: HashMap<String, tf::Term> = HashMap::new();
            h.insert(vm.name.clone(), t.clone());

            for annot_match in vm.annotations.iter() {
                let mut found: bool = false;
                for annot in t.get_annotations().elems.iter() {
                    match check_and_bind_match(annot_match, annot) {
                        None => { },
                        Some(m) => {
                            h.extend(m);
                            found = true;
                            break;
                        }
                    }
                }
                if !found {
                    return None;
                }
            }

            Some(h)
        },
        (Match::StringMatcher(sm), tf::Term::STerm(s, _)) => {
            if s.value == sm.value {
                Some(HashMap::new())
            } else {
                None
            }
        },
        (Match::NumberMatcher(nm), tf::Term::NTerm(n, _)) => {
            if n.value == nm.value {
                Some(HashMap::new())
            } else {
                None
            }
        },
        (Match::ListMatcher(lm), tf::Term::LTerm(lt, _)) => {
            let mut bindings: HashMap<String, tf::Term> = HashMap::new();
            bindings.insert(lm.head.clone(), lt.head());
            
            match &lm.tail {
                None => {
                    if lt.terms.len() != 1 { return None; }
                },
                Some(n) => {
                    if lt.terms.len() == 1 { return None; }
                    bindings.insert(n.clone(), lt.tail());
                }
            }

            Some(bindings)
        }
        (Match::TupleMatcher(tm), tf::Term::TTerm(tt, _)) => {
            let mut bindings: HashMap<String, tf::Term> = HashMap::new();
            for (m, t) in tm.elems.iter().zip(tt.terms.iter()) {
                let sub_bindings = check_and_bind_match(m, t)?;
                bindings.extend(sub_bindings);
            }
            Some(bindings)
        }
        _ => None
    }
}



impl Rewriter {
    pub fn new(f: File) -> Rewriter {
        Rewriter { rules: f, newname_counts: HashMap::new() }
    }

    pub fn new_with_prelude(f: File) -> Rewriter {
        let prelude_code = include_str!("../../std/prelude.rw");
        let prelude = parse_rw_string(&prelude_code).unwrap();
        Rewriter { rules: File::merge(prelude, f), newname_counts: HashMap::new() }
    }

    pub fn rewrite(&mut self, t: tf::Term) -> tf::Term {
        self.interp_function(
            Context::new(),
            &FRef::from(&String::from("main"), &Vec::new()),
            t).unwrap()
    }

    pub fn gen_newname_count(&mut self, n: &String) -> u64 {
        let e = self.newname_counts.entry(n.to_string()).or_insert(0);
        let r = *e;
        *e += 1;
        r
    }

    ///

    pub fn try_run_builtin_function(&mut self, c: Context, function: &FRef, t: tf::Term) -> Option<tf::Term> {
        let meta = &function.meta;
        match (function.name.as_str(), t) {
            ("add", tf::Term::TTerm(rt, _)) if rt.terms.len() == 2 => {
                match (rt.terms.get(0).unwrap(), rt.terms.get(1).unwrap())  {
                    (tf::Term::NTerm(n1, _), tf::Term::NTerm(n2, _)) => Some(tf::Term::new_number_term(n1.value + n2.value)),
                    _ => None
                }
            },
            ("mul", tf::Term::TTerm(rt, _)) if rt.terms.len() == 2 => {
                match (rt.terms.get(0).unwrap(), rt.terms.get(1).unwrap())  {
                    (tf::Term::NTerm(n1, _), tf::Term::NTerm(n2, _)) => Some(tf::Term::new_number_term(n1.value * n2.value)),
                    _ => None
                }
            },
            ("div", tf::Term::TTerm(rt, _)) if rt.terms.len() == 2 => {
                match (rt.terms.get(0).unwrap(), rt.terms.get(1).unwrap())  {
                    (tf::Term::NTerm(n1, _), tf::Term::NTerm(n2, _)) => Some(tf::Term::new_number_term(n1.value / n2.value)),
                    _ => None
                }
            },
            ("min", tf::Term::TTerm(rt, _)) if rt.terms.len() == 2 => {
                match (rt.terms.get(0).unwrap(), rt.terms.get(1).unwrap())  {
                    (tf::Term::NTerm(n1, _), tf::Term::NTerm(n2, _)) => Some(tf::Term::new_number_term(if n1.value < n2.value { n1.value } else { n2.value })),
                    _ => None
                }
            },
            ("max", tf::Term::TTerm(rt, _)) if rt.terms.len() == 2 => {
                match (rt.terms.get(0).unwrap(), rt.terms.get(1).unwrap())  {
                    (tf::Term::NTerm(n1, _), tf::Term::NTerm(n2, _)) => Some(tf::Term::new_number_term(if n1.value < n2.value { n2.value } else { n1.value })),
                    _ => None
                }
            },
            ("all", tf::Term::RTerm(rt, anots)) if meta.len() == 1 => {
                match meta.get(0).unwrap() {
                    Expr::FRef(n) => {
                        let r = rt.terms.iter()
                            .map(|t| { 
                                self.interp_function(c.clone(), &n, t.clone())
                            }).collect::<Option<Vec<tf::Term>>>();

                        
                        match r {
                            None => None,
                            Some(v) => 
                                Some(tf::Term::RTerm(tf::RTerm { constructor: rt.constructor, terms: v }, anots))
                        }
                    },
                    _ => None
                }
            },
            ("all", tf::Term::LTerm(lt, anots)) if meta.len() == 1 => {
                match meta.get(0).unwrap() {
                    Expr::FRef(n) => {
                        let r = lt.terms.iter()
                            .map(|t| { 
                                self.interp_function(c.clone(), &n, t.clone())
                            }).collect::<Option<Vec<tf::Term>>>();

                        
                        match r {
                            None => None,
                            Some(v) => 
                                Some(tf::Term::LTerm(tf::LTerm { terms: v }, anots))
                        }
                    },
                    _ => None
                }
            },
            ("all", t) => Some(t),
            ("debug", t) => {
                println!("{}", t);
                Some(t)
            },
            ("gen_name", tf::Term::STerm(st, _)) => {
                let id = self.gen_newname_count(&st.value);
                let sterm = tf::Term::new_string_term(format!("{}_{}", st.value, id).as_str());

                Some(sterm)
            },
            ("gen_num", tf::Term::STerm(st, _)) => {
                let id = self.gen_newname_count(&st.value);
                let sterm = tf::Term::new_number_term(id as f64);

                Some(sterm)
            },
            _ => None
        }

    }

    fn interp_expr(&mut self, c: Context, e: &Expr, t: tf::Term) -> Option<tf::Term> {
        match e {
            Expr::Tuple(tup) => {
                let mut res = tf::TTerm { terms: Vec::new() };
                for e in &tup.values {
                    let r = self.interp_expr(c.clone(), &e, t.clone())?;
                    res.terms.push(r);
                }

                Some(tf::Term::TTerm(res, tf::Annotations::empty()))
            },
            Expr::Invoke(inv) => {
                let arg = self.interp_expr(c.clone(), &inv.arg, t)?;
                self.interp_function(c, &inv.name, arg)
            },
            Expr::Ref(r) => {
                Some(c.bound_variables.get(&r.name).expect(format!("Cannot resolve reference {}", r.name).as_str()).clone())
            },
            //Expr::FRef(r) => Some(Term::STerm(STerm { value: c.bound_functions.get(&r.name).unwrap().clone() })),
            Expr::Number(n) => Some(tf::Term::new_number_term(n.value)),
            Expr::Op(Op::Or(l, r)) => {
                match self.interp_expr(c.clone(), &*l, t.clone()) {
                    Some(t) => Some(t),
                    None => self.interp_expr(c, &*r, t)
                }
            },
            Expr::Op(Op::And(l, r)) => {
                let lr = self.interp_expr(c.clone(), &*l, t)?;
                self.interp_expr(c.clone(), &*r, lr)
            },
            Expr::Term(et) => {
                let terms = et.terms.iter()
                    .map(|subt| {
                       let r = self.interp_expr(c.clone(), &subt, t.clone());
                       if r.is_none() { panic!(format!("Failed to compute {:?}, environment {:?}", subt, c)) }
                       
                       r.unwrap()
                    }).collect::<Vec<tf::Term>>();

                Some(tf::Term::new_rec_term(&et.constructor.name, terms))
            },
            Expr::This => Some(t),
            Expr::Annotation(a) => {
                let mut inner_term = self.interp_expr(c.clone(), &*a.inner, t.clone())?;
                for subt in a.annotations.iter() {
                    let r = self.interp_expr(c.clone(), &subt, t.clone())?;
                    inner_term.add_annotation(r);
                }

                Some(inner_term)
            },
            Expr::Text(t) => {
                Some(tf::Term::new_string_term(t.value.as_str()))
            }
            _ => panic!("Unsupported expression: {:?}", e)
        }
    }

    fn try_interp_function_instance(&mut self, c: Context, f: &Function, meta: &Vec<Expr>, t: tf::Term) -> Option<tf::Term> {
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
                (Expr::Ref(f), e)  => {
                    new_context.bound_variables.insert(
                        f.name.clone(),
                        self.interp_expr(c.clone(), e, t.clone()).unwrap());
                },
                _ => panic!("Unsupported meta expression")
            }
        }

        // println!("Matched\n\t{:?}\n\twith {}\n\twith context {:?}", f, t, c);
        // Run expr with new context
        self.interp_expr(new_context, &f.body, t)
    }

    pub fn interp_function(&mut self, c: Context, function: &FRef, t: tf::Term) -> Option<tf::Term> {
        match c.bound_functions.get(&function.name) {
            Some(f) => { return self.interp_function(c.clone(), &f /* We are throwing away call site meta args? */, t) },
            None => {}
        }

        let fns: Vec<Function> = self.rules.functions.iter()
            .filter(|f| f.name == *function.name)
            .map(|f| f.clone())
            .collect();

        for f in fns {
            match self.try_interp_function_instance(c.clone(), &f, &function.meta.clone(), t.clone()) {
                Some(t) => { return Some(t); },
                None => { }
            }
        }

        match self.try_run_builtin_function(c.clone(), function, t.clone()) {
            Some(t) => { return Some(t); },
            None => { }
        }

        None
    }
        

}