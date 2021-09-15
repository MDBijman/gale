extern crate nom;
use crate::bytecode::*;
use nom::{
    branch::alt,
    bytes::complete::{tag, take_while, take_while1},
    character::complete::{char, digit1, multispace0, space0},
    combinator::{cut, map},
    error::ParseError,
    multi::{many0, many1, separated_list0},
    sequence::{delimited, pair, preceded, terminated, tuple},
    IResult,
};
use std::fs;

fn ws<'a, F: 'a, O, E: ParseError<&'a str>>(
    inner: F,
) -> impl FnMut(&'a str) -> IResult<&'a str, O, E>
where
    F: Fn(&'a str) -> IResult<&'a str, O, E>,
{
    delimited(multispace0, inner, multispace0)
}

fn spaces_around<'a, F: 'a, O, E: ParseError<&'a str>>(
    inner: F,
) -> impl FnMut(&'a str) -> IResult<&'a str, O, E>
where
    F: Fn(&'a str) -> IResult<&'a str, O, E>,
{
    delimited(space0, inner, space0)
}

/*
* Other
*/

fn parse_u64(i: &str) -> IResult<&str, u64> {
    map(digit1, |s: &str| s.parse::<u64>().unwrap())(i)
}

fn parse_u32(i: &str) -> IResult<&str, u32> {
    map(digit1, |s: &str| s.parse::<u32>().unwrap())(i)
}

fn parse_u16(i: &str) -> IResult<&str, u16> {
    map(digit1, |s: &str| s.parse::<u16>().unwrap())(i)
}

fn parse_var_idx(i: &str) -> IResult<&str, Var> {
    preceded(tag("$"), cut(parse_u16))(i)
}

fn parse_identifier(i: &str) -> IResult<&str, String> {
    map(take_while1(|c| char::is_alphanumeric(c) || c == '_'), |s| {
        String::from(s)
    })(i)
}

fn parse_lbl(i: &str) -> IResult<&str, String> {
    map(preceded(spaces_around(tag("@")), parse_identifier), |id| id)(i)
}

/*
* Types
*/

fn parse_type(i: &str) -> IResult<&str, Type> {
    alt((
        map(tag("u64"), |_| Type::U64()),
        map(delimited(tag("["), parse_type, tag("]")), |t| {
            Type::Array(Box::new(t))
        }),
        map(tag("null"), |_| Type::Null),
        map(tag("str"), |_| Type::Str),
    ))(i)
}

/*
* Expressions
*/

fn parse_constant(i: &str) -> IResult<&str, Value> {
    preceded(
        space0,
        alt((
            map(parse_u64, |n| Value::U64(n)),
            parse_boolean,
            parse_string,
        )),
    )(i)
}

fn parse_boolean(i: &str) -> IResult<&str, Value> {
    alt((
        map(tag("true"), |_| Value::U64(1)),
        map(tag("false"), |_| Value::U64(0)),
    ))(i)
}

fn parse_string(i: &str) -> IResult<&str, Value> {
    map(
        delimited(tag("\""), take_while(|c| c != '\"'), tag("\"")),
        |r| Value::Str(String::from(r)),
    )(i)
}

/*
* Statements
*/

fn parse_three_var_command<'a>(
    op: &'a str,
) -> impl FnMut(&'a str) -> IResult<&str, (Var, Var, Var)> {
    move |i| {
        tuple((
            preceded(tag(op), spaces_around(parse_var_idx)),
            preceded(tag(","), spaces_around(parse_var_idx)),
            preceded(tag(","), spaces_around(parse_var_idx)),
        ))(i)
    }
}

fn parse_two_var_command<'a>(op: &'a str) -> impl FnMut(&'a str) -> IResult<&str, (Var, Var)> {
    move |i| {
        tuple((
            preceded(tag(op), spaces_around(parse_var_idx)),
            preceded(tag(","), spaces_around(parse_var_idx)),
        ))(i)
    }
}

fn parse_one_var_command<'a>(op: &'a str) -> impl FnMut(&'a str) -> IResult<&str, Var> {
    move |i| preceded(tag(op), spaces_around(parse_var_idx))(i)
}

