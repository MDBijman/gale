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
fn test_meta_ref() {
    run_e2e_test(
        "main: f -> .match[1] f;
        match[a]: _ -> a;",
        "Test()",
        "1");

    run_e2e_test(
        "main: f -> .match[.id] f;
        match[.f]: t -> .f t;",
        "Test()",
        "Test()");

    run_e2e_test(
        "main: f -> .apply[.do[.id]] f;
        do[.f]: a -> .apply[.f] a;
        apply[.f]: t -> .f t;",
        "Test()",
        "Test()");
}

#[test]
fn test_recursive() {
    run_e2e_test(
        "main: f -> .env_map[.do[.inc]] ([], f);
        do[.f]: (e, t) -> (e, .f t);
        inc: i -> .add (i, 1);",
        "[1, 2, 3]",
        "([], [2,3,4])");
}

