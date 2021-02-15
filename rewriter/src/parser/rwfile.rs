
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

#[derive(Debug, Clone)]
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
    List(List),
    Term(Term),
    Ref(Ref),
    FRef(FRef),
    Number(Number),
    Text(Text),
    Op(Op),
    Annotation(Annotation),
    This
}

#[derive(Debug, Clone)]
pub struct Invocation {
    pub name: FRef,
    pub arg: Box<Expr>
}

#[derive(Debug, Clone)]
pub struct Term {
    pub constructor: Ref,
    pub terms: Vec<Expr>
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
pub struct Text {
    pub value: String
}

#[derive(Debug, Clone)]
pub struct Tuple {
    pub values: Vec<Expr>
}

#[derive(Debug, Clone)]
pub struct List {
    pub values: Vec<Expr>
}

#[derive(Debug, Clone)]
pub enum Op {
    Or(Box<Expr>, Box<Expr>),
    And(Box<Expr>, Box<Expr>)
}

#[derive(Debug, Clone)]
pub struct Annotation {
    pub inner: Box<Expr>,
    pub annotations: Vec<Expr>
}

#[derive(Debug, Clone)]
pub enum Match {
    TermMatcher(TermMatcher),
    VariableMatcher(VariableMatcher),
    StringMatcher(StringMatcher),
    NumberMatcher(NumberMatcher),
    ListMatcher(ListMatcher),
    TupleMatcher(TupleMatcher),
    AnyMatcher
}

#[derive(Debug, Clone)]
pub struct TermMatcher {
    pub constructor: String,
    pub terms: Vec<Match>,
    pub annotations: Vec<Match>
}

#[derive(Debug, Clone)]
pub struct VariableMatcher {
    pub name: String,
    pub annotations: Vec<Match>
}

#[derive(Debug, Clone)]
pub struct StringMatcher {
    pub value: String
}

#[derive(Debug, Clone)]
pub struct NumberMatcher {
    pub value: f64
}

#[derive(Debug, Clone)]
pub struct ListMatcher {
    pub head: String,
    pub tail: Option<String>
}

#[derive(Debug, Clone)]
pub struct TupleMatcher {
    pub elems: Vec<Match>
}
