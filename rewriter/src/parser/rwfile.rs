use terms_format as tf;

#[derive(Debug)]
pub struct File {
    pub functions: Vec<Function>,
    pub filename: Option<String>
}

impl File {
    pub fn merge(mut a: File, mut b: File) -> File {
        a.functions.append(&mut b.functions);
        if a.filename.is_none() { a.filename = b.filename; }
        a
    }

    pub fn set_filename(&mut self, name: &str) {
        self.filename = Some(String::from(name));
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
    ListCons(ListCons),
    Term(Term),
    Ref(Ref),
    UnrollVariadic(Ref),
    FRef(FRef),
    Number(Number),
    Text(Text),
    Op(Op),
    Annotation(Annotation),
    AddAnnotation(Annotation),
    Let(Let),
    SimpleTerm(tf::Term),
    This
}

impl Expr {
    pub fn to_term(&self) -> Option<tf::Term> {
        match &self {
            Expr::SimpleTerm(t) => Some(t.clone()),
            Expr::Tuple(t) => {
                let mut sub_t: Vec<tf::Term> = vec![];
                for elem in &t.values {
                    sub_t.push(elem.to_term()?);
                }
                Some(tf::Term::new_tuple_term(sub_t))
            },
            Expr::List(t) => {
                let mut sub_t: Vec<tf::Term> = vec![];
                for elem in &t.values {
                    sub_t.push(elem.to_term()?);
                }
                Some(tf::Term::new_list_term(sub_t))
            },
            Expr::Number(t) => {
                Some(tf::Term::new_number_term(t.value))
            },
            Expr::Text(t) => {
                Some(tf::Term::new_string_term(&t.value))
            },
            _ => None
        }
    }
}

#[derive(Debug, Clone)]
pub struct Invocation {
    pub name: FRef,
    pub arg: Box<Expr>
}

#[derive(Debug, Clone)]
pub struct Term {
    pub constructor: Box<Expr>,
    pub terms: Vec<Expr>
}

#[derive(Debug, Clone)]
pub struct Ref {
    pub name: String
}

#[derive(Debug, Copy, Clone, PartialEq)]
pub enum FunctionReferenceType {
    Force, Try
}

#[derive(Debug, Clone)]
pub struct FRef {
    pub name: String,
    pub meta: Vec<Expr>,
    pub ref_type: FunctionReferenceType
}

impl FRef {
    pub fn from(name: &String, margs: &Vec<Expr>, ref_type: FunctionReferenceType) -> FRef {
        FRef { name: name.clone(), meta: margs.clone(), ref_type: ref_type }
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
pub struct ListCons {
    pub head: Box<Expr>,
    pub tail: Box<Expr>
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
pub struct Let {
    pub lhs: Match,
    pub rhs: Box<Expr>,
    pub body: Box<Expr>
}

#[derive(Debug, Clone)]
pub enum Match {
    TermMatcher(TermMatcher),
    VariableMatcher(VariableMatcher),
    StringMatcher(StringMatcher),
    NumberMatcher(NumberMatcher),
    ListMatcher(ListMatcher),
    TupleMatcher(TupleMatcher),
    VariadicElementMatcher(VariadicElementMatcher),
    AnyMatcher
}

#[derive(Debug, Clone)]
pub struct TermMatcher {
    pub constructor: Box<Match>,
    pub terms: Box<Match>,
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
    pub head: Option<Box<Match>>,
    pub tail: Option<Box<Match>>
}

#[derive(Debug, Clone)]
pub struct TupleMatcher {
    pub elems: Vec<Match>
}

#[derive(Debug, Clone)]
pub struct VariadicElementMatcher {
    pub name: String
}
