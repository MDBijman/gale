extern crate nom;
use nom::{IResult, branch::{ alt }, bytes::complete::{tag, take_while, take_while1}, character::complete::{ char, multispace0, digit1, space0 }, combinator::{ map, cut }, error::ParseError, multi::{ separated_list0, many1, many0 }, sequence::{ delimited, tuple, preceded, terminated, pair }};
use crate::bytecode::*;
use std::fs;
use std::collections::HashSet;
use std::collections::HashMap;

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
    map(take_while1(|c| { char::is_alphanumeric(c) || c == '_' }), |s| String::from(s))(i)
}

fn parse_un_op(i: &str) -> IResult<&str, UnOp> {
    map(tag("~"), |_| UnOp::Not)(i)
}

fn parse_bin_op(i: &str) -> IResult<&str, BinOp> {
    alt((
        map(tag("+"), |_| BinOp::Add),
        map(tag("-"), |_| BinOp::Sub),
        map(tag("="), |_| BinOp::Eq),
    ))(i)
}

fn parse_lbl(i: &str) -> IResult<&str, String> {
    map(spaces_around(parse_identifier), |id| id)(i)
}

/*
* Types
*/

fn parse_type(i: &str) -> IResult<&str, Type> {
    alt((
        map(tag("u64"), |_| Type::U64()),
        map(delimited(tag("["), parse_type, tag("]")), |t| Type::Array(Box::new(t))),
        map(tag("null"), |_| Type::Null),
        map(tag("str"), |_| Type::Str)
    ))(i)
}

/*
* Expressions
*/

fn parse_constant(i: &str) -> IResult<&str, Value> {
    preceded(space0, alt((
        map(parse_u64, |n| Value::U64(n)),
        parse_boolean,
        parse_string
    )))(i)
}

