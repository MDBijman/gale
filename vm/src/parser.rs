extern crate nom;
use nom::{
    IResult, Parser, error::ParseError,
    character::complete::{ char, multispace0, alphanumeric1, digit1, space0 },
    bytes::complete::{ tag },
    combinator::{ map, cut },
    branch::{ alt },
    sequence::{ delimited, tuple, preceded, terminated },
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

fn spaces_around<'a, F: 'a, O, E: ParseError<&'a str>>(inner: F) -> impl FnMut(&'a str) -> IResult<&'a str, O, E>
  where
  F: Fn(&'a str) -> IResult<&'a str, O, E>,
{
  delimited(
    space0,
    inner,
    space0
  )
}

/*
* Other
*/

fn parse_u64(i: &str) -> IResult<&str, u64> {
    map(digit1, |s: &str| s.parse::<u64>().unwrap())(i)
}

fn parse_location(i: &str) -> IResult<&str, Location> {
    map(preceded(tag("$"), cut(parse_u64)), |s| Location::Var(s))(i)
}

fn parse_identifier(i: &str) -> IResult<&str, String> {
    map(alphanumeric1, |s| String::from(s))(i)
}

fn parse_op(i: &str) -> IResult<&str, Op> {
    alt((
        map(tag("+"), |_|Op::Add),
        map(tag("-"), |_|Op::Sub)
    ))(i)
}

/*
* Expressions
*/

fn parse_expression(i: &str) -> IResult<&str, Expression> {
    preceded(space0, alt((
        map(parse_u64, |n| Expression::Value(Value::U64(n))),
        parse_var,
        parse_array
    )))(i)
}

fn parse_var(i: &str) -> IResult<&str, Expression> {
    map(parse_location, |l| Expression::Var(l))(i)
}

fn parse_array(i: &str) -> IResult<&str, Expression> {
    map(delimited(char('['), 
            cut(separated_list0(spaces_around(char(',')), parse_expression)),
        spaces_around(char(']'))),
        |e| Expression::Array(e))(i)
}

/*
* Statements
*/

fn parse_instruction(i: &str) -> IResult<&str, Vec<Instruction>> {
    terminated(alt((
        map(parse_call, |i| vec![i]),
        map(parse_return, |i| vec![i]),
        map(parse_print, |i| vec![i]),
        parse_store, 
    )), multispace0)(i)
}

fn parse_call(i: &str) -> IResult<&str, Instruction> {
    map(tuple((
        parse_location,
        spaces_around(tag("=")),
        spaces_around(tag("call")),
        cut(parse_identifier),
        spaces_around(tag("(")),
        many0(parse_expression),
        spaces_around(tag(")")),
    )),
    |(l, _, _, id, _, e, _)| Instruction::Call(l, id, e))(i)
}

fn parse_store(i: &str) -> IResult<&str, Vec<Instruction>> {
    map(terminated(tuple((
        parse_location,
        ws(tag("=")),
        many1(alt((
            map(parse_expression, |e| Instruction::Push(e)),
            map(preceded(space0, parse_op), |o| Instruction::Op(o))))),
    )), multispace0),
    |(l, _, mut instrs)| {
        if instrs.len() == 1 {
            // Fold single operand into store instruction
            match instrs.remove(0) {
                Instruction::Push(e) => {
                    vec![Instruction::Store(l, e)]
                },
                _ => panic!("Invalid operand")
            }
        } else {
            // Otherwise use stack and pop
            instrs.push(Instruction::Pop(l));
            instrs
        }
    })(i)
}

fn parse_return(i: &str) -> IResult<&str, Instruction> {
    map(preceded(tag("ret"), cut(parse_expression)),
        |e| Instruction::Return(e))(i)
}

fn parse_print(i: &str) -> IResult<&str, Instruction> {
    map(preceded(tag("print"), cut(parse_expression)),
        |e| Instruction::Print(e))(i)
}

/*
* File
*/

fn parse_meta(i: &str) -> IResult<&str, FunctionMeta> {
    map(
        delimited(spaces_around(tag("[")), 
            tuple((
                spaces_around(tag("vars")),
                spaces_around(char(':')),
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
    |(_, name, meta, instrs, _)| Function::new(name, meta, instrs.into_iter().flatten().collect()))(i)
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