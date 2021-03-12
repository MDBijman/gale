extern crate nom;
use nom::{
    IResult, Parser, error::ParseError, error::ErrorKind, error::Error,
    number::complete::double,
    character::complete::{multispace0, char},
    sequence::{delimited, tuple, pair, terminated, preceded},
    branch::alt,
    multi::{separated_list0, many0},
    bytes::complete::{take_till1, tag, is_not, take_while},
    combinator::{ map_res, opt, map, cut, verify }
};
use std::fs;
use crate::parser::rwfile::*;
use terms_format;


fn ws<'a, O, E: ParseError<&'a str>, F: Parser<&'a str, O, E>>(f: F) -> impl Parser<&'a str, O, E> {
    delimited(multispace0, f, multispace0)
}

fn dbg_in<'a, O, E: ParseError<&'a str>, F: Parser<&'a str, O, E>>(mut f: F) -> impl Parser<&'a str, O, E>
where
 O: std::fmt::Debug,
 E: std::fmt::Debug,
{
    move |input: &'a str| {
        println!("[dbg] {}", input);
        f.parse(input)
    }
}

fn dbg_out<'a, O, E: ParseError<&'a str>, F: Parser<&'a str, O, E>>(mut f: F) -> impl Parser<&'a str, O, E>
where
 O: std::fmt::Debug,
 E: std::fmt::Debug,
{
    move |input: &'a str| {
        let r = f.parse(input);
        println!("[dbg] {:?}", r);
        r
    }
}

fn parse_name(input: &str) -> IResult<&str, &str> {
    let (input, res) = take_till1(|c| { !(char::is_alphanumeric(c) || c == '_') })(input)?;

    match res.parse::<f64>() {
        Ok(_)  => return Err(nom::Err::Error(Error::from_error_kind(input, ErrorKind::Char))),
        Err(_) => {}
    };
    
    // Keywords
    if ["in"].contains(&res) {
        return Err(nom::Err::Error(Error::from_error_kind(input, ErrorKind::Char)));
    }

    Ok((input, res))
}


fn parse_function_name(input: &str) -> IResult<&str, (FunctionReferenceType, &str)> {
    let (input, c) = alt((char('.'), char('!'))).parse(input)?;
    let reftype = match c {
        '.' => FunctionReferenceType::Try,
        '!' => FunctionReferenceType::Force,
        _ => panic!("Unexpected function name begin character")
    };
    let (input, name) = parse_name(input)?;
    Ok((input, (reftype, name)))
}

fn parse_var_term_name(i: &str) -> IResult<&str, &str> {
    preceded(tag("?"), parse_name)(i)
}

fn parse_ws(i: &str) -> IResult<&str, &str> {
    multispace0(i)
}

fn parse_comment(i: &str) -> IResult<&str, ()> {
    let (i, _) = tuple((char('#'), is_not("\n\r"), take_while(|c| c == '\n' || c == '\r'))).parse(i)?;
    Ok((i, ()))
}

/*
* Matching
*/

fn parse_term_name_match(i: &str) -> IResult<&str, Match> {
    alt((
        map(parse_var_term_name, |n| Match::VariableMatcher(VariableMatcher { name: n.to_string(), annotations: vec![] })),
        map(parse_name, |n| Match::StringMatcher(StringMatcher { value: n.to_string() })),
    ))(i)
}

fn parse_term_match(i: &str) -> IResult<&str, Match> {
    map(tuple((
        parse_term_name_match,
        parse_tuple_match,
        cut(opt(delimited(ws(char('{')), ws(separated_list0(ws(tag(",")), parse_match)), char('}'))))
    )), |(name, args_match, annot_matches)| {
        Match::TermMatcher(TermMatcher {
            constructor: Box::from(name),
            terms: Box::from(args_match),
            annotations: annot_matches.unwrap_or(Vec::new()) })
    })(i)
}

