use aterms::Term;
use checker::{check, desugar};
use compiler::{compile, lower, print};
use parser::parse_gale_string;
use std::thread;

#[test]
fn test_id_main() {
    let builder = thread::Builder::new()
        .name("rewriter".into())
        .stack_size(32 * 1000 * 1000);
    let handler = builder
        .spawn(move || {
            let term = parse_gale_string("mod id; main: [str] -> ui64\nmain s = s;").unwrap();
            let checked_term = check(&term);
            let desugared_term = desugar(&checked_term);
            let lowered_term = lower(desugared_term);
            let compiled_term = compile(lowered_term);
            let printed_term = print(compiled_term);
            println!("{}", printed_term);
            assert_eq!(
                printed_term,
                Term::new_string_term(
                    "mod id

.code
fn main ($0: &[&str]) -> ui64
    movc $1, 1
    neg $2, $1
    mov $3, $2
    jmpif $3 @lbl_0
    mov $4, $0
    mov $5, $4
  lbl_0:
    ret $5
endfn
"
                )
            );
        })
        .unwrap();

    handler.join().unwrap();
}
