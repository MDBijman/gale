use crate::memory::{Heap, Pointer, ARRAY_HEADER_SIZE, STRING_HEADER_SIZE};
use crate::parser::parse_bytecode_file;
use std::collections::{HashMap, HashSet};
use std::convert::TryInto;
use std::fmt::{self, Display};

pub struct ModuleLoader {
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
    pub fn add(&mut self, mut m: Module) -> ModuleId {
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

    pub fn get_by_id(&self, id: ModuleId) -> Option<Option<&Module>> {
        self.implementations.get(id as usize).map(|o| o.as_ref())
    }

    pub fn get_by_name(&self, name: &str) -> Option<&Option<Module>> {
        self.implementations
            .iter()
            .find(|m| m.is_some() && m.as_ref().unwrap().name.as_str() == name)
    }

    pub fn from_module(m: Module) -> ModuleLoader {
        let mut ml = ModuleLoader::default();
        ml.add(m);
        ml
    }

    pub fn load_module(&mut self, filename: &str) -> ModuleId {
        let module = parse_bytecode_file(&self, filename).expect("module load");
        self.add(module)
    }

    pub fn from_module_file(filename: &str) -> Self {
        let mut empty_loader = Self::default();
        empty_loader.load_module(filename);
        empty_loader
    }
}

impl Default for ModuleLoader {
    fn default() -> Self {
        Self {
            interfaces: Default::default(),
            implementations: Default::default(),
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

    pub fn from_long_functions(
        module_loader: &ModuleLoader,
        name: String,
        mut types: Vec<TypeDecl>,
        consts: Vec<ConstDecl>,
        fns: Vec<LongFunction>,
    ) -> Module {
        let mut compact_fns = Vec::new();

        let mut mapping = HashMap::new();
        for (idx, f) in fns.iter().enumerate() {
            mapping.insert(f.name.clone(), idx as u16);
        }

        for f in fns.into_iter() {
            compact_fns.push(Function::from_long_instr(
                f,
                module_loader,
                &mapping,
                &mut types,
            ));
        }

        let mut compact_consts = Vec::new();
        for c in consts.into_iter() {
            compact_consts.push(ConstDecl {
                idx: c.idx,
                type_: c.type_,
                value: c.value,
            });
        }

        Module::from(name, types, compact_consts, compact_fns)
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

    pub fn get_function_by_id(&self, lbl: FnId) -> Option<&Function> {
        Some(&self.functions[lbl as usize])
    }

    pub fn get_type_by_id(&self, lbl: TypeId) -> Option<&Type> {
        Some(self.types.iter().find(|f| f.idx == lbl).map(|x| &x.type_)?)
    }

    pub fn get_constant_by_id(&self, lbl: ConstId) -> Option<&ConstDecl> {
        Some(self.constants.iter().find(|f| f.idx == lbl)?)
    }
}

/*
* Long datatypes that are parsed from files.
*/

#[derive(Debug, Clone)]
pub enum LongInstruction {
    ConstU32(VarId, u32),
    Copy(VarId, VarId),
    EqVarVar(VarId, VarId, VarId),
    LtVarVar(VarId, VarId, VarId),
    SubVarVar(VarId, VarId, VarId),
    AddVarVar(VarId, VarId, VarId),
    NotVar(VarId, VarId),

    Return(VarId),
    Print(VarId),
    Call(VarId, String, VarId),
    ModuleCall(VarId, String, String, VarId),
    Jmp(String),
    JmpIf(String, VarId),
    JmpIfNot(String, VarId),

    Alloc(VarId, Type),
    LoadConst(VarId, ConstId),
    LoadVar(VarId, VarId, Type),
    StoreVar(VarId, VarId, Type),
    Index(VarId, VarId, VarId, Type),

    Lbl(String),
}

impl Into<Instruction> for LongInstruction {
    fn into(self) -> Instruction {
        use LongInstruction::*;

        match self {
            ConstU32(a, b) => Instruction::ConstU32(a, b),
            Copy(a, b) => Instruction::Copy(a, b),
            EqVarVar(a, b, c) => Instruction::EqVarVar(a, b, c),
            LtVarVar(a, b, c) => Instruction::LtVarVar(a, b, c),
            SubVarVar(a, b, c) => Instruction::SubVarVar(a, b, c),
            AddVarVar(a, b, c) => Instruction::AddVarVar(a, b, c),
            NotVar(a, b) => Instruction::NotVar(a, b),
            Return(v) => Instruction::Return(v),
            Print(v) => Instruction::Print(v),
            LoadConst(a, b) => Instruction::LoadConst(a, b),
            s => panic!(
                "This instruction variant cannot be converted automatically: {:?}",
                s
            ),
        }
    }
}

#[derive(Debug, Clone)]
pub struct LongFunction {
    pub name: String,
    pub varcount: u8,
    pub instructions: Vec<LongInstruction>,
    pub type_: Type,
}

impl LongFunction {
    pub fn new(
        name: String,
        varcount: u8,
        instructions: Vec<LongInstruction>,
        type_: Type,
    ) -> Self {
        Self {
            name,
            varcount,
            instructions,
            type_,
        }
    }
}

#[derive(Debug, Clone)]
pub struct Param {
    pub var: VarId,
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
    Array(Box<Type>, Size),
    Tuple(Vec<Type>),
    Pointer(Box<Type>),
    Fn(Box<Type>, Box<Type>),
    Str(Size),
    Unit,
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

pub type VarId = u8;
pub type TypeId = u16;
pub type FnId = u16;
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

#[derive(Debug, Clone, Copy)]
pub enum Instruction {
    ConstU32(VarId, u32),
    Copy(VarId, VarId),
    EqVarVar(VarId, VarId, VarId),
    LtVarVar(VarId, VarId, VarId),
    SubVarVar(VarId, VarId, VarId),
    AddVarVar(VarId, VarId, VarId),
    NotVar(VarId, VarId),

    Return(VarId),
    Print(VarId),
    Call(VarId, FnId, VarId),
    ModuleCall(VarId, CallTarget, VarId),
    Jmp(InstrLbl),
    JmpIf(InstrLbl, VarId),
    JmpIfNot(InstrLbl, VarId),

    Alloc(VarId, TypeId),
    LoadConst(VarId, ConstId),
    LoadVar(VarId, VarId, TypeId),
    StoreVar(VarId, VarId, TypeId),
    Index(VarId, VarId, VarId, TypeId),
    Lbl,
    Nop,
}

impl Instruction {
    pub fn write_variables(&self) -> Vec<VarId> {
        use Instruction::*;
        match self {
            ConstU32(r, _)
            | LtVarVar(r, _, _)
            | EqVarVar(r, _, _)
            | Copy(r, _)
            | SubVarVar(r, _, _)
            | AddVarVar(r, _, _)
            | Alloc(r, _)
            | LoadVar(r, _, _)
            | NotVar(r, _)
            | Call(r, _, _)
            | Index(r, _, _, _)
            | LoadConst(r, _)
            | ModuleCall(r, _, _) => vec![*r],
            JmpIfNot(_, _)
            | JmpIf(_, _)
            | Jmp(_)
            | Return(_)
            | Print(_)
            | Nop
            | StoreVar(_, _, _)
            | Lbl => vec![],
        }
    }

    pub fn read_variables(&self) -> Vec<VarId> {
        use Instruction::*;
        match self {
            LtVarVar(_, b, c)
            | EqVarVar(_, b, c)
            | SubVarVar(_, b, c)
            | StoreVar(b, c, _)
            | Index(_, b, c, _)
            | AddVarVar(_, b, c) => vec![*b, *c],
            JmpIfNot(_, a)
            | NotVar(_, a)
            | Call(_, _, a)
            | ModuleCall(_, _, a)
            | Return(a)
            | Print(a)
            | JmpIf(_, a)
            | LoadVar(_, a, _)
            | Copy(_, a) => vec![*a],
            Lbl | ConstU32(_, _) | Jmp(_) | Alloc(_, _) | LoadConst(_, _) | Nop => vec![],
        }
    }

    pub fn get_call_target(&self, default_mod: ModuleId) -> Option<CallTarget> {
        use Instruction::*;
        match self {
            Call(_, loc, _) => Some(CallTarget::new(default_mod, *loc)),
            ModuleCall(_, ct, _) => Some(*ct),
            _ => None,
        }
    }
}

impl Display for Instruction {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        use Instruction::*;
        match self {
            ConstU32(a, b) => write!(f, "movc ${}, {}", a, b),
            Copy(a, b) => write!(f, "cp ${}, {}", a, b),
            EqVarVar(a, b, c) => write!(f, "eq ${}, ${}, ${}", a, b, c),
            LtVarVar(a, b, c) => write!(f, "lt ${}, ${}, ${}", a, b, c),
            SubVarVar(a, b, c) => write!(f, "sub ${}, ${}, ${}", a, b, c),
            AddVarVar(a, b, c) => write!(f, "add ${}, ${}, ${}", a, b, c),
            NotVar(_, _) => todo!(),
            Return(a) => write!(f, "ret ${}", a),
            Print(r) => write!(f, "print ${}", r),
            Call(a, b, c) => write!(f, "call ${}, @{} (${})", a, b, c),
            ModuleCall(a, b, c) => {
                write!(f, "call ${}, @{}:{} (${})", a, b.module, b.function, c)
            }
            Jmp(l) => write!(f, "jmp @{}", l),
            JmpIf(l, c) => write!(f, "jmpif ${} @{}", c, l),
            JmpIfNot(l, c) => write!(f, "jmpifn ${} @{}", c, l),
            LoadConst(_, _) => todo!(),
            LoadVar(a, b, c) => write!(f, "load ${}, ${}, %{}", a, b, c),
            StoreVar(a, b, c) => write!(f, "store ${}, ${}, %{}", a, b, c),
            Index(_, _, _, _) => todo!(),
            Alloc(a, b) => write!(f, "alloc ${}, %{}", a, b),
            Lbl => write!(f, "label:"),
            Nop => todo!(),
        }
    }
}

#[derive(Debug, Clone)]
pub struct Function {
    pub name: String,
    pub location: FnId,
    pub type_id: TypeId,
    pub implementation: FunctionImpl,
}

impl Function {
    pub fn new(
        name: String,
        varcount: u8,
        instructions: Vec<Instruction>,
        type_id: TypeId,
    ) -> Function {
        Function {
            name,
            location: FnId::MAX,
            type_id,
            implementation: FunctionImpl::Bytecode(BytecodeImpl {
                varcount,
                instructions,
                has_native_impl: false,
            }),
        }
    }

    pub fn new_native(name: String, type_id: TypeId, native: extern "C" fn() -> ()) -> Function {
        Function {
            name,
            location: FnId::MAX,
            type_id,
            implementation: FunctionImpl::Native(NativeImpl { fn_ptr: native }),
        }
    }

    /// Creates a new Function from the output of the bytecode parser,
    /// converting long instructions to short (8 byte) instructions.
    /// This loses some information in the process.
    pub fn from_long_instr(
        long_fn: LongFunction,
        module_loader: &ModuleLoader,
        fns: &HashMap<String, FnId>,
        type_decls: &mut Vec<TypeDecl>,
    ) -> Function {
        let mut jump_table: HashMap<String, usize> = HashMap::new();
        let mut pc: usize = 0;
        for instr in long_fn.instructions.iter() {
            match instr {
                LongInstruction::Lbl(name) => {
                    jump_table.insert(name.clone(), pc);
                }
                _ => {}
            }

            pc += 1;
        }

        let fn_type_id: u16 = type_decls.len().try_into().unwrap();
        type_decls.push(TypeDecl::new(fn_type_id, long_fn.type_.clone()));

        let mut compact_instructions = Vec::new();
        pc = 0;
        for instr in long_fn.instructions.iter() {
            match instr {
                LongInstruction::Jmp(n) => {
                    compact_instructions.push(Instruction::Jmp(
                        (jump_table[n] as isize - pc as isize).try_into().unwrap(),
                    ));
                }
                LongInstruction::JmpIf(n, e) => {
                    compact_instructions.push(Instruction::JmpIf(
                        (jump_table[n] as isize - pc as isize).try_into().unwrap(),
                        e.clone(),
                    ));
                }
                LongInstruction::JmpIfNot(n, e) => {
                    compact_instructions.push(Instruction::JmpIfNot(
                        (jump_table[n] as isize - pc as isize).try_into().unwrap(),
                        e.clone(),
                    ));
                }
                LongInstruction::Lbl(_) => {
                    compact_instructions.push(Instruction::Lbl);
                }
                LongInstruction::Call(res, n, var) => compact_instructions.push(Instruction::Call(
                    res.clone(),
                    fns[n].try_into().unwrap(),
                    *var,
                )),
                LongInstruction::ModuleCall(res, module_name, function, var) => {
                    let module = module_loader
                        .get_by_name(module_name.as_str())
                        .expect("missing module")
                        .as_ref()
                        .expect("missing impl");

                    let function_id = module
                        .find_function_id(function.as_str())
                        .expect("missing fn");

                    compact_instructions.push(Instruction::ModuleCall(
                        res.clone(),
                        CallTarget::new(module.id, function_id),
                        *var,
                    ));
                }
                LongInstruction::LoadVar(res, v, ty) => {
                    let idx: u16 = type_decls.len().try_into().unwrap();
                    type_decls.push(TypeDecl::new(idx, ty.clone()));
                    compact_instructions.push(Instruction::LoadVar(*res, *v, idx));
                }
                LongInstruction::Alloc(res, ty) => {
                    let idx: u16 = type_decls.len().try_into().unwrap();
                    type_decls.push(TypeDecl::new(idx, ty.clone()));
                    compact_instructions.push(Instruction::Alloc(*res, idx));
                }
                LongInstruction::StoreVar(res_ptr, src, ty) => {
                    let idx: u16 = type_decls.len().try_into().unwrap();
                    type_decls.push(TypeDecl::new(idx, ty.clone()));
                    compact_instructions.push(Instruction::StoreVar(*res_ptr, *src, idx));
                }
                LongInstruction::Index(a, b, c, ty) => {
                    let idx: u16 = type_decls.len().try_into().unwrap();
                    type_decls.push(TypeDecl::new(idx, ty.clone()));
                    compact_instructions.push(Instruction::Index(*a, *b, *c, idx));
                }
                instr => compact_instructions.push(instr.clone().into()),
            }
            pc += 1;
        }

        assert_eq!(compact_instructions.len(), long_fn.instructions.len());

        Function::new(
            long_fn.name,
            long_fn.varcount,
            compact_instructions,
            fn_type_id,
        )
    }

    /// For each variable, computes the first and last instruction in which it is used (write or read).
    pub fn liveness_intervals(&self) -> HashMap<VarId, (InstrLbl, InstrLbl)> {
        let implementation = self.implementation.as_bytecode().expect("bytecode impl");
        let mut intervals: HashMap<VarId, (InstrLbl, InstrLbl)> = HashMap::new();

        intervals.insert(0, (0, 0));

        for (idx, instr) in implementation.instructions.iter().enumerate() {
            let reads_from = instr.read_variables();
            let writes_to = instr.write_variables();

            for var in reads_from {
                let entry = intervals
                    .entry(var)
                    .or_insert((idx as InstrLbl, idx as InstrLbl));
                entry.1 = idx as InstrLbl;
            }

            for var in writes_to {
                let entry = intervals
                    .entry(var)
                    .or_insert((idx as InstrLbl, idx as InstrLbl));
                entry.1 = idx as InstrLbl;
            }
        }

        intervals
    }

    /// Takes a map of liveness intervals and prints them neatly next to the instructions of this function.
    pub fn print_liveness(&self, intervals: &HashMap<VarId, (InstrLbl, InstrLbl)>) {
        let implementation = self.implementation.as_bytecode().expect("bytecode impl");

        let mut current_vars: Vec<Option<VarId>> = Vec::new();

        for (idx, instr) in implementation.instructions.iter().enumerate() {
            for entry in intervals.iter() {
                // First instr where var is live
                if idx == entry.1 .0 as usize {
                    let e = current_vars.iter_mut().find(|e| e.is_none());
                    if e.is_some() {
                        *e.unwrap() = Some(*entry.0);
                    } else {
                        current_vars.push(Some(*entry.0));
                    }
                }
            }

            let s = format!("{}", instr);
            print!("{:<20}", s);

            for v in current_vars.iter() {
                if v.is_some() {
                    print!("{:^4}", v.unwrap());
                } else {
                    print!("    ");
                }
            }

            for entry in intervals.iter() {
                // Last instr where var is live
                if idx == entry.1 .1 as usize {
                    let e = current_vars
                        .iter_mut()
                        .find(|v| v.is_some() && v.unwrap().eq(entry.0));
                    // "deallocate"
                    *e.unwrap() = None;
                }
            }
            println!();
        }
    }
}

#[derive(Debug, Clone)]
pub enum FunctionImpl {
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

#[derive(Debug, Clone)]
pub struct BytecodeImpl {
    pub varcount: u8,
    pub has_native_impl: bool,
    pub instructions: Vec<Instruction>,
}

#[derive(Debug, Clone)]
pub struct NativeImpl {
    // We'll use transmute when calling so the in-out types don't matter
    // But maybe there is a better way to represent this?
    pub fn_ptr: extern "C" fn() -> (),
}

pub struct ControlFlowGraph {
    pub blocks: Vec<BasicBlock>,
    pub from_fn: FnId,
}

pub struct BasicBlock {
    pub first: InstrLbl,
    pub last: InstrLbl,
    pub children: Vec<usize>,
    pub parents: Vec<usize>,
}

impl BasicBlock {
    pub fn starting_from(starting_at: InstrLbl) -> Self {
        Self {
            first: starting_at,
            last: starting_at,
            children: Vec::new(),
            parents: Vec::new(),
        }
    }

    pub fn extend_one(&mut self) {
        self.last += 1;
    }

    pub fn add_child(&mut self, idx: usize) {
        self.children.push(idx);
    }

    pub fn add_parent(&mut self, idx: usize) {
        self.parents.push(idx);
    }
}

impl ControlFlowGraph {
    pub fn from_function(fun: &Function) -> Self {
        let implementation = fun.implementation.as_bytecode().expect("bytecode impl");

        // Create basic blocks
        // and edges between basic blocks
        // Greedily eat instructions and push into current basic block
        // When encountering a jump or label:
        //   create new basic block and repeat
        // Link up basic blocks - how?/

        let mut basic_blocks = Vec::new();

        let mut pc = 0;
        let mut current_block = BasicBlock::starting_from(pc);

        for instr in implementation.instructions.iter() {
            match instr {
                Instruction::Return(_) => {
                    basic_blocks.push(current_block);
                    current_block = BasicBlock::starting_from(pc + 1);
                }
                Instruction::Jmp(_) => {
                    basic_blocks.push(current_block);
                    current_block = BasicBlock::starting_from(pc + 1);
                }
                Instruction::JmpIf(_, _) => {
                    basic_blocks.push(current_block);
                    current_block = BasicBlock::starting_from(pc + 1);
                }
                Instruction::JmpIfNot(_, _) => {
                    basic_blocks.push(current_block);
                    current_block = BasicBlock::starting_from(pc + 1);
                }
                Instruction::Lbl => {
                    // If this is not the first in the block, we need to make a new block
                    if current_block.first != pc {
                        // Remove this label from this current_block
                        current_block.last -= 1;

                        // Create a new block starting from this lbl
                        // do not replace current_block yet, need to update its children
                        let mut new_block = BasicBlock::starting_from(pc);

                        // We were not the first in the block, so previous instruction
                        // was not a jump, so this is child of last block
                        // current_block will be at basic_blocks.len, new block adds 1
                        current_block.add_child(basic_blocks.len() + 1);
                        new_block.add_parent(basic_blocks.len());

                        // Commit the current_block
                        basic_blocks.push(current_block);

                        // Replace the current with the new
                        current_block = new_block;

                        // Extend one
                        current_block.extend_one();
                    } else {
                        current_block.extend_one();
                    }
                }
                _ => {
                    current_block.extend_one();
                }
            }

            pc += 1;
        }

        fn find_basic_block_starting_at(blocks: &Vec<BasicBlock>, pc: i16) -> usize {
            blocks
                .iter()
                .position(|x| x.first == pc)
                .expect("Basic block doesn't exist")
        }

        fn find_basic_block_ending_at(blocks: &Vec<BasicBlock>, pc: i16) -> usize {
            blocks
                .iter()
                .position(|x| x.last == pc)
                .expect("Basic block doesn't exist")
        }

        for (pc, instr) in implementation.instructions.iter().enumerate() {
            match instr {
                Instruction::Jmp(l) => {
                    let target = l + pc as i16;
                    let child = find_basic_block_starting_at(&basic_blocks, target);
                    let parent = find_basic_block_ending_at(&basic_blocks, pc as i16);
                    basic_blocks[parent].add_child(child);
                    basic_blocks[child].add_parent(parent);
                }
                Instruction::JmpIf(l, _) => {
                    let target = l + pc as i16;
                    let child = find_basic_block_starting_at(&basic_blocks, target);
                    let child_no_jump = find_basic_block_starting_at(&basic_blocks, pc as i16 + 1);
                    let parent = find_basic_block_ending_at(&basic_blocks, pc as i16);
                    basic_blocks[parent].add_child(child);
                    basic_blocks[parent].add_child(child_no_jump);
                    basic_blocks[child].add_parent(parent);
                    basic_blocks[child_no_jump].add_parent(parent);
                }
                Instruction::JmpIfNot(l, _) => {
                    let target = l + pc as i16;
                    let child = find_basic_block_starting_at(&basic_blocks, target);
                    let child_no_jump = find_basic_block_starting_at(&basic_blocks, pc as i16 + 1);
                    let parent = find_basic_block_ending_at(&basic_blocks, pc as i16);
                    basic_blocks[parent].add_child(child);
                    basic_blocks[parent].add_child(child_no_jump);
                    basic_blocks[child].add_parent(parent);
                    basic_blocks[child_no_jump].add_parent(parent);
                }
                _ => {}
            }
        }

        Self {
            blocks: basic_blocks,
            from_fn: fun.location,
        }
    }

    pub fn print_dot(&self, src_fn: &Function) {
        fn print_block(bb: &BasicBlock, instructions: &Vec<Instruction>) {
            print!("\"");

            for instr in instructions[(bb.first as usize)..=(bb.last as usize)].iter() {
                print!("{}\\n", instr);
            }

            print!("\"");
        }

        let implementation = src_fn.implementation.as_bytecode().expect("bytecode impl");

        assert_eq!(self.from_fn, src_fn.location);

        println!("digraph g {{");
        for (i, bb) in self.blocks.iter().enumerate() {
            print!("\t{} [label=", i);
            print_block(bb, &implementation.instructions);
            println!("]");
        }
        for (i, bb) in self.blocks.iter().enumerate() {
            for c in bb.children.iter() {
                println!("\t{} -> {};", i, c);
            }
        }

        println!("}}");
    }
}