fn parse_variable_match(input: &str) -> IResult<&str, Match> {
    let (input, res) = parse_name(input)?;
    let (input, a) = opt(delimited(char('{'), ws(separated_list0(ws(tag(",")), parse_match)), char('}')))(input)?;
    Ok((input, Match::VariableMatcher(VariableMatcher { name: res.to_string(), annotations: a.unwrap_or(Vec::new()) })))
}

fn parse_string_match(input: &str) -> IResult<&str, Match> {
    let (input, res) = terms_format::parse_string(input)?;
    match res {
        terms_format::Term::STerm(terms_format::STerm { value: n }, _) => Ok((input, Match::StringMatcher(StringMatcher { value: n.to_string() }))),
        _ => panic!("Unexpected result from parsing string")
    }
}

fn parse_number_match(input: &str) -> IResult<&str, Match> {
    let (input, res) = terms_format::parse_number(input)?;
    match res {
        terms_format::Term::NTerm(terms_format::NTerm { value: n }, _) => Ok((input, Match::NumberMatcher(NumberMatcher { value: n }))),
        _ => panic!("Unexpected result from parsing number")
    }
}

fn parse_variadic_elem_match(i: &str) -> IResult<&str, Match> {
    map(preceded(tag(".."), cut(parse_name)),
        |n| Match::VariadicElementMatcher(VariadicElementMatcher { name: n.to_string() }))(i)
}

fn parse_tuple_match(input: &str) -> IResult<&str, Match> {
    verify(map_res(
            delimited(ws(char('(')), 
                cut(ws(separated_list0(ws(tag(",")), alt((parse_match, parse_variadic_elem_match))))), 
                ws(char(')'))),
            |r: Vec<Match>| -> Result<Match, String> { Ok(Match::TupleMatcher(TupleMatcher { elems: r })) }),
        |m: &Match| match m {
            Match::TupleMatcher(tm) => {
                if tm.elems.len() == 0 {
                    return true
                }

                let l = tm.elems.len() - 1;
                for i in 0..l {
                    match &tm.elems[i] {
                        Match::VariadicElementMatcher(_) => panic!("Variadic element matcher must be at the end"),
                        _ => {}
                    }
                }
                true
            },
            _ => false
        })(input)
}

fn parse_list_match(input: &str) -> IResult<&str, Match> {
    let (input, _ ) = ws(tag("[")).parse(input)?;
    let (input, opt_head_match) = opt(parse_match)(input)?;
    match opt_head_match {
        Some(head_match) => {
            let (mut input, sep) = opt(ws(tag("|"))).parse(input)?;

            let tail_match = match sep {
                None => None,
                Some(_) => {
                    let (_input, tail_match) = parse_match(input)?;
                    input = _input;
                    Some(Box::from(tail_match))
                }
            };

            let (input, _ ) = ws(tag("]")).parse(input)?;

            let res = Match::ListMatcher(ListMatcher { head: Some(Box::from(head_match)), tail: tail_match });

            Ok((input, res))
        },
        _ => {
            let (input, _ ) = ws(tag("]")).parse(input)?;

            let res = Match::ListMatcher(ListMatcher { head: None, tail: None });

            Ok((input, res))
        }
    }
}

fn parse_match(i: &str) -> IResult<&str, Match> {
    alt((
        parse_list_match,
        parse_tuple_match,
        parse_term_match,
        parse_variable_match,
        parse_string_match,
        parse_number_match))(i)
}

/*
* Expressions
*/

fn parse_function_ref(input: &str) -> IResult<&str, Expr> {
    let (input, (ref_type, func_name)) = parse_function_name(input)?;
    let (input, meta) = parse_meta_args(input)?;
    Ok((input, Expr::FRef(FRef::from(&func_name.to_string(), &meta, ref_type))))
}

