use crate::dialect::{Dialect, Instruction, InstructionBuffer, InstructionParseError, Var};
use crate::memory::{Heap, Pointer, ARRAY_HEADER_SIZE, STRING_HEADER_SIZE};
use crate::parser::{
    parse_bytecode_file_new, FunctionTerm, InstructionTerm, ModuleTerm, ParserError, Term, TypeTerm,
};
use std::collections::{HashMap, HashSet};
use std::{convert::TryInto, fmt::Display};

#[derive(Default)]
pub struct ModuleLoader {
    dialects: HashMap<&'static str, Box<dyn Dialect>>,
    default_dialect: Option<&'static str>,
    /*
     * Each module has an accompanying module interface used to resolve calls.
     * An interface can exist without an implementation, to break cycles.
     * I.e. we can resolve calls in modules that depend on each other by
     * loading interfaces before loading implementations.
     */
    interfaces: Vec<ModuleInterface>,
    implementations: Vec<Option<Module>>,
}

impl ModuleLoader {
    pub fn new() -> ModuleLoader {
        let mut ml = ModuleLoader::default();
        ml
    }

    pub fn from_module(m: Module) -> ModuleLoader {
        let mut ml = ModuleLoader::default();
        ml.add_module(m);
        ml
    }

    pub fn from_module_file(filename: &str) -> Result<Self, ParserError> {
        let mut empty_loader = Self::default();
        empty_loader.load_module(filename)?;
        Ok(empty_loader)
    }

    pub fn set_default_dialect(&mut self, tag: &'static str) {
        self.default_dialect = Some(tag);
    }

    pub fn add_dialect(&mut self, d: Box<dyn Dialect>) {
        self.dialects.insert(d.get_dialect_tag(), d);
    }

    pub fn add_module(&mut self, mut m: Module) -> ModuleId {
        assert!(self.interfaces.len() != u16::MAX.into());

        m.id = self
            .implementations
            .len()
            .try_into()
            .expect("too many modules");

        let id = m.id;

        self.interfaces.push((&m).into());
        self.implementations.push(Some(m));

        id
    }

    pub fn get_module_by_id(&self, id: ModuleId) -> Option<&Module> {
        match self.implementations.get(id as usize).map(|o| o.as_ref()) {
            Some(Some(m)) => Some(m),
            _ => None,
        }
    }

    pub fn get_module_by_name(&self, name: &str) -> Option<&Module> {
        match self
            .implementations
            .iter()
            .find(|m| m.is_some() && m.as_ref().unwrap().name.as_str() == name)
        {
            Some(Some(m)) => Some(m),
            _ => None,
        }
    }

    pub fn make_type_from_term(&self, tterm: &TypeTerm) -> Type {
        match tterm {
            TypeTerm::Tuple(ts) => Type::Tuple(
                ts.into_iter()
                    .map(|t| self.make_type_from_term(t))
                    .collect(),
            ),
            TypeTerm::Array(inner, opt_len) => match opt_len {
                Some(len) => {
                    Type::sized_array(self.make_type_from_term(inner), (*len).try_into().unwrap())
                }
                None => Type::unsized_array(self.make_type_from_term(inner)),
            },
            TypeTerm::Name(n) => match n.as_str() {
                "ui64" => Type::U64,
                "str" => Type::Str(Size::Unsized),
                _ => panic!("Unknown type"),
            },
            TypeTerm::Fn(i, o) => Type::Fn(
                Box::from(self.make_type_from_term(i)),
                Box::from(self.make_type_from_term(o)),
            ),
            TypeTerm::Unit => Type::Unit,
            TypeTerm::Any => Type::Any,
            TypeTerm::Ref(t) => Type::Pointer(Box::from(self.make_type_from_term(t))),
        }
    }

    fn make_instruction_from_term(
        &self,
        module: &mut Module,
        t: &Term,
    ) -> Result<Box<dyn Instruction>, InstructionParseError> {
        let i = match t {
            Term::Instruction(i) => i,
            Term::LabeledInstruction(_, i) => i,
            _ => panic!(),
        };

        let dialect = i
            .dialect
            .clone()
            .unwrap_or(String::from(self.default_dialect.expect(
                "Instruction without dialect and no default dialect specified",
            )));

        match self.dialects.get(dialect.as_str()) {
            Some(dialect) => dialect.make_instruction(self, module, t),
            None => {
                panic!("Unknown dialect: {}", dialect);
            }
        }
    }

