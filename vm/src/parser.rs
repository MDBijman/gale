extern crate nom;
use crate::bytecode::{
    ConstDecl, ConstId, Constant, LongFunction, LongInstruction, Module, ModuleLoader, Param, Type,
    TypeDecl, VarId,
};
use nom::{
    branch::alt,
    bytes::complete::{tag, take_while, take_while1},
    character::complete::{char, digit1, multispace0, space0},
    combinator::{cut, map, opt},
    error::{convert_error, ParseError, VerboseError},
    multi::{many0, many1, separated_list0, separated_list1},
    sequence::{delimited, pair, preceded, separated_pair, terminated, tuple},
    IResult,
};
use std::{fmt::Display, fs};

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

fn parse_bool(i: &str) -> MyParseResult<bool> {
    alt((
        map(tag("true"), |_| true),
        map(tag("false"), |_| false)
    ))(i)
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
        map(
            delimited(
                ws(char('(')),
                separated_pair(parse_type, ws(tag("->")), parse_type),
                ws(char(')')),
            ),
            |(i, o)| Type::Fn(Box::from(i), Box::from(o)),
        ),
        map(
            delimited(
                ws(char('(')),
                separated_list1(ws(char(',')), parse_type),
                ws(char(')')),
            ),
            |t| Type::Tuple(t),
        ),
    ))(i)
}

/*
* Expressions
*/

fn parse_constant(i: &str) -> MyParseResult<Constant> {
    preceded(
        space0,
        alt((
            map(parse_u64, |n| Constant::U64(n)),
            parse_boolean,
            parse_string,
        )),
    )(i)
}

fn parse_boolean(i: &str) -> MyParseResult<Constant> {
    alt((
        map(tag("true"), |_| Constant::U64(1)),
        map(tag("false"), |_| Constant::U64(0)),
    ))(i)
}

