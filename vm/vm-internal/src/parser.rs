extern crate nom;
use nom::{
    branch::alt,
    bytes::complete::{tag, take_while1},
    character::complete::{char, digit1, multispace0, space0},
    combinator::{cut, map},
    error::{convert_error, ParseError, VerboseError},
    multi::{many0, many1, separated_list0, separated_list1},
    sequence::{delimited, pair, preceded, separated_pair, tuple},
    IResult,
};
use std::{convert::TryInto, fmt::Display, fs};

use crate::dialect::Var;

#[derive(Debug)]
pub enum ParserError {
    ParseFailure(String),
    FileNotFound(),
}

impl Display for ParserError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::FileNotFound() => write!(f, "File not found"),
            Self::ParseFailure(message) => write!(f, "{}", message),
        }
    }
}

type MyParseResult<'a, O> = IResult<&'a str, O, VerboseError<&'a str>>;

#[derive(Debug)]
pub struct ModuleTerm {
    pub name: String,
    pub functions: Vec<FunctionTerm>,
}

#[derive(Debug)]
pub struct FunctionTerm {
    pub name: String,
    pub params: Vec<Var>,
    pub ty: TypeTerm,
    pub body: Vec<Term>,
}

#[derive(Debug)]
pub struct InstructionTerm {
    pub dialect: String,
    pub name: String,
    pub elements: Vec<Term>,
}

#[derive(Debug)]
pub enum Term {
    Instruction(InstructionTerm),
    Variable(Var),
    Number(i64),
    Bool(bool),
    Name(Vec<String>),
    Type(TypeTerm),
    Tuple(Vec<Term>),
}

impl Term {
    pub fn as_variable(&self) -> Option<&Var> {
        if let Self::Variable(v) = self {
            Some(v)
        } else {
            None
        }
    }

    pub fn as_number(&self) -> Option<&i64> {
        if let Self::Number(v) = self {
            Some(v)
        } else {
            None
        }
    }

    pub fn as_bool(&self) -> Option<&bool> {
        if let Self::Bool(v) = self {
            Some(v)
        } else {
            None
        }
    }

    pub fn as_name(&self) -> Option<&Vec<String>> {
        if let Self::Name(v) = self {
            Some(v)
        } else {
            None
        }
    }

    pub fn as_instruction(&self) -> Option<&InstructionTerm> {
        if let Self::Instruction(v) = self {
            Some(v)
        } else {
            None
        }
    }
}

#[derive(Debug)]
pub enum TypeTerm {
    Tuple(Vec<TypeTerm>),
    Ref(Box<TypeTerm>),
    Array(Box<TypeTerm>, Option<u64>),
    Name(String),
    Fn(Box<TypeTerm>, Box<TypeTerm>),
    Unit,
    Any,
}

pub fn parse_instruction_elem(i: &str) -> MyParseResult<Term> {
    map(
        pair(parse_instruction_name, parse_instruction_elements),
        |((dialect, name), elements)| {
            Term::Instruction(InstructionTerm {
                dialect,
                name,
                elements,
            })
        },
    )(i)
}

fn parse_instruction_name(i: &str) -> MyParseResult<(String, String)> {
    separated_pair(parse_identifier, char(':'), parse_identifier)(i)
}

fn parse_instruction_elements(i: &str) -> MyParseResult<Vec<Term>> {
    separated_list0(char(','), ws(parse_instruction_element))(i)
}

fn parse_instruction_element(i: &str) -> MyParseResult<Term> {
    alt((
        parse_variable_elem,
        parse_number_elem,
        parse_bool_elem,
        parse_name_elem,
        map(parse_type_elem, |t| Term::Type(t)),
        parse_tuple_elem,
    ))(i)
}

fn parse_variable_elem(i: &str) -> MyParseResult<Term> {
    map(preceded(char('$'), cut(parse_u8)), |v| {
        Term::Variable(Var(v))
    })(i)
}

fn parse_number_elem(i: &str) -> MyParseResult<Term> {
    map(parse_u64, |n| Term::Number(n.try_into().unwrap()))(i)
}

fn parse_bool_elem(i: &str) -> MyParseResult<Term> {
    map(parse_bool, |b| Term::Bool(b))(i)
}

fn parse_name_elem(i: &str) -> MyParseResult<Term> {
    map(
        preceded(char('@'), separated_list1(char(':'), parse_identifier)),
        |ns| Term::Name(ns),
    )(i)
}

fn parse_type_elem(i: &str) -> MyParseResult<TypeTerm> {
    alt((
        map(
            delimited(tag("["), cut(parse_type_elem), cut(tag("]"))),
            |t| TypeTerm::Array(Box::from(t), None),
        ),
        map(tag("()"), |_| TypeTerm::Unit),
        map(tag("_"), |_| TypeTerm::Any),
        map(preceded(tag("&"), cut(parse_type_elem)), |t| {
            TypeTerm::Ref(Box::from(t))
        }),
        map(parse_identifier, |id| TypeTerm::Name(id)),
        map(
            delimited(
                ws(char('(')),
                separated_pair(parse_type_elem, ws(tag("->")), parse_type_elem),
                ws(char(')')),
            ),
            |(i, o)| TypeTerm::Fn(Box::from(i), Box::from(o)),
        ),
        map(
            delimited(
                ws(char('(')),
                separated_list1(ws(char(',')), parse_type_elem),
                ws(char(')')),
            ),
            |t| TypeTerm::Tuple(t),
        ),
    ))(i)
}