    fn make_function_from_term(&self, module: &mut Module, fterm: &FunctionTerm) -> Function {
        let fn_type_id: u16 = module.types.len().try_into().unwrap();
        module.types.push(TypeDecl::new(
            fn_type_id,
            self.make_type_from_term(&fterm.ty),
        ));

        let mut func = Function::new(fterm.name.clone(), TypeId(fn_type_id));

        let mut labels = HashMap::new();
        let mut instructions = Vec::new();
        for (i, term) in fterm.body.iter().enumerate() {
            match term {
                Term::LabeledInstruction(name, _) => {
                    assert!(name.len() == 1);
                    labels.insert(name[0].clone(), i);
                }
                _ => {}
            };

            let r = self.make_instruction_from_term(module, term);

            match r {
                Ok(i) => instructions.push(i),
                Err(e) => panic!("{}", e),
            }
        }

        // Quick and dirty, number of vars is maximum over all instruction reads and writes
        // It would be better to do some internal renaming so that gaps between var ids dont lead
        // to wasted memory
        let varcount_body = instructions.iter().fold(0, |acc, i| {
            u8::max(
                acc,
                i.writes()
                    .iter()
                    .chain(i.reads().iter())
                    .max_by(|l, r| l.0.cmp(&r.0))
                    .map(|v| v.0)
                    .unwrap_or(0),
            )
        }) + 1 /* because 0 indexed */;

        let varcount_params = fterm.params.iter().fold(0, |acc, i| u8::max(acc, i.0)) + 1;

        let varcount = u8::max(varcount_body, varcount_params);

        func.implementations.push(FunctionImpl::AST(ASTImpl {
            varcount,
            instructions,
            labels,
        }));

        func
    }

    fn make_module_from_term(&self, mterm: &ModuleTerm) -> Module {
        let mut m = Module::new(mterm.name.clone());

        let mut fns: Vec<Function> = mterm
            .functions
            .iter()
            .map(|f| self.make_function_from_term(&mut m, f))
            .collect();

        for f in fns.iter_mut().enumerate() {
            f.1.location = f.0.try_into().unwrap();
        }

        m.functions.append(&mut fns);

        m
    }

    pub fn load_module(&mut self, filename: &str) -> Result<ModuleId, ParserError> {
        match parse_bytecode_file_new(filename) {
            Ok(mterm) => {
                let module = self.make_module_from_term(&mterm);
                Ok(self.add_module(module))
            }
            Err(e) => Err(e),
        }
    }
}

#[derive(Debug, Clone)]
pub struct ModuleInterface {
    pub name: String,
    pub functions: HashSet<String>,
}

impl From<&Module> for ModuleInterface {
    fn from(module: &Module) -> Self {
        Self {
            name: module.name.clone(),
            functions: module.functions.iter().map(|f| f.name.clone()).collect(),
        }
    }
}

#[derive(Debug, Clone)]
pub struct Module {
    pub name: String,
    pub id: ModuleId,
    pub types: Vec<TypeDecl>,
    pub constants: Vec<ConstDecl>,
    pub functions: Vec<Function>,
}

impl Module {
    pub fn new(name: String) -> Module {
        Module {
            name,
            id: ModuleId::MAX,
            types: Vec::new(),
            constants: Vec::new(),
            functions: Vec::new(),
        }
    }

    pub fn from(
        name: String,
        types: Vec<TypeDecl>,
        consts: Vec<ConstDecl>,
        fns: Vec<Function>,
    ) -> Module {
        let mut module = Module::new(name);
        module.functions = fns;
        for (idx, func) in module.functions.iter_mut().enumerate() {
            func.location = idx.try_into().expect("Too many functions!");
        }

        module.constants = consts;
        module.types = types;

        module
    }

    // pub fn from_long_functions(
    //     module_loader: &ModuleLoader,
    //     name: String,
    //     mut types: Vec<TypeDecl>,
    //     consts: Vec<ConstDecl>,
    //     fns: Vec<LongFunction>,
    // ) -> Module {
    //     let mut compact_fns = Vec::new();

    //     let mut mapping = HashMap::new();
    //     for (idx, f) in fns.iter().enumerate() {
    //         mapping.insert(f.name.clone(), idx as u16);
    //     }

    //     for f in fns.into_iter() {
    //         compact_fns.push(Function::from_long_instr(
    //             f,
    //             module_loader,
    //             &mapping,
    //             &mut types,
    //         ));
    //     }

