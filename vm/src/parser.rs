extern crate nom;
use crate::bytecode::{
    ConstId, LongInstruction, Module, ModuleLoader, Param, LongConstDecl, LongConstant,
    LongFunction, Type, TypeDecl, VarId,
};
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

/*
* Parser logic
*/

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

fn parse_u8(i: &str) -> MyParseResult<u8> {
    map(digit1, |s: &str| s.parse::<u8>().unwrap())(i)
}

fn parse_var_idx(i: &str) -> MyParseResult<VarId> {
    preceded(tag("$"), cut(parse_u8))(i)
}

fn parse_identifier(i: &str) -> MyParseResult<String> {
    map(take_while1(|c| char::is_alphanumeric(c) || c == '_'), |s| {
        String::from(s)
    })(i)
}

fn parse_lbl(i: &str) -> MyParseResult<String> {
    preceded(spaces_around(tag("@")), parse_identifier)(i)
}

fn parse_call_target(i: &str) -> MyParseResult<(Option<String>, String)> {
    preceded(
        tag("@"),
        pair(
            opt(terminated(parse_identifier, tag(":"))),
            parse_identifier,
        ),
    )(i)
}

/*
* Types
*/

fn parse_type(i: &str) -> MyParseResult<Type> {
    alt((
        map(tag("ui64"), |_| Type::U64),
        map(delimited(tag("["), cut(parse_type), cut(tag("]"))), |t| {
            Type::unsized_array(t.clone())
        }),
        map(tag("()"), |_| Type::Unit),
        map(tag("str"), |_| Type::unsized_string()),
        map(preceded(tag("&"), cut(parse_type)), |t| Type::ptr(t)),
    ))(i)
}

/*
* Expressions
*/

fn parse_constant(i: &str) -> MyParseResult<LongConstant> {
    preceded(
        space0,
        alt((
            map(parse_u64, |n| LongConstant::U64(n)),
            parse_boolean,
            parse_string,
        )),
    )(i)
}

fn parse_boolean(i: &str) -> MyParseResult<LongConstant> {
    alt((
        map(tag("true"), |_| LongConstant::U64(1)),
        map(tag("false"), |_| LongConstant::U64(0)),
    ))(i)
}

fn parse_string(i: &str) -> MyParseResult<LongConstant> {
    map(
        delimited(tag("\""), take_while(|c| c != '\"'), tag("\"")),
        |r| LongConstant::Str(String::from(r)),
    )(i)
}

/*
* Statements
*/

fn parse_three_var_command<'a>(
    op: &'a str,
) -> impl FnMut(&'a str) -> MyParseResult<(VarId, VarId, VarId)> {
    move |i| {
        tuple((
            preceded(tag(op), spaces_around(parse_var_idx)),
            preceded(tag(","), spaces_around(parse_var_idx)),
            preceded(tag(","), spaces_around(parse_var_idx)),
        ))(i)
    }
}

fn parse_two_var_command<'a>(op: &'a str) -> impl FnMut(&'a str) -> MyParseResult<(VarId, VarId)> {
    move |i| {
        tuple((
            preceded(tag(op), spaces_around(parse_var_idx)),
            preceded(tag(","), spaces_around(parse_var_idx)),
        ))(i)
    }
}

fn parse_two_var_one_type_command<'a>(
    op: &'a str,
) -> impl FnMut(&'a str) -> MyParseResult<(VarId, VarId, Type)> {
    move |i| {
        tuple((
            preceded(tag(op), spaces_around(parse_var_idx)),
            preceded(tag(","), spaces_around(parse_var_idx)),
            preceded(tag(","), spaces_around(parse_type)),
        ))(i)
    }
}

fn parse_one_var_command<'a>(op: &'a str) -> impl FnMut(&'a str) -> MyParseResult<VarId> {
    move |i| preceded(tag(op), spaces_around(parse_var_idx))(i)
}

