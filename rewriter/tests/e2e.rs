use rewriter;

#[test]
fn test_parse_prelude() {
    match rewriter::parse_rewrite_file("std/prelude.rw") {
        Ok(_) => {},
        Err(_) => panic!("failure")
    }
}

#[test]
fn test_simple() {
    let r = rewriter::parse_rewrite_string("
        main: f -> .env_bottomup[.index] f;
        .index: (Counter(i), t) -> (Counter(.add (i, 1)), t { Counter(i) });
    ");

    assert!(r.is_ok());
}