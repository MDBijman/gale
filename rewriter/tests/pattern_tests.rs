use rewriter::{parse_rewrite_file, Rewriter};
use terms_format::parse_term_from_string;

fn run_e2e_test(rw_string: &str, in_term: &str, out_term: &str) {
    let r = rewriter::parse_rewrite_string(rw_string).unwrap();
    let mut rw = Rewriter::new_with_prelude(r);
    let input = parse_term_from_string(in_term).unwrap();
    let result = rw.rewrite(input);
    let expected = parse_term_from_string(out_term).unwrap();
    assert_eq!(result, expected);
}

#[test]
fn test_term_match() {
    run_e2e_test(
        "main: f -> .match_head f;
        match_head: Test(a, b) -> (a, b);",
        "Test(1, 2)",
        "(1, 2)");
}

#[test]
fn test_term_name_variable() {
    run_e2e_test(
        "main: f -> .match f;
        match: ?n(a, b) -> (n, a, b);",
        "Test(1, 2)",
        "(\"Test\", 1, 2)");

    run_e2e_test(
        "main: f -> .match f;
        match: ?con(..sub) -> ?con(..sub);",
        "Test(1, 3)",
        "Test(1, 3)");

    run_e2e_test(
        "main: f -> .match f;
        match: ?con(..sub) -> ?con(..sub);",
        "Test(1)",
        "Test(1)");
}

#[test]
fn test_variadic_subterms() {
    run_e2e_test(
        "main: f -> .match f;
        match: Test(a, ..b) -> b;",
        "Test(1, 2, 3)",
        "[2, 3]");

    run_e2e_test(
        "main: f -> .match f;
        match: Test(a, ..b) -> (a, ..b);",
        "Test(1, 2, 3)",
        "(1, 2, 3)");

    run_e2e_test(
        "main: f -> .match f;
        match: Test(..b) -> (..b);",
        "Test()",
        "()");

    run_e2e_test(
        "main: f -> .match f;
        match: Test(..b) -> (..b);",
        "Test()",
        "()");
}

