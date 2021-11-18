extern crate aterms;
use aterms::*;
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

fn newline(input: &str) -> MyParseResult<&str> {
    let (input, _) = take_while(|c: char| c == ' ' || c == '\t')(input)?;
    alt((tag("\n"), tag("\r\n"))).parse(input)
}

fn u16_hex(i: &str) -> MyParseResult<u16> {
    map_res(take(4usize), |s| u16::from_str_radix(s, 16))(i)
}

fn dbg_in<'a, O, E: ParseError<&'a str>, F: Parser<&'a str, O, E>>(
    mut f: F,
) -> impl Parser<&'a str, O, E>
where
    O: std::fmt::Debug,
    E: std::fmt::Debug,
{
    move |input: &'a str| {
        println!("[dbg] {}", input);
        f.parse(input)
    }
}

fn dbg_out<'a, O, E: ParseError<&'a str>, F: Parser<&'a str, O, E>>(
    mut f: F,
) -> impl Parser<&'a str, O, E>
where
    O: std::fmt::Debug,
    E: std::fmt::Debug,
{
    move |input: &'a str| {
        let r = f.parse(input);
        println!("[dbg] {:?}", r);
        r
    }
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
        |s: &str| s != "in" && s != "let" && s != "match" && s != "for" && s != "if",
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
    map(context("name", parse_name), |name: String| {
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

fn parse_type_term(input: &str) -> MyParseResult<Term> {
    alt((
        map_res(parse_name, |n| -> Result<Term, String> {
            Ok(Term::new_rec_term(
                "TypeId",
                vec![Term::new_string_term(n.as_str())],
            ))
        }),
        map_res(tag("()"), |_| -> Result<Term, String> {
            Ok(Term::new_rec_term("TypeTuple", vec![]))
        }),
        parse_type_array,
    ))(input)
}

fn parse_type(input: &str) -> MyParseResult<Term> {
    let (input, in_type) = parse_type_term(input)?;
    let (input, r) = opt(ws(tag("->")))(input)?;
    match r {
        Some(_) => {
            let (input, out_type) = parse_type(input)?;
            Ok((input, Term::new_rec_term("FnType", vec![in_type, out_type])))
        }
        _ => Ok((input, in_type)),
    }
}

fn parse_lambda(i: &str) -> MyParseResult<Term> {
    map(
        tuple((
            ws(tag("\\")),
            alt((
                map_res(parse_name, |n| -> Result<Vec<Term>, String> {
                    Ok(vec![Term::new_string_term(n.as_str())])
                }),
                delimited(
                    ws(char('(')),
                    separated_list0(
                        ws(char(',')),
                        map_res(parse_name, |n| -> Result<Term, String> {
                            Ok(Term::new_string_term(n.as_str()))
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
                char('('),
                separated_list0(ws(char(',')), parse_expr),
                char(')'),
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

fn parse_arr_index(input: &str) -> MyParseResult<Term> {
    let (input, lhs) = parse_value(input)?;
    match ws::<_, (_, _), _>(tag("@")).parse(input) {
        Ok((input, _)) => {
            let (input, rhs) = parse_arr_index(input)?;

            Ok((input, Term::new_rec_term("ArrIndex", vec![lhs, rhs])))
        }
        _ => Ok((input, lhs)),
    }
}

#[derive(Debug)]
enum Op {
    Add,
    Sub,
    Mul,
    Eq,
}

fn parse_num_op(i: &str) -> MyParseResult<Op> {
    alt((
        map(tag("+"), |_| Op::Add),
        map(tag("-"), |_| Op::Sub),
        map(tag("*"), |_| Op::Mul),
    ))(i)
}

fn parse_num_ops(i: &str) -> MyParseResult<Term> {
    map(
        pair(parse_arr_index, opt(pair(ws(parse_num_op), parse_num_ops))),
        |(lhs, opt_rhs)| match opt_rhs {
            Some((Op::Add, rhs)) => Term::new_rec_term("Add", vec![lhs, rhs]),
            Some((Op::Sub, rhs)) => Term::new_rec_term("Sub", vec![lhs, rhs]),
            Some((Op::Mul, rhs)) => Term::new_rec_term("Mul", vec![lhs, rhs]),
            Some(_) => panic!("Cannot parse this operand here!"),
            None => lhs,
        },
    )(i)
}

fn parse_bool_op(i: &str) -> MyParseResult<Op> {
    map(tag("=="), |_| Op::Eq)(i)
}

fn parse_bool_ops(i: &str) -> MyParseResult<Term> {
    map(
        pair(parse_num_ops, opt(pair(ws(parse_bool_op), parse_bool_ops))),
        |(lhs, opt_rhs)| match opt_rhs {
            Some((Op::Eq, rhs)) => Term::new_rec_term("Eq", vec![lhs, rhs]),
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

fn parse_let_decl(i: &str) -> MyParseResult<Term> {
    map(
        context(
            "binding",
            tuple((
                ws(parse_name),
                ws(tag(":")),
                parse_type,
                ws(tag("=")),
                parse_expr,
            )),
        ),
        |(n, _, t, _, e)| Term::new_rec_term("Decl", vec![Term::new_string_term(n.as_str()), t, e]),
    )(i)
}

fn parse_let(i: &str) -> MyParseResult<Term> {
    alt((
        map(
            ws(context(
                "let",
                tuple((
                    tag("let"),
                    cut(separated_list1(ws(char(',')), parse_let_decl)),
                    cut(opt(preceded(ws(tag("in")), cut(parse_expr)))),
                )),
            )),
            |(_, decls, opt_expr)| match opt_expr {
                Some(expr) => Term::new_rec_term("Let", vec![Term::new_list_term(decls), expr]),
                None => Term::new_rec_term("Let", vec![Term::new_list_term(decls)]),
            },
        ),
        parse_appl,
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
    map(parse_name, |n| {
        Term::new_rec_term("PatternVar", vec![Term::new_string_term(n.as_str())])
    })(i)
}

fn parse_pattern_int(i: &str) -> MyParseResult<Term> {
    map(parse_number, |n| Term::new_rec_term("PatternNum", vec![n]))(i)
}

fn parse_pattern_str(i: &str) -> MyParseResult<Term> {
    map(parse_string, |n| Term::new_rec_term("PatternStr", vec![n]))(i)
}

fn parse_pattern(i: &str) -> MyParseResult<Term> {
    alt((parse_pattern_int, parse_pattern_var, parse_pattern_str))(i)
}

/*
* Top-level
*/

fn parse_module_declaration(i: &str) -> MyParseResult<Term> {
    map(delimited(tag("mod"), ws(parse_name), tag(";")), |s| {
        Term::new_rec_term("ModuleName", vec![Term::new_string_term(s.as_str())])
    })(i)
}

fn parse_function_declaration(input: &str) -> MyParseResult<Term> {
    let (input, name) = parse_name(input)?;
    let (input, _) = ws(tag(":")).parse(input)?;
    let (input, fn_type) = cut(parse_type)(input)?;
    let (input, _) = newline(input)?;

    Ok((
        input,
        Term::new_rec_term(
            "FnDecl",
            vec![Term::new_string_term(name.as_str()), fn_type],
        ),
    ))
}

fn parse_function_definition(i: &str) -> MyParseResult<Term> {
    map(
        context(
            "function definition",
            tuple((
                parse_name,
                ws(parse_pattern),
                cut(ws(tag("="))),
                cut(parse_expr),
                cut(ws(tag(";"))),
            )),
        ),
        |(func, pattern, _, body, _)| {
            Term::new_rec_term(
                "FnDef",
                vec![Term::new_string_term(func.as_str()), pattern, body],
            )
        },
    )(i)
}

pub fn parse_gale_string(i: &str) -> Result<Term, &str> {
    match all_consuming(pair(
        terminated(parse_module_declaration, multispace0),
        many0(terminated(
            alt((parse_function_definition, parse_function_declaration)),
            multispace0,
        )),
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
            let r = parse_gale_string(file.as_str());
            // This doesn't make much sense but the file contents (which are part of the error)
            // cannot be returned from this function, so has to be refactored
            Ok(r.unwrap())
        }
        _ => panic!("Cannot find file {}", filename),
    }
}
