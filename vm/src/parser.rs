extern crate nom;
use nom::{
    IResult, Parser, error::ParseError,
    character::complete::{ char, multispace0, alphanumeric1, digit1 },
    bytes::complete::{ tag },
    combinator::{ map, cut },
    branch::{ alt },
    sequence::{ delimited, tuple, pair, preceded },
    multi::{ separated_list0, many1, many0 },
};
use crate::bytecode::*;
use std::fs;

fn ws<'a, F: 'a, O, E: ParseError<&'a str>>(inner: F) -> impl FnMut(&'a str) -> IResult<&'a str, O, E>
  where
  F: Fn(&'a str) -> IResult<&'a str, O, E>,
{
  delimited(
    multispace0,
    inner,
    multispace0
  )
}

/*
* Other
*/

fn parse_u64(i: &str) -> IResult<&str, u64> {
    map(digit1, |s: &str| s.parse::<u64>().unwrap())(i)
}

fn parse_location(i: &str) -> IResult<&str, Location> {
    map(preceded(ws(tag("$")), cut(parse_u64)), |s| Location::Var(s))(i)
}

fn parse_identifier(i: &str) -> IResult<&str, String> {
    map(ws(alphanumeric1), |s| String::from(s))(i)
}

/*
* Expressions
*/

fn parse_expression(i: &str) -> IResult<&str, Expression> {
    alt((
        map(ws(parse_u64), |n| Expression::Value(Value::U64(n))),
        parse_var,
        parse_array
    ))(i)
}

fn parse_var(i: &str) -> IResult<&str, Expression> {
    map(parse_location, |l| Expression::Var(l))(i)
}

fn parse_array(i: &str) -> IResult<&str, Expression> {
    map(delimited(char('['), 
            cut(separated_list0(ws(char(',')), parse_expression)),
        char(']')),
        |e| Expression::Array(e))(i)
}

/*
* Statements
*/

fn parse_instruction(i: &str) -> IResult<&str, Instruction> {
    alt((
        parse_store,
        parse_call,
        parse_return,
        parse_print
    ))(i)
}

fn parse_call(i: &str) -> IResult<&str, Instruction> {
    map(tuple((
        parse_location,
        ws(tag("=")),
        tag("call"),
        cut(ws(parse_identifier)),
        many0(ws(parse_expression))
    )),
    |(l, _, _, id, e)| Instruction::Call(l, id, e))(i)
}

fn parse_store(i: &str) -> IResult<&str, Instruction> {
    map(tuple((
        parse_location,
        ws(tag("=")),
        parse_expression
    )),
    |(l, _, e)| Instruction::Store(l, e))(i)
}

fn parse_return(i: &str) -> IResult<&str, Instruction> {
    map(preceded(ws(tag("return")), cut(parse_expression)),
        |e| Instruction::Return(e))(i)
}

fn parse_print(i: &str) -> IResult<&str, Instruction> {
    map(preceded(ws(tag("print")), cut(parse_expression)),
        |e| Instruction::Print(e))(i)
}

/*
* File
*/

fn parse_meta(i: &str) -> IResult<&str, FunctionMeta> {
    map(
        delimited(ws(tag("[")), 
            tuple((
                ws(tag("vars")),
                ws(char(':')),
                parse_u64
            )),
        ws(tag("]"))),
        |m| FunctionMeta { vars: m.2 }
    )(i)
}

fn parse_function(i: &str) -> IResult<&str, Function> {
    map(tuple((
        ws(tag("def")), cut(parse_identifier), parse_meta, 
        many1(parse_instruction),
        multispace0
    )),
    |(_, name, meta, instrs, _)| Function::new(name, meta, instrs))(i)
}

fn parse_bytecode_string(i: &str) -> IResult<&str, Vec<Function>> {
    many1(parse_function)(i)
}

pub fn parse_bytecode_file(filename: &str) -> Module {
    let f = fs::read_to_string(filename).unwrap();
    match parse_bytecode_string(String::as_str(&f)) {
        Ok((leftover, fns)) => {
            if leftover.len() > 0 {
                panic!(format!("Didn't parse entire file: {}", leftover));
            }
            Module::from(fns)
        },
        Err(err) => panic!(format!("{:?}", err))
    }
}