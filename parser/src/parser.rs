extern crate terms_format;
use nom::{
    IResult, Parser, error::ParseError, named,
    number::complete::double,
    character::complete::{multispace0, char, anychar, none_of, one_of},
    character::{is_space},
    sequence::{delimited, preceded, separated_pair},
    branch::alt,
    multi::{separated_list0, separated_list1, many0, fold_many0 },
    bytes::complete::{tag, take_while, take_while1},
    combinator::{ map_res, opt, map_opt, map, verify, not }
};
use terms_format::*;

use std::fs;

fn ws<'a, O, E: ParseError<&'a str>, F: Parser<&'a str, O, E>>(f: F) -> impl Parser<&'a str, O, E> {
    delimited(multispace0, f, multispace0)
}

fn newline(input: &str) -> IResult<&str, &str> {
    let (input, _) = take_while(|c: char| c == ' ' || c == '\t')(input)?;
    alt((tag("\n"), tag("\r\n"))).parse(input)
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

pub fn parse_name(input: &str) -> IResult<&str, &str> {
    not(tag("in"))(input)?;
    not(tag("let"))(input)?;
    take_while1(|c| { char::is_alphanumeric(c) || c == '_' })(input)
}

named!(parse_number(&str) -> Term, map_res!(double, |d| -> Result<Term, String> { Ok(Term::new_rec_term("Int", vec![Term::new_number_term(d)])) }));

named!(parse_ref(&str) -> Term, map_res!(
    separated_list1!(tag("."), parse_name), 
    |names: Vec<&str>| -> Result<Term, String> { 
        Ok(Term::new_rec_term("Ref", names.iter().map(|n| Term::new_string_term(n)).collect()))
    }
));

pub fn parse_string(input: &str) -> IResult<&str, Term> {
    let (input, t) = delimited(
        char('"'),
        fold_many0(character, String::new(), |mut string, c| {
          string.push(c);
          string
        }),
        char('"'),
    )(input)?;
    Ok((input, Term::new_rec_term("String", vec![Term::new_string_term(&t)])))
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
    map_res(delimited(char('['), separated_list0(ws(char(',')), parse_expr), char(']')), |r| -> Result<Term, String> { Ok(Term::new_rec_term("Array", vec![Term::new_list_term(r)])) })(input)
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
            let (input, rhs) = parse_arr_index(input)?;

            Ok((input, Term::new_rec_term("ArrIndex", vec![lhs, rhs])))
        },
        _ => {
            Ok((input, lhs))
        }
    }
}

fn parse_binop(input: &str) -> IResult<&str, Term> {
    let (input, lhs) = parse_arr_index(input)?;
    match ws::<_, (_, _), _>(tag("+")).parse(input) {
        Ok((input, _)) => {
            let (input, rhs) = parse_expr(input)?;

            Ok((input, Term::new_rec_term("Add", vec![lhs, rhs])))
        },
        _ => {
            Ok((input, lhs))
        }
    }
}

fn parse_appl(input: &str) -> IResult<&str, Term> {
    let (input, n) = parse_binop(input)?;

    match ws(parse_expr).parse(input) {
        Ok((input, rhs)) => Ok((input, Term::new_rec_term("Appl", vec![n, rhs]))),
        _ => Ok((input, n))
    }
}

fn parse_let_decl(input: &str) -> IResult<&str, Term> {
    let (input, name) = parse_name(input)?;
    let (input, _) = ws(tag(":")).parse(input)?;
    let (input, type_) = parse_type(input)?;
    let (input, _) = ws(tag("=")).parse(input)?;
    let (input, expr) = parse_expr(input)?;

    Ok((input, Term::new_rec_term("Decl", vec![Term::new_string_term(name), type_, expr])))
}

fn parse_let(input: &str) -> IResult<&str, Term> {
    let (input, r) = opt(ws(tag("let"))).parse(input)?;
    match r {
        Some(_) => {
            let (input, decls) = separated_list1(ws(char(',')), parse_let_decl)(input)?;
            let (input, _) = ws(tag("in")).parse(input)?;
            let (input, expr) = parse_expr(input)?;

            Ok((input, Term::new_rec_term("Let", vec![Term::new_list_term(decls), expr])))
        },
        _ => {
            parse_appl(input)
        }
    }
}

fn parse_expr(input: &str) -> IResult<&str, Term> {
    parse_let(input)
}

fn parse_function_declaration(input: &str) -> IResult<&str, Term> {
    let (input, name) = parse_name(input)?;
    let (input, _) = ws(tag(":")).parse(input)?;
    let (input, fn_type) = parse_type(input)?;
    let (input, _) = newline(input)?;

    Ok((input, Term::new_rec_term("FnDecl", vec![Term::new_string_term(name), fn_type])))
}

fn parse_function_definition(input: &str) -> IResult<&str, Term> {
    let (input, func) = parse_name(input)?;
    let (input, param) = ws(parse_name).parse(input)?;
    let (input, _) = ws(tag("=")).parse(input)?;
    let (input, body) = parse_expr(input)?;
    let (input, _) = ws(tag(".")).parse(input)?;

    Ok((input, Term::new_rec_term("FnDef", vec![Term::new_string_term(func), Term::new_string_term(param), body])))
}

pub fn parse_gale_string(input: &str) -> IResult<&str, Term> {
    let (input, r) = many0(alt((parse_function_definition, parse_function_declaration)))(input)?;
    if input.len() > 0 {
        panic!(format!("Input left after parsing:\n{}", input));
    }
    Ok((input, Term::new_rec_term("File", vec![Term::new_list_term(r)])))
}

pub fn parse_gale_file(filename: &str) -> Term {
    let f = fs::read_to_string(filename).unwrap();
    let (_, res) = parse_gale_string(String::as_str(&f)).unwrap();
    res
}