fn parse_tuple_elem(i: &str) -> MyParseResult<Term> {
    map(
        delimited(
            ws(char('(')),
            separated_list0(ws(char(',')), parse_instruction_element),
            ws(char(')')),
        ),
        |es| Term::Tuple(es),
    )(i)
}

fn parse_function_elem(i: &str) -> MyParseResult<FunctionTerm> {
    map(
        preceded(
            ws(tag("fn")),
            cut(tuple((
                cut(parse_identifier),
                delimited(
                    ws(char('(')),
                    ws(many0(separated_pair(
                        parse_variable_elem,
                        cut(ws(char(':'))),
                        cut(parse_type_elem),
                    ))),
                    ws(char(')')),
                ),
                preceded(ws(tag("->")), parse_type_elem),
                ws(char('{')),
                preceded(multispace0, many1(parse_instruction_elem)),
                multispace0,
                char('}'),
            ))),
        ),
        |(name, parameters, return_type, _, instructions, _, _)| {
            let mut arg_types: Vec<TypeTerm> = vec![];
            let mut params: Vec<Var> = vec![];
            for p in parameters.into_iter() {
                arg_types.push(p.1);
                params.push(*p.0.as_variable().unwrap());
            }

            let ty = TypeTerm::Fn(
                Box::from(TypeTerm::Tuple(arg_types)),
                Box::from(return_type),
            );

            FunctionTerm {
                name,
                params,
                ty,
                body: instructions,
            }
        },
    )(i)
}

fn parse_module_declaration(i: &str) -> MyParseResult<String> {
    whitespace_around(preceded(tag("mod"), spaces_around(parse_identifier)))(i)
}

fn parse_module_elem(i: &str) -> MyParseResult<ModuleTerm> {
    map(
        pair(parse_module_declaration, many1(parse_function_elem)),
        |(name, functions)| ModuleTerm { name, functions },
    )(i)
}

pub fn parse_bytecode_file_new(filename: &str) -> Result<ModuleTerm, ParserError> {
    match fs::read_to_string(filename) {
        Ok(f) => match parse_module_elem(f.as_str()) {
            Ok((leftover, m)) => {
                if leftover.len() > 0 {
                    return Err(ParserError::ParseFailure(format!(
                        "Didn't parse entire file: {}",
                        leftover
                    )));
                }

                return Ok(m);
            }
            Err(nom::Err::Failure(err)) | Err(nom::Err::Error(err)) => {
                return Err(ParserError::ParseFailure(format!(
                    "{}",
                    convert_error(f.as_str(), err)
                )))
            }
            Err(err) => Err(ParserError::ParseFailure(format!("{}", err))),
        },
        Err(_) => Err(ParserError::FileNotFound()),
    }
}

/*
* Helpers
*/

fn ws<'a, F: 'a, O, E: ParseError<&'a str>>(
    inner: F,
) -> impl FnMut(&'a str) -> IResult<&'a str, O, E>
where
    F: FnMut(&'a str) -> IResult<&'a str, O, E>,
{
    delimited(multispace0, inner, multispace0)
}

fn spaces_around<'a, F: 'a, O, E: ParseError<&'a str>>(
    inner: F,
) -> impl FnMut(&'a str) -> IResult<&'a str, O, E>
where
    F: FnMut(&'a str) -> IResult<&'a str, O, E>,
{
    delimited(space0, inner, space0)
}

fn whitespace_around<'a, F: 'a, O, E: ParseError<&'a str>>(
    inner: F,
) -> impl FnMut(&'a str) -> IResult<&'a str, O, E>
where
    F: FnMut(&'a str) -> IResult<&'a str, O, E>,
{
    delimited(multispace0, inner, multispace0)
}

fn parse_u64(i: &str) -> MyParseResult<u64> {
    map(digit1, |s: &str| s.parse::<u64>().unwrap())(i)
}

fn parse_u32(i: &str) -> MyParseResult<u32> {
    map(digit1, |s: &str| s.parse::<u32>().unwrap())(i)
}

fn parse_u16(i: &str) -> MyParseResult<u16> {
    map(digit1, |s: &str| s.parse::<u16>().unwrap())(i)
}

fn parse_u8(i: &str) -> MyParseResult<u8> {
    map(digit1, |s: &str| s.parse::<u8>().unwrap())(i)
}

fn parse_bool(i: &str) -> MyParseResult<bool> {
    alt((map(tag("true"), |_| true), map(tag("false"), |_| false)))(i)
}

fn parse_identifier(i: &str) -> MyParseResult<String> {
    map(take_while1(|c| char::is_alphanumeric(c) || c == '_'), |s| {
        String::from(s)
    })(i)
}
