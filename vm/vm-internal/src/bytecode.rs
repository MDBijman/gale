use crate::dialect::{Dialect, Instruction, InstructionBuffer, InstructionParseError, Var, LabelInstruction};
use crate::memory::{Heap, Pointer};
use crate::parser::{
    parse_bytecode_file_new, FunctionTerm, ModuleTerm, ParserError, Term, TypeTerm,
};
use crate::typechecker::{TypeEnvironment, Type, Size};
use std::collections::{HashMap, HashSet};
use std::{convert::TryInto};

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
        ModuleLoader::default()
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
            Term::Label(n) => {
               return Ok(Box::from(LabelInstruction(n.clone())));
            },
            t => panic!("unexpected {:?}", t),
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
                Term::Label(n) => {
                    assert!(n.len() == 1);
                    labels.insert(n[0].clone(), i);
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

    pub fn get_function_by_name_mut(&mut self, name: &str) -> Option<&mut Function> {
        Some(self.functions.iter_mut().find(|f| f.name == name)?)
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

#[derive(Debug, Clone)]
pub struct Function {
    pub name: String,
    pub location: FnId,
    pub type_id: TypeId,
    pub variable_types: Option<TypeEnvironment>,
    pub implementations: Vec<FunctionImpl>,
}

impl Function {
    pub fn new(name: String, type_id: TypeId) -> Function {
        Function {
            name,
            location: FnId::MAX,
            type_id,
            variable_types: None,
            implementations: vec![],
        }
    }

    pub fn new_native(name: String, type_id: TypeId, native: extern "C" fn() -> ()) -> Function {
        Function {
            name,
            location: FnId::MAX,
            type_id,
            variable_types: None,
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

    pub fn is_typechecked(&self) -> bool {
        self.variable_types.is_some()
    }

    pub fn variable_types(&self) -> Option<&TypeEnvironment> {
        self.variable_types.as_ref()
    }

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