    //     let mut compact_consts = Vec::new();
    //     for c in consts.into_iter() {
    //         compact_consts.push(ConstDecl {
    //             idx: c.idx,
    //             type_: c.type_,
    //             value: c.value,
    //         });
    //     }

    //     Module::from(name, types, compact_consts, compact_fns)
    // }

    pub extern "C" fn write_constant_to_heap(
        heap: &mut Heap,
        ty: &Type,
        constant: &Constant,
    ) -> Pointer {
        let ptr = heap.allocate_type(&ty);

        match (&ty, &constant) {
            (Type::U64, Constant::U64(n)) => unsafe {
                heap.store::<u64>(ptr, *n);
            },
            (Type::Pointer(t), Constant::Str(string)) => match &**t {
                Type::Str(_) => unsafe {
                    heap.store_string(ptr, string);
                },
                _ => panic!("Invalid pointer type"),
            },
            _ => panic!("Invalid constant type"),
        }

        ptr
    }

    pub fn find_function_id(&self, name: &str) -> Option<FnId> {
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

    pub fn get_function_by_id(&self, lbl: FnId) -> Option<&Function> {
        Some(&self.functions[lbl as usize])
    }

    pub fn get_type_by_id(&self, lbl: TypeId) -> Option<&Type> {
        Some(
            self.types
                .iter()
                .find(|f| f.idx == lbl.0)
                .map(|x| &x.type_)?,
        )
    }

    pub fn get_constant_by_id(&self, lbl: ConstId) -> Option<&ConstDecl> {
        Some(self.constants.iter().find(|f| f.idx == lbl)?)
    }
}

/*
* Long datatypes that are parsed from files.
*/

// #[derive(Debug, Clone)]
// pub enum LongInstruction {
//     ConstU32(VarId, u32),
//     ConstBool(VarId, bool),
//     Copy(VarId, VarId),
//     CopyAddress(VarId, Option<String>, String),
//     CopyIntoIndex(VarId, u8, VarId),
//     CopyAddressIntoIndex(VarId, u8, Option<String>, String),
//     EqVarVar(VarId, VarId, VarId),
//     LtVarVar(VarId, VarId, VarId),
//     SubVarVar(VarId, VarId, VarId),
//     AddVarVar(VarId, VarId, VarId),
//     MulVarVar(VarId, VarId, VarId),
//     NotVar(VarId, VarId),

//     Return(VarId),
//     Print(VarId),

//     Call(VarId, String, Vec<VarId>),
//     ModuleCall(VarId, String, String, Vec<VarId>),
//     VarCall(VarId, VarId, Vec<VarId>),

//     Jmp(String),
//     JmpIf(String, VarId),
//     JmpIfNot(String, VarId),

//     Alloc(VarId, Type),
//     LoadConst(VarId, ConstId),
//     Sizeof(VarId, VarId),
//     LoadVar(VarId, VarId, Type),
//     StoreVar(VarId, VarId, Type),
//     Index(VarId, VarId, VarId, Type),
//     Tuple(VarId, u8),

//     Lbl(String),
//     Panic,
// }

// impl Into<Instruction> for LongInstruction {
//     fn into(self) -> Instruction {
//         use LongInstruction::*;

//         match self {
//             ConstU32(a, b) => Instruction::ConstU32(a, b),
//             ConstBool(a, b) => Instruction::ConstBool(a, b),
//             Copy(a, b) => Instruction::Copy(a, b),
//             EqVarVar(a, b, c) => Instruction::EqVarVar(a, b, c),
//             LtVarVar(a, b, c) => Instruction::LtVarVar(a, b, c),
//             SubVarVar(a, b, c) => Instruction::SubVarVar(a, b, c),
//             AddVarVar(a, b, c) => Instruction::AddVarVar(a, b, c),
//             MulVarVar(a, b, c) => Instruction::MulVarVar(a, b, c),
//             NotVar(a, b) => Instruction::NotVar(a, b),
//             Return(v) => Instruction::Return(v),
//             Print(v) => Instruction::Print(v),
//             Panic => Instruction::Panic,
//             Sizeof(a, b) => Instruction::Sizeof(a, b),
//             LoadConst(a, b) => Instruction::LoadConst(a, b),
//             Tuple(a, b) => Instruction::Tuple(a, b),
//             CopyIntoIndex(a, b, c) => Instruction::CopyIntoIndex(a, b, c),

//             s => panic!(
//                 "This instruction variant cannot be converted automatically: {:?}",
//                 s
//             ),
//         }
//     }
// }

// #[derive(Debug, Clone)]
// pub struct LongFunction {
//     pub name: String,
//     pub varcount: u8,
//     pub instructions: Vec<LongInstruction>,
//     pub type_: Type,
// }

// impl LongFunction {
//     pub fn new(
//         name: String,
//         varcount: u8,
//         instructions: Vec<LongInstruction>,
//         type_: Type,
//     ) -> Self {
//         Self {
//             name,
//             varcount,
//             instructions,
//             type_,
//         }
//     }
// }

#[derive(Debug, Clone)]
pub struct Param {
    pub var: Var,
    pub type_: Type,
}

#[derive(Debug, Clone)]
pub enum Constant {
    U64(u64),
    Str(String),
}

#[derive(Debug, Clone)]
pub struct ConstDecl {
    pub idx: u16,
    pub type_: Type,
    pub value: Constant,
}

#[derive(Debug, Clone)]
pub struct TypeDecl {
    pub idx: u16,
    pub type_: Type,
}

impl TypeDecl {
    pub fn new(idx: u16, ty: Type) -> TypeDecl {
        TypeDecl { idx, type_: ty }
    }
}

#[derive(Debug, Clone, PartialEq)]
pub enum Size {
    Unsized,
    Sized(usize),
}

#[derive(Debug, Clone, PartialEq)]
pub enum Type {
    U64,
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
            Self::U64 => 8,
            Self::Pointer(_) => 8,
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

/*
* Compact representations of the larger datatypes for interpretation and compilation
*/

pub type InstrLbl = i16;

/*
* Bunch of identifier declarations that we use to keep instructions compact.
* Generally we then use the id to access the full data somewhere else.
*/

#[derive(Debug, Clone, Copy)]
pub struct TypeId(pub u16);

pub type FnId = u16;
// ModuleId::MAX indicates the current module
pub type ModuleId = u16;
pub type ConstId = u16;

// We need repr(C) since we pass a CallTarget to the JIT::trampoline from compiled code
#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct CallTarget {
    pub module: ModuleId,
    pub function: FnId,
}

impl CallTarget {
    pub fn new(module: ModuleId, function: FnId) -> Self {
        Self { module, function }
    }
}

// #[derive(Debug, Clone, Copy)]
// pub enum Instruction {
//     ConstU32(VarId, u32),
//     ConstBool(VarId, bool),
//     Copy(VarId, VarId),
//     CopyAddress(VarId, CallTarget),
//     CopyIntoIndex(VarId, u8, VarId),
//     CopyAddressIntoIndex(VarId, u8, CallTarget),
//     EqVarVar(VarId, VarId, VarId),
//     LtVarVar(VarId, VarId, VarId),
//     SubVarVar(VarId, VarId, VarId),
//     AddVarVar(VarId, VarId, VarId),
//     MulVarVar(VarId, VarId, VarId),
//     NotVar(VarId, VarId),

//     Return(VarId),
//     Print(VarId),
//     CallN(VarId, FnId, VarId, u8),
//     ModuleCall(VarId, CallTarget, VarId),
//     VarCallN(VarId, VarId, VarId, u8),
//     Jmp(InstrLbl),
//     JmpIf(InstrLbl, VarId),
//     JmpIfNot(InstrLbl, VarId),

//     Tuple(VarId, u8),
//     Alloc(VarId, TypeId),
//     LoadConst(VarId, ConstId),
//     LoadVar(VarId, VarId, TypeId),
//     StoreVar(VarId, VarId, TypeId),
//     Index(VarId, VarId, VarId, TypeId),
//     Sizeof(VarId, VarId),
//     Lbl,
//     Nop,
//     Panic,
// }

// impl Instruction {
//     pub fn write_variables(&self) -> Vec<VarId> {
//         use Instruction::*;
//         match self {
//             ConstU32(r, _)
//             | ConstBool(r, _)
//             | LtVarVar(r, _, _)
//             | EqVarVar(r, _, _)
//             | Copy(r, _)
//             | CopyAddress(r, _)
//             | CopyIntoIndex(r, _, _)
//             | CopyAddressIntoIndex(r, _, _)
//             | SubVarVar(r, _, _)
//             | AddVarVar(r, _, _)
//             | MulVarVar(r, _, _)
//             | Alloc(r, _)
//             | LoadVar(r, _, _)
//             | NotVar(r, _)
//             | CallN(r, _, _, _)
//             | VarCallN(r, _, _, _)
//             | Index(r, _, _, _)
//             | LoadConst(r, _)
//             | Sizeof(r, _)
//             | Tuple(r, _)
//             | ModuleCall(r, _, _) => vec![*r],
//             JmpIfNot(_, _)
//             | JmpIf(_, _)
//             | Jmp(_)
//             | Return(_)
//             | Print(_)
//             | Nop
//             | Panic
//             | StoreVar(_, _, _)
//             | Lbl => vec![],
//         }
//     }

//     pub fn read_variables(&self) -> Vec<VarId> {
//         use Instruction::*;
//         match self {
//             LtVarVar(_, b, c)
//             | EqVarVar(_, b, c)
//             | SubVarVar(_, b, c)
//             | MulVarVar(_, b, c)
//             | StoreVar(b, c, _)
//             | Index(_, b, c, _)
//             | AddVarVar(_, b, c) => vec![*b, *c],
//             JmpIfNot(_, a)
//             | NotVar(_, a)
//             | ModuleCall(_, _, a)
//             | Return(a)
//             | Print(a)
//             | Sizeof(_, a)
//             | JmpIf(_, a)
//             | LoadVar(_, a, _)
//             | CopyIntoIndex(_, _, a)
//             | Copy(_, a) => vec![*a],
//             Lbl
//             | ConstU32(_, _)
//             | ConstBool(_, _)
//             | Jmp(_)
//             | Alloc(_, _)
//             | LoadConst(_, _)
//             | Tuple(_, _)
//             | Nop
//             | CopyAddress(_, _)
//             | CopyAddressIntoIndex(_, _, _)
//             | Panic => vec![],
//             CallN(_, _, a, b) => (*a..=(a + *b)).collect(),
//             VarCallN(_, t, a, b) => (*a..=(a + *b)).chain(vec![*t].into_iter()).collect(),
//         }
//     }

//     pub fn get_call_target(
//         &self,
//         state: &crate::interpreter::InterpreterState,
//         default_mod: ModuleId,
//     ) -> Option<CallTarget> {
//         use Instruction::*;
//         match self {
//             CallN(_, loc, _, _) => Some(CallTarget::new(default_mod, *loc)),
//             ModuleCall(_, ct, _) => Some(*ct),
//             VarCallN(_, loc, _, _) => Some(
//                 state
//                     .get_stack_var(*loc)
//                     .as_call_target()
//                     .expect("invalid type")
//                     .clone(),
//             ),
//             _ => None,
//         }
//     }
// }

// impl Display for Instruction {
//     fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
//         use Instruction::*;
//         match self {
//             ConstU32(a, b) => write!(f, "ui32 ${}, {}", a, b),
//             ConstBool(a, b) => write!(f, "bool ${}, {}", a, b),
//             Copy(a, b) => write!(f, "mov ${}, ${}", a, b),
//             EqVarVar(a, b, c) => write!(f, "eq ${}, ${}, ${}", a, b, c),
//             LtVarVar(a, b, c) => write!(f, "lt ${}, ${}, ${}", a, b, c),
//             SubVarVar(a, b, c) => write!(f, "sub ${}, ${}, ${}", a, b, c),
//             AddVarVar(a, b, c) => write!(f, "add ${}, ${}, ${}", a, b, c),
//             MulVarVar(a, b, c) => write!(f, "mul ${}, ${}, ${}", a, b, c),
//             Sizeof(a, b) => write!(f, "sizeof ${}, ${}", a, b),
//             NotVar(a, b) => write!(f, "neg ${}, ${}", a, b),
//             Return(a) => write!(f, "ret ${}", a),
//             Print(r) => write!(f, "print ${}", r),
//             CallN(a, b, c, d) => write!(f, "call ${}, @{} (${}..${})", a, b, c, d + c),
//             ModuleCall(a, b, c) => {
//                 write!(f, "call ${}, @{}:{} (${})", a, b.module, b.function, c)
//             }
//             Jmp(l) => write!(f, "jmp @{}", l),
//             JmpIf(l, c) => write!(f, "jmpif ${} @{}", c, l),
//             JmpIfNot(l, c) => write!(f, "jmpifn ${} @{}", c, l),
//             LoadConst(_, _) => todo!(),
//             LoadVar(a, b, c) => write!(f, "load ${}, ${}, %{}", a, b, c),
//             StoreVar(a, b, c) => write!(f, "store ${}, ${}, %{}", a, b, c),
//             Index(a, b, c, d) => write!(f, "idx ${}, ${}, ${}, %{}", a, b, c, d),
//             Alloc(a, b) => write!(f, "alloc ${}, %{}", a, b),
//             Panic => write!(f, "panic!"),
//             Lbl => write!(f, "label:"),
//             Nop => todo!(),
//             VarCallN(a, b, c, d) => write!(f, "call ${}, ${} (${}..${})", a, b, c, d + c),
//             Tuple(a, b) => write!(f, "tup ${}, ${}", a, b),
//             CopyAddressIntoIndex(a, b, c) => match c.module {
//                 ModuleId::MAX => write!(f, "movi ${}, {}, @{}", a, b, c.function),
//                 _ => write!(f, "movi ${}, {}, @{}:{}", a, b, c.module, c.function),
//             },
//             CopyIntoIndex(a, b, c) => write!(f, "mov ${}, {}, ${}", a, b, c),
//             CopyAddress(a, b) => match b.module {
//                 ModuleId::MAX => write!(f, "mov ${}, @{}", a, b.function),
//                 _ => write!(f, "mov ${}, @{}:{}", a, b.module, b.function),
//             },
//         }
//     }
// }

#[derive(Debug, Clone)]
pub struct Function {
    pub name: String,
    pub location: FnId,
    pub type_id: TypeId,
    pub implementations: Vec<FunctionImpl>,
}

impl Function {
    pub fn new(name: String, type_id: TypeId) -> Function {
        Function {
            name,
            location: FnId::MAX,
            type_id,
            implementations: vec![],
        }
    }

