use galevmlib::Instruction;

#[test]
fn test_instr_size() {
    use std::mem::size_of;
    assert_eq!(size_of::<Instruction>(), 8);
}
