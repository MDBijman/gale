use std::{collections::HashMap, fmt::Display};

use crate::{
    bytecode::{Function, Module, FunctionLocation},
    dialect::Var,
    memory::{Pointer, ARRAY_HEADER_SIZE, STRING_HEADER_SIZE},
    vm::VM,
};

#[derive(Debug, Clone, PartialEq)]
pub enum Size {
    Unsized,
    Sized(usize),
}

#[derive(Debug, Clone, PartialEq)]
pub enum Type {
    U64,
    Bool,
    Any,
    Array(Box<Type>, Size),
    Tuple(Vec<Type>),
    Pointer(Box<Type>),
    Fn(Box<Type>, Box<Type>),
    Str(Size),
    Unit,
}

impl Display for Type {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Type::U64 => write!(f, "ui64"),
            Type::Bool => write!(f, "bool"),
            Type::Str(_) => write!(f, "str"),
            Type::Any => write!(f, "?"),
            Type::Tuple(t) => {
                write!(f, "(")?;
                let mut i = t.iter();
                match i.next() {
                    Some(e) => {
                        write!(f, "{}", e)?;
                        for e in i {
                            write!(f, ", {}", e)?;
                        }
                    }
                    None => {}
                }
                write!(f, ")")
            }
            Type::Fn(i, o) => write!(f, "{} -> {}", i, o),
            Type::Pointer(r) => write!(f, "&{}", r),
            Type::Array(e, s) => match s {
                Size::Sized(size) => {
                    write!(f, "[{}; {}]", e, size)
                }
                Size::Unsized => write!(f, "[{}]", e),
            },
            Type::Unit => write!(f, "()"),
        }
    }
}

impl Type {
    pub fn sized_array(type_: Type, len: usize) -> Type {
        Type::Array(Box::from(type_), Size::Sized(len))
    }

    pub fn unsized_array(type_: Type) -> Type {
        Type::Array(Box::from(type_), Size::Unsized)
    }

    pub fn unsized_string() -> Type {
        Type::Str(Size::Unsized)
    }

    pub fn tuple(types: Vec<Type>) -> Type {
        Type::Tuple(types)
    }

    pub fn fun_n(from: Vec<Type>, to: Type) -> Type {
        Type::Fn(Box::from(Type::Tuple(from)), Box::from(to))
    }

    pub fn fun(from: Type, to: Type) -> Type {
        Type::Fn(Box::from(from), Box::from(to))
    }

    pub fn ptr(inner: Type) -> Type {
        Type::Pointer(Box::from(inner))
    }

    pub fn size(&self) -> usize {
        match self {
            Self::U64 => std::mem::size_of::<u64>(),
            Self::Pointer(_) => std::mem::size_of::<Pointer>(),
            Self::Array(_ty, len) => match len {
                Size::Unsized => panic!("Cannot get size of unsized array"),
                Size::Sized(len) => _ty.size() * len + ARRAY_HEADER_SIZE,
            },
            Self::Tuple(_tys) => _tys.iter().map(|t| t.size()).sum(),
            Self::Str(len) => match len {
                Size::Unsized => panic!("Cannot get size of unsized string"),
                Size::Sized(len) => len + STRING_HEADER_SIZE + 1, /* \0 byte */
            },
            _ => panic!("No such size"),
        }
    }
}

#[derive(Default, Debug, Clone)]
pub struct TypeEnvironment {
    pub types: HashMap<Var, Type>,
}

impl TypeEnvironment {
    pub fn get(&self, v: Var) -> Option<&Type> {
        self.types.get(&v)
    }

    pub fn insert(&mut self, v: Var, t: Type) {
        self.types.insert(v, t);
    }

    pub fn insert_or_check(&mut self, v: Var, t: Type) -> bool {
        match self.get(v) {
            Some(y) => {
                return t == *y;
            }
            None => {
                self.insert(v, t);
                return true;
            }
        }
    }
}

pub fn typecheck(vm: &VM, fun: FunctionLocation) -> Result<(), TypeError> {
    let mut t = TypeEnvironment::default();
    let (module, function) = vm.module_loader.lookup(fun).unwrap();

    let ast =  function.ast_impl().unwrap();

    for instr in ast.instructions.iter() {
        let r = instr.typecheck(vm, module, function, &mut t);
        if r.is_err() {
            return r;
        }
    }

    Ok(())
}

#[derive(Debug)]
pub enum TypeError {
    Mismatch { expected: Type, was: Type, var: Var },
    Missing { var: Var }
}

pub trait InstrTypecheck {
    fn typecheck(
        &self,
        vm: &VM,
        module: &Module,
        fun: &Function,
        env: &mut TypeEnvironment,
    ) -> Result<(), TypeError>;
}