    pub fn new_native(name: String, type_id: TypeId, native: extern "C" fn() -> ()) -> Function {
        Function {
            name,
            location: FnId::MAX,
            type_id,
            implementations: vec![FunctionImpl::Native(NativeImpl { fn_ptr: native })],
        }
    }

    pub fn has_ast_impl(&self) -> bool {
        self.implementations.iter().find(|x| x.is_ast()).is_some()
    }

    pub fn has_bytecode_impl(&self) -> bool {
        self.implementations
            .iter()
            .find(|x| x.is_bytecode())
            .is_some()
    }

    pub fn has_native_impl(&self) -> bool {
        self.implementations
            .iter()
            .find(|x| x.is_native())
            .is_some()
    }

    pub fn ast_impl(&self) -> Option<&ASTImpl> {
        self.implementations
            .iter()
            .find(|x| x.is_ast())
            .map(|x| x.as_ast().unwrap())
    }

    pub fn bytecode_impl(&self) -> Option<&BytecodeImpl> {
        self.implementations
            .iter()
            .find(|x| x.is_bytecode())
            .map(|x| x.as_bytecode().unwrap())
    }

    pub fn native_impl(&self) -> Option<&NativeImpl> {
        self.implementations
            .iter()
            .find(|x| x.is_native())
            .map(|x| x.as_native().unwrap())
    }

