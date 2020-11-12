extern crate nom;
use nom::{
    IResult, Parser, error::ParseError,
    character::complete::{ char, multispace0, none_of, anychar },
    bytes::complete::{ tag, take_while, take },
    combinator::{ map, map_opt, map_res, verify },
    sequence::{ delimited, preceded, separated_pair },
    multi::{ separated_list0, fold_many0 },
    branch::alt,
    number::complete::double
};
use std::fs;
use std::fmt;

#[derive(Debug, Clone)]
pub enum Term {
    RTerm(RTerm),
    STerm(STerm),
    NTerm(NTerm),
    TTerm(TTerm)
}

impl Term {
    pub fn new_rec_term(n: &str, t: Vec<Term>) -> Term {
        Term::RTerm(RTerm { constructor: n.to_string(), terms: t })
    }

    pub fn new_string_term(n: &str) -> Term {
        Term::STerm(STerm { value: n.to_string() })
    }
}

// Recursive Term
#[derive(Debug, Clone)]
pub struct RTerm {
    pub constructor: String,
    pub terms: Vec<Term>
}

// String Term
#[derive(Debug, Clone)]
pub struct STerm {
    pub value: String
}

// Numerical Term
#[derive(Debug, Clone)]
pub struct NTerm {
    pub value: f64
}

// Tuple Term
#[derive(Debug, Clone)]
pub struct TTerm {
    pub terms: Vec<Term>
}

impl TTerm {
    pub fn new() -> TTerm { TTerm { terms: Vec::new() } }
}

// Helpers

fn ws<'a, O, E: ParseError<&'a str>, F: Parser<&'a str, O, E>>(f: F) -> impl Parser<&'a str, O, E> {
  delimited(multispace0, f, multispace0)
}

fn u16_hex(input: &str) -> IResult<&str, u16> {
    map_res(take(4usize), |s| u16::from_str_radix(s, 16))(input)
}

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

pub fn parse_number(input: &str) -> IResult<&str, Term> {
    let (input, res) = double(input)?;
    Ok((input, Term::NTerm(NTerm { value: res })))    
}

pub fn parse_string(input: &str) -> IResult<&str, Term> {
    let (input, t) = delimited(
        char('"'),
        fold_many0(character, String::new(), |mut string, c| {
          string.push(c);
          string
        }),
        char('"'),
    )(input)?;
    Ok((input, Term::STerm(STerm { value: t })))
}

pub fn parse_term(input: &str) -> IResult<&str, Term> {
    let (input, con) = take_while(char::is_alphanumeric)(input)?;
    let (input, r) = delimited(char('('), ws(separated_list0(ws(tag(",")), alt((parse_term, parse_string, parse_number)))), char(')'))(input)?;
    Ok((input, Term::RTerm(RTerm { constructor: con.to_string(), terms:r })))
}

pub fn parse_term_file(path: &String) -> Term {
    let f = fs::read_to_string(path).unwrap();
    let (_, res) = parse_term(String::as_str(&f)).unwrap();
    res
}

/*
* Display implementation
*/

impl fmt::Display for Term {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Term::RTerm(rt) => write!(f, "{}", rt),
            Term::STerm(st) => write!(f, "{}", st),
            Term::NTerm(nt) => write!(f, "{}", nt),
            Term::TTerm(tt) => write!(f, "{}", tt)
        }
    }
}


impl fmt::Display for RTerm {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}(", self.constructor)?;
        let mut first = true;
        for term in &self.terms {
            if !first {
                write!(f, ", ")?;
            }
            first = false;

            write!(f, "{}", term)?;
        }
        write!(f, ")")
    }
}

impl fmt::Display for STerm {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{:?}", self.value)
    }
}

impl fmt::Display for NTerm {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}", self.value)
    }
}

impl fmt::Display for TTerm {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "(")?;
        let mut first = true;
        for term in &self.terms {
            if !first {
                write!(f, ", ")?;
            }
            first = false;

            write!(f, "{}", term)?;
        }
        write!(f, ")")
    }
}