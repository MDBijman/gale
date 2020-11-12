
#[derive(Debug)]
pub struct File {
    pub functions: Vec<Function>
}

impl File {
    pub fn merge(mut a: File, mut b: File) -> File {
        a.functions.append(&mut b.functions);
        a
    }
}

#[derive(Debug)]
pub struct Function {
    pub name: String,
    pub meta: Vec<Expr>,
    pub matcher: Match,
    pub body: Expr
}

#[derive(Debug, Clone)]
pub enum Expr {
    Invoke(Invocation),
    Tuple(Tuple),
    Ref(Ref),
    FRef(FRef),
    Number(Number),
    Op(Op),
    This
}

#[derive(Debug, Clone)]
pub struct Invocation {
    pub name: FRef,
    pub arg: Box<Expr>
}

#[derive(Debug, Clone)]
pub struct Ref {
    pub name: String
}

#[derive(Debug, Clone)]
pub struct FRef {
    pub name: String,
    pub meta: Vec<Expr>
}

impl FRef {
    pub fn from(name: &String, margs: &Vec<Expr>) -> FRef {
        FRef { name: name.clone(), meta: margs.clone() }
    }
}

#[derive(Debug, Clone)]
pub struct Number {
    pub value: f64
}

#[derive(Debug, Clone)]
pub struct Tuple {
    pub values: Vec<Expr>
}

#[derive(Debug, Clone)]
pub enum Op {
    Or(Box<Expr>, Box<Expr>),
    And(Box<Expr>, Box<Expr>)
}

#[derive(Debug)]
pub enum Match {
    TermMatcher(TermMatcher),
    VariableMatcher(VariableMatcher),
    StringMatcher(StringMatcher),
    NumberMatcher(NumberMatcher),
    AnyMatcher
}

#[derive(Debug)]
pub struct TermMatcher {
    pub constructor: String,
    pub terms: Vec<Match>
}

#[derive(Debug)]
pub struct VariableMatcher {
    pub name: String
}

#[derive(Debug)]
pub struct StringMatcher {
    pub value: String
}

#[derive(Debug)]
pub struct NumberMatcher {
    pub value: f64
}