fn parse_string(i: &str) -> MyParseResult<Constant> {
    map(
        delimited(tag("\""), take_while(|c| c != '\"'), tag("\"")),
        |r| Constant::Str(String::from(r)),
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
            map(parse_three_var_command("mul"), |(l, e1, e2)| {
                LongInstruction::MulVarVar(l, e1, e2)
            }),
            map(parse_two_var_command("neg"), |(l, e)| {
                LongInstruction::NotVar(l, e)
            }),
            map(parse_two_var_command("cp"), |(l, e)| {
                LongInstruction::Copy(l, e)
            }),
            map(parse_two_var_command("sizeof"), |(l, e)| {
                LongInstruction::Sizeof(l, e)
            }),
            map(spaces_around(tag("panic!")), |_| LongInstruction::Panic),
            map(
                pair(
                    parse_three_var_command("idx"),
                    preceded(tag(","), spaces_around(parse_type)),
                ),
                |((r, a, i), t)| LongInstruction::Index(r, a, i, t),
            ),
            map(
                tuple((
                    preceded(spaces_around(tag("ui32")), cut(parse_var_idx)),
                    preceded(spaces_around(tag(",")), parse_u32),
                )),
                |(a, b)| LongInstruction::ConstU32(a, b),
            ),
            map(
                tuple((
                    preceded(spaces_around(tag("bool")), cut(parse_var_idx)),
                    preceded(spaces_around(tag(",")), parse_bool),
                )),
                |(a, b)| LongInstruction::ConstBool(a, b),
            ),
            alt((
                map(
                    tuple((
                        preceded(spaces_around(tag("movi")), cut(parse_var_idx)),
                        preceded(spaces_around(tag(",")), cut(parse_u8)),
                        preceded(spaces_around(tag(",")), parse_var_idx),
                    )),
                    |(a, b, c)| LongInstruction::CopyIntoIndex(a, b, c),
                ),
                map(
                    tuple((
                        preceded(spaces_around(tag("movi")), cut(parse_var_idx)),
                        preceded(spaces_around(tag(",")), cut(parse_u8)),
                        preceded(spaces_around(tag(",")), cut(parse_call_target)),
                    )),
                    |(a, b, (c, d))| LongInstruction::CopyAddressIntoIndex(a, b, c, d),
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
                        preceded(spaces_around(tag("mov")), cut(parse_var_idx)),
                        preceded(spaces_around(tag(",")), parse_call_target),
                    )),
                    |(a, (b, c))| LongInstruction::CopyAddress(a, b, c),
                ),
                map(
                    tuple((
                        preceded(spaces_around(tag("tup")), cut(parse_var_idx)),
                        preceded(spaces_around(tag(",")), cut(parse_u8)),
                    )),
                    |(a, b)| LongInstruction::Tuple(a, b),
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
    alt((
        // Call target embedded directly
        map(
            tuple((
                preceded(spaces_around(tag("call")), cut(parse_var_idx)),
                preceded(spaces_around(tag(",")), parse_call_target),
                space0,
                delimited(
                    tag("("),
                    separated_list0(spaces_around(tag(",")), parse_var_idx),
                    tag(")"),
                ),
            )),
            |(r, target, _, args)| match target.0 {
                Some(module) => LongInstruction::ModuleCall(r, module, target.1, args),
                None => LongInstruction::Call(r, target.1, args),
            },
        ),
        // Call target stored in variable
        map(
            tuple((
                preceded(spaces_around(tag("call")), cut(parse_var_idx)),
                preceded(spaces_around(tag(",")), cut(parse_var_idx)),
                space0,
                delimited(
                    tag("("),
                    separated_list0(spaces_around(tag(",")), parse_var_idx),
                    tag(")"),
                ),
            )),
            |(r, target, _, args)| LongInstruction::VarCall(r, target, args),
        ),
    ))(i)
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
                    LongInstruction::ConstBool(i, _) => {
                        vars = VarId::max(vars, *i);
                    }
                    LongInstruction::Copy(i, _) => {
                        vars = VarId::max(vars, *i);
                    }
                    LongInstruction::CopyAddress(a, _, _) => {
                        vars = VarId::max(vars, *a);
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
                    LongInstruction::MulVarVar(a, b, c) => {
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
                    LongInstruction::CopyIntoIndex(a, _, c) => {
                        vars = VarId::max(vars, *a);
                        vars = VarId::max(vars, *c);
                    }
                    LongInstruction::CopyAddressIntoIndex(a, _, _, _) => {
                        vars = VarId::max(vars, *a);
                    }
                    LongInstruction::Tuple(a, _) => {
                        vars = VarId::max(vars, *a);
                    }
                    LongInstruction::Sizeof(a, b) => {
                        vars = VarId::max(vars, *a);
                        vars = VarId::max(vars, *b);
                    }
                    LongInstruction::StoreVar(a, b, _) => {
                        vars = VarId::max(vars, *a);
                        vars = VarId::max(vars, *b);
                    }
                    LongInstruction::Alloc(i, _) => {
                        vars = VarId::max(vars, *i);
                    }
                    LongInstruction::Call(a, _, b) => {
                        vars = VarId::max(vars, *a);
                        for v in b {
                            vars = VarId::max(vars, *v);
                        }
                    }
                    LongInstruction::VarCall(a, b, c) => {
                        vars = VarId::max(vars, *a);
                        vars = VarId::max(vars, *b);
                        for v in c {
                            vars = VarId::max(vars, *v);
                        }
                    }
                    LongInstruction::ModuleCall(i, _, _, _) => {
                        vars = VarId::max(vars, *i);
                    }
                    LongInstruction::Return(_)
                    | LongInstruction::Print(_)
                    | LongInstruction::Jmp(_)
                    | LongInstruction::JmpIf(_, _)
                    | LongInstruction::JmpIfNot(_, _)
                    | LongInstruction::Lbl(_)
                    | LongInstruction::Panic => {}
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

enum Section {
    Constants(Vec<ConstDecl>),
    Types(Vec<TypeDecl>),
    Functions(Vec<LongFunction>),
}

fn parse_const_section(i: &str) -> MyParseResult<Section> {
    map(
        preceded(
            ws(tag(".const")),
            many0(map(
                tuple((parse_u64, tag(":"), ws(parse_type), ws(parse_constant))),
                |(idx, _, type_, value)| ConstDecl {
                    idx: std::convert::TryInto::try_into(idx).unwrap(),
                    type_,
                    value,
                },
            )),
        ),
        |r| Section::Constants(r),
    )(i)
}

fn parse_types_section(i: &str) -> MyParseResult<Section> {
    map(
        preceded(
            ws(tag(".types")),
            many0(map(
                tuple((parse_u64, tag(":"), ws(parse_type))),
                |(idx, _, type_)| TypeDecl {
                    idx: std::convert::TryInto::try_into(idx).unwrap(),
                    type_,
                },
            )),
        ),
        |r| Section::Types(r),
    )(i)
}

fn parse_code_section(i: &str) -> MyParseResult<Section> {
    map(
        preceded(
            ws(tag(".code")),
            many0(preceded(multispace0, parse_function)),
        ),
        |fs| Section::Functions(fs),
    )(i)
}

fn parse_section(i: &str) -> MyParseResult<Section> {
    alt((parse_const_section, parse_types_section, parse_code_section))(i)
}

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

pub fn parse_bytecode_string(module_loader: &ModuleLoader, i: &str) -> Result<Module, ParserError> {
    match map(
        whitespace_around(tuple((parse_module_declaration, many0(parse_section)))),
        |(name, sections)| {
            let mut types = vec![];
            let mut consts = vec![];
            let mut functions = vec![];

            for section in sections {
                match section {
                    Section::Functions(fs) => functions.extend(fs.into_iter()),
                    Section::Constants(cs) => consts.extend(cs.into_iter()),
                    Section::Types(ts) => types.extend(ts.into_iter()),
                }
            }

            Module::from_long_functions(module_loader, name, types, consts, functions)
        },
    )(i)
    {
        Ok((leftover, m)) => {
            if leftover.len() > 0 {
                return Err(ParserError::ParseFailure(format!(
                    "Didn't parse entire file: {}",
                    leftover
                )));
            }

            Ok(m)
        }
        Err(nom::Err::Failure(err)) | Err(nom::Err::Error(err)) => Err(ParserError::ParseFailure(
            format!("{}", convert_error(i, err)),
        )),
        Err(err) => Err(ParserError::ParseFailure(format!("{}", err))),
    }
}

pub fn parse_bytecode_file(
    module_loader: &ModuleLoader,
    filename: &str,
) -> Result<Module, ParserError> {
    match fs::read_to_string(filename) {
        Ok(f) => parse_bytecode_string(module_loader, f.as_str()),
        Err(_) => Err(ParserError::FileNotFound()),
    }
}
