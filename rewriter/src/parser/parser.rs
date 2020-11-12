extern crate nom;
use nom::{
    IResult, Parser, error::ParseError, eof,
    number::complete::double,
    character::{is_alphanumeric},
    character::complete::{multispace0, char, one_of},
    sequence::{delimited},
    branch::alt,
    multi::{separated_list0, many0},
    bytes::complete::{take_until, take_till1, take_while, take_while1, tag},
    combinator::{ map_res, opt }
};
use std::fs;
use crate::parser::rwfile::*;
use terms_format::{ parse_number, parse_string, STerm, NTerm, Term };


fn ws<'a, O, E: ParseError<&'a str>, F: Parser<&'a str, O, E>>(f: F) -> impl Parser<&'a str, O, E> {
    delimited(multispace0, f, multispace0)
}

fn parse_name(input: &str) -> IResult<&str, &str> {
    Ok(take_while1(|c| { char::is_alphanumeric(c) || c == '_' })(input)?)
}

fn parse_function_name(input: &str) -> IResult<&str, &str> {
    let (input, _) = char('.').parse(input)?;
    Ok(parse_name(input)?)
}

fn parse_term_match(input: &str) -> IResult<&str, Match> {
    let (input, con) = parse_name(input)?;
    let (input, r) = delimited(char('('), ws(separated_list0(ws(tag(",")), parse_match)), char(')'))(input)?;
    Ok((input, Match::TermMatcher(TermMatcher { constructor: con.to_string(), terms: r })))
}

fn parse_variable_match(input: &str) -> IResult<&str, Match> {
    let (input, res) = parse_name(input)?;
    Ok((input, Match::VariableMatcher(VariableMatcher { name: res.to_string() })))
}

fn parse_string_match(input: &str) -> IResult<&str, Match> {
    let (input, res) = parse_string(input)?;
    match res {
        Term::STerm(STerm { value: n }) => Ok((input, Match::StringMatcher(StringMatcher { value: n.to_string() }))),
        _ => panic!("Unexpected result from parsing string")
    }
}

fn parse_number_match(input: &str) -> IResult<&str, Match> {
    let (input, res) = parse_number(input)?;
    match res {
        Term::NTerm(NTerm { value: n }) => Ok((input, Match::NumberMatcher(NumberMatcher { value: n }))),
        _ => panic!("Unexpected result from parsing number")
    }
}

fn parse_match(input: &str) -> IResult<&str, Match> {
    let (input, m) = alt((parse_term_match, parse_variable_match, parse_string_match, parse_number_match))(input)?;
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
        map_res(parse_name,          |n| -> Result<Expr, String> { Ok(Expr::Ref (Ref  { name: n.to_string() })) })
    ))), ws(char(']'))))(input)?;

    Ok((input, meta.unwrap_or(Vec::new())))
}

fn parse_value(input: &str) -> IResult<&str, Expr> {
    alt((
        map_res(ws(parse_name), |r: &str| -> Result<Expr, String> { Ok(Expr::Ref(Ref { name: r.to_string() })) }), 
        map_res(ws(double), |n: f64| -> Result<Expr, String> { Ok(Expr::Number(Number { value: n })) })))(input)
}

fn parse_tuple(input: &str) -> IResult<&str, Expr> {
    map_res(delimited(ws(char('(')), ws(separated_list0(ws(tag(",")), parse_expr)), ws(char(')'))), |r: Vec<Expr>| -> Result<Expr, String> { Ok(Expr::Tuple(Tuple { values: r })) })(input)
}

fn parse_term(input: &str) -> IResult<&str, Expr> {
    alt((parse_tuple, parse_value))(input)
}

fn parse_invocation(input: &str) -> IResult<&str, Expr> {
    let (input, fref) = opt(parse_function_ref)(input)?;
    match fref {
        Some(Expr::FRef(fref)) => {
            let (input, arg) = opt(parse_term)(input)?;
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

fn parse_file(input: &str) -> IResult<&str, File> {
    let (input, r) = many0(ws(parse_function))(input)?;
    if input.len() > 0 {
        panic!(format!("Input left after parsing:\n{}", input));
    }
    Ok((input, File { functions: r }))
}

pub fn parse_rw_file(filename: &str) -> File {
    let f = fs::read_to_string(filename).unwrap();
    let (_, res) = parse_file(String::as_str(&f)).unwrap();
    res
}