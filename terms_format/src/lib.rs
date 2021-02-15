extern crate nom;
use nom::{
    IResult, Parser, error::ParseError,
    character::complete::{ char, multispace0, none_of, anychar },
    bytes::complete::{ tag, take_while, take },
    combinator::{ map, map_opt, opt, map_res, verify },
    sequence::{ delimited, preceded, separated_pair },
    multi::{ separated_list0, fold_many0 },
    branch::alt,
    number::complete::double
};
use std::fs;
use std::fmt;
use std::mem;
use std::hash::{Hash, Hasher};
use indenter::{indented, Format};
use core::fmt::Write;

#[derive(Debug, Clone, Hash)]
pub struct Annotations {
    pub elems: Vec<Term>
}

impl Annotations {
    pub fn empty() -> Annotations {
        Annotations { elems: Vec::new() }
    }

    pub fn from(a: Vec<Term>) -> Annotations {
        Annotations { elems: a }
    }
}

#[derive(Debug, Clone, Hash)]
pub enum Term {
    RTerm(RTerm, Annotations),
    STerm(STerm, Annotations),
    NTerm(NTerm, Annotations),
    TTerm(TTerm, Annotations),
    LTerm(LTerm, Annotations),
}

impl Term {
    pub fn new_rec_term(n: &str, t: Vec<Term>) -> Term {
        Term::RTerm(RTerm { constructor: n.to_string(), terms: t }, Annotations::empty())
    }

    pub fn new_anot_rec_term(n: &str, t: Vec<Term>, a: Vec<Term>) -> Term {
        Term::RTerm(RTerm { constructor: n.to_string(), terms: t }, Annotations::from(a))
    }

    pub fn new_string_term(n: &str) -> Term {
        Term::STerm(STerm { value: n.to_string() }, Annotations::empty())
    }

    pub fn new_number_term(n: f64) -> Term {
        Term::NTerm(NTerm { value: n }, Annotations::empty())
    }

    pub fn new_anot_list_term(t: Vec<Term>, anots: Vec<Term>) -> Term {
        Term::LTerm(LTerm { terms: t }, Annotations::from(anots))
    }

    pub fn new_list_term(t: Vec<Term>) -> Term {
        Term::LTerm(LTerm { terms: t }, Annotations::empty())
    }

    pub fn new_tuple_term(t: Vec<Term>) -> Term {
        Term::TTerm(TTerm { terms: t }, Annotations::empty())
    }

    pub fn get_annotations(&self) -> &Annotations {
        match self {
            Term::RTerm(_, a) => a,
            Term::STerm(_, a) => a,
            Term::NTerm(_, a) => a,
            Term::TTerm(_, a) => a,
            Term::LTerm(_, a) => a,
        }
    }

    pub fn add_annotation(&mut self, annotation: Term) {
        match self {
            Term::RTerm(_, a) => a.elems.push(annotation),
            Term::STerm(_, a) => a.elems.push(annotation),
            Term::NTerm(_, a) => a.elems.push(annotation),
            Term::TTerm(_, a) => a.elems.push(annotation),
            Term::LTerm(_, a) => a.elems.push(annotation),
        }
    }
}

// Recursive Term
#[derive(Debug, Clone, Hash)]
pub struct RTerm {
    pub constructor: String,
    pub terms: Vec<Term>
}

// String Term
#[derive(Debug, Clone, Hash)]
pub struct STerm {
    pub value: String
}

// Numerical Term
#[derive(Debug, Clone)]
pub struct NTerm {
    pub value: f64
}

fn integer_decode(val: f64) -> (u64, i16, i8) {
    let bits: u64 = unsafe { mem::transmute(val) };
    let sign: i8 = if bits >> 63 == 0 { 1 } else { -1 };
    let mut exponent: i16 = ((bits >> 52) & 0x7ff) as i16;
    let mantissa = if exponent == 0 {
        (bits & 0xfffffffffffff) << 1
    } else {
        (bits & 0xfffffffffffff) | 0x10000000000000
    };

    exponent -= 1023 + 52;
    (mantissa, exponent, sign)
}

impl Hash for NTerm {
    fn hash<H: Hasher>(&self, state: &mut H) {
        integer_decode(self.value).hash(state)
    }
}

// Tuple Term
#[derive(Debug, Clone, Hash)]
pub struct TTerm {
    pub terms: Vec<Term>
}