fn parse_instruction(i: &str) -> MyParseResult<LongInstruction> {
    terminated(
        alt((
            map(parse_two_var_one_type_command("store"), |(l, e, t)| {
                LongInstruction::StoreVar(l, e, t)
            }),
            map(parse_two_var_one_type_command("load"), |(l, e, t)| {
                LongInstruction::LoadVar(l, e, t)
            }),
            map(parse_three_var_command("eq"), |(a, b, c)| {
                LongInstruction::EqVarVar(a, b, c)
            }),
            map(parse_three_var_command("lt"), |(a, b, c)| {
                LongInstruction::LtVarVar(a, b, c)
            }),
            map(parse_three_var_command("sub"), |(l, e1, e2)| {
                LongInstruction::SubVarVar(l, e1, e2)
            }),
            map(parse_three_var_command("add"), |(l, e1, e2)| {
                LongInstruction::AddVarVar(l, e1, e2)
            }),
            map(parse_two_var_command("neg"), |(l, e)| {
                LongInstruction::NotVar(l, e)
            }),
            map(parse_two_var_command("cp"), |(l, e)| {
                LongInstruction::Copy(l, e)
            }),
            map(
                pair(
                    parse_three_var_command("idx"),
                    preceded(tag(","), spaces_around(parse_type)),
                ),
                |((r, a, i), t)| LongInstruction::Index(r, a, i, t),
            ),
            map(
                tuple((
                    preceded(spaces_around(tag("movc")), cut(parse_var_idx)),
                    preceded(spaces_around(tag(",")), parse_u32),
                )),
                |(a, b)| LongInstruction::ConstU32(a, b),
            ),
            map(
                tuple((
                    preceded(spaces_around(tag("mov")), cut(parse_var_idx)),
                    preceded(spaces_around(tag(",")), parse_var_idx),
                )),
                |(a, b)| LongInstruction::Copy(a, b),
            ),
            map(
                tuple((
                    preceded(spaces_around(tag("loadc")), cut(parse_var_idx)),
                    preceded(spaces_around(tag(",")), parse_u32),
                )),
                |(a, b)| LongInstruction::LoadConst(a, b as ConstId),
            ),
            map(parse_one_var_command("ret"), |e| LongInstruction::Return(e)),
            map(parse_one_var_command("print"), |e| {
                LongInstruction::Print(e)
            }),
            parse_call,
            parse_alloc,
            parse_jmp,
            parse_lbl_instr,
        )),
        multispace0,
    )(i)
}

fn parse_alloc(i: &str) -> MyParseResult<LongInstruction> {
    map(
        tuple((
            preceded(spaces_around(tag("alloc")), cut(parse_var_idx)),
            preceded(spaces_around(tag(",")), cut(parse_type)),
        )),
        |(r, t)| LongInstruction::Alloc(r, t),
    )(i)
}

fn parse_call(i: &str) -> MyParseResult<LongInstruction> {
    map(
        tuple((
            preceded(spaces_around(tag("call")), cut(parse_var_idx)),
            preceded(spaces_around(tag(",")), cut(parse_call_target)),
            space0,
            delimited(
                tag("("),
                separated_list0(spaces_around(tag(",")), parse_var_idx),
                tag(")"),
            ),
        )),
        |(r, target, _, args)| match target.0 {
            Some(module) => LongInstruction::ModuleCall(r, module, target.1, args[0]),
            None => LongInstruction::Call(r, target.1, args[0]),
        },
    )(i)
}

fn parse_jmp(i: &str) -> MyParseResult<LongInstruction> {
    alt((
        map(
            preceded(
                spaces_around(tag("jmpifn")),
                pair(cut(spaces_around(parse_var_idx)), cut(parse_lbl)),
            ),
            |(v, l)| LongInstruction::JmpIfNot(l, v),
        ),
        map(
            preceded(
                spaces_around(tag("jmpif")),
                pair(cut(spaces_around(parse_var_idx)), cut(parse_lbl)),
            ),
            |(v, l)| LongInstruction::JmpIf(l, v),
        ),
        map(preceded(spaces_around(tag("jmp")), cut(parse_lbl)), |l| {
            LongInstruction::Jmp(l)
        }),
    ))(i)
}

