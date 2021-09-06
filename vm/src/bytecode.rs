use std::collections::HashMap;
use std::fmt;

#[derive(Debug, Clone)]
pub struct Module {
    pub constants: Vec<Value>,
    pub functions: Vec<Function>,
}

impl Module {
    pub fn new() -> Module {
        Module {
            constants: Vec::new(),
            functions: Vec::new(),
        }
    }

    pub fn from(consts: Vec<Value>, fns: Vec<Function>) -> Module {
        let mut module = Module::new();
        module.functions = fns; 
        for (idx, func) in module.functions.iter_mut().enumerate() {
            func.location = idx;
        }

        module.constants = consts;

        module
    }


    pub fn compute_direct_function_calls(&mut self) {
        let mut mapping = HashMap::new();
        for f in self.functions.iter() {
            mapping.insert(f.name.clone(), f.location);
        }

        for f in self.functions.iter_mut() {
            for i in f.instructions.iter_mut() {
                match i {
                    Instruction::IndirectCall(res, name, exprs) => {
                        *i = Instruction::DirectCall(res.clone(), Location::FnLbl(*mapping.get(name).unwrap()), exprs.to_vec());
                    },
                    _ => {}
                }
            }
        }
    }

}

#[derive(Debug, Clone)]
pub enum Type {
    U64(),
    Array(Box<Type>),
    Pointer(Box<Type>),
    Null,
    Str,
}

#[derive(Debug, Clone)]
pub enum Value {
    U64(u64),
    Array(Vec<Value>),
    Pointer(u64),
    Null,
    Str(String),
}

#[derive(Debug, Clone)]
pub enum UnOp {
    Not,
}

#[derive(Debug, Clone)]
pub enum BinOp {
    Add,
    Sub,
    Eq,
    Lt,
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
            Value::Null => write!(f, "null"),
            Value::Pointer(b) => write!(f, "&{}", b),
            Value::Str(s) => write!(f, "\"{}\"", s),
        }
    }
}

#[derive(Debug, Clone)]
pub enum Expression {
    Var(Location),
    Value(Value),
    Array(Vec<Expression>),
}

#[derive(Debug, Clone)]
pub enum Location {
    Var(u64),
    Lbl(String),
    FnLbl(usize),
}

#[derive(Debug, Clone)]
pub enum Instruction {
    Set(Location, Expression),
    Return(Expression),
    Print(Expression),
    IndirectCall(Location, String, Vec<Expression>),
    DirectCall(Location, Location, Vec<Expression>),
    Jmp(Location),
    JmpIf(Location, Expression),
    JmpIfNot(Location, Expression),
    Lbl(String),

    Pop(Location),
    Push(Expression),
    BinOp(BinOp),
    UnOp(UnOp),

    Alloc(Location, Type),
    Load(Location, Location),
    LoadConst(Location, u64),
    Store(Location, Expression),
    Index(Location, Location, Location),
}

#[derive(Debug, Clone)]
pub struct Parameter {
    pub location: Location,
}

#[derive(Debug, Clone)]
pub struct FunctionMeta {
    pub vars: u64,
}

#[derive(Debug, Clone)]
pub struct Function {
    pub name: String,
    pub location: usize,
    pub meta: FunctionMeta,
    pub instructions: Vec<Instruction>,
    pub jump_table: HashMap<String, u64>,
}

impl Function {
    pub fn new(
        name: String,
        meta: FunctionMeta,
        instructions: Vec<Instruction>,
        jump_table: HashMap<String, u64>,
    ) -> Function {
        Function {
            name,
            location: usize::MAX,
            meta,
            instructions,
            jump_table,
        }
    }
}
