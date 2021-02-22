use terms_format as tf;
use crate::parser::rwfile::*;
use std::collections::HashMap;
use crate::parser::parser::parse_rw_string;
use std::fmt;

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

impl fmt::Display for Context {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "context\n")?;
        for (k, v) in self.bound_variables.iter() {
            write!(f, "{}: {}\n", k, v)?;
        }
        for (k, v) in self.bound_functions.iter() {
            write!(f, "{}: {} [{:?}]\n", k, v.name, v.meta)?;
        }
        Ok(())
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
            match &lm.head {
                None => {
                    if lt.terms.len() == 0 {
                        Some(bindings)
                    } else {
                        None
                    }
                },
                Some(head_matcher) => {
                    if lt.terms.len() == 0 { return None; };
                    let sub_bindings = check_and_bind_match(&*head_matcher, &lt.terms[0])?;
                    bindings.extend(sub_bindings);

                    match &lm.tail {
                        None => {
                            if lt.terms.len() == 1 {
                                Some(bindings)
                            } else {
                                None
                            }
                        },
                        Some(tail_matcher) => {
                            let sub_bindings = check_and_bind_match(&*tail_matcher, &tf::Term::new_list_term(lt.terms[1..].to_vec()))?;
                            bindings.extend(sub_bindings);

                            Some(bindings)
                        }
                    }
                }
            }
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
            &FRef::from(&String::from("main"), &Vec::new(), FunctionReferenceType::Force),
            t).unwrap().to_term().unwrap()
    }

    pub fn gen_newname_count(&mut self, n: &String) -> u64 {
        let e = self.newname_counts.entry(n.to_string()).or_insert(0);
        let r = *e;
        *e += 1;
        r
    }

    ///
     
    fn is_builtin(name: &str) -> bool {
        ["add", "mul", "div", "min", "max", "subterms", "debug", "debug_context", "fail", "gen_name", "gen_num", "eq"].contains(&name)
    }

    pub fn try_run_builtin_function(&mut self, c: Context, function: &FRef, t: tf::Term) -> Option<Expr> {
        let meta = &function.meta;
        match (function.name.as_str(), t) {
            ("add", tf::Term::TTerm(rt, _)) if rt.terms.len() == 2 => {
                match (rt.terms.get(0).unwrap(), rt.terms.get(1).unwrap())  {
                    (tf::Term::NTerm(n1, _), tf::Term::NTerm(n2, _)) => Some(Rewriter::term_to_expr(&tf::Term::new_number_term(n1.value + n2.value))),
                    _ => None
                }
            },
            ("mul", tf::Term::TTerm(rt, _)) if rt.terms.len() == 2 => {
                match (rt.terms.get(0).unwrap(), rt.terms.get(1).unwrap())  {
                    (tf::Term::NTerm(n1, _), tf::Term::NTerm(n2, _)) => Some(Rewriter::term_to_expr(&tf::Term::new_number_term(n1.value * n2.value))),
                    _ => None
                }
            },
            ("div", tf::Term::TTerm(rt, _)) if rt.terms.len() == 2 => {
                match (rt.terms.get(0).unwrap(), rt.terms.get(1).unwrap())  {
                    (tf::Term::NTerm(n1, _), tf::Term::NTerm(n2, _)) => Some(Rewriter::term_to_expr(&tf::Term::new_number_term(n1.value / n2.value))),
                    _ => None
                }
            },
            ("min", tf::Term::TTerm(rt, _)) if rt.terms.len() == 2 => {
                match (rt.terms.get(0).unwrap(), rt.terms.get(1).unwrap())  {
                    (tf::Term::NTerm(n1, _), tf::Term::NTerm(n2, _)) => Some(Rewriter::term_to_expr(&tf::Term::new_number_term(if n1.value < n2.value { n1.value } else { n2.value }))),
                    _ => None
                }
            },
            ("max", tf::Term::TTerm(rt, _)) if rt.terms.len() == 2 => {
                match (rt.terms.get(0).unwrap(), rt.terms.get(1).unwrap())  {
                    (tf::Term::NTerm(n1, _), tf::Term::NTerm(n2, _)) => Some(Rewriter::term_to_expr(&tf::Term::new_number_term(if n1.value < n2.value { n2.value } else { n1.value }))),
                    _ => None
                }
            },
            ("subterms", tf::Term::RTerm(rt, _)) if meta.len() == 0 => {
                Some(Rewriter::term_to_expr(&tf::Term::new_list_term(rt.terms)))
            },
            ("debug", t) => {
                println!("{}", t);
                Some(Rewriter::term_to_expr(&t))
            },
            ("debug_context", t) => {
                println!("{}", c);
                Some(Rewriter::term_to_expr(&t))
            },
            ("fail", _) => {
                None
            },
            ("gen_name", tf::Term::STerm(st, _)) => {
                let id = self.gen_newname_count(&st.value);
                let sterm = tf::Term::new_string_term(format!("{}_{}", st.value, id).as_str());

                Some(Rewriter::term_to_expr(&sterm))
            },
            ("gen_num", tf::Term::STerm(st, _)) => {
                let id = self.gen_newname_count(&st.value);
                let sterm = tf::Term::new_number_term(id as f64);

                Some(Rewriter::term_to_expr(&sterm))
            },
            ("eq", tf::Term::TTerm(tt, _)) if tt.terms.len() == 2 => {
                let lhs = &tt.terms[0];
                let rhs = &tt.terms[1];

                if lhs == rhs {
                    Some(Rewriter::term_to_expr(&tf::Term::new_tuple_term(vec![lhs.clone(), rhs.clone()])))
                } else {
                    None
                }
            },
            // This assumes built-ins are always resolved last
            _ => None // panic!("No function with name {}", function.name)
        }
    }

    fn reduce(&mut self, c: Context, e: &Expr, t: tf::Term) -> Option<Expr> {
        match e {
            Expr::FRef(f) => {
                let new_meta: Vec<Expr> = f.meta.iter().map(|e| self.reduce(c.clone(), e, t.clone()).unwrap()).collect();
                Some(Expr::FRef(FRef {
                    name: f.name.clone(),
                    meta: new_meta,
                    ref_type: f.ref_type
                }))
            },
            Expr::Tuple(tup) => {
                let mut res = Tuple { values: Vec::new() };
                for e in &tup.values {
                    res.values.push(self.reduce(c.clone(), &e, t.clone())?);
                }

                Some(Expr::Tuple(res))
            },
            Expr::Invoke(inv) => {
                let arg = self.reduce(c.clone(), &inv.arg, t)?.to_term().unwrap();
                //println!("-> {:?}", &inv);
                self.interp_function(c, &inv.name, arg)
            },
            Expr::Ref(r) => {
                Some(Rewriter::term_to_expr(&c.bound_variables.get(&r.name).expect(format!("Cannot resolve reference {}", r.name).as_str())))
            },
            Expr::Number(n) => Some(Rewriter::term_to_expr(&tf::Term::new_number_term(n.value))),
            Expr::Op(Op::Or(l, r)) => {
                match self.reduce(c.clone(), &*l, t.clone()) {
                    Some(t) => Some(t),
                    None => self.reduce(c, &*r, t)
                }
            },
            Expr::Op(Op::And(l, r)) => {
                let lr = self.reduce(c.clone(), &*l, t)?.to_term()?;
                self.reduce(c.clone(), &*r, lr)
            },
            Expr::Term(et) => {
                let terms = et.terms.iter()
                    .map(|subt| {
                       let r = self.reduce(c.clone(), &subt, t.clone());
                       if r.is_none() { panic!(format!("Failed to compute\n{:#?}\nenvironment {}", subt, c)) }
                       
                       r.unwrap().to_term().unwrap()
                    }).collect::<Vec<tf::Term>>();

                Some(Rewriter::term_to_expr(&tf::Term::new_rec_term(&et.constructor.name, terms)))
            },
            Expr::This => Some(Rewriter::term_to_expr(&t)),
            Expr::Annotation(a) => {
                let mut inner_term = self.reduce(c.clone(), &*a.inner, t.clone())?.to_term()?;
                for subt in a.annotations.iter() {
                    let r = self.reduce(c.clone(), &subt, t.clone())?.to_term()?;
                    inner_term.add_annotation(r);
                }

                Some(Rewriter::term_to_expr(&inner_term))
            },
            Expr::Text(t) => {
                Some(Rewriter::term_to_expr(&tf::Term::new_string_term(t.value.as_str())))
            },
            Expr::List(l) => {
                let mut res = tf::LTerm { terms: Vec::new() };
                for e in &l.values {
                    let r = self.reduce(c.clone(), &e, t.clone())?.to_term()?;
                    res.terms.push(r);
                }
                Some(Rewriter::term_to_expr(&tf::Term::LTerm(res, tf::Annotations::empty())))
            },
            Expr::Let(l) => {
                let rhs_res = self.reduce(c.clone(), &*l.rhs, t.clone())?.to_term()?;
                match check_and_bind_match(&l.lhs, &rhs_res) {
                    Some(bindings) => {
                        let mut new_c = c.clone();
                        new_c.merge_variable_bindings(bindings);
                        self.reduce(new_c, &*l.body, t)
                    },
                    None => None
                }
            },
            Expr::ListCons(l) => {
                let head_res = self.reduce(c.clone(), &*l.head, t.clone())?.to_term()?;
                match self.reduce(c.clone(), &*l.tail, t.clone())?.to_term()? {
                    tf::Term::LTerm(tail_res, a) => {
                        let mut res = vec![head_res];
                        res.extend(tail_res.terms.into_iter());
                        Some(Rewriter::term_to_expr(&tf::Term::new_anot_list_term(res, a.elems)))
                    },
                    _ => {
                        None
                    }
                }
            },
            st@Expr::SimpleTerm(_) => Some(st.clone())
        }
    }

    fn term_to_expr(t: &tf::Term) -> Expr {
        Expr::SimpleTerm(t.clone())
    }

    fn try_interp_function_instance(&mut self, c: Context, f: &Function, meta: &Vec<Expr>, t: tf::Term) -> Option<Expr> {
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
                        self.reduce(c.clone(), e, t.clone()).unwrap().to_term().unwrap());
                },
                _ => panic!("Unsupported meta expression")
            }
        }

        self.reduce(new_context, &f.body, t)
    }

    pub fn interp_function(&mut self, c: Context, function: &FRef, t: tf::Term) -> Option<Expr> {
        // Evaluate meta args
        let new_meta: Vec<Expr> = function.meta.iter().map(|e| self.reduce(c.clone(), e, t.clone()).unwrap()).collect();

        // Try dynamically bound function
        match c.bound_functions.get(&function.name) {
            Some(f) => { 
                let merged_meta = f.meta.clone().into_iter().chain(new_meta.into_iter()).collect::<Vec<Expr>>();
                let new_fref = FRef { name: f.name.clone(), meta: merged_meta, ref_type: f.ref_type };
                return self.interp_function(c.clone(), &new_fref, t)
            },
            None => { }
        }

        // Find user-defined functions
        let fns: Vec<Function> = self.rules.functions.iter()
            .filter(|f| f.name == *function.name)
            .map(|f| f.clone())
            .collect();

        if fns.len() == 0 && !Rewriter::is_builtin(&function.name) {
            panic!("No function with this name: {}", function.name);
        }

        // Try user-defined function
        for f in fns {
            match self.try_interp_function_instance(c.clone(), &f, &new_meta, t.clone()) {
                Some(t) => { return Some(t); },
                None => {}
            }
        }

        // Try built-in function
       match self.try_run_builtin_function(c.clone(), function, t.clone()) {
           Some(t) => { return Some(t); },
           None => {}
       }

        if function.ref_type == FunctionReferenceType::Force {
            panic!(format!("Failed to compute function {}\n{:#?}\nenvironment {}", function.name, t, c));
        } else {
            None
        }
    }
}