fn parse_meta_args(input: &str) -> IResult<&str, Vec<Expr>> {
    let (input, meta) = opt(delimited(char('['), separated_list0(ws(char(',')), alt((
        map_res(parse_function_ref,  |n| -> Result<Expr, String> { Ok(n) }),
        map_res(parse_expr,          |e| -> Result<Expr, String> { Ok(e) })
    ))), ws(char(']'))))(input)?;

    Ok((input, meta.unwrap_or(Vec::new())))
}

fn parse_value(input: &str) -> IResult<&str, Expr> {
    map_res(ws(double), |n: f64| -> Result<Expr, String> { Ok(Expr::Number(Number { value: n })) })(input)
}

fn parse_string(input: &str) -> IResult<&str, Expr> {
    let (input, res) = terms_format::parse_string(input)?;
    match res {
        terms_format::Term::STerm(s, _) => 
            Ok((input, Expr::Text(Text { value: s.value }))),
        _ => panic!("Unexpected term type from parse_string")
    }
}

enum AnotType {
    Set,
    Add
}

fn parse_anot_opener(i: &str) -> IResult<&str, AnotType> {
    alt((
        map(tag("{+"), |_| AnotType::Add),
        map(tag("{"), |_| AnotType::Set)
    ))(i)
}

fn parse_unroll_variadic(i: &str) -> IResult<&str, Expr> {
    map(preceded(tag(".."), cut(parse_name)), |n| Expr::UnrollVariadic(Ref { name: n.to_string() }))(i)
}

fn parse_rec_term(i: &str) -> IResult<&str, Expr> {
    map_res(tuple((
        alt((
            map(parse_name, |n| Expr::Text(Text { value: n.to_string() })),
            map(parse_var_term_name, |n| Expr::Ref(Ref { name: n.to_string() })))),
        pair(
            opt(delimited(ws(char('(')), cut(ws(separated_list0(ws(tag(",")), parse_expr))), ws(char(')')))),
            opt(terminated(pair(ws(parse_anot_opener), ws(separated_list0(ws(tag(",")), parse_expr))), ws(char('}')))))
    )), |(name, (opt_args, opt_annots))| {
            let inner = match opt_args {
                Some(args) => Expr::Term(Term { constructor: Box::from(name), terms: args }),
                None => match name {
                    Expr::Text(t) => Expr::Ref(Ref { name: t.value }),
                    _ => return Err("Cannot have ?name expression outside of term building"),
                }
            };

            match opt_annots {
                Some((AnotType::Add, annot)) => Ok(Expr::AddAnnotation(Annotation { 
                    inner: Box::from(inner),
                    annotations: annot
                })),
                Some((AnotType::Set, annot)) => Ok(Expr::Annotation(Annotation { 
                    inner: Box::from(inner),
                    annotations: annot
                })),
                None => Ok(inner)
            }
        })(i)
}

fn parse_tuple(i: &str) -> IResult<&str, Expr> {
    alt((
        map(delimited(ws(char('(')), parse_expr, ws(char(')'))), 
            |expr| -> Expr {
                expr
            }),
        map(delimited(ws(char('(')), ws(separated_list0(ws(tag(",")), parse_expr)), ws(char(')'))),
            |r: Vec<Expr>| -> Expr {
                Expr::Tuple(Tuple { values: r })
            }),
    ))(i)
}

fn parse_list(input: &str) -> IResult<&str, Expr> {
    alt((
        // [ a, b, c ]
        map_res(delimited(ws(char('[')), ws(separated_list0(ws(tag(",")), parse_expr)), ws(char(']'))), |r: Vec<Expr>| -> Result<Expr, String> { Ok(Expr::List(List { values: r })) }),
        // [ h | t ]
        map_res(delimited(ws(char('[')), tuple((parse_expr, ws(char('|')), parse_expr)), ws(char(']'))), |t: (Expr, char, Expr)| -> Result<Expr, String> { Ok(Expr::ListCons(ListCons { head: Box::from(t.0), tail: Box::from(t.2) })) })
    ))(input)
}

