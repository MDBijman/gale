use rewriter::{parse_rewrite_file, Rewriter};
use terms_format::parse_term_from_string;


#[test]
fn test_parse_prelude() {
    match rewriter::parse_rewrite_file("std/prelude.rw") {
        Ok(_) => {},
        Err(_) => panic!("failure")
    }
}

fn run_e2e_test(rw_string: &str, in_term: &str, out_term: &str) {
    let r = rewriter::parse_rewrite_string(rw_string).unwrap();
    let mut rw = Rewriter::new_with_prelude(r);
    let input = parse_term_from_string(in_term).unwrap();
    let result = rw.rewrite(input);
    let expected = parse_term_from_string(out_term).unwrap();
    assert_eq!(result, expected);
}

#[test]
fn test_subterms() {
    run_e2e_test(
        "main: f -> .subterms f;",
        "File(1, 2, 3)",
        "[1, 2, 3]")
}

#[test]
fn test_try() {
    run_e2e_test(
        "main: f -> .try[.fail] f;",
        "Term(1, 2, 3)",
        "Term(1, 2, 3)");
}

#[test]
fn test_map() {
    run_e2e_test(
        "main: f -> .map[.inc] f;
         inc: a -> .add (a, 1);",
        "[1, 2, 3]",
        "[2, 3, 4]");
}

#[test]
fn test_env_map() {
    run_e2e_test(
        "main: f -> .env_map[.inc_inc] (1, f);
         inc_inc: (i, a) -> (.add (i, 1), .add (a, i));",
        "[1, 2, 3]",
        "(4, [2, 4, 6])");
}

#[test]
fn test_second() {
    run_e2e_test("main: f -> .snd;", "(1, 2)", "2");
    run_e2e_test("main: f -> .second;", "(1, 2)", "2");
}

#[test]
fn test_first() {
    run_e2e_test("main: f -> .fst;", "(1, 2)", "1");
    run_e2e_test("main: f -> .first;", "(1, 2)", "1");
}

#[test]
fn test_flatmap() {
    run_e2e_test(
        "main: f -> .flatmap[.inc] f;
         inc: a -> [a, .add (a, 1)];",
        "[1, 2, 3]",
        "[1, 2, 2, 3, 3, 4]");
}

#[test]
fn test_flatmap_some() {
    run_e2e_test(
        "main: f -> .flatmap_some[.inc] f;
         inc: a -> [a, .add (a, 1)];
         inc: a -> a;",
        "[1, \"a\", 3]",
        "[1, 2, \"a\", 3, 4]");
}

#[test]
fn test_intersperse() {
    run_e2e_test(
        "main: f -> .intersperse[\",\"] f;",
        "[1, 2, 3]",
        "[1, \",\", 2, \",\", 3]");

    run_e2e_test("main: f -> .intersperse[\",\"] f;", "[]", "[]");
}

#[test]
fn test_flatten() {
    run_e2e_test(
        "main: f -> .flatten f;",
        "[[1], [2], [3]]",
        "[1, 2, 3]");

    run_e2e_test(
        "main: f -> .flatten f;",
        "[[1], [2], [3, [4, [5]]]]",
        "[1, 2, 3, [4, [5]]]");
}

#[test]
fn test_flatten_some() {
    run_e2e_test(
        "main: f -> .flatten_some f;",
        "[1, 2, [3]]",
        "[1, 2, 3]");

    run_e2e_test(
        "main: f -> .flatten_some f;",
        "[1, 2, [3, [4, [5]]]]",
        "[1, 2, 3, [4, [5]]]");
}

#[test]
fn test_bottomup() {
    run_e2e_test(
        "main: f -> !bottomup[.id] f;",
        "Test([1, 2, 3], 4, (1, 2))",
        "Test([1, 2, 3], 4, (1, 2))");
}

#[test]
fn test_topdown() {
    run_e2e_test(
        "main: f -> !topdown[.id] f;",
        "Test([1, 2, 3], 4, (1, 2))",
        "Test([1, 2, 3], 4, (1, 2))");
}

#[test]
fn test_env_bottomup() {
    run_e2e_test(
        "main: f -> .env_bottomup[.id] ([], f);",
        "[1, 2, 3]",
        "([], [1, 2, 3])");

    run_e2e_test(
        "main: f -> .env_bottomup[.id] ([], f);",
        "Test(1, 2, Test(\"a\"), [1, 2, 3])",
        "([], Test(1, 2, Test(\"a\"), [1, 2, 3]))");


}

#[test]
fn test_bug() {
    run_e2e_test(
        "main: f -> .env_bottomup[.try[.hoist]] ([], f) > .second;
         hoist[.f]: (e, UI8(i)) -> .debug>(e, i);",
        "[\"d\", Array([UI8(1), UI8(2), UI8(3)])]",
        "[\"d\", Array([1, 2, 3])]");
}
