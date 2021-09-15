use std::collections::HashMap;
use std::convert::TryInto;
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

    pub fn from_long_functions(consts: Vec<Value>, fns: Vec<ParsedFunction>) -> Module {
        let mut compact_fns = Vec::new();

        let mut mapping = HashMap::new();
        for (idx, f) in fns.iter().enumerate() {
            mapping.insert(f.name.clone(), idx);
        }

        for f in fns.into_iter() {
            compact_fns.push(Function::from_long_instr(f, &mapping));
        }

        Module::from(consts, compact_fns)
    }

    pub fn find_function_id(&self, name: &str) -> Option<FnLbl> {
        Some(
            self.functions
                .iter()
                .position(|f| f.name == name)?
                .try_into()
                .unwrap(),
        )
    }

    pub fn get_function_by_name(&self, name: &str) -> Option<&Function> {
        Some(self.functions.iter().find(|f| f.name == name)?)
    }

    pub fn get_function_by_id(&self, lbl: FnLbl) -> Option<&Function> {
        Some(self.functions.iter().find(|f| f.location == lbl.into())?)
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

#[derive(Debug, Clone, PartialEq, PartialOrd)]
pub enum Value {
    U64(u64),
    Array(Vec<Value>),
    Pointer(u64),
    Null,
    Str(String),
}

impl Value {
    pub fn as_u64(&self) -> Option<&u64> {
        if let Self::U64(v) = self {
            Some(v)
        } else {
            None
        }
    }
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

pub type Var = u16;
pub type InstrLbl = i16;
pub type FnLbl = u16;
pub type CodeLbl = (FnLbl, InstrLbl);
pub type ConstLbl = u16;

#[derive(Debug, Clone, Copy)]
pub enum Instruction {
    ConstU32(Var, u32),
    Copy(Var, Var),
    EqVarVar(Var, Var, Var),
    LtVarVar(Var, Var, Var),
    SubVarVar(Var, Var, Var),
    SubVarU64(Var, Var, Var),
    AddVarVar(Var, Var, Var),
    NotVar(Var, Var),

    Return(Var),
    Print(Var),
    Call(Var, FnLbl, Var),
    Jmp(InstrLbl),
    JmpIf(InstrLbl, Var),
    JmpIfNot(InstrLbl, Var),

    //Alloc(Var, Type),
    LoadConst(Var, ConstLbl),
    LoadVar(Var, Var),
    StoreVar(Var, Var),
    Index(Var, Var, Var),
    Lbl,
    Nop,
}

#[derive(Debug, Clone)]
pub struct Param {
    pub var: Var,
    pub type_: Type,
}

#[derive(Debug, Clone)]
pub struct FunctionMeta {
    pub vars: Var,
}

#[derive(Debug, Clone)]
pub struct Function {
    pub name: String,
    pub location: usize,
    pub meta: FunctionMeta,
    pub instructions: Vec<Instruction>,
    pub has_native_impl: bool,
}

impl Function {
    pub fn new(name: String, meta: FunctionMeta, instructions: Vec<Instruction>) -> Function {
        Function {
            name,
            location: usize::MAX,
            meta,
            instructions,
            has_native_impl: false,
        }
    }

    pub fn from_long_instr(long_fn: ParsedFunction, fns: &HashMap<String, usize>) -> Function {
        let mut jump_table: HashMap<String, usize> = HashMap::new();
        let mut pc: usize = 0;
        for instr in long_fn.instructions.iter() {
            match instr {
                ParsedInstruction::Lbl(name) => {
                    jump_table.insert(name.clone(), pc);
                }
                _ => {}
            }

            pc += 1;
        }

        let mut compact_instructions = Vec::new();
        pc = 0;
        for (idx, instr) in long_fn.instructions.iter().enumerate() {
            match instr {
                ParsedInstruction::IndirectJmp(n) => {
                    compact_instructions.push(Instruction::Jmp(
                        (jump_table[n] as isize - pc as isize).try_into().unwrap(),
                    ));
                }
                ParsedInstruction::IndirectJmpIf(n, e) => {
                    compact_instructions.push(Instruction::JmpIf(
                        (jump_table[n] as isize - pc as isize).try_into().unwrap(),
                        e.clone(),
                    ));
                }
                ParsedInstruction::IndirectJmpIfNot(n, e) => {
                    compact_instructions.push(Instruction::JmpIfNot(
                        (jump_table[n] as isize - pc as isize).try_into().unwrap(),
                        e.clone(),
                    ));
                }
                ParsedInstruction::Lbl(lbl) => {
                    compact_instructions.push(Instruction::Lbl);
                }
                ParsedInstruction::Instr(instr) => compact_instructions.push(instr.clone()),
                ParsedInstruction::IndirectCall(res, n, vars) => compact_instructions.push(
                    Instruction::Call(res.clone(), fns[n].try_into().unwrap(), vars[0].clone()),
                ),
            }

            pc += 1;
        }

        assert_eq!(compact_instructions.len(), long_fn.instructions.len());

        Function::new(
            long_fn.name,
            FunctionMeta {
                vars: long_fn.meta.vars,
            },
            compact_instructions,
        )
    }
}

#[derive(Debug, Clone)]
pub struct ParsedFunctionMeta {
    pub vars: Var,
}

#[derive(Debug, Clone)]
pub enum ParsedInstruction {
    Instr(Instruction),
    IndirectCall(Var, String, Vec<Var>),
    IndirectJmp(String),
    IndirectJmpIf(String, Var),
    IndirectJmpIfNot(String, Var),
    Lbl(String),
}

#[derive(Debug, Clone)]
pub struct ParsedFunction {
    pub name: String,
    pub meta: ParsedFunctionMeta,
    pub instructions: Vec<ParsedInstruction>,
}

impl ParsedFunction {
    pub fn new(
        name: String,
        meta: ParsedFunctionMeta,
        instructions: Vec<ParsedInstruction>,
    ) -> Self {
        Self {
            name,
            meta,
            instructions,
        }
    }
}
