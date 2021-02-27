use std::collections::HashMap;
use std::fmt;

#[derive(Debug)]
pub struct Module {
    pub functions: HashMap<String, Function>
}

impl Module {
    pub fn new() -> Module {
        Module { functions: HashMap::new() }
    }

    pub fn from(fns: Vec<Function>) -> Module {
        let mut module = Module::new();
        for func in fns {
            module.functions.insert(func.name.clone(), func);
        }

        module
    }
}

#[derive(Debug, Clone)]
pub enum Value {
    U64(u64),
    Array(Vec<Value>)
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
            }
        }
    }
}

#[derive(Debug)]
pub enum Expression {
    Var(Location),
    Array(Vec<Expression>),
    Value(Value),
}

#[derive(Debug)]
pub enum Location {
    Var(u64)
}

#[derive(Debug)]
pub enum Instruction {
    Store(Location, Expression),
    Return(Expression),
    Print(Expression),
    Call(Location, String, Vec<Expression>)
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
    pub instructions: Vec<Instruction>
}

impl Function {
    pub fn new(name: String, meta: FunctionMeta, instrs: Vec<Instruction>) -> Function {
        Function {
            name: name,
            meta: meta,
            instructions: instrs
        }
    }
}