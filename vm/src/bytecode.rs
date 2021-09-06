use std::collections::HashMap;
use std::fmt;

#[derive(Debug)]
pub struct Module {
    pub constants: Vec<Value>,
    pub functions: HashMap<String, Function>
}

impl Module {
    pub fn new() -> Module {
        Module { constants: Vec::new(), functions: HashMap::new() }
    }

    pub fn from(consts: Vec<Value>, fns: Vec<Function>) -> Module {
        let mut module = Module::new();
        for func in fns {
            module.functions.insert(func.name.clone(), func);
        }

        module.constants = consts;

        module
    }
}

#[derive(Debug, Clone)]
pub enum Type {
    U64(),
    Array(Box<Type>),
    Pointer(Box<Type>),
    Null,
    Str
}

#[derive(Debug, Clone)]
pub enum Value {
    U64(u64),
    Array(Vec<Value>),
    Pointer(u64),
    Null,
    Str(String)
}

#[derive(Debug, Clone)]
pub enum UnOp {
    Not
}

#[derive(Debug, Clone)]
pub enum BinOp {
    Add,
    Sub,
    Eq
}

impl fmt::Display for Value {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Value::U64(v) => write!(f, "{}", v),
            Value::Array(values) => {
                write!(f, "[")?;
                let mut first = true;
                for v in values {
                    if !first {
                        write!(f, ", ")?;
                    }
                    write!(f, "{}", v)?;
                    first = false;
                }
                write!(f, "]")
            },
            Value::Null => write!(f, "null"),
            Value::Pointer(b) => write!(f, "&{}", b),
            Value::Str(s) => write!(f, "\"{}\"", s),
        }
    }
}

#[derive(Debug)]
pub enum Expression {
    Var(Location),
    Value(Value),
    Array(Vec<Expression>),
}

#[derive(Debug)]
pub enum Location {
    Var(u64),
    Lbl(String)
}

#[derive(Debug)]
pub enum Instruction {
    Set(Location, Expression),
    Return(Expression),
    Print(Expression),
    Call(Location, String, Vec<Expression>),
    Jmp(Location),
    JmpIf(Location, Expression),
    Lbl(String),

    Pop(Location),
    Push(Expression),
    BinOp(BinOp),
    UnOp(UnOp),

    Alloc(Location, Type),
    Load(Location, Location),
    LoadConst(Location, u64),
    Store(Location, Expression),
    Index(Location, Location, Location)
}

#[derive(Debug)]
pub struct Parameter {
    pub location: Location
}

#[derive(Debug)]
pub struct FunctionMeta {
    pub vars: u64
}

#[derive(Debug)]
pub struct Function {
    pub name: String,
    pub meta: FunctionMeta,
    pub instructions: Vec<Instruction>,
    pub jump_table: HashMap<String, u64>
}

impl Function {
    pub fn new(name: String, meta: FunctionMeta, instructions: Vec<Instruction>, jump_table: HashMap<String, u64>) -> Function {
        Function { name, meta, instructions, jump_table }
    }
}


