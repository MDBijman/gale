extern crate nom;
use crate::bytecode::{ConstDecl, ConstId, Module, Param, Type, TypeDecl, Value, VarId, ModuleLoader};
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

#[derive(Debug, Clone)]
pub enum ParsedInstruction {
    ConstU32(VarId, u32),
    Copy(VarId, VarId),
    EqVarVar(VarId, VarId, VarId),
    LtVarVar(VarId, VarId, VarId),
    SubVarVar(VarId, VarId, VarId),
    AddVarVar(VarId, VarId, VarId),
    NotVar(VarId, VarId),

    Return(VarId),
    Print(VarId),
    Call(VarId, String, VarId),
    ModuleCall(VarId, String, String, VarId),
    Jmp(String),
    JmpIf(String, VarId),
    JmpIfNot(String, VarId),

    Alloc(VarId, Type),
    LoadConst(VarId, ConstId),
    LoadVar(VarId, VarId, Type),
    StoreVar(VarId, VarId, Type),
    Index(VarId, VarId, VarId, Type),

    Lbl(String),
}

impl Into<crate::bytecode::Instruction> for ParsedInstruction {
    fn into(self) -> crate::bytecode::Instruction {
        use crate::bytecode::Instruction;
        use ParsedInstruction::*;

        match self {
            ConstU32(a, b) => Instruction::ConstU32(a, b),
            Copy(a, b) => Instruction::Copy(a, b),
            EqVarVar(a, b, c) => Instruction::EqVarVar(a, b, c),
            LtVarVar(a, b, c) => Instruction::LtVarVar(a, b, c),
            SubVarVar(a, b, c) => Instruction::SubVarVar(a, b, c),
            AddVarVar(a, b, c) => Instruction::AddVarVar(a, b, c),
            NotVar(a, b) => Instruction::NotVar(a, b),
            Return(v) => Instruction::Return(v),
            Print(v) => Instruction::Print(v),
            LoadConst(a, b) => Instruction::LoadConst(a, b),
            s => panic!(
                "This instruction variant cannot be converted automatically: {:?}",
                s
            ),
        }
    }
}

#[derive(Debug, Clone)]
pub struct ParsedFunction {
    pub name: String,
    pub varcount: u8,
    pub instructions: Vec<ParsedInstruction>,
    pub type_: Type,
}

impl ParsedFunction {
    pub fn new(
        name: String,
        varcount: u8,
        instructions: Vec<ParsedInstruction>,
        type_: Type,
    ) -> Self {
        Self {
            name,
            varcount,
            instructions,
            type_,
        }
    }
}

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
    preceded(tag("@"), pair(opt(terminated(parse_identifier, tag(":"))), parse_identifier))(i)
}

/*
* Types
*/