impl TTerm {
    pub fn new() -> TTerm { TTerm { terms: Vec::new() } }
}

// List Term
#[derive(Debug, Clone, Hash)]
pub struct LTerm {
    pub terms: Vec<Term>
}

impl LTerm {
    pub fn tail(&self) -> Term {
        Term::LTerm(LTerm { terms: self.terms[1..].iter().cloned().collect::<Vec<_>>() }, Annotations::empty())
    }

    pub fn head(&self) -> Term {
        self.terms[0].clone()
    }
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
    Ok((input, Term::new_number_term(res)))    
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
    Ok((input, Term::new_string_term(&t)))
}

pub fn parse_list(input: &str) -> IResult<&str, Term> {
    let (input, r) = delimited(char('['), ws(separated_list0(ws(tag(",")), alt((parse_term, parse_string, parse_number, parse_list)))), char(']'))(input)?;
    let (input, maybe_annots) = opt(delimited(char('{'), ws(separated_list0(ws(tag(",")), alt((parse_term, parse_string, parse_number, parse_list)))), char('}')))(input)?;
    match maybe_annots {
        Some(annots) => Ok((input, Term::new_anot_list_term(r, annots))),
        None         => Ok((input, Term::new_list_term(r))),
    }
}

pub fn parse_term(input: &str) -> IResult<&str, Term> {
    let (input, con) = take_while(char::is_alphanumeric)(input)?;
    let (input, r) = delimited(char('('), ws(separated_list0(ws(tag(",")), alt((parse_term, parse_string, parse_number, parse_list)))), char(')'))(input)?;
    let (input, maybe_annots) = opt(delimited(char('{'), ws(separated_list0(ws(tag(",")), alt((parse_term, parse_string, parse_number, parse_list)))), char('}')))(input)?;
    match maybe_annots {
        Some(annots) => Ok((input, Term::new_anot_rec_term(&con.to_string(), r, annots))),
        None         => Ok((input, Term::new_rec_term(&con.to_string(), r))),
    }
}

pub fn parse_term_file(path: &String) -> Term {
    let f = fs::read_to_string(path).unwrap();
    let (_, res) = parse_term(String::as_str(&f)).unwrap();
    res
}

/*
* Display implementation
*/

impl fmt::Display for Annotations {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        if self.elems.is_empty() {
            Ok(())
        } else {
            write!(f, "{{")?;
            let mut first = true;
            for v in self.elems.iter() {
                if !first {
                    write!(f, ", ")?;
                }
                first = false;

                write!(f, "{}", v)?;
            }
            write!(f, "}}")?;
            Ok(())
        }
    }
}

impl fmt::Display for Term {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Term::RTerm(rt, a) => write!(f, "{}{}", rt, a),
            Term::STerm(st, a) => write!(f, "{}{}", st, a),
            Term::NTerm(nt, a) => write!(f, "{}{}", nt, a),
            Term::TTerm(nt, a) => write!(f, "{}{}", nt, a),
            Term::LTerm(lt, a) => write!(f, "{}{}", lt, a),
        }
    }
}

impl fmt::Display for RTerm {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self.terms.len() {
            0 => { write!(f, "{}()", self.constructor) },
            1 => { write!(f, "{}({})", self.constructor, self.terms[0]) },
            _ => {
                write!(f, "{}(\n  ", self.constructor)?;
                let mut out = String::new();
                let mut first = true;
                for term in &self.terms {
                    if first {
                        out += format!("{}\n", term).as_str();
                        first = false;
                    } else {
                        out += format!(", {}\n", term).as_str();
                    }
                }
                out += ")";
                write!(indented(f).with_format(Format::Uniform{ indentation: "  " }), "{}", out)
            }
        }
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

impl fmt::Display for LTerm {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self.terms.len() {
            0 => write!(f, "[]"),
            1 => write!(f, "[{}]", self.terms[0]),
            _ => {
                write!(f, "[\n  ")?;
                let mut out = String::new();
                let mut first = true;
                for term in &self.terms {
                    if first {
                        out += format!("{}\n", term).as_str();
                        first = false;
                    } else {
                        out += format!(", {}\n", term).as_str();
                    }
                }
                out += "]";
                write!(indented(f).with_format(Format::Uniform{ indentation: "  " }), "{}", out)
            }
        }
    }
}