fn parse_instruction(i: &str) -> IResult<&str, ParsedInstruction> {
    terminated(
        alt((
            map(parse_two_var_command("store"), |(l, e)| {
                ParsedInstruction::Instr(Instruction::StoreVar(l, e))
            }),
            map(parse_two_var_command("load"), |(l, e)| {
                ParsedInstruction::Instr(Instruction::LoadVar(l, e))
            }),
            map(parse_three_var_command("eq"), |(a, b, c)| {
                ParsedInstruction::Instr(Instruction::EqVarVar(a, b, c))
            }),
            map(parse_three_var_command("lt"), |(a, b, c)| {
                ParsedInstruction::Instr(Instruction::LtVarVar(a, b, c))
            }),
            map(parse_three_var_command("sub"), |(l, e1, e2)| {
                ParsedInstruction::Instr(Instruction::SubVarVar(l, e1, e2))
            }),
            map(parse_three_var_command("add"), |(l, e1, e2)| {
                ParsedInstruction::Instr(Instruction::AddVarVar(l, e1, e2))
            }),
            map(parse_two_var_command("neg"), |(l, e)| {
                ParsedInstruction::Instr(Instruction::NotVar(l, e))
            }),
            map(parse_two_var_command("cp"), |(l, e)| {
                ParsedInstruction::Instr(Instruction::Copy(l, e))
            }),
            map(parse_three_var_command("movidx"), |(r, a, i)| {
                ParsedInstruction::Instr(Instruction::Index(r, a, i))
            }),
            map(
                tuple((
                    preceded(spaces_around(tag("movc")), cut(parse_var_idx)),
                    preceded(spaces_around(tag(",")), parse_u32),
                )),
                |(a, b)| ParsedInstruction::Instr(Instruction::ConstU32(a, b)),
            ),
            map(
                tuple((
                    preceded(spaces_around(tag("loadc")), cut(parse_var_idx)),
                    preceded(spaces_around(tag(",")), parse_u32),
                )),
                |(a, b)| ParsedInstruction::Instr(Instruction::LoadConst(a, b as ConstLbl)),
            ),
            map(parse_one_var_command("ret"), |e| {
                ParsedInstruction::Instr(Instruction::Return(e))
            }),
            map(parse_one_var_command("print"), |e| {
                ParsedInstruction::Instr(Instruction::Print(e))
            }),
            parse_call,
            parse_jmp,
            parse_lbl_instr,
        )),
        multispace0,
    )(i)
}

/*
fn parse_alloc(i: &str) -> IResult<&str, ParsedInstruction> {
    map(
        tuple((
            parse_var_idx,
            spaces_around(tag("=")),
            spaces_around(tag("alloc")),
            parse_type,
        )),
        |(l, _, _, t)| ParsedInstruction::Instr(Instruction::Alloc(l, t)),
    )(i)
}
*/

fn parse_call(i: &str) -> IResult<&str, ParsedInstruction> {
    map(
        tuple((
            preceded(spaces_around(tag("call")), cut(parse_var_idx)),
            preceded(spaces_around(tag(",")), cut(parse_lbl)),
            space0,
            delimited(
                tag("("),
                separated_list0(spaces_around(tag(",")), parse_var_idx),
                tag(")"),
            ),
        )),
        |(r, id, _, args)| ParsedInstruction::IndirectCall(r, id, args),
    )(i)
}

fn parse_jmp(i: &str) -> IResult<&str, ParsedInstruction> {
    alt((
        map(
            preceded(
                spaces_around(tag("jmpifn")),
                pair(cut(spaces_around(parse_var_idx)), cut(parse_lbl)),
            ),
            |(v, l)| ParsedInstruction::IndirectJmpIfNot(l, v),
        ),
        map(
            preceded(
                spaces_around(tag("jmpif")),
                pair(cut(spaces_around(parse_var_idx)), cut(parse_lbl)),
            ),
            |(v, l)| ParsedInstruction::IndirectJmpIf(l, v),
        ),
        map(preceded(spaces_around(tag("jmp")), cut(parse_lbl)), |l| {
            ParsedInstruction::IndirectJmp(l)
        }),
    ))(i)
}

fn parse_lbl_instr(i: &str) -> IResult<&str, ParsedInstruction> {
    map(terminated(parse_identifier, tag(":")), |lbl| {
        ParsedInstruction::Lbl(lbl)
    })(i)
}

/*
* File
*/

