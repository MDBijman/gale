extern crate terms_format;
use nom::{
    IResult, Parser, error::ParseError, eof, named,
    number::complete::double,
    character::{is_alphanumeric},
    character::complete::{multispace0, char, anychar, none_of, one_of},
    sequence::{delimited, preceded, separated_pair, pair},
    branch::alt,
    multi::{separated_list0, many0, fold_many0},
    bytes::complete::{take_until, take, take_till1, take_while, take_while1, tag},
    combinator::{ map_res, opt, map_opt, map, verify }
};
use terms_format::*;

use std::fs;

fn ws<'a, O, E: ParseError<&'a str>, F: Parser<&'a str, O, E>>(f: F) -> impl Parser<&'a str, O, E> {
    delimited(multispace0, f, multispace0)
}

named!(u16_hex(&str) -> u16, map_res!(take!(4usize), |s| u16::from_str_radix(s, 16)));

fn unicode_escape(input: &str) -> IResult<&str, char> {
    map_opt(
        alt((
        // Not a surrogate
        map(verify(u16_hex, |cp| !(0xD800..0xE000).contains(cp)), |cp| {
            cp as u32
        }),
        // See https://en.wikipedia.org/wiki/UTF-16#Code_points_from_U+010000_to_U+10FFFF for details
        map(
            verify(
                separated_pair(u16_hex, tag("\\u"), u16_hex),
                |(high, low)| (0xD800..0xDC00).contains(high) && (0xDC00..0xE000).contains(low),
            ),
            |(high, low)| {
                let high_ten = (high as u32) - 0xD800;
                let low_ten = (low as u32) - 0xDC00;
                (high_ten << 10) + low_ten + 0x10000
                },
            ),
        )),
        // Could be probably replaced with .unwrap() or _unchecked due to the verify checks
        std::char::from_u32,
    )(input)
}

fn character(input: &str) -> IResult<&str, char> {
    let (input, c) = none_of("\"")(input)?;
    if c == '\\' {
        alt((
            map_res(anychar, |c| {
            Ok(match c {
                '"' | '\\' | '/' => c,
                'b' => '\x08',
                'f' => '\x0C',
                'n' => '\n',
                'r' => '\r',
                't' => '\t',
                _ => return Err(()),
            })
            }),
            preceded(char('u'), unicode_escape),
        ))(input)
    } else {
        Ok((input, c))
    }
}

// Parsers

named!(parse_name(&str) -> &str, take_while1!(|c| { char::is_alphanumeric(c) || c == '_' }));
named!(parse_number(&str) -> Term, map_res!(double, |d| -> Result<Term, String> { Ok(Term::NTerm(NTerm { value: d })) }));

named!(parse_ref(&str) -> Term, map_res!(separated_list1!(tag("."), parse_name), |names: Vec<&str>| -> Result<Term, String> { Ok(Term::new_rec_term("Ref", names.iter().map(|n| Term::new_string_term(n)).collect())) }));

pub fn parse_string(input: &str) -> IResult<&str, Term> {
    let (input, t) = delimited(
        char('"'),
        fold_many0(character, String::new(), |mut string, c| {
          string.push(c);
          string
        }),
        char('"'),
    )(input)?;
    Ok((input, Term::RTerm(RTerm { constructor: String::from("String"), terms: vec![Term::STerm(STerm { value: t })] })))
}

fn parse_type_array(input: &str) -> IResult<&str, Term> {
    let (input, _) = ws(tag("[")).parse(input)?;
    let (input, type_) = parse_type(input)?;
    let (input, _) = ws(tag(";")).parse(input)?;
    let (input, count) = parse_number(input)?;
    let (input, _) = ws(tag("]")).parse(input)?;
    Ok((input, Term::new_rec_term("TypeArray", vec![type_, count])))
}

fn parse_type_term(input: &str) -> IResult<&str, Term> {
    alt((
        map_res(parse_name, |n| -> Result<Term, String> { Ok(Term::new_rec_term("TypeId", vec![Term::new_string_term(n)])) }),
        map_res(tag("()"),  |_| -> Result<Term, String> { Ok(Term::new_rec_term("TypeTuple", vec![])) }),
        parse_type_array
    ))(input)
}

fn parse_type(input: &str) -> IResult<&str, Term> {
    let (input, in_type) = parse_type_term(input)?;
    let (input, r) = opt(ws(tag("->")))(input)?;
    match r {
        Some(_) => {
            let (input, out_type) = parse_type(input)?;
            Ok((input, Term::new_rec_term("FnType", vec![in_type, out_type])))
        },
        _ => {
            Ok((input, in_type))
        }
    }
}

