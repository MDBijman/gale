
use terms_format::*;

fn indent(indent: usize, elems: Vec<String>) -> Vec<String> {
    elems.into_iter().map(|e| indent_single(indent, e)).collect()
}

fn indent_single(indent: usize, s: String) -> String {
    String::from("\t").repeat(indent) + &s
}

fn join(elems: Vec<String>) -> String {
    elems.into_iter().fold(String::new(), |acc, s| acc + &s)
}

fn separate_with(elems: Vec<String>, delim: &str) -> String {
    match elems.len() {
        0 => String::new(),
        _ => {
            let first = elems[0].clone();
            elems[1..].into_iter().fold(first, |acc, t| (acc + delim + t))
        }
    }
}

fn append_each(elems: Vec<String>, delim: &str) -> Vec<String> {
    elems.into_iter().map(|e| e + delim).collect()
}

fn delimit(lhs: &str, elems: &String, rhs: &str) -> String {
    String::from(lhs) + elems + rhs
}

pub fn pretty_print_gale_term(term: &Term) -> String {
    match term {
        Term::RTerm(rt, _) => {
            let mut children: Vec<String> = rt.terms.iter().map(|t| pretty_print_gale_term(t)).collect();
            match rt.constructor.as_str() {
                "File"      => separate_with(append_each(children, ";"), "\n"),
                "Let"       => format!("let {}: {} = {}", children.get(0).unwrap(), children.get(1).unwrap(), children.get(2).unwrap()),
                "FnType"    => format!("{} -> {}", children.get(0).unwrap(), children.get(1).unwrap()),
                "TypeId"    => children.get(0).unwrap().clone(),
                "TypeTuple" => delimit("(", &separate_with(children, ","), ")"),
                "TypeArray" => format!("[{}; {}]", children.get(0).unwrap(), children.get(1).unwrap()),
                "Params"    => delimit("(", &separate_with(children, ","), ")"),
                "Ref"       => separate_with(children, "."),
                "Appl"      => children.get(0).unwrap().clone() + " " + &children.get(1).unwrap().clone(),
                "Return"    => format!("return {}", &children.get(0).unwrap()),
                "ArrIndex"  => format!("{}!!{}", &children.get(0).unwrap(), &children.get(1).unwrap()),
                "Array"     => delimit("[", &separate_with(children, ", "), "]"),
                "Tuple"     => delimit("(", &separate_with(children, ", "), ")"),
                "Block"     => {
                    match rt.terms.last() {
                        // Last one doesn't need to be wrapped in a return
                        Some(Term::RTerm(ret, _)) if ret.constructor == "Return" => {
                            let _ = children.pop();
                            let stmts = join(append_each(indent(1, children), ";\n"));
                            let res   = indent_single(1, pretty_print_gale_term(&ret.terms[0]));
                            delimit("{\n", &(stmts + &res), "\n}")
                        },
                        _ => delimit("{\n", &separate_with(indent(1, children), ";\n"), "\n}")
                    }
                },
                "Lambda"    => format!("\\{} => {}", children.get(0).unwrap(), children.get(1).unwrap()),
                "String"    => delimit("\"", children.get(0).unwrap(), "\""),
                "Brackets"  => delimit("(", children.get(0).unwrap(), ")"),
                _ => panic!(format!("Unknown constructor: {}", rt.constructor))
            }
        },
        Term::NTerm(n, _) => n.value.to_string(),
        Term::STerm(s, _) => s.value.clone(),
        Term::TTerm(_, _) => unimplemented!(),
        Term::LTerm(_, _) => unimplemented!(),
    }
}