extern crate nom;
use crate::bytecode::*;
use nom::{
    branch::alt,
    bytes::complete::{tag, take_while, take_while1},
    character::complete::{digit1, multispace0, space0},
    combinator::{cut, map, opt},
    error::{convert_error, ParseError, VerboseError},
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

type MyParseResult<'a, O> = IResult<&'a str, O, VerboseError<&'a str>>;

/*
* Other
*/

fn parse_u64(i: &str) -> MyParseResult<u64> {
    map(digit1, |s: &str| s.parse::<u64>().unwrap())(i)
}

fn parse_u32(i: &str) -> MyParseResult<u32> {
    map(digit1, |s: &str| s.parse::<u32>().unwrap())(i)
}

fn parse_u16(i: &str) -> MyParseResult<u16> {
    map(digit1, |s: &str| s.parse::<u16>().unwrap())(i)
}

fn parse_var_idx(i: &str) -> MyParseResult<Var> {
    preceded(tag("$"), cut(parse_u16))(i)
}

fn parse_type_idx(i: &str) -> MyParseResult<Var> {
    preceded(tag("%"), cut(parse_u16))(i)
}

fn parse_identifier(i: &str) -> MyParseResult<String> {
    map(take_while1(|c| char::is_alphanumeric(c) || c == '_'), |s| {
        String::from(s)
    })(i)
}

fn parse_lbl(i: &str) -> MyParseResult<String> {
    map(preceded(spaces_around(tag("@")), parse_identifier), |id| id)(i)
}

/*
* Types
*/

fn parse_type(i: &str) -> MyParseResult<Type> {
    alt((
        map(tag("u64"), |_| Type::U64),
        map(delimited(tag("["), parse_type, tag("]")), |t| {
            Type::Array(Box::new(t))
        }),
        map(tag("null"), |_| Type::Null),
        map(tag("str"), |_| Type::Str),
        map(preceded(tag("&"), parse_type), |t| Type::Pointer(Box::from(t))),
    ))(i)
}

/*
* Expressions
*/

fn parse_constant(i: &str) -> MyParseResult<Value> {
    preceded(
        space0,
        alt((
            map(parse_u64, |n| Value::U64(n)),
            parse_boolean,
            parse_string,
        )),
    )(i)
}

fn parse_boolean(i: &str) -> MyParseResult<Value> {
    alt((
        map(tag("true"), |_| Value::U64(1)),
        map(tag("false"), |_| Value::U64(0)),
    ))(i)
}

fn parse_string(i: &str) -> MyParseResult<Value> {
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
) -> impl FnMut(&'a str) -> MyParseResult<(Var, Var, Var)> {
    move |i| {
        tuple((
            preceded(tag(op), spaces_around(parse_var_idx)),
            preceded(tag(","), spaces_around(parse_var_idx)),
            preceded(tag(","), spaces_around(parse_var_idx)),
        ))(i)
    }
}

fn parse_two_var_command<'a>(op: &'a str) -> impl FnMut(&'a str) -> MyParseResult<(Var, Var)> {
    move |i| {
        tuple((
            preceded(tag(op), spaces_around(parse_var_idx)),
            preceded(tag(","), spaces_around(parse_var_idx)),
        ))(i)
    }
}

fn parse_two_var_one_type_command<'a>(op: &'a str) -> impl FnMut(&'a str) -> MyParseResult<(Var, Var, TypeLbl)> {
    move |i| {
        tuple((
            preceded(tag(op), spaces_around(parse_var_idx)),
            preceded(tag(","), spaces_around(parse_var_idx)),
            preceded(tag(","), spaces_around(parse_type_idx)),
        ))(i)
    }
}

fn parse_one_var_command<'a>(op: &'a str) -> impl FnMut(&'a str) -> MyParseResult<Var> {
    move |i| preceded(tag(op), spaces_around(parse_var_idx))(i)
}