    // /// Creates a new Function from the output of the bytecode parser,
    // /// converting long instructions to short (8 byte) instructions.
    // /// This loses some information in the process.
    // pub fn from_long_instr(
    //     long_fn: LongFunction,
    //     module_loader: &ModuleLoader,
    //     fns: &HashMap<String, FnId>,
    //     type_decls: &mut Vec<TypeDecl>,
    // ) -> Function {
    //     let mut jump_table: HashMap<String, usize> = HashMap::new();
    //     let mut pc: usize = 0;
    //     for instr in long_fn.instructions.iter() {
    //         match instr {
    //             LongInstruction::Lbl(name) => {
    //                 jump_table.insert(name.clone(), pc);
    //             }
    //             _ => {}
    //         }

    //         pc += 1;
    //     }

    //     let fn_type_id: u16 = type_decls.len().try_into().unwrap();
    //     type_decls.push(TypeDecl::new(fn_type_id, long_fn.type_.clone()));

    //     let mut compact_instructions = Vec::new();
    //     pc = 0;
    //     for instr in long_fn.instructions.iter() {
    //         match instr {
    //             LongInstruction::Jmp(n) => {
    //                 compact_instructions.push(Instruction::Jmp(
    //                     (jump_table[n] as isize - pc as isize).try_into().unwrap(),
    //                 ));
    //             }
    //             LongInstruction::JmpIf(n, e) => {
    //                 compact_instructions.push(Instruction::JmpIf(
    //                     (jump_table[n] as isize - pc as isize).try_into().unwrap(),
    //                     e.clone(),
    //                 ));
    //             }
    //             LongInstruction::JmpIfNot(n, e) => {
    //                 compact_instructions.push(Instruction::JmpIfNot(
    //                     (jump_table[n] as isize - pc as isize).try_into().unwrap(),
    //                     e.clone(),
    //                 ));
    //             }
    //             LongInstruction::Lbl(_) => {
    //                 compact_instructions.push(Instruction::Lbl);
    //             }
    //             LongInstruction::Call(res, n, vars) => {
    //                 let mut iter = vars.iter();