fn parse_expression(i: &str) -> IResult<&str, Expression> {
    preceded(space0, alt((
        map(parse_u64, |n| Expression::Value(Value::U64(n))),
        parse_var,
        parse_array,
        map(parse_boolean, |n| Expression::Value(n))
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

fn parse_boolean(i: &str) -> IResult<&str, Value> {
    alt((
        map(tag("true"), |_| Value::U64(1)),
        map(tag("false"), |_| Value::U64(0)),
    ))(i)
}

fn parse_string(i: &str) -> IResult<&str, Value> {
    map(delimited(tag("\""), take_while(|c| c != '\"'), tag("\"")), |r| Value::Str(String::from(r)))(i)
}

/*
* Statements
*/

fn parse_instruction(i: &str) -> IResult<&str, Vec<Instruction>> {
    terminated(alt((
        map(parse_call, |i| vec![i]),
        map(parse_return, |i| vec![i]),
        map(parse_print, |i| vec![i]),
        map(parse_alloc, |i| vec![i]),
        map(parse_index, |i| vec![i]),
        map(parse_loadc, |i| vec![i]),
        map(parse_load, |i| vec![i]),
        map(parse_store, |i| vec![i]),
        map(parse_jmp, |i| vec![i]),
        map(parse_lbl_instr, |i| vec![i]),
        parse_set, 
    )), multispace0)(i)
}

fn parse_load(i: &str) -> IResult<&str, Instruction> {
    map(tuple((
        parse_location,
        spaces_around(tag("=")),
        spaces_around(tag("load")),
        parse_location,
    )),
    |(l, _, _, s)| Instruction::Load(l, s))(i)
}

fn parse_store(i: &str) -> IResult<&str, Instruction> {
    map(tuple((
        spaces_around(tag("store")),
        parse_location,
        parse_expression,
    )),
    |(_, l, e)| Instruction::Store(l, e))(i)
}

fn parse_alloc(i: &str) -> IResult<&str, Instruction> {
    map(tuple((
        parse_location,
        spaces_around(tag("=")),
        spaces_around(tag("alloc")),
        parse_type
    )),
    |(l, _, _, t)| Instruction::Alloc(l, t))(i)
}

fn parse_index(i: &str) -> IResult<&str, Instruction> {
    map(tuple((
        parse_location,
        spaces_around(tag("=")),
        spaces_around(tag("index")),
        spaces_around(parse_location),
        parse_location,
    )),
    |(res, _, _, a, b)| Instruction::Index(res, a, b))(i)
}

fn parse_loadc(i: &str) -> IResult<&str, Instruction> {
    map(tuple((
        parse_location,
        spaces_around(tag("=")),
        spaces_around(tag("loadc")),
        parse_u64
    )),
    |(res, _, _, a)| Instruction::LoadConst(res, a))(i)
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

fn parse_set(i: &str) -> IResult<&str, Vec<Instruction>> {
    map(terminated(tuple((
        parse_location,
        ws(tag("=")),
        cut(many1(alt((
            map(parse_expression, |e| Instruction::Push(e)),
            map(preceded(space0, parse_un_op), |o| Instruction::UnOp(o)),
            map(preceded(space0, parse_bin_op), |o| Instruction::BinOp(o))
        )))),
    )), multispace0),
    |(l, _, mut instrs)| {
        if instrs.len() == 1 {
            // Fold single operand into store instruction
            match instrs.remove(0) {
                Instruction::Push(e) => {
                    vec![Instruction::Set(l, e)]
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

fn parse_jmp(i: &str) -> IResult<&str, Instruction> {
    alt((
        map(preceded(spaces_around(tag("jmpif")), pair(cut(parse_expression), cut(parse_lbl))), |(v, l)| Instruction::JmpIf(Location::Lbl(l), v)),
        map(preceded(spaces_around(tag("jmp")), cut(parse_lbl)), |l| Instruction::Jmp(Location::Lbl(l)))
    ))(i)
}

fn parse_lbl_instr(i: &str) -> IResult<&str, Instruction> {
    map(terminated(parse_lbl, tag(":")), |lbl| Instruction::Lbl(lbl))(i)
}

/*
* File
*/

fn parse_args(i: &str) -> IResult<&str, Vec<Location>> {
    delimited(
        spaces_around(tag("(")), 
        separated_list0(spaces_around(tag(",")), parse_location),
        spaces_around(tag(")")))(i)
}

fn parse_function(i: &str) -> IResult<&str, Function> {
    map(tuple((
        ws(tag("func")), cut(parse_identifier), parse_args, ws(tag("{")),
        many1(parse_instruction),
        ws(tag("}")),
        multispace0
    )),
    |(_, name, arguments, _, many_instrs, _, _)| {
        let instructions: Vec<Instruction> = many_instrs.into_iter().flatten().collect();

        /*
        * Compute local variables frame size
        */

        let mut vars = HashSet::new();

        for arg in arguments.iter() {
            match arg {
                Location::Var(i) => { vars.insert(i); },
                _ => {}
            }
        }

        for instr in instructions.iter() {
            match instr {
                Instruction::Set(Location::Var(i), _)       => { vars.insert(i); },
                Instruction::Pop(Location::Var(i))          => { vars.insert(i); },
                Instruction::Call(Location::Var(i), _, _)   => { vars.insert(i); },
                Instruction::Load(Location::Var(i), _)      => { vars.insert(i); },
                Instruction::LoadConst(Location::Var(i), _) => { vars.insert(i); },
                Instruction::Index(Location::Var(i), _, _)  => { vars.insert(i); },
                Instruction::Store(Location::Var(i), _)     => { vars.insert(i); },
                Instruction::Alloc(Location::Var(i), _)     => { vars.insert(i); },
                _ => {},
            }
        }

        /*
        * Compute Jump Table
        */

        let mut jump_table: HashMap<String, u64> = HashMap::new();
        let mut pc: u64 = 0;
        for instr in instructions.iter() {
            match instr {
                Instruction::Lbl(name) => { jump_table.insert(name.clone(), pc); },
                _ => {}
            }

            pc += 1;
        }

        Function::new(name,
            FunctionMeta { vars: vars.len() as u64 },
            instructions,
            jump_table)
    })(i)
}

fn parse_constants(i: &str) -> IResult<&str, Vec<Value>> {
    preceded(
        ws(tag(".const")),
        many0(preceded(tuple((parse_u64, tag(":"), ws(parse_type))), ws(parse_constant)))
    )(i)
}

fn parse_functions(i: &str) -> IResult<&str, Vec<Function>> {
    preceded(
        ws(tag(".code")),
        many1(parse_function)
    )(i)
}

pub fn parse_bytecode_string(i: &str) -> Module {
    match map(tuple((
        parse_constants,
        parse_functions
    )), |(c, f)| Module::from(c, f))(i) {
        Ok((leftover, m)) => {
            if leftover.len() > 0 {
                panic!("Didn't parse entire file: {}", leftover);
            }
            m
        },
        Err(err) => panic!("{:?}", err)
    }
}

pub fn parse_bytecode_file(filename: &str) -> Module {
    let f = fs::read_to_string(filename).unwrap();
    parse_bytecode_string(f.as_str())
}