fn parse_term(input: &str) -> IResult<&str, Expr> {
    let (input, term) = alt((parse_tuple, parse_rec_term, parse_value, parse_string, parse_unroll_variadic, parse_list))(input)?;
    let (input, anot) = opt(delimited(ws(char('{')), ws(separated_list0(ws(tag(",")), parse_expr)), char('}')))(input)?;
    match anot {
        Some(annotation) => Ok((input, Expr::Annotation(Annotation { inner: Box::from(term), annotations: annotation }))),
        None => Ok((input, term))
    }
}

fn parse_invocation(input: &str) -> IResult<&str, Expr> {
    let (input, fref) = opt(parse_function_ref)(input)?;
    match fref {
        Some(Expr::FRef(fref)) => {
            let (input, arg) = opt(ws(parse_invocation))(input)?;
            match arg {
                Some(arg) => 
                    Ok((input, Expr::Invoke(Invocation { name: fref, arg: Box::from(arg) }))),
                None => 
                    Ok((input, Expr::Invoke(Invocation { name: fref, arg: Box::from(Expr::This) }))),
            }
        },
        Some(e) => Ok((input, e)),
        _ => parse_term(input)
    }
}

fn parse_let(i: &str) -> IResult<&str, Expr> {
    alt((
        map(tuple((parse_match, ws(tag(":=")), cut(parse_expr), ws(tag("in")), parse_expr)), |(match_, _, val, _, body)| {
            Expr::Let(Let { lhs: match_, rhs: Box::from(val), body: Box::from(body) })
        }),
        parse_invocation))(i)
}

fn parse_and(input: &str) -> IResult<&str, Expr> {
    let (input, lhs) = parse_let(input)?;
    let (input, r)   = opt(ws(char('>')))(input)?;
    match r {
        Some(_) => {
            let (input, rhs) = parse_and(input)?;
            return Ok((input, Expr::Op(Op::And(Box::from(lhs), Box::from(rhs)))));
        },
        _ => {}
    }

    Ok((input, lhs))
}

fn parse_or(input: &str) -> IResult<&str, Expr> {
    let (input, lhs) = parse_and(input)?;
    let (input, r)   = opt(ws(char('+')))(input)?;
    match r {
        Some(_) => {
            let (input, rhs) = parse_or(input)?;
            return Ok((input, Expr::Op(Op::Or(Box::from(lhs), Box::from(rhs)))));
        },
        _ => {}
    }
    Ok((input, lhs))
}

fn parse_expr(input: &str) -> IResult<&str, Expr> {
    parse_or(input)
}

fn parse_function(input: &str) -> IResult<&str, Function> {
    let (input, _) = opt(many0(pair(parse_comment, opt(parse_ws))))(input)?;
    let (input, name) = parse_name(input)?;
    let (input, meta) = parse_meta_args(input)?;
    let (input, _) = ws(tag(":")).parse(input)?;
    let (input, matcher) = parse_match(input)?;
    let (input, _) = ws(tag("->")).parse(input)?;
    let (input, body) = parse_expr(input)?;
    let (input, _) = ws(tag(";")).parse(input)?;
    Ok((input, Function { name: name.to_string(), meta, matcher, body }))
}

pub fn parse_rw_string(input: &str) -> Result<File, String> {
    match many0(ws(parse_function))(input) {
        Ok((input, f)) => {
            if input.len() > 0 {
                panic!(format!("Input left after parsing:\n{}", input));
            }

            Ok(File { functions: f, filename: None })
        },
        Err(e) => panic!(format!("Something went wrong: {}", e))
    }
}

pub fn parse_rw_file(filename: &str) -> Result<File, String> {
    let f = fs::read_to_string(filename).unwrap();
    let mut rw_file = parse_rw_string(String::as_str(&f))?;
    rw_file.filename = Some(String::from(filename));
    Ok(rw_file)
}