    //                 if let Some(mut prev) = iter.next() {
    //                     for i in iter {
    //                         assert!(*i == prev + 1);
    //                         prev = i;
    //                     }
    //                 }

    //                 compact_instructions.push(Instruction::CallN(
    //                     res.clone(),
    //                     fns[n].try_into().unwrap(),
    //                     *vars.get(0).unwrap_or(&0),
    //                     vars.len()
    //                         .try_into()
    //                         .expect(format!("too many arguments, max is {}", u8::MAX).as_str()),
    //                 ))
    //             }
    //             LongInstruction::VarCall(res, function, vars) => {
    //                 let mut iter = vars.iter();
    //                 if let Some(mut prev) = iter.next() {
    //                     for i in iter {
    //                         assert!(*i == prev + 1);
    //                         prev = i;
    //                     }
    //                 }

    //                 compact_instructions.push(Instruction::VarCallN(
    //                     res.clone(),
    //                     *function,
    //                     *vars.get(0).unwrap_or(&0),
    //                     vars.len()
    //                         .try_into()
    //                         .expect(format!("too many arguments, max is {}", u8::MAX).as_str()),
    //                 ))
    //             }
    //             LongInstruction::ModuleCall(res, module_name, function, vars) => {
    //                 let module = module_loader
    //                     .get_module_by_name(module_name.as_str())
    //                     .expect("missing module impl");