fn parse_type(i: &str) -> MyParseResult<Type> {
    alt((
        map(tag("ui64"), |_| Type::U64),
        map(delimited(tag("["), parse_type, tag("]")), |t| {
            Type::Array(Box::new(t))
        }),
        map(tag("()"), |_| Type::Unit),
        map(tag("str"), |_| Type::Str),
        map(preceded(tag("&"), parse_type), |t| {
            Type::Pointer(Box::from(t))
        }),
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

fn parse_instruction(i: &str) -> MyParseResult<ParsedInstruction> {
    terminated(
        alt((
            map(parse_two_var_one_type_command("store"), |(l, e, t)| {
                ParsedInstruction::StoreVar(l, e, t)
            }),
            map(parse_two_var_one_type_command("load"), |(l, e, t)| {
                ParsedInstruction::LoadVar(l, e, t)
            }),
            map(parse_three_var_command("eq"), |(a, b, c)| {
                ParsedInstruction::EqVarVar(a, b, c)
            }),
            map(parse_three_var_command("lt"), |(a, b, c)| {
                ParsedInstruction::LtVarVar(a, b, c)
            }),
            map(parse_three_var_command("sub"), |(l, e1, e2)| {
                ParsedInstruction::SubVarVar(l, e1, e2)
            }),
            map(parse_three_var_command("add"), |(l, e1, e2)| {
                ParsedInstruction::AddVarVar(l, e1, e2)
            }),
            map(parse_two_var_command("neg"), |(l, e)| {
                ParsedInstruction::NotVar(l, e)
            }),
            map(parse_two_var_command("cp"), |(l, e)| {
                ParsedInstruction::Copy(l, e)
            }),
            map(
                pair(
                    parse_three_var_command("idx"),
                    preceded(tag(","), spaces_around(parse_type)),
                ),
                |((r, a, i), t)| ParsedInstruction::Index(r, a, i, t),
            ),
            map(
                tuple((
                    preceded(spaces_around(tag("movc")), cut(parse_var_idx)),
                    preceded(spaces_around(tag(",")), parse_u32),
                )),
                |(a, b)| ParsedInstruction::ConstU32(a, b),
            ),
            map(
                tuple((
                    preceded(spaces_around(tag("mov")), cut(parse_var_idx)),
                    preceded(spaces_around(tag(",")), parse_var_idx),
                )),
                |(a, b)| ParsedInstruction::Copy(a, b),
            ),
            map(
                tuple((
                    preceded(spaces_around(tag("loadc")), cut(parse_var_idx)),
                    preceded(spaces_around(tag(",")), parse_u32),
                )),
                |(a, b)| ParsedInstruction::LoadConst(a, b as ConstId),
            ),
            map(parse_one_var_command("ret"), |e| {
                ParsedInstruction::Return(e)
            }),
            map(parse_one_var_command("print"), |e| {
                ParsedInstruction::Print(e)
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
            preceded(spaces_around(tag(",")), cut(parse_type)),
        )),
        |(r, t)| ParsedInstruction::Alloc(r, t),
    )(i)
}

fn parse_call(i: &str) -> MyParseResult<ParsedInstruction> {
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
            Some(module) => ParsedInstruction::ModuleCall(r, module, target.1, args[0]),
            None => ParsedInstruction::Call(r, target.1, args[0]),
        },
    )(i)
}

fn parse_jmp(i: &str) -> MyParseResult<ParsedInstruction> {
    alt((
        map(
            preceded(
                spaces_around(tag("jmpifn")),
                pair(cut(spaces_around(parse_var_idx)), cut(parse_lbl)),
            ),
            |(v, l)| ParsedInstruction::JmpIfNot(l, v),
        ),
        map(
            preceded(
                spaces_around(tag("jmpif")),
                pair(cut(spaces_around(parse_var_idx)), cut(parse_lbl)),
            ),
            |(v, l)| ParsedInstruction::JmpIf(l, v),
        ),
        map(preceded(spaces_around(tag("jmp")), cut(parse_lbl)), |l| {
            ParsedInstruction::Jmp(l)
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
                    ParsedInstruction::ConstU32(i, _) => {
                        vars = VarId::max(vars, *i);
                    }
                    ParsedInstruction::Copy(i, _) => {
                        vars = VarId::max(vars, *i);
                    }
                    ParsedInstruction::EqVarVar(a, b, c) => {
                        vars = VarId::max(vars, *a);
                        vars = VarId::max(vars, *b);
                        vars = VarId::max(vars, *c);
                    }
                    ParsedInstruction::LtVarVar(a, b, c) => {
                        vars = VarId::max(vars, *a);
                        vars = VarId::max(vars, *b);
                        vars = VarId::max(vars, *c);
                    }
                    ParsedInstruction::SubVarVar(a, b, c) => {
                        vars = VarId::max(vars, *a);
                        vars = VarId::max(vars, *b);
                        vars = VarId::max(vars, *c);
                    }
                    ParsedInstruction::AddVarVar(a, b, c) => {
                        vars = VarId::max(vars, *a);
                        vars = VarId::max(vars, *b);
                        vars = VarId::max(vars, *c);
                    }
                    ParsedInstruction::NotVar(a, b) => {
                        vars = VarId::max(vars, *a);
                        vars = VarId::max(vars, *b);
                    }
                    ParsedInstruction::LoadVar(a, b, _) => {
                        vars = VarId::max(vars, *a);
                        vars = VarId::max(vars, *b);
                    }
                    ParsedInstruction::LoadConst(i, _) => {
                        vars = VarId::max(vars, *i);
                    }
                    ParsedInstruction::Index(a, b, c, _) => {
                        vars = VarId::max(vars, *a);
                        vars = VarId::max(vars, *b);
                        vars = VarId::max(vars, *c);
                    }
                    ParsedInstruction::StoreVar(a, b, _) => {
                        vars = VarId::max(vars, *a);
                        vars = VarId::max(vars, *b);
                    }
                    ParsedInstruction::Alloc(i, _) => {
                        vars = VarId::max(vars, *i);
                    }
                    ParsedInstruction::Call(i, _, _) => {
                        vars = VarId::max(vars, *i);
                    }
                    _ => {}
                }
            }

            ParsedFunction::new(
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
    preceded(
        ws(tag(".code")),
        many1(preceded(multispace0, parse_function)),
    )(i)
}

pub fn parse_bytecode_string(module_loader: &ModuleLoader, i: &str) -> Module {
    match map(
        whitespace_around(tuple((
            parse_module_declaration,
            parse_types_section,
            parse_const_section,
            parse_code_section,
        ))),
        |(name, types, consts, functions)| {
            Module::from_long_functions(module_loader, name, types, consts, functions)
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
