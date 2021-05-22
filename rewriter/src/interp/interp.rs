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

    pub fn bind_variable(&mut self, name: &str, term: tf::Term) {
        self.bound_variables.insert(String::from(name), term);
    }

    pub fn merge_variable_bindings(&mut self, b: HashMap<String, tf::Term>) {
        for (k, v) in b {
            self.bound_variables.insert(k, v);
        }
    }

    pub fn merge_function_bindings(&mut self, b: HashMap<String, FRef>) {
        for (k, v) in b {
            self.bound_functions.insert(k, v);
        }
    }

    pub fn merge(&mut self, other: Context) {
        self.merge_variable_bindings(other.bound_variables);
        self.merge_function_bindings(other.bound_functions);
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

#[derive(Debug)]
pub struct Failure {
    error_message: String,
    callstack: Vec<String>,
    failure_context: Context,
    term: tf::Term
}

impl Failure {
    pub fn with_call(&mut self, call: String) {
        self.callstack.push(call);
    }
}

fn check_and_bind_annotations(matchers: &Vec<Match>, mut annots: tf::Annotations)  -> Option<Context> {
    let mut h: Context = Context::new();

    for matcher_idx in 0..matchers.len() {
        let matcher = &matchers[matcher_idx];
        match matcher {
            Match::VariadicElementMatcher(v) => {
                h.bound_variables.insert(v.name.clone(), tf::Term::new_list_term(annots.elems));
                assert!(matcher_idx == matchers.len() - 1);
                break;
            },
            _ => {
                let mut found_idx = None;
                
                for (idx, annot) in annots.elems.iter().enumerate() {
                    match check_and_bind_match(&matcher, annot) {
                        Some(m) => {
                            h.merge(m);
                            found_idx = Some(idx);
                            break;
                        },
                        None => {}
                    }
                }

                if let Some(idx) = found_idx {
                    annots.elems.remove(idx)
                } else {
                    return None
                };
            }
        }
    }

    Some(h)
}

fn check_and_bind_match(m: &Match, t: &tf::Term) -> Option<Context> {
    match (m, t) {
        (Match::VariadicElementMatcher(_), _) => panic!("Cannot match variadic element matcher"),
        (Match::AnyMatcher, _) => Some(Context::new()),
        (Match::TermMatcher(tm), tf::Term::RTerm(rec_term, term_annot)) => {
            let mut bindings: Context = Context::new();

            let head_binding = check_and_bind_match(&tm.constructor, &tf::Term::new_string_term(&rec_term.constructor));
            if head_binding.is_none() { return None; }
            bindings.merge(head_binding.unwrap());

            let mut i = 0;
            for m in tm.terms.iter() {
                match m {
                    Match::VariadicElementMatcher(v) => {
                        bindings.bound_variables.insert(v.name.clone(), tf::Term::new_list_term(rec_term.terms[i..].to_vec()));
                        break;
                    },
                    _ => {
                        if i as i64 > (rec_term.terms.len() as i64) - 1 { return None };
                        let sub_bindings = check_and_bind_match(m, &rec_term.terms[i])?;
                        bindings.merge(sub_bindings);
                    }
                }
                    
                i += 1;
            }

            match check_and_bind_annotations(&tm.annotations, term_annot.clone()) {
                None => return None,
                Some(b) => bindings.merge(b)
            }

            Some(bindings)
        },
        (Match::VariableMatcher(vm), t) => {
            let mut bindings: Context = Context::new();
            bindings.bind_variable(vm.name.as_str(), t.clone());

            match check_and_bind_annotations(&vm.annotations, t.get_annotations().clone()) {
                None => return None,
                Some(b) => bindings.merge(b)
            }

            Some(bindings)
        },
        (Match::StringMatcher(sm), tf::Term::STerm(s, _)) => {
            if s.value == sm.value {
                Some(Context::new())
            } else {
                None
            }
        },
        (Match::NumberMatcher(nm), tf::Term::NTerm(n, _)) => {
            if n.value == nm.value {
                Some(Context::new())
            } else {
                None
            }
        },
        (Match::ListMatcher(lm), tf::Term::LTerm(lt, _)) => {
            let mut bindings: Context = Context::new();
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
                    bindings.merge(sub_bindings);

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
                            bindings.merge(sub_bindings);

                            Some(bindings)
                        }
                    }
                }
            }
        }
        (Match::TupleMatcher(tm), tf::Term::TTerm(tt, annots)) => {
            // 1 to 1 matching elements with matcher
            let mut bindings: Context = Context::new();
            let mut i = 0;
            for m in tm.elems.iter() {
                match m {
                    Match::VariadicElementMatcher(v) => {
                        bindings.bind_variable(v.name.as_str(), tf::Term::new_list_term(tt.terms[i..].to_vec()));
                        return Some(bindings);
                    },
                    _ => {
                        if i as i64 > (tt.terms.len() as i64) - 1 { return None };
                        let sub_bindings = check_and_bind_match(m, &tt.terms[i])?;
                        bindings.merge(sub_bindings);
                    }
                }
                    
                i += 1;
            }

            match check_and_bind_annotations(&tm.annotations, annots.clone()) {
                None => return None,
                Some(b) => bindings.merge(b)
            }

            Some(bindings)
        },
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
        let result = self.interp_function(
            Context::new(),
            &FRef::from(&String::from("main"), &Vec::new(), FunctionReferenceType::Force),
            t);

        match result {
            Err(f) => {
                println!("");
                println!("{}", f.error_message);

                for call in f.callstack {
                    println!("in {}", call);
                }

                if self.rules.filename.is_some() {
                    println!("in {}", self.rules.filename.as_ref().unwrap());
                }

                println!("{}", f.failure_context);
                println!("{}", f.term);
                panic!("");
            },
            Ok(t) => {
               t.unwrap().to_term().unwrap() 
            }
        }
    }

    pub fn get_newname_count(&mut self, n: &String) -> u64 {
        *self.newname_counts.entry(n.to_string()).or_insert(0)
    }

    pub fn gen_newname_count(&mut self, n: &String) -> u64 {
        let e = self.newname_counts.entry(n.to_string()).or_insert(0);
        let r = *e;
        *e += 1;
        r
    }

    pub fn reset_newname_count(&mut self, n: &String) {
        self.newname_counts.remove(n);
    }
    ///
     
    fn is_builtin(name: &str) -> bool {
        ["add", "mul", "div", "min", "max", "subterms", "with_subterms", "debug", "debug_context", "fail", "gen_name", "gen_num", "get_num", "reset_num", "eq", "concat_str", "to_str"].contains(&name)
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
            ("with_subterms", tf::Term::TTerm(rt, _)) if meta.len() == 0 => {
                match (&rt.terms[0], &rt.terms[1]) {
                    (tf::Term::RTerm(constr, _), tf::Term::LTerm(elems, _)) => {
                        Some(Rewriter::term_to_expr(&tf::Term::new_rec_term(constr.constructor.as_str(), elems.terms.clone())))
                    },
                    _ => None
                }
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
            ("get_num", tf::Term::STerm(st, _)) => {
                let id = self.get_newname_count(&st.value);
                let sterm = tf::Term::new_number_term(id as f64);

                Some(Rewriter::term_to_expr(&sterm))
            },
            ("reset_num", tf::Term::STerm(st, a)) => {
                self.reset_newname_count(&st.value);

                Some(Rewriter::term_to_expr(&tf::Term::STerm(st, a)))
            },
            ("concat_str", tf::Term::LTerm(lt, _)) => {
                let mut out: String = String::new();

                for item in lt.terms {
                    match item {
                        tf::Term::STerm(s, _) => out.push_str(s.value.as_str()),
                        _ => return None
                    }
                }

                Some(Rewriter::term_to_expr(&tf::Term::new_string_term(&out)))
            },
            ("to_str", t) => {
                Some(Rewriter::term_to_expr(&tf::Term::new_string_term(&format!("{}", t))))
            }
            ("eq", tf::Term::TTerm(tt, _)) if tt.terms.len() == 2 => {
                let lhs = &tt.terms[0];
                let rhs = &tt.terms[1];

                if lhs == rhs {
                    Some(Rewriter::term_to_expr(&tf::Term::new_tuple_term(vec![lhs.clone(), rhs.clone()])))
                } else {
                    None
                }
            },
            _ => None
        }
    }

    fn reduce(&mut self, c: Context, e: &Expr, t: tf::Term) -> Result<Option<Expr>, Failure> {
        Ok(match e {
            Expr::FRef(f) => {
                let new_meta: Vec<Expr> = f.meta.iter().map(|e| self.reduce(c.clone(), e, t.clone()).unwrap().unwrap()).collect();

                match c.bound_functions.get(&f.name) {
                    Some(f) => Some(Expr::FRef(FRef {
                        name: f.name.clone(),
                        meta: [f.meta.clone(), new_meta].concat(),
                        ref_type: f.ref_type
                    })),
                    _ => Some(Expr::FRef(FRef {
                        name: f.name.clone(),
                        meta: new_meta,
                        ref_type: f.ref_type
                    }))
                }
            },
            Expr::Tuple(tup) => {
                let mut res: Vec<tf::Term> = vec![];
                for e in &tup.values {
                    match e {
                        // Variadic unrolling such as (a, b, ..c)
                        Expr::UnrollVariadic(l) => {
                            let list = c.bound_variables.get(&l.name).expect(format!("Cannot resolve reference {}", l.name).as_str());
                            match list {
                                // Reference must resolve to list term
                                tf::Term::LTerm(l, _) => {
                                    for elem in &l.terms {
                                        res.push(elem.clone());
                                    }
                                },
                                _ => return Ok(None)
                            }
                        },

                        // Not variadic
                        _ => {
                            let value = self.reduce(c.clone(), &e, t.clone())?;
                            if value.is_none() {
                                return Ok(None)
                            }

                            res.push(Rewriter::expr_to_term(&value.unwrap()))
                        }
                    }
                }

                Some(Expr::SimpleTerm(tf::Term::new_tuple_term(res)))
            },
            Expr::Invoke(inv) => {
                let arg = match self.reduce(c.clone(), &inv.arg, t.clone())? {
                    Some(t) => t.to_term().unwrap(),
                    None => return Ok(None)
                };

                return self.interp_function(c, &inv.name, arg)
            },
            Expr::Ref(r) => {
                let res = &c.bound_variables.get(&r.name);
                if res.is_none() {
                    return Err(Failure {
                        error_message: String::from(format!("Could not resolve variable, {}", r.name)),
                        callstack: vec![],
                        failure_context: c.clone(),
                        term: t.clone()
                    })
                }
                Some(Rewriter::term_to_expr(res.unwrap()))
            },
            Expr::UnrollVariadic(_) => {
                panic!("Cannot interp Expr::UnrollVariadic");
            },
            Expr::Number(n) => Some(Rewriter::term_to_expr(&tf::Term::new_number_term(n.value))),
            Expr::Op(Op::Choice(cond, th, el)) => {
                match self.reduce(c.clone(), &*cond, t.clone())? {
                    Some(res) => self.reduce(c.clone(), &*th, res.to_term().unwrap().clone())?,
                    None => self.reduce(c.clone(), &*el, t.clone())?,
                }
            },
            Expr::Op(Op::Or(l, r)) => {
                match self.reduce(c.clone(), &*l, t.clone())? {
                    Some(t) => Some(t),
                    None => self.reduce(c, &*r, t)?
                }
            },
            Expr::Op(Op::And(l, r)) => {
                let lr = match self.reduce(c.clone(), &*l, t.clone())? {
                    Some(t) => t.to_term().unwrap(),
                    None => return Ok(None)
                };

                self.reduce(c.clone(), &*r, lr)?
            },
            Expr::Term(et) => {
                let head = self.reduce(c.clone(), &et.constructor, t.clone())?;

                if head.is_none() { return Ok(None); }

                // Interpret subterms as tuple
                let terms = match self.reduce(c.clone(), &Expr::Tuple(Tuple { values: et.terms.clone() }), t.clone())? {
                    Some(Expr::SimpleTerm(tf::Term::TTerm(ts, _))) => {
                        ts.terms
                    },
                    _ => return Ok(None)
                };

                match head.unwrap() {
                    Expr::SimpleTerm(tf::Term::STerm(s, _)) => 
                        Some(Rewriter::term_to_expr(&tf::Term::new_rec_term(&s.value, terms))),
                    _ => None
                }
            },
            Expr::This => Some(Rewriter::term_to_expr(&t)),
            Expr::AddAnnotation(a) => {
                let mut inner_term = match self.reduce(c.clone(), &*a.inner, t.clone())? {
                    Some(t) => t.to_term().unwrap(),
                    None => return Ok(None)
                };

                for subt in a.annotations.iter() {
                    let r = match self.reduce(c.clone(), &subt, t.clone())? {
                        Some(t) => t.to_term().unwrap(),
                        None => return Ok(None)
                    };
                    inner_term.add_annotation(r);
                }

                Some(Rewriter::term_to_expr(&inner_term))
            },
            Expr::Annotation(a) => {
                let mut inner_term = match self.reduce(c.clone(), &*a.inner, t.clone())? {
                    Some(t) => t.to_term().unwrap(),
                    None => return Ok(None)
                };

                inner_term.clear_annotations();
                for subt in a.annotations.iter() {
                    match subt {
                        Expr::UnrollVariadic(n) => {
                            let list = c.bound_variables.get(&n.name).expect(format!("Cannot resolve reference {}", n.name).as_str());
                            match list {
                                // Reference must resolve to list term
                                tf::Term::LTerm(l, _) => {
                                    for elem in &l.terms {
                                        inner_term.add_annotation(elem.clone());
                                    }
                                },
                                _ => return Ok(None)
                            }
                        },
                        _ => {
                            let r = match self.reduce(c.clone(), &subt, t.clone())? {
                                Some(t) => t.to_term().unwrap(),
                                None => return Ok(None)
                            };

                            inner_term.add_annotation(r);
                        }
                    }
                }

                Some(Rewriter::term_to_expr(&inner_term))
            },
            Expr::Text(t) => {
                Some(Rewriter::term_to_expr(&tf::Term::new_string_term(t.value.as_str())))
            },
            Expr::List(l) => {
                let mut res = tf::LTerm { terms: Vec::new() };
                for e in &l.values {
                    let r = match self.reduce(c.clone(), &e, t.clone())? {
                        Some(t) => t.to_term().unwrap(),
                        None => return Ok(None)
                    };

                    res.terms.push(r);
                }
                Some(Rewriter::term_to_expr(&tf::Term::LTerm(res, tf::Annotations::empty())))
            },
            Expr::Let(l) => {
                let rhs_res = match self.reduce(c.clone(), &*l.rhs, t.clone())? {
                    Some(t) => t.to_term().unwrap(),
                    None => return Ok(None)
                };

                match check_and_bind_match(&l.lhs, &rhs_res) {
                    Some(bindings) => {
                        let mut new_c = c.clone();
                        new_c.merge(bindings);
                        self.reduce(new_c, &*l.body, t)?
                    },
                    None => None
                }
            },
            Expr::ListCons(l) => {
                let head_res = match self.reduce(c.clone(), &*l.head, t.clone())? {
                    Some(t) => t.to_term().unwrap(),
                    None => return Ok(None)
                };

                match self.reduce(c.clone(), &*l.tail, t.clone())? {
                    Some(t) => match t.to_term() {
                        Some(tf::Term::LTerm(tail_res, a)) => {
                            let mut res = vec![head_res];
                            res.extend(tail_res.terms.into_iter());
                            Some(Rewriter::term_to_expr(&tf::Term::new_anot_list_term(res, a.elems)))
                        },
                        _ => {
                            None
                        }
                    }
                    None => return Ok(None)
                }
            },
            st@Expr::SimpleTerm(_) => Some(st.clone())
        })
    }

    fn term_to_expr(t: &tf::Term) -> Expr {
        Expr::SimpleTerm(t.clone())
    }

    fn expr_to_term(t: &Expr) -> tf::Term {
        match t {
            Expr::SimpleTerm(t) => t.clone(),
            _ => panic!("Can only unwrap SimpleTerm")
        }
    }

    fn check_is_term(e: Expr) -> Option<Expr> {
        match e {
            t@Expr::SimpleTerm(_) => Some(t),
            _ => None
        }
    }

    fn try_interp_function_instance(&mut self, c: Context, f: &Function, meta: &Vec<Expr>, t: tf::Term) -> Result<Option<Expr>, Failure> {
        // Check matcher and bind variables to terms
        let maybe_bindings = check_and_bind_match(&f.matcher, &t);
        if maybe_bindings.is_none() { return Ok(None) }
        let bindings = maybe_bindings.unwrap();

        let mut new_context: Context = Context::new();

        new_context.merge(bindings);

        // Bind metas
        for (p, a) in f.meta.iter().zip(meta.iter()) {
            match (p, a) {
                (Expr::FRef(param), arg@Expr::FRef(_)) => { 
                    let resolved_arg = self.reduce(c.clone(), arg, t.clone())?;
                    match resolved_arg {
                        Some(Expr::FRef(f)) => {
                            new_context.bound_functions.insert(param.name.clone(), f);
                        },
                        _ => { return Ok(None); }
                    }
                },
                (Expr::Ref(f), e)  => {
                    new_context.bound_variables.insert(
                        f.name.clone(),
                        self.reduce(c.clone(), e, t.clone())?.unwrap().to_term().unwrap());
                },
                _ => panic!("Unsupported meta expression")
            }
        }

        self.reduce(new_context, &f.body, t)
    }

    pub fn interp_function(&mut self, c: Context, function: &FRef, t: tf::Term) -> Result<Option<Expr>, Failure> {
        
        // Evaluate meta args
        let new_meta: Vec<Expr> = function.meta.iter().map(|e| self.reduce(c.clone(), e, t.clone()).unwrap().unwrap()).collect();

        // Try to dereference dynamically bound function
        let derefed_function = match c.bound_functions.get(&function.name) {
            Some(f) => {
                let merged_meta = f.meta.clone().into_iter().chain(new_meta.into_iter()).collect::<Vec<Expr>>();
                FRef { name: f.name.clone(), meta: merged_meta, ref_type: f.ref_type }
            },
            None => { FRef { name: function.name.clone(), meta: new_meta, ref_type: function.ref_type } } 
        };


        // Find user-defined functions
        let fns: Vec<Function> = self.rules.functions.iter()
            .filter(|f| f.name == *derefed_function.name)
            .map(|f| f.clone())
            .collect();

        if fns.len() == 0 && !Rewriter::is_builtin(&derefed_function.name) {
            panic!("No function with this name: {}", derefed_function.name);
        }

        // Try user-defined function
        for f in fns {
            match self.try_interp_function_instance(c.clone(), &f, &derefed_function.meta, t.clone()) {
                Err(mut fail) => {
                    fail.with_call(f.name);
                    return Err(fail);
                }
                Ok(res) => match res {
                    Some(t) => { return Ok(Some(Rewriter::check_is_term(t).unwrap())); },
                    None => {}
                }
            }
        }

        // Try built-in function
       match self.try_run_builtin_function(c.clone(), function, t.clone()) {
           Some(t) => { return Ok(Some(t)); },
           None => {}
       }

        if function.ref_type == FunctionReferenceType::Force {
            Err(Failure {
                error_message: String::from(format!("Failed to compute function {}", function.name.clone())),
                callstack: vec![],
                failure_context: c.clone(),
                term: t.clone()
            })
        } else {
            Ok(None)
        }
    }
}