    //                 let function_id = module
    //                     .find_function_id(function.as_str())
    //                     .expect("missing fn");

    //                 compact_instructions.push(Instruction::ModuleCall(
    //                     res.clone(),
    //                     CallTarget::new(module.id, function_id),
    //                     vars[0],
    //                 ));
    //             }
    //             LongInstruction::LoadVar(res, v, ty) => {
    //                 let idx: u16 = type_decls.len().try_into().unwrap();
    //                 type_decls.push(TypeDecl::new(idx, ty.clone()));
    //                 compact_instructions.push(Instruction::LoadVar(*res, *v, idx));
    //             }
    //             LongInstruction::Alloc(res, ty) => {
    //                 let idx: u16 = type_decls.len().try_into().unwrap();
    //                 type_decls.push(TypeDecl::new(idx, ty.clone()));
    //                 compact_instructions.push(Instruction::Alloc(*res, idx));
    //             }
    //             LongInstruction::StoreVar(res_ptr, src, ty) => {
    //                 let idx: u16 = type_decls.len().try_into().unwrap();
    //                 type_decls.push(TypeDecl::new(idx, ty.clone()));
    //                 compact_instructions.push(Instruction::StoreVar(*res_ptr, *src, idx));
    //             }
    //             LongInstruction::Index(a, b, c, ty) => {
    //                 let idx: u16 = type_decls.len().try_into().unwrap();
    //                 type_decls.push(TypeDecl::new(idx, ty.clone()));
    //                 compact_instructions.push(Instruction::Index(*a, *b, *c, idx));
    //             }
    //             LongInstruction::CopyAddress(res, module_name, function_name) => {
    //                 let (module, function) = match module_name {
    //                     Some(module_name) => {
    //                         let module = module_loader
    //                             .get_module_by_name(module_name.as_str())
    //                             .expect("missing module impl");
    //                         let function = module
    //                             .find_function_id(function_name.as_str())
    //                             .expect("missing fn");

    //                         (module.id, function)
    //                     }
    //                     _ => (ModuleId::MAX, fns[function_name].try_into().unwrap()),
    //                 };

    //                 compact_instructions.push(Instruction::CopyAddress(
    //                     res.clone(),
    //                     CallTarget::new(module, function),
    //                 ));
    //             }
    //             LongInstruction::CopyAddressIntoIndex(res, idx, module_name, function_name) => {
    //                 let (module, function) = match module_name {
    //                     Some(module_name) => {
    //                         let module = module_loader
    //                             .get_module_by_name(module_name.as_str())
    //                             .expect("missing module impl");
    //                         let function = module
    //                             .find_function_id(function_name.as_str())
    //                             .expect("missing fn");

    //                         (module.id, function)
    //                     }
    //                     _ => (ModuleId::MAX, fns[function_name].try_into().unwrap()),
    //                 };