fn parse_instruction(i: &str) -> MyParseResult<ParsedInstruction> {
    terminated(
        alt((
            map(parse_two_var_one_type_command("store"), |(l, e, t)| {
                ParsedInstruction::Instr(Instruction::StoreVar(l, e, t))
            }),
            map(parse_two_var_one_type_command("load"), |(l, e, t)| {
                ParsedInstruction::Instr(Instruction::LoadVar(l, e, t))
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
            map(parse_three_var_command("idx"), |(r, a, i)| {
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
            parse_alloc,
            parse_jmp,
            parse_lbl_instr,
        )),
        multispace0,
    )(i)
}

fn parse_alloc(i: &str) -> MyParseResult<ParsedInstruction> {
    map(
        tuple((
            preceded(spaces_around(tag("alloc")), cut(parse_var_idx)),
            preceded(spaces_around(tag(",")), cut(parse_type_idx)),
        )),
        |(r, t)| ParsedInstruction::Instr(Instruction::Alloc(r, t)),
    )(i)
}

fn parse_call(i: &str) -> MyParseResult<ParsedInstruction> {
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

fn parse_jmp(i: &str) -> MyParseResult<ParsedInstruction> {
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

fn parse_lbl_instr(i: &str) -> MyParseResult<ParsedInstruction> {
    map(terminated(parse_identifier, tag(":")), |lbl| {
        ParsedInstruction::Lbl(lbl)
    })(i)
}

/*
* File
*/

fn parse_param(i: &str) -> MyParseResult<Param> {
    map(
        tuple((parse_var_idx, cut(spaces_around(tag(":"))), parse_type)),
        |(i, _, t)| Param { var: i, type_: t },
    )(i)
}

fn parse_params(i: &str) -> MyParseResult<Vec<Param>> {
    delimited(
        spaces_around(tag("(")),
        separated_list0(spaces_around(tag(",")), parse_param),
        spaces_around(tag(")")),
    )(i)
}

fn parse_function(i: &str) -> MyParseResult<ParsedFunction> {
    map(
        tuple((
            ws(tag("fn")),
            cut(parse_identifier),
            ws(parse_params),
            preceded(ws(tag("->")), parse_type),
            preceded(multispace0, many1(parse_instruction)),
            multispace0,
            tag("endfn"),
        )),
        |(_, name, parameters, return_type, instructions, _, _)| {
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
                        Instruction::LoadVar(a, b, _) => {
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
                        Instruction::StoreVar(a, b, _) => {
                            vars = Var::max(vars, *a);
                            vars = Var::max(vars, *b);
                        }
                        Instruction::Alloc(i, _) => {
                            vars = Var::max(vars, *i);
                        }
                        _ => {}
                    },
                    ParsedInstruction::IndirectCall(i, _, _) => {
                        vars = Var::max(vars, *i);
                    }
                    _ => {}
                }
            }

            ParsedFunction::new(name, vars + 1, instructions)
        },
    )(i)
}

fn parse_const_section(i: &str) -> MyParseResult<Vec<ConstDecl>> {
    map(
        opt(preceded(
            ws(tag(".const")),
            many0(map(
                tuple((parse_u64, tag(":"), ws(parse_type), ws(parse_constant))),
                |(idx, _, type_, value)| ConstDecl {
                    idx: std::convert::TryInto::try_into(idx).unwrap(),
                    type_,
                    value,
                },
            )),
        )),
        |r| match r {
            None => Vec::new(),
            Some(r) => r,
        },
    )(i)
}

fn parse_types_section(i: &str) -> MyParseResult<Vec<TypeDecl>> {
    map(
        opt(preceded(
            ws(tag(".types")),
            many0(map(
                tuple((parse_u64, tag(":"), ws(parse_type))),
                |(idx, _, type_)| TypeDecl {
                    idx: std::convert::TryInto::try_into(idx).unwrap(),
                    type_,
                },
            )),
        )),
        |r| match r {
            None => Vec::new(),
            Some(r) => r,
        },
    )(i)
}

fn parse_code_section(i: &str) -> MyParseResult<Vec<ParsedFunction>> {
    preceded(ws(tag(".code")), many1(parse_function))(i)
}

pub fn parse_bytecode_string(i: &str) -> Module {
    match map(
        tuple((parse_types_section, parse_const_section, parse_code_section)),
        |(t, c, f)| Module::from_long_functions(t, c, f),
    )(i)
    {
        Ok((leftover, m)) => {
            if leftover.len() > 0 {
                panic!("Didn't parse entire file: {}", leftover);
            }

            m
        }
        Err(nom::Err::Failure(err)) | Err(nom::Err::Error(err)) => {
            panic!("{}", convert_error(i, err));
        }
        Err(err) => {
            panic!("{}", err);
        }
    }
}

pub fn parse_bytecode_file(filename: &str) -> Option<Module> {
    match fs::read_to_string(filename) {
        Ok(f) => Some(parse_bytecode_string(f.as_str())),
        Err(_) => None,
    }
}
