extern crate nom;
use nom::{
    IResult, Parser, error::ParseError, error::ErrorKind, error::Error,
    number::complete::double,
    character::complete::{multispace0, char},
    sequence::{delimited},
    branch::alt,
    multi::{separated_list0, many0},
    bytes::complete::{take_till1, tag},
    combinator::{ map_res, opt }
};
use std::fs;
use crate::parser::rwfile::*;
use terms_format;


fn ws<'a, O, E: ParseError<&'a str>, F: Parser<&'a str, O, E>>(f: F) -> impl Parser<&'a str, O, E> {
    delimited(multispace0, f, multispace0)
}

fn parse_name(input: &str) -> IResult<&str, &str> {
    let (input, res) = take_till1(|c| { !(char::is_alphanumeric(c) || c == '_') })(input)?;

    match res.parse::<f64>() {
        Ok(_)  => Err(nom::Err::Error(Error::from_error_kind(input, ErrorKind::Char))),
        Err(_) => Ok((input, res))
    }
}

fn parse_function_name(input: &str) -> IResult<&str, &str> {
    let (input, _) = char('.').parse(input)?;
    Ok(parse_name(input)?)
}

fn parse_term_match(input: &str) -> IResult<&str, Match> {
    let (input, con) = parse_name(input)?;
    let (input, r) = delimited(char('('), ws(separated_list0(ws(tag(",")), parse_match)), char(')'))(input)?;
    let (input, a) = opt(delimited(char('{'), ws(separated_list0(ws(tag(",")), parse_match)), char('}')))(input)?;
    Ok((input, Match::TermMatcher(TermMatcher { constructor: con.to_string(), terms: r, annotations: a.unwrap_or(Vec::new()) })))
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

fn parse_tuple_match(input: &str) -> IResult<&str, Match> {
    map_res(delimited(ws(char('(')), ws(separated_list0(ws(tag(",")), parse_match)), ws(char(')'))), |r: Vec<Match>| -> Result<Match, String> { Ok(Match::TupleMatcher(TupleMatcher { elems: r })) })(input)
}

fn parse_list_match(input: &str) -> IResult<&str, Match> {
    let (input, _ ) = ws(tag("[")).parse(input)?;
    let (input, head_name) = parse_name(input)?;
    let (mut input, sep) = opt(ws(tag("|"))).parse(input)?;

    let tail_name = match sep {
        None => None,
        Some(_) => {
            let (_input, tail_name) = parse_name(input)?;
            input = _input;
            Some(String::from(tail_name))
        }
    };

    let (input, _ ) = ws(tag("]")).parse(input)?;

    let res = Match::ListMatcher(ListMatcher { head: String::from(head_name), tail: tail_name });

    Ok((input, res))
}

fn parse_match(input: &str) -> IResult<&str, Match> {
    let (input, m) = alt((parse_list_match, parse_tuple_match, parse_term_match, parse_variable_match, parse_string_match, parse_number_match))(input)?;
    Ok((input, m))
}

fn parse_function_ref(input: &str) -> IResult<&str, Expr> {
    let (input, func_name) = parse_function_name(input)?;
    let (input, meta) = parse_meta_args(input)?;
    Ok((input, Expr::FRef(FRef::from(&func_name.to_string(), &meta))))
}

fn parse_meta_args(input: &str) -> IResult<&str, Vec<Expr>> {
    let (input, meta) = opt(delimited(ws(char('[')), separated_list0(ws(char(',')), alt((
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

fn parse_rec_term(input: &str) -> IResult<&str, Expr> {
    let (input, n) = parse_name(input)?;
    let (input, args) = opt(delimited(ws(char('(')), ws(separated_list0(ws(tag(",")), parse_expr)), ws(char(')'))))(input)?;
    match args {
        Some(exprs) => { 
            let (input, annot) = opt(delimited(ws(char('{')), ws(separated_list0(ws(tag(",")), parse_expr)), ws(char('}'))))(input)?;
            match annot {
                Some(annot) => Ok((input, Expr::Annotation(Annotation { 
                    inner: Box::from(Expr::Term(Term { constructor: Ref { name: n.to_string() }, terms: exprs })),
                    annotations: annot
                }))),
                None => Ok((input, Expr::Term(Term { constructor: Ref { name:n.to_string() }, terms: exprs })))
            }
        }, 
        None => Ok((input, Expr::Ref(Ref { name: n.to_string() })))
    }
    
}

fn parse_tuple(input: &str) -> IResult<&str, Expr> {
    map_res(delimited(ws(char('(')), ws(separated_list0(ws(tag(",")), parse_expr)), ws(char(')'))), |r: Vec<Expr>| -> Result<Expr, String> { Ok(Expr::Tuple(Tuple { values: r })) })(input)
}

fn parse_list(input: &str) -> IResult<&str, Expr> {
    map_res(delimited(ws(char('[')), ws(separated_list0(ws(tag(",")), parse_expr)), ws(char(']'))), |r: Vec<Expr>| -> Result<Expr, String> { Ok(Expr::List(List { values: r })) })(input)
}

fn parse_term(input: &str) -> IResult<&str, Expr> {
    let (input, term) = alt((parse_tuple, parse_rec_term, parse_value, parse_string, parse_list))(input)?;
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
            let (input, arg) = opt(ws(parse_term))(input)?;
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

fn parse_comb(input: &str) -> IResult<&str, Expr> {
    let (input, lhs) = parse_invocation(input)?;
    let (input, r)   = opt(ws(char('+')))(input)?;
    match r {
        Some(_) => {
            let (input, rhs) = parse_expr(input)?;
            return Ok((input, Expr::Op(Op::Or(Box::from(lhs), Box::from(rhs)))));
        },
        _ => {}
    }

    let (input, r)   = opt(ws(char('>')))(input)?;
    match r {
        Some(_) => {
            let (input, rhs) = parse_expr(input)?;
            return Ok((input, Expr::Op(Op::And(Box::from(lhs), Box::from(rhs)))));
        },
        _ => {}
    }

    Ok((input, lhs))
}

fn parse_expr(input: &str) -> IResult<&str, Expr> {
    parse_comb(input)
}

fn parse_function(input: &str) -> IResult<&str, Function> {
    let (input, name) = parse_name(input)?;
    let (input, meta) = parse_meta_args(input).unwrap();
    let (input, _) = ws::<&str, (_, _), _>(tag(":")).parse(input).unwrap();
    let (input, matcher) = parse_match(input).unwrap();
    let (input, _) = ws::<&str, (_, _), _>(tag("->")).parse(input).unwrap();
    let (input, body) = parse_expr(input).unwrap();
    let (input, _) = ws::<&str, (_, _), _>(tag(";")).parse(input).unwrap();
    Ok((input, Function { name: name.to_string(), meta: meta, matcher: matcher, body: body }))
}

pub fn parse_rw_string(input: &str) -> Result<File, String> {
    match many0(ws(parse_function))(input) {
        Ok((input, f)) => {
            if input.len() > 0 {
                panic!(format!("Input left after parsing:\n{}", input));
            }

            Ok(File { functions: f })
        },
        _ => panic!("Something went wrong")
    }
}

pub fn parse_rw_file(filename: &str) -> Result<File, String> {
    let f = fs::read_to_string(filename).unwrap();
    parse_rw_string(String::as_str(&f))
}