    //                 compact_instructions.push(Instruction::CopyAddressIntoIndex(
    //                     res.clone(),
    //                     *idx,
    //                     CallTarget::new(module, function),
    //                 ));
    //             }
    //             instr => compact_instructions.push(instr.clone().into()),
    //         }
    //         pc += 1;
    //     }

    //     assert_eq!(compact_instructions.len(), long_fn.instructions.len());

    //     Function::new(
    //         long_fn.name,
    //         long_fn.varcount,
    //         compact_instructions,
    //         fn_type_id,
    //     )
    // }

    /// Takes a map of liveness intervals and prints them neatly next to the instructions of this function.
    pub fn print_liveness(&self, intervals: &HashMap<Var, (InstrLbl, InstrLbl)>) {
        let implementation = self.ast_impl().expect("bytecode impl");

        let mut current_vars: Vec<Option<Var>> = Vec::new();

        for (idx, instr) in implementation.instructions.iter().enumerate() {
            // Find a location for variables used for the first time
            for entry in intervals.iter() {
                // First instr where var is live
                if idx == entry.1 .0 as usize {
                    let e = current_vars.iter_mut().find(|e| e.is_none());
                    // Put var in spot that is now empty
                    if e.is_some() {
                        *e.unwrap() = Some(*entry.0);
                    // No empty spots, create new
                    } else {
                        current_vars.push(Some(*entry.0));
                    }
                }
            }

            // Print locations of variables at current instruction
            let s = format!("{:<3} {}", idx, instr);
            print!("{:<30}", s);

            for v in current_vars.iter() {
                if v.is_some() {
                    print!("{:^4}", v.unwrap());
                } else {
                    print!("    ");
                }
            }

            // Remove variables that are not live anymore after this instruction
            for entry in intervals.iter() {
                // Last instr where var is live
                if idx == entry.1 .1 as usize {
                    let e = current_vars
                        .iter_mut()
                        .find(|v| v.is_some() && v.unwrap().eq(entry.0));
                    // "deallocate"
                    if e.is_some() {
                        *e.unwrap() = None;
                    }
                }
            }
            println!();
        }
    }
}

#[derive(Debug, Clone)]
pub enum FunctionImpl {
    AST(ASTImpl),
    Bytecode(BytecodeImpl),
    Native(NativeImpl),
}

impl FunctionImpl {
    /// Returns `true` if the function impl is [`Native`].
    ///
    /// [`Native`]: FunctionImpl::Native
    pub fn is_native(&self) -> bool {
        matches!(self, Self::Native(..))
    }

    pub fn as_native(&self) -> Option<&NativeImpl> {
        if let Self::Native(v) = self {
            Some(v)
        } else {
            None
        }
    }

    /// Returns `true` if the function impl is [`AST`].
    ///
    /// [`AST`]: FunctionImpl::AST
    pub fn is_ast(&self) -> bool {
        matches!(self, Self::AST(..))
    }

    pub fn as_ast(&self) -> Option<&ASTImpl> {
        if let Self::AST(v) = self {
            Some(v)
        } else {
            None
        }
    }

    /// Returns `true` if the function impl is [`Bytecode`].
    ///
    /// [`Bytecode`]: FunctionImpl::Bytecode
    pub fn is_bytecode(&self) -> bool {
        matches!(self, Self::Bytecode(..))
    }

    pub fn as_bytecode(&self) -> Option<&BytecodeImpl> {
        if let Self::Bytecode(v) = self {
            Some(v)
        } else {
            None
        }
    }
}

/// AST implementation of the function. This can be interpreted.
#[derive(Debug, Clone)]
pub struct ASTImpl {
    pub varcount: u8,
    pub instructions: Vec<Box<dyn Instruction>>,
    pub labels: HashMap<String, usize>,
}

/// Bytecode implementation of function. This implementation is generated as part of the JIT phase.
/// It is not directly interpreted but instead compiled further down to machine code.
#[derive(Debug, Clone)]
pub struct BytecodeImpl {
    pub varcount: u8,
    pub instructions: InstructionBuffer,
}

/// Native implementation. This implementation can be used to implement built-in (i.e. Rust) implementations
/// of functions. It is not the implementation generated by the JIT.
#[derive(Debug, Clone)]
pub struct NativeImpl {
    // We'll use transmute when calling so the in-out types don't matter
    // But maybe there is a better way to represent this?
    pub fn_ptr: extern "C" fn() -> (),
}