fn parse_param(i: &str) -> IResult<&str, Param> {
    map(
        tuple((parse_var_idx, cut(spaces_around(tag(":"))), parse_type)),
        |(i, _, t)| Param { var: i, type_: t },
    )(i)
}

fn parse_params(i: &str) -> IResult<&str, Vec<Param>> {
    delimited(
        spaces_around(tag("(")),
        separated_list0(spaces_around(tag(",")), parse_param),
        spaces_around(tag(")")),
    )(i)
}

fn parse_function(i: &str) -> IResult<&str, ParsedFunction> {
    map(
        tuple((
            ws(tag("fn")),
            cut(parse_identifier),
            ws(parse_params),
            multispace0,
            many1(parse_instruction),
            multispace0,
            tag("endfn"),
        )),
        |(_, name, parameters, _, instructions, _, _)| {
            /*
             * Compute local variables frame size
             */

            let mut vars = 0;

            for par in parameters.iter() {
                vars = Var::max(vars, par.var);
            }

            for instr in instructions.iter() {
                match instr {
                    ParsedInstruction::Instr(instr) => match instr {
                        Instruction::ConstU32(i, _) => {
                            vars = Var::max(vars, *i);
                        }
                        Instruction::Copy(i, _) => {
                            vars = Var::max(vars, *i);
                        }
                        Instruction::EqVarVar(a, b, c) => {
                            vars = Var::max(vars, *a);
                            vars = Var::max(vars, *b);
                            vars = Var::max(vars, *c);
                        }
                        Instruction::LtVarVar(a, b, c) => {
                            vars = Var::max(vars, *a);
                            vars = Var::max(vars, *b);
                            vars = Var::max(vars, *c);
                        }
                        Instruction::SubVarVar(a, b, c) => {
                            vars = Var::max(vars, *a);
                            vars = Var::max(vars, *b);
                            vars = Var::max(vars, *c);
                        }
                        Instruction::AddVarVar(a, b, c) => {
                            vars = Var::max(vars, *a);
                            vars = Var::max(vars, *b);
                            vars = Var::max(vars, *c);
                        }
                        Instruction::NotVar(a, b) => {
                            vars = Var::max(vars, *a);
                            vars = Var::max(vars, *b);
                        }
                        Instruction::LoadVar(a, b) => {
                            vars = Var::max(vars, *a);
                            vars = Var::max(vars, *b);
                        }
                        Instruction::LoadConst(i, _) => {
                            vars = Var::max(vars, *i);
                        }
                        Instruction::Index(a, b, c) => {
                            vars = Var::max(vars, *a);
                            vars = Var::max(vars, *b);
                            vars = Var::max(vars, *c);
                        }
                        Instruction::StoreVar(a, b) => {
                            vars = Var::max(vars, *a);
                            vars = Var::max(vars, *b);
                        }
                        /*Instruction::Alloc(i, _) => {
                            vars = Var::max(vars, *i);
                        }*/
                        _ => {}
                    },
                    ParsedInstruction::IndirectCall(i, _, _) => {
                        vars = Var::max(vars, *i);
                    }
                    _ => {}
                }
            }

            ParsedFunction::new(name, ParsedFunctionMeta { vars: vars + 1 }, instructions)
        },
    )(i)
}

fn parse_constants(i: &str) -> IResult<&str, Vec<Value>> {
    preceded(
        ws(tag(".const")),
        many0(preceded(
            tuple((parse_u64, tag(":"), ws(parse_type))),
            ws(parse_constant),
        )),
    )(i)
}

fn parse_functions(i: &str) -> IResult<&str, Vec<ParsedFunction>> {
    preceded(ws(tag(".code")), many1(parse_function))(i)
}

pub fn parse_bytecode_string(i: &str) -> Module {
    match map(tuple((parse_constants, parse_functions)), |(c, f)| {
        Module::from_long_functions(c, f)
    })(i)
    {
        Ok((leftover, m)) => {
            if leftover.len() > 0 {
                panic!("Didn't parse entire file: {}", leftover);
            }

            m
        }
        Err(err) => panic!("{:?}", err),
    }
}

pub fn parse_bytecode_file(filename: &str) -> Module {
    let f = fs::read_to_string(filename).unwrap();
    parse_bytecode_string(f.as_str())
}
