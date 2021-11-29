extern crate aterms;
use aterms::base::*;
use nom::{
    branch::alt,
    bytes::complete::{tag, take, take_while, take_while1},
    character::complete::{anychar, char, digit1, multispace0, none_of},
    combinator::{all_consuming, cut, map, map_opt, map_res, opt, verify},
    error::{context, convert_error, ParseError, VerboseError},
    multi::{fold_many0, many0, many1, separated_list0, separated_list1},
    number::complete::double,
    sequence::{delimited, pair, preceded, separated_pair, terminated, tuple},
    IResult, Parser,
};

use std::fs;

/*
* Parser helpers
*/

type MyParseResult<'a, O> = IResult<&'a str, O, VerboseError<&'a str>>;

fn ws<'a, O, E: ParseError<&'a str>, F: Parser<&'a str, O, E>>(f: F) -> impl Parser<&'a str, O, E> {
    delimited(multispace0, f, multispace0)
}

fn u16_hex(i: &str) -> MyParseResult<u16> {
    map_res(take(4usize), |s| u16::from_str_radix(s, 16))(i)
}

fn unicode_escape(input: &str) -> MyParseResult<char> {
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

fn character(input: &str) -> MyParseResult<char> {
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

/*
* Parsers
*/

pub fn parse_name(input: &str) -> MyParseResult<String> {
    verify(
        map(
            separated_list1(
                tag(":"),
                take_while1(|c| char::is_alphanumeric(c) || c == '_'),
            ),
            |s: Vec<&str>| {
                s.into_iter()
                    .map(|s| String::from(s))
                    .collect::<Vec<String>>()
                    .join(":")
            },
        ),
        |s: &str| {
            s != "in"
                && s != "let"
                && s != "match"
                && s != "for"
                && s != "if"
                && s != "as"
                && s != "use"
                && s != "import"
        },
    )(input)
}

fn parse_i64(i: &str) -> MyParseResult<Term> {
    map(digit1, |s: &str| {
        Term::new_rec_term(
            "Int",
            vec![Term::new_number_term(s.parse::<i64>().unwrap() as f64)],
        )
    })(i)
}

fn parse_double(i: &str) -> MyParseResult<Term> {
    map(double, |d| {
        Term::new_rec_term("Double", vec![Term::new_number_term(d)])
    })(i)
}

fn parse_number(i: &str) -> MyParseResult<Term> {
    context("number", alt((parse_i64, parse_double)))(i)
}

fn parse_ref(i: &str) -> MyParseResult<Term> {
    map(context("name", ws(parse_name)), |name: String| {
        Term::new_rec_term("Ref", vec![Term::new_string_term(name.as_str())])
    })(i)
}

fn parse_assign(i: &str) -> MyParseResult<Term> {
    map(
        context("assign", tuple((ws(parse_ref), ws(tag("=")), parse_expr))),
        |(name, _, res)| Term::new_rec_term("Assign", vec![name, res]),
    )(i)
}

pub fn parse_string(input: &str) -> MyParseResult<Term> {
    let (input, t) = delimited(
        char('"'),
        fold_many0(character, String::new(), |mut string, c| {
            string.push(c);
            string
        }),
        char('"'),
    )(input)?;
    Ok((
        input,
        Term::new_rec_term("String", vec![Term::new_string_term(&t)]),
    ))
}

fn parse_type_array(input: &str) -> MyParseResult<Term> {
    // TODO rewrite to match style
    let (input, _) = ws(tag("[")).parse(input)?;
    let (input, type_) = parse_type(input)?;

    let (input, semi) = ws(opt(tag(";"))).parse(input)?;
    if semi.is_some() {
        let (input, count) = parse_number(input)?;
        let (input, _) = ws(tag("]")).parse(input)?;
        Ok((input, Term::new_rec_term("TypeArray", vec![type_, count])))
    } else {
        let (input, _) = ws(tag("]")).parse(input)?;
        Ok((input, Term::new_rec_term("TypeHeapArray", vec![type_])))
    }
}

fn parse_type_term(i: &str) -> MyParseResult<Term> {
    alt((
        map(ws(parse_name), |n| {
            Term::new_rec_term("TypeId", vec![Term::new_string_term(n.as_str())])
        }),
        map(
            context(
                "tuple type",
                delimited(
                    ws(tag("(")),
                    cut(separated_list0(ws(tag(",")), parse_type)),
                    cut(ws(tag(")"))),
                ),
            ),
            |ts| match ts.len() {
                0 => Term::new_rec_term("UnitType", vec![]),
                _ => Term::new_rec_term("TypeTuple", vec![Term::new_list_term(ts)]),
            },
        ),
        context("array type", parse_type_array),
    ))(i)
}

fn parse_type(i: &str) -> MyParseResult<Term> {
    context(
        "type",
        map(
            tuple((
                parse_type_term,
                context(
                    "function type",
                    opt(preceded(ws(tag("->")), cut(parse_type))),
                ),
            )),
            |(t, ot)| match ot {
                Some(ot) => Term::new_rec_term("FnType", vec![t, ot]),
                None => t,
            },
        ),
    )(i)
}

fn parse_lambda(i: &str) -> MyParseResult<Term> {
    map(
        tuple((
            ws(tag("\\")),
            alt((
                map(parse_name, |n| -> Vec<Term> {
                    vec![Term::new_string_term(n.as_str())]
                }),
                delimited(
                    ws(char('(')),
                    separated_list0(
                        ws(char(',')),
                        map(parse_name, |n| -> Term {
                            Term::new_string_term(n.as_str())
                        }),
                    ),
                    ws(char(')')),
                ),
            )),
            ws(tag("=>")),
            parse_expr,
        )),
        |(_, t, _, e)| {
            let p = Term::new_rec_term("Params", t);
            Term::new_rec_term("Lambda", vec![p, e])
        },
    )(i)
}

fn parse_array(input: &str) -> MyParseResult<Term> {
    map_res(
        context(
            "array",
            delimited(
                char('['),
                separated_list0(ws(char(',')), parse_expr),
                char(']'),
            ),
        ),
        |r| -> Result<Term, String> {
            Ok(Term::new_rec_term("Array", vec![Term::new_list_term(r)]))
        },
    )(input)
}

fn parse_seq(input: &str) -> MyParseResult<Term> {
    map_res(
        context(
            "block",
            delimited(
                ws(char('{')),
                pair(
                    many0(terminated(parse_expr, ws(char(';')))),
                    opt(parse_expr),
                ),
                cut(ws(char('}'))),
            ),
        ),
        |(stmts, res)| -> Result<Term, String> {
            match res {
                Some(res_term) => Ok(Term::new_rec_term(
                    "Seq",
                    vec![Term::new_list_term(stmts), res_term],
                )),
                None => Ok(Term::new_rec_term("Seq", vec![Term::new_list_term(stmts)])),
            }
        },
    )(input)
}

fn parse_tuple(input: &str) -> MyParseResult<Term> {
    map_res(
        context(
            "tuple",
            delimited(
                ws(char('(')),
                cut(separated_list0(ws(tag(",")), parse_expr)),
                cut(ws(char(')'))),
            ),
        ),
        |r| -> Result<Term, String> { Ok(Term::new_rec_term("Tuple", r)) },
    )(input)
}

fn parse_brackets(input: &str) -> MyParseResult<Term> {
    map_res(
        delimited(ws(char('(')), parse_expr, ws(char(')'))),
        |r| -> Result<Term, String> { Ok(Term::new_rec_term("Brackets", vec![r])) },
    )(input)
}

fn parse_value(input: &str) -> MyParseResult<Term> {
    alt((
        parse_number,
        parse_string,
        parse_lambda,
        parse_assign,
        parse_ref,
        parse_array,
        parse_brackets,
        parse_tuple,
        parse_seq,
    ))(input)
}

fn parse_arr_index(i: &str) -> MyParseResult<Term> {
    map(
        pair(parse_value, opt(pair(ws(tag("@")), parse_arr_index))),
        |(lhs, rest)| match rest {
            Some((_, rhs)) => Term::new_rec_term("ArrIndex", vec![lhs, rhs]),
            None => lhs,
        },
    )(i)
}

#[derive(Debug)]
enum Op {
    Add,
    Sub,
    Mul,
    Div,
    Eq,
    And,
    Or,
    GtEq,
    Gt,
    LtEq,
    Lt,
}

fn parse_num_op(i: &str) -> MyParseResult<Op> {
    alt((
        map(tag("+"), |_| Op::Add),
        map(tag("-"), |_| Op::Sub),
        map(tag("*"), |_| Op::Mul),
        map(tag("/"), |_| Op::Div),
    ))(i)
}

fn parse_num_ops(i: &str) -> MyParseResult<Term> {
    map(
        pair(parse_arr_index, opt(pair(ws(parse_num_op), parse_num_ops))),
        |(lhs, opt_rhs)| match opt_rhs {
            Some((Op::Add, rhs)) => Term::new_rec_term("Add", vec![lhs, rhs]),
            Some((Op::Sub, rhs)) => Term::new_rec_term("Sub", vec![lhs, rhs]),
            Some((Op::Mul, rhs)) => Term::new_rec_term("Mul", vec![lhs, rhs]),
            Some((Op::Div, rhs)) => Term::new_rec_term("Div", vec![lhs, rhs]),
            Some(_) => panic!("Cannot parse this operand here!"),
            None => lhs,
        },
    )(i)
}

fn parse_bool_op(i: &str) -> MyParseResult<Op> {
    alt((
        map(tag("=="), |_| Op::Eq),
        map(tag(">="), |_| Op::GtEq),
        map(tag("<="), |_| Op::LtEq),
        map(tag(">"), |_| Op::Gt),
        map(tag("<"), |_| Op::Lt),
        map(tag("&&"), |_| Op::And),
        map(tag("||"), |_| Op::Or),
    ))(i)
}

fn parse_bool_ops(i: &str) -> MyParseResult<Term> {
    map(
        pair(parse_num_ops, opt(pair(ws(parse_bool_op), parse_bool_ops))),
        |(lhs, opt_rhs)| match opt_rhs {
            Some((Op::Eq, rhs)) => Term::new_rec_term("Eq", vec![lhs, rhs]),
            Some((Op::GtEq, rhs)) => Term::new_rec_term("GtEq", vec![lhs, rhs]),
            Some((Op::LtEq, rhs)) => Term::new_rec_term("LtEq", vec![lhs, rhs]),
            Some((Op::Gt, rhs)) => Term::new_rec_term("Gt", vec![lhs, rhs]),
            Some((Op::Lt, rhs)) => Term::new_rec_term("Lt", vec![lhs, rhs]),
            Some((Op::And, rhs)) => Term::new_rec_term("And", vec![lhs, rhs]),
            Some((Op::Or, rhs)) => Term::new_rec_term("Or", vec![lhs, rhs]),
            Some(_) => panic!("Cannot parse this operand here!"),
            None => lhs,
        },
    )(i)
}

fn parse_for(i: &str) -> MyParseResult<Term> {
    alt((
        map(
            tuple((
                ws(tag("for")),
                cut(ws(parse_name)),
                cut(ws(tag("in"))),
                cut(parse_bool_ops),
                cut(parse_seq),
            )),
            |(_, binding, _, list, body)| {
                Term::new_rec_term(
                    "For",
                    vec![Term::new_string_term(binding.as_str()), list, body],
                )
            },
        ),
        parse_bool_ops,
    ))(i)
}

fn parse_if(i: &str) -> MyParseResult<Term> {
    alt((
        map(
            tuple((ws(tag("if")), cut(parse_bool_ops), cut(parse_seq))),
            |(_, cond, body)| Term::new_rec_term("If", vec![cond, body]),
        ),
        parse_for,
    ))(i)
}

fn parse_appl(i: &str) -> MyParseResult<Term> {
    map(
        pair(ws(parse_if), context("appl", many0(ws(parse_bool_ops)))),
        |(lhs, exprs)| match exprs.as_slice() {
            [] => lhs,
            _ => {
                let mut result = lhs;

                for arg in exprs {
                    result = Term::new_rec_term("Appl", vec![result, arg]);
                }

                result
            }
        },
    )(i)
}

fn parse_function_composition(i: &str) -> MyParseResult<Term> {
    map(
        tuple((
            parse_appl,
            opt(pair(
                ws(tag(".")),
                context("fn composition", cut(parse_function_composition)),
            )),
        )),
        |(lhs, rest)| match rest {
            Some((_, rhs)) => Term::new_rec_term("FnComposition", vec![lhs, rhs]),
            None => lhs,
        },
    )(i)
}

fn parse_lhs(i: &str) -> MyParseResult<Term> {
    context(
        "lhs",
        alt((
            map(ws(parse_name), |n| Term::new_string_term(n.as_str())),
            map(
                delimited(
                    cut(ws(tag("("))),
                    cut(separated_list1(ws(tag(",")), parse_lhs)),
                    cut(ws(tag(")"))),
                ),
                |n| Term::new_list_term(n),
            ),
        )),
    )(i)
}

fn parse_let_decl(i: &str) -> MyParseResult<Term> {
    map(
        context(
            "let",
            tuple((
                parse_lhs,
                opt(preceded(ws(tag(":")), cut(parse_type))),
                ws(tag("=")),
                parse_expr,
            )),
        ),
        |(n, t, _, e)| match t {
            Some(t) => Term::new_rec_term("Decl", vec![n, t, e]),
            None => Term::new_rec_term("Decl", vec![n, e]),
        },
    )(i)
}

fn parse_let(i: &str) -> MyParseResult<Term> {
    alt((
        map(
            context(
                "let",
                tuple((
                    ws(tag("let")),
                    cut(separated_list1(ws(char(',')), parse_let_decl)),
                    cut(opt(preceded(ws(tag("in")), cut(parse_expr)))),
                )),
            ),
            |(_, decls, opt_expr)| match opt_expr {
                Some(expr) => Term::new_rec_term("Let", vec![Term::new_list_term(decls), expr]),
                None => Term::new_rec_term("Let", vec![Term::new_list_term(decls)]),
            },
        ),
        parse_function_composition,
    ))(i)
}

fn parse_match(i: &str) -> MyParseResult<Term> {
    alt((
        map(
            context(
                "match",
                tuple((
                    tag("match"),
                    parse_expr,
                    many1(preceded(
                        ws(tag("|")),
                        separated_pair(parse_pattern, ws(tag("->")), parse_expr),
                    )),
                )),
            ),
            |(_, e, ps)| {
                let branches: Vec<Term> = ps
                    .into_iter()
                    .map(|t| Term::new_rec_term("MatchBranch", vec![t.0, t.1]))
                    .collect();

                Term::new_rec_term("Match", vec![e, Term::new_list_term(branches)])
            },
        ),
        parse_let,
    ))(i)
}

fn parse_expr(input: &str) -> MyParseResult<Term> {
    parse_match(input)
}

/*
* Matchers
*/

fn parse_pattern_var(i: &str) -> MyParseResult<Term> {
    map(ws(parse_name), |n| {
        Term::new_rec_term("PatternVar", vec![Term::new_string_term(n.as_str())])
    })(i)
}

fn parse_pattern_int(i: &str) -> MyParseResult<Term> {
    map(ws(parse_number), |n| Term::new_rec_term("PatternNum", vec![n]))(i)
}

fn parse_pattern_str(i: &str) -> MyParseResult<Term> {
    map(ws(parse_string), |n| Term::new_rec_term("PatternStr", vec![n]))(i)
}

fn parse_pattern_tuple(i: &str) -> MyParseResult<Term> {
    map(
        context("tuple pattern", delimited(
            ws(tag("(")),
            cut(separated_list0(ws(char(',')), parse_pattern)),
            cut(ws(tag(")"))),
        )),
        |ts| Term::new_rec_term("PatternTuple", ts),
    )(i)
}

fn parse_pattern(i: &str) -> MyParseResult<Term> {
    alt((
        parse_pattern_int,
        parse_pattern_var,
        parse_pattern_str,
        parse_pattern_tuple,
    ))(i)
}

/*
* Top-level
*/

fn parse_module_declaration(i: &str) -> MyParseResult<Term> {
    map(delimited(tag("mod"), ws(cut(parse_name)), ws(cut(char(';')))), |s| {
        Term::new_rec_term("ModuleName", vec![Term::new_string_term(s.as_str())])
    })(i)
}

fn parse_function_declaration(i: &str) -> MyParseResult<Term> {
    map(
        tuple((parse_name, ws(tag(":")), cut(parse_type))),
        |(n, _, t)| Term::new_rec_term("FnDecl", vec![Term::new_string_term(n.as_str()), t]),
    )(i)
}

fn parse_function_definition(i: &str) -> MyParseResult<Term> {
    map(
        context(
            "function",
            tuple((
                parse_name,
                many1(parse_pattern),
                cut(ws(char('='))),
                cut(parse_expr),
                cut(ws(char(';'))),
            )),
        ),
        |(func, patterns, _, body, _)| {
            Term::new_rec_term(
                "FnDef",
                vec![Term::new_string_term(func.as_str()), Term::new_list_term(patterns), body],
            )
        },
    )(i)
}

fn parse_import(i: &str) -> MyParseResult<Term> {
    context(
        "import",
        delimited(
            ws(tag("import")),
            cut(ws(map(parse_name, |n| Term::new_string_term(n.as_str())))),
            cut(ws(char(';'))),
        ),
    )(i)
}

fn parse_use(i: &str) -> MyParseResult<Term> {
    context(
        "use",
        map(
            tuple((
                ws(tag("use")),
                cut(ws(map(parse_name, |n| Term::new_string_term(n.as_str())))),
                cut(ws(tag("as"))),
                cut(ws(map(parse_name, |n| Term::new_string_term(n.as_str())))),
                cut(ws(char(';'))),
            )),
            |(_, a, _, b, _)| Term::new_rec_term("UseModuleAs", vec![a, b]),
        ),
    )(i)
}

fn parse_toplevel(i: &str) -> MyParseResult<Term> {
    alt((
        parse_function_definition,
        parse_function_declaration,
        parse_import,
        parse_use,
    ))(i)
}

pub fn parse_gale_string(i: &str) -> Result<Term, &str> {
    match all_consuming(pair(
        terminated(parse_module_declaration, multispace0),
        many0(terminated(parse_toplevel, multispace0)),
    ))(i)
    {
        Ok((_, (module_decl, r))) => Ok(Term::new_rec_term(
            "File",
            vec![module_decl, Term::new_list_term(r)],
        )),
        Err(nom::Err::Failure(err)) | Err(nom::Err::Error(err)) => {
            panic!("{}", convert_error(i, err));
        }
        Err(e) => {
            panic!("{}", e)
        }
    }
}

pub fn parse_gale_file(filename: &str) -> Result<Term, &str> {
    match fs::read_to_string(filename) {
        Ok(file) => {
            // This doesn't make much sense but the file contents (which are part of the error)
            // cannot be returned from this function, so has to be refactored
            let r = parse_gale_string(file.as_str());
            Ok(r.unwrap())
        }
        _ => panic!("Cannot find file {}", filename),
    }
}