fn parse_lambda(input: &str) -> IResult<&str, Term> {
    let (input, _) = ws(tag("\\")).parse(input)?;
    let (input, params) = alt((
        map_res(parse_name, |n| -> Result<Vec<Term>, String> { Ok(vec![Term::new_string_term(n)]) }),
        delimited(ws(char('(')), 
            separated_list0(ws(char(',')), map_res(parse_name, |n| -> Result<Term, String> { Ok(Term::new_string_term(n)) })
        ), ws(char(')')))
    ))(input)?;
    let p = Term::new_rec_term("Params", params);

    let (input, _) = ws::<_, (_, _), _>(tag("=>")).parse(input).unwrap();
    let (input, expr) = parse_expr(input)?;

    Ok((input, Term::new_rec_term("Lambda", vec![p, expr])))
}

fn parse_array(input: &str) -> IResult<&str, Term> {
    map_res(delimited(char('['), separated_list0(ws(char(',')), parse_expr), char(']')), |r| -> Result<Term, String> { Ok(Term::new_rec_term("Array", r)) })(input)
}

fn parse_tuple(input: &str) -> IResult<&str, Term> {
    map_res(delimited(char('('), separated_list0(ws(char(',')), parse_expr), char(')')), |r| -> Result<Term, String> { Ok(Term::new_rec_term("Tuple", r)) })(input)
}

fn parse_brackets(input: &str) -> IResult<&str, Term> {
    map_res(delimited(ws(char('(')), parse_expr, ws(char(')'))), 
        |r| -> Result<Term, String> { Ok(Term::new_rec_term("Brackets", vec![r])) })(input)
}

fn parse_value(input: &str) -> IResult<&str, Term> {
    alt((parse_number, parse_string, parse_lambda, parse_ref, parse_array, parse_brackets, parse_tuple))(input)
}

fn parse_arr_index(input: &str) -> IResult<&str, Term> {
    let (input, lhs) = parse_value(input)?;
    match ws::<_, (_, _), _>(tag("!!")).parse(input) {
        Ok((input, _)) => {
            let (input, rhs) = parse_value(input)?;

            Ok((input, Term::new_rec_term("ArrIndex", vec![lhs, rhs])))
        },
        _ => {
            Ok((input, lhs))
        }
    }
}

fn parse_appl(input: &str) -> IResult<&str, Term> {
    let (input, n) = parse_arr_index(input)?;

    match ws(parse_expr).parse(input) {
        Ok((input, rhs)) => Ok((input, Term::new_rec_term("Appl", vec![n, rhs]))),
        _ => Ok((input, n))
    }
}

fn parse_block(input: &str) -> IResult<&str, Term> {
    let is_block = ws::<_, (_, _), _>(tag("{")).parse(input);
    match is_block {
        Ok((input, _)) => {
            let (input, stmts) = many0(parse_statement)(input)?;
            let (input, opt_ret_expr) = opt(parse_expr)(input)?;
            let ret = match opt_ret_expr {
                Some(e) => { Term::new_rec_term("Return", vec![e]) },
                None    => { Term::new_rec_term("Return", vec![])  }
            };
            let (input, _) = ws::<_, (_, _), _>(tag("}")).parse(input).unwrap();
            Ok((input, Term::new_rec_term("Block", [stmts, vec![ret]].concat())))
        },
        _ => {
            parse_appl(input)
        }
    }
}

fn parse_expr(input: &str) -> IResult<&str, Term> {
    parse_block(input)
}

fn parse_statement(input: &str) -> IResult<&str, Term> {
    let (input, s) = alt((parse_let, parse_expr))(input)?;
    let (input, _) = ws(tag(";")).parse(input)?;
    Ok((input, s))
}

fn parse_let(input: &str) -> IResult<&str, Term> {
    let (input, _) = ws(tag("let")).parse(input)?;
    let (input, name) = parse_name(input)?;
    let (input, _) = ws(tag(":")).parse(input)?;
    let (input, fn_type) = parse_type(input)?;
    let (input, _) = ws(tag("=")).parse(input)?;
    let (input, value) = parse_expr(input)?;

    Ok((input, Term::new_rec_term("Let", vec![Term::new_string_term(name), fn_type, value])))
}

fn parse_file(input: &str) -> IResult<&str, Term> {
    let (input, r) = many0(pair(ws(parse_let), ws(tag(";"))))(input)?;
    if input.len() > 0 {
        panic!(format!("Input left after parsing:\n{}", input));
    }
    Ok((input, Term::new_rec_term("File", r.into_iter().map(|(a, _)| a).collect())))
}

pub fn parse_gale_file(filename: &String) -> Term {
    let f = fs::read_to_string(filename).unwrap();
    let (_, res) = parse_file(String::as_str(&f)).unwrap();
    res
}
