use parser::parse_gale_string;
use checker::check;
use compiler::{ lower, compile, print };
use terms_format::Term;

#[test]
fn test_id_main() {
    let term          = parse_gale_string("main: ui8 -> ui8\nmain a = a;").unwrap();
    let checked_term  = check(&term);
    let lowered_term  = lower(checked_term);
    let compiled_term = compile(lowered_term);
    let printed_term  = print(compiled_term);
    println!("{}", printed_term);
    assert_eq!(printed_term, Term::new_string_term(
"func main ($0) {
  $1 = $0
  ret $1
}
"));
}