fn parse_lbl_instr(i: &str) -> MyParseResult<LongInstruction> {
    map(terminated(parse_identifier, tag(":")), |lbl| {
        LongInstruction::Lbl(lbl)
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

fn parse_function(i: &str) -> MyParseResult<LongFunction> {
    map(
        preceded(
            ws(tag("fn")),
            cut(tuple((
                cut(parse_identifier),
                ws(parse_params),
                preceded(ws(tag("->")), parse_type),
                preceded(multispace0, many1(parse_instruction)),
                multispace0,
                tag("endfn"),
            ))),
        ),
        |(name, parameters, return_type, instructions, _, _)| {
            /*
             * Compute local variables frame size
             */

            let mut vars = 0;

            for par in parameters.iter() {
                vars = VarId::max(vars, par.var);
            }
            let arg_types = parameters.into_iter().map(|a| a.type_).collect();

            for instr in instructions.iter() {
                match instr {
                    LongInstruction::ConstU32(i, _) => {
                        vars = VarId::max(vars, *i);
                    }
                    LongInstruction::Copy(i, _) => {
                        vars = VarId::max(vars, *i);
                    }
                    LongInstruction::EqVarVar(a, b, c) => {
                        vars = VarId::max(vars, *a);
                        vars = VarId::max(vars, *b);
                        vars = VarId::max(vars, *c);
                    }
                    LongInstruction::LtVarVar(a, b, c) => {
                        vars = VarId::max(vars, *a);
                        vars = VarId::max(vars, *b);
                        vars = VarId::max(vars, *c);
                    }
                    LongInstruction::SubVarVar(a, b, c) => {
                        vars = VarId::max(vars, *a);
                        vars = VarId::max(vars, *b);
                        vars = VarId::max(vars, *c);
                    }
                    LongInstruction::AddVarVar(a, b, c) => {
                        vars = VarId::max(vars, *a);
                        vars = VarId::max(vars, *b);
                        vars = VarId::max(vars, *c);
                    }
                    LongInstruction::NotVar(a, b) => {
                        vars = VarId::max(vars, *a);
                        vars = VarId::max(vars, *b);
                    }
                    LongInstruction::LoadVar(a, b, _) => {
                        vars = VarId::max(vars, *a);
                        vars = VarId::max(vars, *b);
                    }
                    LongInstruction::LoadConst(i, _) => {
                        vars = VarId::max(vars, *i);
                    }
                    LongInstruction::Index(a, b, c, _) => {
                        vars = VarId::max(vars, *a);
                        vars = VarId::max(vars, *b);
                        vars = VarId::max(vars, *c);
                    }
                    LongInstruction::StoreVar(a, b, _) => {
                        vars = VarId::max(vars, *a);
                        vars = VarId::max(vars, *b);
                    }
                    LongInstruction::Alloc(i, _) => {
                        vars = VarId::max(vars, *i);
                    }
                    LongInstruction::Call(i, _, _) => {
                        vars = VarId::max(vars, *i);
                    }
                    LongInstruction::ModuleCall(i, _, _, _) => {
                        vars = VarId::max(vars, *i);
                    }
                    LongInstruction::Return(_)
                    | LongInstruction::Print(_)
                    | LongInstruction::Jmp(_)
                    | LongInstruction::JmpIf(_, _)
                    | LongInstruction::JmpIfNot(_, _)
                    | LongInstruction::Lbl(_) => {}
                }
            }

            LongFunction::new(
                name,
                vars + 1,
                instructions,
                Type::Fn(Box::from(Type::Tuple(arg_types)), Box::from(return_type)),
            )
        },
    )(i)
}

/*
* Top level sections:
* - module declaration
* - types declarations
* - const declarations
* - function declarations
*/

fn parse_module_declaration(i: &str) -> MyParseResult<String> {
    whitespace_around(preceded(tag("mod"), spaces_around(parse_identifier)))(i)
}

fn parse_const_section(i: &str) -> MyParseResult<Vec<LongConstDecl>> {
    map(
        opt(preceded(
            ws(tag(".const")),
            many0(map(
                tuple((parse_u64, tag(":"), ws(parse_type), ws(parse_constant))),
                |(idx, _, type_, value)| LongConstDecl {
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

fn parse_code_section(i: &str) -> MyParseResult<Vec<LongFunction>> {
    preceded(
        ws(tag(".code")),
        many1(preceded(multispace0, parse_function)),
    )(i)
}

pub fn parse_bytecode_string(module_loader: &ModuleLoader, i: &str) -> Module {
    match map(
        whitespace_around(tuple((
            parse_module_declaration,
            opt(parse_types_section),
            opt(parse_const_section),
            opt(parse_code_section),
        ))),
        |(name, types, consts, functions)| {
            Module::from_long_functions(
                module_loader,
                name,
                types.unwrap_or(vec![]),
                consts.unwrap_or(vec![]),
                functions.unwrap_or(vec![]),
            )
        },
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

pub fn parse_bytecode_file(module_loader: &ModuleLoader, filename: &str) -> Option<Module> {
    match fs::read_to_string(filename) {
        Ok(f) => Some(parse_bytecode_string(module_loader, f.as_str())),
        Err(_) => None,
    }
}
