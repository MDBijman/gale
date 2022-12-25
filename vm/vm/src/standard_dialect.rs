use std::{
    collections::HashSet,
    convert::TryInto,
    fmt::{Display, Formatter},
};

use dynasmrt::{dynasm, DynasmApi};
use dynasmrt::{x64::Rq, DynasmLabelApi};
use memoffset::offset_of;
use vm_internal::{
    bytecode::{CallTarget, Function, InstrLbl, Module, ModuleLoader, TypeId},
    cfg::{ControlFlowBehaviour, InstrControlFlow},
    define_instr_common,
    dialect::{
        Dialect, FromTerm, InstrInterpret, InstrToBytecode, InstrToX64, Instruction,
        InstructionParseError, LabelInstruction, Name, Var,
    },
    interpreter::{CallInfo, InterpreterStatus, Value},
    jit::{
        self, load_registers, store_registers, store_volatiles_except, FunctionJITState, VarLoc,
    },
    memory::{Heap, Pointer, ARRAY_HEADER_SIZE},
    parser::{Term, TypeTerm},
    typechecker::{InstrTypecheck, Type, TypeEnvironment, TypeError},
    vm::{VMState, VM},
    x64_asm, x64_asm_resolve_mem, x64_reg,
};
use vm_proc_macros::FromTerm;

macro_rules! construct {
    ($module_loader:expr, $module:expr, $i:expr, $e:expr, $(($l:literal, $t:ty)),+) => {
        match $e {
            $($l => Ok(Box::from(
                <$t>::construct($module_loader, $module, $i).unwrap(),
            )),)+
            _ => panic!()
        }
    }
}

macro_rules! emit_x64 {
    ($vm:expr, $module:expr, $jit_state:expr, $instr_lbl:expr, $instr:expr, $(($d:literal, $l:literal, $t:ty)),+) => {
        match ($instr.dialect(), $instr.tag()) {
            $(($d, $l) =>
                $instr
                    .as_any()
                    .downcast_ref::<$t>()
                    .unwrap()
                    .emit($vm, $module, $jit_state, $instr_lbl),
            )+
            (d, l) => panic!("Don't know how to compile instruction {}:{}", d, l)
        }
    }
}

/*
* The standard dialect defines commonly used instructions.
*/
pub struct StandardDialect {}

impl StandardDialect {}

impl Dialect for StandardDialect {
    fn compile(
        &self,
        vm: &VM,
        module: &Module,
        state: &mut FunctionJITState,
        instr: &dyn Instruction,
        instr_lbl: InstrLbl,
    ) {
        emit_x64!(
            vm,
            module,
            state,
            instr_lbl,
            instr,
            ("std", "ui32", ConstU32),
            ("std", "bool", ConstBool),
            ("std", "ret", Return),
            ("std", "cadr", ComputeAddress),
            ("std", "load", Load),
            ("std", "add", AddVarVar),
            ("std", "sub", SubVarVar),
            ("std", "call", Call),
            ("std", "lt", LtVarVar),
            ("std", "jmpifn", JmpIfNot),
            ("core", "lbl", LabelInstruction)
        );
    }

    fn make_instruction(
        &self,
        module_loader: &ModuleLoader,
        module: &mut Module,
        i: &Term,
    ) -> Result<Box<dyn Instruction>, InstructionParseError> {
        // This takes a &Term instead of &InstructionTerm so
        // that we can use construct(..) without needing to copy
        // the instruction term into a Term.

        let p = match i {
            Term::Instruction(i) => i,
            _ => panic!(),
        };

        construct!(
            module_loader,
            module,
            i,
            p.name.as_str(),
            ("ui32", ConstU32),
            ("bool", ConstBool),
            ("ret", Return),
            ("cadr", ComputeAddress),
            ("load", Load),
            ("add", AddVarVar),
            ("sub", SubVarVar),
            ("call", Call),
            ("lt", LtVarVar),
            ("jmpifn", JmpIfNot)
        )
    }

    fn get_dialect_tag(&self) -> &'static str {
        "std"
    }
}

/*
* Below are implementations for each bytecode op in the standard dialect.
* Each operation needs to implement the Instruction + Display traits.
* Additionally each operation can implement InstrToBytecode and InstrToX64 traits.
*/

/*
* ConstU32
*/

#[derive(FromTerm, Clone, Debug)]
struct ConstU32(Var, u32);

impl Instruction for ConstU32 {
    define_instr_common!("std", "ui32");

    fn reads(&self) -> HashSet<Var> {
        HashSet::default()
    }

    fn writes(&self) -> HashSet<Var> {
        let mut r = HashSet::new();
        r.insert(self.0);
        r
    }
}

impl InstrInterpret for ConstU32 {
    fn interpret(&self, _: &VM, state: &mut VMState) -> bool {
        Self::set_var(state, self.0, Value::UI64(self.1 as u64));
        Self::inc_ip(state);
        true
    }
}

impl InstrToBytecode for ConstU32 {
    fn op_size(&self) -> u32 {
        std::convert::TryInto::try_into(std::mem::size_of::<Self>()).unwrap()
    }

    fn emplace(&self, bytes: &mut [u8]) {
        bytes[0] = self.0 .0;
        bytes[1..5].copy_from_slice(&self.1.to_be_bytes());
    }
}

impl InstrToX64 for ConstU32 {
    fn emit(&self, _: &VM, _: &Module, fn_state: &mut FunctionJITState, _: InstrLbl) {
        // Ops cannot be referenced in x64_asm macros if it's inside fn_state
        let ops = &mut fn_state.ops;
        let memory = fn_state
            .variable_locations
            .as_ref()
            .expect("Memory actions must be initialized when emitting instructions");

        let r = self.0;
        let c: i32 = self.1 as i32;
        x64_asm_resolve_mem!(ops, memory; mov v(r), e(c));
    }
}

impl InstrControlFlow for ConstU32 {
    fn behaviour(&self) -> ControlFlowBehaviour {
        ControlFlowBehaviour::Linear
    }
}

impl InstrTypecheck for ConstU32 {
    fn typecheck(
        &self,
        _: &VM,
        _: &Module,
        _: &Function,
        env: &mut TypeEnvironment,
    ) -> Result<(), TypeError> {
        if !env.insert_or_check(self.0, Type::U64) {
            Err(TypeError::Mismatch {
                was: env.get(self.0).unwrap().clone(),
                expected: Type::U64,
                var: self.0,
            })
        } else {
            Ok(())
        }
    }
}

impl Display for ConstU32 {
    fn fmt(&self, f: &mut Formatter<'_>) -> Result<(), std::fmt::Error> {
        write!(f, "ui32 {}, {}", self.0, self.1)
    }
}

/*
* ConstBool
*/

#[derive(FromTerm, Clone, Debug)]
struct ConstBool(Var, bool);

impl Instruction for ConstBool {
    define_instr_common!("std", "bool");

    fn reads(&self) -> HashSet<Var> {
        HashSet::default()
    }

    fn writes(&self) -> HashSet<Var> {
        let mut r = HashSet::new();
        r.insert(self.0);
        r
    }
}

impl InstrInterpret for ConstBool {
    fn interpret(&self, _: &VM, state: &mut VMState) -> bool {
        Self::set_var(state, self.0, Value::Bool(self.1));
        Self::inc_ip(state);
        true
    }
}

impl InstrToBytecode for ConstBool {
    fn op_size(&self) -> u32 {
        std::convert::TryInto::try_into(std::mem::size_of::<Self>()).unwrap()
    }

    fn emplace(&self, bytes: &mut [u8]) {
        bytes[0] = self.0 .0;
        bytes[1] = self.1 as u8;
    }
}

impl InstrToX64 for ConstBool {
    fn emit(&self, _: &VM, _: &Module, _: &mut FunctionJITState, _: InstrLbl) {
        todo!()
    }
}

impl InstrControlFlow for ConstBool {
    fn behaviour(&self) -> ControlFlowBehaviour {
        ControlFlowBehaviour::Linear
    }
}

impl InstrTypecheck for ConstBool {
    fn typecheck(
        &self,
        _: &VM,
        _: &Module,
        _: &Function,
        env: &mut TypeEnvironment,
    ) -> Result<(), TypeError> {
        if !env.insert_or_check(self.0, Type::Bool) {
            Err(TypeError::Mismatch {
                was: env.get(self.0).unwrap().clone(),
                expected: Type::Bool,
                var: self.0,
            })
        } else {
            Ok(())
        }
    }
}

impl Display for ConstBool {
    fn fmt(&self, f: &mut Formatter<'_>) -> Result<(), std::fmt::Error> {
        write!(f, "bool ${}, {}", self.0, self.1)
    }
}

/*
* Copy
*/

#[derive(FromTerm, Clone, Debug)]
struct Copy(Var, Var);

impl Instruction for Copy {
    define_instr_common!("std", "mov");

    fn reads(&self) -> HashSet<Var> {
        let mut r = HashSet::new();
        r.insert(self.1);
        r
    }

    fn writes(&self) -> HashSet<Var> {
        let mut r = HashSet::new();
        r.insert(self.0);
        r
    }
}

impl InstrInterpret for Copy {
    fn interpret(&self, _: &VM, state: &mut VMState) -> bool {
        let val = Self::clone_var(state, self.0);
        Self::set_var(state, self.0, val);
        Self::inc_ip(state);
        true
    }
}

impl InstrToBytecode for Copy {
    fn op_size(&self) -> u32 {
        std::convert::TryInto::try_into(std::mem::size_of::<Self>()).unwrap()
    }

    fn emplace(&self, bytes: &mut [u8]) {
        bytes[0] = self.0 .0;
        bytes[1] = self.1 .0;
    }
}

impl InstrToX64 for Copy {
    fn emit(&self, _: &VM, _: &Module, _: &mut FunctionJITState, _: InstrLbl) {
        todo!()
    }
}

impl InstrControlFlow for Copy {
    fn behaviour(&self) -> ControlFlowBehaviour {
        ControlFlowBehaviour::Linear
    }
}

impl InstrTypecheck for Copy {
    fn typecheck(
        &self,
        _: &VM,
        _: &Module,
        _: &Function,
        env: &mut TypeEnvironment,
    ) -> Result<(), TypeError> {
        let rhs_type = match env.get(self.1) {
            None => return Err(TypeError::Missing { var: self.1 }),
            Some(t) => t.clone(),
        };

        match env.insert_or_check(self.0, rhs_type.clone()) {
            true => Ok(()),
            false => Err(TypeError::Mismatch {
                expected: rhs_type,
                was: env.get(self.0).unwrap().clone(),
                var: self.0,
            }),
        }
    }
}

impl Display for Copy {
    fn fmt(&self, f: &mut Formatter<'_>) -> Result<(), std::fmt::Error> {
        write!(f, "mov ${}, {}", self.0, self.1)
    }
}

/*
* EqVarVar
*/

#[derive(FromTerm, Debug, Clone)]
struct EqVarVar(Var, Var, Var);

impl Instruction for EqVarVar {
    define_instr_common!("std", "eq");

    fn reads(&self) -> HashSet<Var> {
        let mut r = HashSet::new();
        r.insert(self.1);
        r.insert(self.2);
        r
    }

    fn writes(&self) -> HashSet<Var> {
        let mut r = HashSet::new();
        r.insert(self.0);
        r
    }
}

impl InstrInterpret for EqVarVar {
    fn interpret(&self, _vm: &VM, _state: &mut VMState) -> bool {
        todo!()
    }
}

impl InstrToBytecode for EqVarVar {
    fn op_size(&self) -> u32 {
        std::convert::TryInto::try_into(std::mem::size_of::<Self>()).unwrap()
    }

    fn emplace(&self, _bytes: &mut [u8]) {
        todo!();
    }
}

impl InstrToX64 for EqVarVar {
    fn emit(&self, _: &VM, _: &Module, _: &mut FunctionJITState, _: InstrLbl) {
        todo!();
    }
}

impl InstrControlFlow for EqVarVar {
    fn behaviour(&self) -> ControlFlowBehaviour {
        ControlFlowBehaviour::Linear
    }
}

impl InstrTypecheck for EqVarVar {
    fn typecheck(
        &self,
        _: &VM,
        _: &Module,
        _: &Function,
        _: &mut TypeEnvironment,
    ) -> Result<(), TypeError> {
        todo!();
    }
}

impl Display for EqVarVar {
    fn fmt(&self, _f: &mut Formatter<'_>) -> Result<(), std::fmt::Error> {
        todo!()
    }
}

/*
* LtVarVar
*/

#[derive(FromTerm, Debug, Clone)]
struct LtVarVar(Var, Var, Var);

impl Instruction for LtVarVar {
    define_instr_common!("std", "lt");

    fn reads(&self) -> HashSet<Var> {
        let mut r = HashSet::new();
        r.insert(self.1);
        r.insert(self.2);
        r
    }

    fn writes(&self) -> HashSet<Var> {
        let mut r = HashSet::new();
        r.insert(self.0);
        r
    }
}

impl InstrInterpret for LtVarVar {
    fn interpret(&self, _vm: &VM, state: &mut VMState) -> bool {
        let lhs = *Self::ref_var(state, self.1)
            .as_ui64()
            .expect("invalid type");

        let rhs = *Self::ref_var(state, self.2)
            .as_ui64()
            .expect("invalid type");

        let res = lhs < rhs;

        Self::set_var(state, self.0, Value::Bool(res));

        Self::inc_ip(state);

        true
    }
}

impl InstrToBytecode for LtVarVar {
    fn op_size(&self) -> u32 {
        std::convert::TryInto::try_into(std::mem::size_of::<Self>()).unwrap()
    }

    fn emplace(&self, _bytes: &mut [u8]) {
        todo!();
    }
}

impl InstrToX64 for LtVarVar {
    fn emit(&self, _: &VM, _: &Module, state: &mut FunctionJITState, _: InstrLbl) {
        let res_reg = *state.lookup(self.0).unwrap().as_register().unwrap();
        let ops = &mut state.ops;
        let memory = state.variable_locations.as_ref().unwrap();
        x64_asm!(ops
                ;; x64_asm_resolve_mem!(ops, memory; cmp v(self.1), v(self.2))
                ;; x64_asm_resolve_mem!(ops, memory; mov v(self.0), e(0))
                ; mov rdi, 1
                ; cmovl Rq(res_reg as u8), rdi);
    }
}

impl InstrControlFlow for LtVarVar {
    fn behaviour(&self) -> ControlFlowBehaviour {
        ControlFlowBehaviour::Linear
    }
}

impl InstrTypecheck for LtVarVar {
    fn typecheck(
        &self,
        _: &VM,
        _: &Module,
        _: &Function,
        env: &mut TypeEnvironment,
    ) -> Result<(), TypeError> {
        let lhs_type = match env.get(self.1) {
            Some(lhs) => lhs,
            None => return Err(TypeError::Missing { var: self.1 }),
        };

        let rhs_type = match env.get(self.2) {
            Some(rhs) => rhs,
            None => return Err(TypeError::Missing { var: self.2 }),
        };

        if *lhs_type != Type::U64 {
            return Err(TypeError::Mismatch {
                expected: Type::U64,
                was: lhs_type.clone(),
                var: self.1,
            });
        }

        if *rhs_type != Type::U64 {
            return Err(TypeError::Mismatch {
                expected: Type::U64,
                was: rhs_type.clone(),
                var: self.2,
            });
        }

        if !env.insert_or_check(self.0, Type::Bool) {
            return Err(TypeError::Mismatch {
                expected: Type::Bool,
                was: env.get(self.0).unwrap().clone(),
                var: self.0,
            });
        }

        Ok(())
    }
}

impl Display for LtVarVar {
    fn fmt(&self, f: &mut Formatter<'_>) -> Result<(), std::fmt::Error> {
        write!(f, "lt {}, {}, {}", self.0, self.1, self.2)
    }
}

/*
* SubVarVar
*/

#[derive(FromTerm, Debug, Clone)]
struct SubVarVar(Var, Var, Var);

impl Instruction for SubVarVar {
    define_instr_common!("std", "sub");

    fn reads(&self) -> HashSet<Var> {
        let mut r = HashSet::new();
        r.insert(self.1);
        r.insert(self.2);
        r
    }

    fn writes(&self) -> HashSet<Var> {
        let mut r = HashSet::new();
        r.insert(self.0);
        r
    }
}

impl InstrInterpret for SubVarVar {
    fn interpret(&self, _vm: &VM, state: &mut VMState) -> bool {
        let lhs = *Self::ref_var(state, self.1)
            .as_ui64()
            .expect("invalid type");

        let rhs = *Self::ref_var(state, self.2)
            .as_ui64()
            .expect("invalid type");

        let res = lhs - rhs;

        Self::set_var(state, self.0, Value::UI64(res));
        Self::inc_ip(state);

        true
    }
}

impl InstrToBytecode for SubVarVar {
    fn op_size(&self) -> u32 {
        std::convert::TryInto::try_into(std::mem::size_of::<Self>()).unwrap()
    }

    fn emplace(&self, _bytes: &mut [u8]) {
        todo!();
    }
}

impl InstrToX64 for SubVarVar {
    fn emit(&self, _: &VM, _: &Module, state: &mut FunctionJITState, _: InstrLbl) {
        let ops = &mut state.ops;
        let memory = state.variable_locations.as_ref().unwrap();
        x64_asm_resolve_mem!(ops, memory; mov r(rsi), v(self.1));
        x64_asm_resolve_mem!(ops, memory; sub r(rsi), v(self.2));
        x64_asm_resolve_mem!(ops, memory; mov v(self.0), r(rsi));
    }
}

impl InstrControlFlow for SubVarVar {
    fn behaviour(&self) -> ControlFlowBehaviour {
        ControlFlowBehaviour::Linear
    }
}

impl InstrTypecheck for SubVarVar {
    fn typecheck(
        &self,
        _: &VM,
        _: &Module,
        _: &Function,
        env: &mut TypeEnvironment,
    ) -> Result<(), TypeError> {
        let lhs_type = match env.get(self.1) {
            Some(lhs) => lhs,
            None => return Err(TypeError::Missing { var: self.1 }),
        };

        let rhs_type = match env.get(self.2) {
            Some(rhs) => rhs,
            None => return Err(TypeError::Missing { var: self.2 }),
        };

        if *lhs_type != Type::U64 {
            return Err(TypeError::Mismatch {
                expected: Type::U64,
                was: lhs_type.clone(),
                var: self.1,
            });
        }

        if *rhs_type != Type::U64 {
            return Err(TypeError::Mismatch {
                expected: Type::U64,
                was: rhs_type.clone(),
                var: self.2,
            });
        }

        if !env.insert_or_check(self.0, Type::U64) {
            return Err(TypeError::Mismatch {
                expected: Type::U64,
                was: env.get(self.0).unwrap().clone(),
                var: self.0,
            });
        }

        Ok(())
    }
}

impl Display for SubVarVar {
    fn fmt(&self, f: &mut Formatter<'_>) -> Result<(), std::fmt::Error> {
        write!(f, "sub {}, {}, {}", self.0, self.1, self.2)
    }
}

/*
* AddVarVar
*/

#[derive(FromTerm, Debug, Clone)]
struct AddVarVar(Var, Var, Var);

impl Instruction for AddVarVar {
    define_instr_common!("std", "add");

    fn reads(&self) -> HashSet<Var> {
        let mut r = HashSet::new();
        r.insert(self.1);
        r.insert(self.2);
        r
    }

    fn writes(&self) -> HashSet<Var> {
        let mut r = HashSet::new();
        r.insert(self.0);
        r
    }
}

impl InstrInterpret for AddVarVar {
    fn interpret(&self, _vm: &VM, state: &mut VMState) -> bool {
        let lhs = *Self::ref_var(state, self.1)
            .as_ui64()
            .expect("invalid type");

        let rhs = *Self::ref_var(state, self.2)
            .as_ui64()
            .expect("invalid type");

        let res = lhs + rhs;

        Self::set_var(state, self.0, Value::UI64(res));
        Self::inc_ip(state);

        true
    }
}

impl InstrToBytecode for AddVarVar {
    fn op_size(&self) -> u32 {
        std::convert::TryInto::try_into(std::mem::size_of::<Self>()).unwrap()
    }

    fn emplace(&self, _bytes: &mut [u8]) {
        todo!();
    }
}

impl InstrToX64 for AddVarVar {
    fn emit(&self, _: &VM, _: &Module, state: &mut FunctionJITState, _: InstrLbl) {
        let ops = &mut state.ops;
        let memory = state.variable_locations.as_ref().unwrap();
        x64_asm_resolve_mem!(ops, memory; mov r(rsi), v(self.1));
        x64_asm_resolve_mem!(ops, memory; add r(rsi), v(self.2));
        x64_asm_resolve_mem!(ops, memory; mov v(self.0), r(rsi));
    }
}

impl InstrControlFlow for AddVarVar {
    fn behaviour(&self) -> ControlFlowBehaviour {
        ControlFlowBehaviour::Linear
    }
}

impl InstrTypecheck for AddVarVar {
    fn typecheck(
        &self,
        _: &VM,
        _: &Module,
        _: &Function,
        env: &mut TypeEnvironment,
    ) -> Result<(), TypeError> {
        let lhs_type = match env.get(self.1) {
            Some(lhs) => lhs,
            None => return Err(TypeError::Missing { var: self.1 }),
        };

        let rhs_type = match env.get(self.2) {
            Some(rhs) => rhs,
            None => return Err(TypeError::Missing { var: self.2 }),
        };

        if *lhs_type != Type::U64 {
            return Err(TypeError::Mismatch {
                expected: Type::U64,
                was: lhs_type.clone(),
                var: self.1,
            });
        }

        if *rhs_type != Type::U64 {
            return Err(TypeError::Mismatch {
                expected: Type::U64,
                was: rhs_type.clone(),
                var: self.2,
            });
        }

        if !env.insert_or_check(self.0, Type::U64) {
            return Err(TypeError::Mismatch {
                expected: Type::U64,
                was: env.get(self.0).unwrap().clone(),
                var: self.0,
            });
        }

        Ok(())
    }
}

impl Display for AddVarVar {
    fn fmt(&self, f: &mut Formatter<'_>) -> Result<(), std::fmt::Error> {
        write!(f, "add {}, {}, {}", self.0, self.1, self.2)
    }
}

/*
* MulVarVar
*/

#[derive(FromTerm, Debug, Clone)]
struct MulVarVar(Var, Var, Var);

impl Instruction for MulVarVar {
    define_instr_common!("std", "mul");

    fn reads(&self) -> HashSet<Var> {
        let mut r = HashSet::new();
        r.insert(self.1);
        r.insert(self.2);
        r
    }

    fn writes(&self) -> HashSet<Var> {
        let mut r = HashSet::new();
        r.insert(self.0);
        r
    }
}

impl InstrInterpret for MulVarVar {
    fn interpret(&self, _vm: &VM, _state: &mut VMState) -> bool {
        todo!()
    }
}

impl InstrToBytecode for MulVarVar {
    fn op_size(&self) -> u32 {
        std::convert::TryInto::try_into(std::mem::size_of::<Self>()).unwrap()
    }

    fn emplace(&self, _bytes: &mut [u8]) {
        todo!();
    }
}

impl InstrToX64 for MulVarVar {
    fn emit(&self, _: &VM, _: &Module, _: &mut FunctionJITState, _: InstrLbl) {
        todo!();
    }
}

impl InstrControlFlow for MulVarVar {
    fn behaviour(&self) -> ControlFlowBehaviour {
        ControlFlowBehaviour::Linear
    }
}

impl InstrTypecheck for MulVarVar {
    fn typecheck(
        &self,
        _: &VM,
        _: &Module,
        _: &Function,
        _: &mut TypeEnvironment,
    ) -> Result<(), TypeError> {
        todo!();
    }
}

impl Display for MulVarVar {
    fn fmt(&self, _f: &mut Formatter<'_>) -> Result<(), std::fmt::Error> {
        todo!()
    }
}

/*
* NotVar
*/

#[derive(FromTerm, Debug, Clone)]
struct NotVar(Var, Var);

impl Instruction for NotVar {
    define_instr_common!("std", "not");

    fn reads(&self) -> HashSet<Var> {
        let mut r = HashSet::new();
        r.insert(self.1);
        r
    }

    fn writes(&self) -> HashSet<Var> {
        let mut r = HashSet::new();
        r.insert(self.0);
        r
    }
}

impl InstrInterpret for NotVar {
    fn interpret(&self, _vm: &VM, _state: &mut VMState) -> bool {
        todo!()
    }
}

impl InstrToBytecode for NotVar {
    fn op_size(&self) -> u32 {
        std::convert::TryInto::try_into(std::mem::size_of::<Self>()).unwrap()
    }

    fn emplace(&self, _bytes: &mut [u8]) {
        todo!();
    }
}

impl InstrToX64 for NotVar {
    fn emit(&self, _: &VM, _: &Module, _: &mut FunctionJITState, _: InstrLbl) {
        todo!();
    }
}

impl InstrControlFlow for NotVar {
    fn behaviour(&self) -> ControlFlowBehaviour {
        ControlFlowBehaviour::Linear
    }
}

impl InstrTypecheck for NotVar {
    fn typecheck(
        &self,
        _: &VM,
        _: &Module,
        _: &Function,
        _: &mut TypeEnvironment,
    ) -> Result<(), TypeError> {
        todo!();
    }
}

impl Display for NotVar {
    fn fmt(&self, _f: &mut Formatter<'_>) -> Result<(), std::fmt::Error> {
        todo!()
    }
}

/*
* Return
*/

#[derive(FromTerm, Debug, Clone)]
struct Return(Var);

impl Instruction for Return {
    define_instr_common!("std", "ret");

    fn reads(&self) -> HashSet<Var> {
        let mut r = HashSet::new();
        r.insert(self.0);
        r
    }

    fn writes(&self) -> HashSet<Var> {
        HashSet::default()
    }
}

impl InstrInterpret for Return {
    fn interpret(&self, _: &VM, state: &mut VMState) -> bool {
        let val = Self::clone_var(state, self.0);
        let ci = state.interpreter_state.call_info.pop().unwrap();

        // Finished execution
        if state.interpreter_state.call_info.len() == 0 {
            state.interpreter_state.status = InterpreterStatus::Finished;
            state.interpreter_state.result = val;
            return false;
        }

        state.interpreter_state.ip = ci.prev_ip;
        state.interpreter_state.func = ci.prev_fn;
        state.interpreter_state.module = ci.prev_mod;

        state.interpreter_state.stack.dealloc(ci.framesize);

        Self::set_var(state, ci.result_var, val.clone());

        // Native function
        if !ci.called_by_native {
            Self::inc_ip(state);
        } else {
            // Todo: return a value to a native function
            unimplemented!();
        }

        true
    }
}

impl InstrToBytecode for Return {
    fn op_size(&self) -> u32 {
        std::convert::TryInto::try_into(std::mem::size_of::<Self>()).unwrap()
    }

    fn emplace(&self, _bytes: &mut [u8]) {
        todo!();
    }
}

impl InstrToX64 for Return {
    fn emit(&self, _: &VM, _: &Module, state: &mut FunctionJITState, _: InstrLbl) {
        let ops = &mut state.ops;
        let memory = state.variable_locations.as_ref().unwrap();
        x64_asm!(ops
                ;; x64_asm_resolve_mem!(ops, memory; mov r(rax), v(self.0))
                ; jmp ->_cleanup)
    }
}

impl InstrControlFlow for Return {
    fn behaviour(&self) -> ControlFlowBehaviour {
        ControlFlowBehaviour::Jump(vec![])
    }
}

impl InstrTypecheck for Return {
    fn typecheck(
        &self,
        _: &VM,
        m: &Module,
        f: &Function,
        env: &mut TypeEnvironment,
    ) -> Result<(), TypeError> {
        match m.get_type_by_id(f.type_id) {
            Some(Type::Fn(_, to)) => {
                if let Some(variable_type) = env.get(self.0) {
                    if **to == *variable_type {
                        Ok(())
                    } else {
                        Err(TypeError::Mismatch {
                            expected: *to.clone(),
                            was: variable_type.clone(),
                            var: self.0,
                        })
                    }
                } else {
                    panic!()
                }
            }
            Some(_) => panic!(),
            None => panic!(),
        }
    }
}

impl Display for Return {
    fn fmt(&self, f: &mut Formatter<'_>) -> Result<(), std::fmt::Error> {
        write!(f, "ret {}", self.0)
    }
}

/*
* ComputeAddress
*/

#[derive(FromTerm, Debug, Clone)]
struct ComputeAddress(Var, Var, TypeId, Var);

impl Instruction for ComputeAddress {
    define_instr_common!("std", "cadr");

    fn reads(&self) -> HashSet<Var> {
        let mut r = HashSet::new();
        r.insert(self.1);
        r.insert(self.3);
        r
    }

    fn writes(&self) -> HashSet<Var> {
        let mut r = HashSet::new();
        r.insert(self.0);
        r
    }
}

impl InstrInterpret for ComputeAddress {
    fn interpret(&self, vm: &VM, state: &mut VMState) -> bool {
        let current_module = vm
            .module_loader
            .get_module_by_id(state.interpreter_state.module)
            .unwrap();

        let idx_value = Self::ref_var(state, self.3)
            .as_ui64()
            .expect("invalid index");

        let res = match current_module
            .get_type_by_id(self.2)
            .expect("invalid type id")
        {
            Type::Pointer(referenced_type) => match &**referenced_type {
                Type::Tuple(inner_types) => {
                    let mut offset = 0;
                    assert!(*idx_value < inner_types.len() as u64);

                    for i in 0..*idx_value {
                        offset += inner_types[i as usize].size();
                    }

                    let p: Pointer = *Self::ref_var(state, self.1).as_pointer().unwrap();

                    p.index(offset)
                }
                Type::Array(t, _) => {
                    let offset = t.size() * (*idx_value as usize) + ARRAY_HEADER_SIZE;

                    let p: Pointer = *Self::ref_var(state, self.1).as_pointer().unwrap();

                    p.index(offset)
                }
                _ => panic!("invalid operand"),
            },
            _ => panic!("invalid operand"),
        };

        Self::set_var(state, self.0, Value::Pointer(res));

        Self::inc_ip(state);

        true
    }
}

impl InstrToBytecode for ComputeAddress {
    fn op_size(&self) -> u32 {
        std::convert::TryInto::try_into(std::mem::size_of::<Self>()).unwrap()
    }

    fn emplace(&self, _bytes: &mut [u8]) {
        todo!();
    }
}

impl InstrToX64 for ComputeAddress {
    fn emit(&self, _: &VM, module: &Module, state: &mut FunctionJITState, lbl: InstrLbl) {
        let ops = &mut state.ops;
        let memory = state.variable_locations.as_ref().unwrap();

        let element_size: i32 = match module.get_type_by_id(self.2).expect("type idx") {
            Type::Pointer(inner_type) => match &**inner_type {
                Type::Array(element_type, _) => element_type.size() as i32,
                _ => panic!("Illegal cadr operand: {:?}", inner_type),
            },
            ty => panic!("Illegal cadr operand: {:?}", ty),
        };

        let saved = store_registers(
            ops,
            memory,
            &vec![Rq::RAX, Rq::RCX, Rq::R8],
            &vec![self.0],
            lbl,
            false,
        );

        x64_asm!(ops
            // Load first because they might be overriden
            ;; x64_asm_resolve_mem!(ops, memory; mov r(r8), v(self.1))
            ;; x64_asm_resolve_mem!(ops, memory; mov r(rcx), v(self.3))

            // 2) compute pointer to element
            // base (self.1 in r8) + element_size * index (rcx) + 8 (array header)
            ; mov rax, element_size
            ; mul rcx
            // Offset for array header
            ; add rax, 8
            // Offset with base pointer
            ; add rax, r8

            ;; x64_asm_resolve_mem!(ops, memory; mov v(self.0), r(rax))
        );

        load_registers(ops, &saved, false);
    }
}

impl InstrControlFlow for ComputeAddress {
    fn behaviour(&self) -> ControlFlowBehaviour {
        ControlFlowBehaviour::Linear
    }
}

impl InstrTypecheck for ComputeAddress {
    fn typecheck(
        &self,
        _: &VM,
        m: &Module,
        _: &Function,
        env: &mut TypeEnvironment,
    ) -> Result<(), TypeError> {
        let specified_type = m.get_type_by_id(self.2).unwrap();

        if let Some(t) = env.get(self.1) {
            if t != specified_type {
                return Err(TypeError::Mismatch {
                    expected: specified_type.clone(),
                    was: t.clone(),
                    var: self.1,
                });
            }
        } else {
            return Err(TypeError::Missing { var: self.1 });
        }

        if let Some(t) = env.get(self.3) {
            if *t != Type::U64 {
                return Err(TypeError::Mismatch {
                    expected: Type::U64,
                    was: t.clone(),
                    var: self.1,
                });
            }
        } else {
            return Err(TypeError::Missing { var: self.3 });
        }

        let result_type = match specified_type {
            Type::Pointer(t) => match &**t {
                Type::Array(t, _) => Type::Pointer(t.clone()),
                _ => panic!(),
            },
            t => {
                return Err(TypeError::Mismatch {
                    expected: Type::Unit,
                    was: t.clone(),
                    var: self.1,
                })
            }
        };

        if !env.insert_or_check(self.0, result_type.clone()) {
            return Err(TypeError::Mismatch {
                expected: result_type.clone(),
                was: env.get(self.0).unwrap().clone(),
                var: self.0,
            });
        }

        Ok(())
    }
}

impl Display for ComputeAddress {
    fn fmt(&self, f: &mut Formatter<'_>) -> Result<(), std::fmt::Error> {
        write!(f, "cadr {}, {}, {}, {}", self.0, self.1, "<?>", self.3)
    }
}

/*
* Load
*/

#[derive(FromTerm, Debug, Clone)]
struct Load(Var, Var, TypeId);

impl Instruction for Load {
    define_instr_common!("std", "load");

    fn reads(&self) -> HashSet<Var> {
        let mut r = HashSet::new();
        r.insert(self.1);
        r
    }

    fn writes(&self) -> HashSet<Var> {
        let mut r = HashSet::new();
        r.insert(self.0);
        r
    }
}

impl InstrInterpret for Load {
    fn interpret(&self, vm: &VM, state: &mut VMState) -> bool {
        let current_module = vm
            .module_loader
            .get_module_by_id(state.interpreter_state.module)
            .unwrap();

        let ptr = *Self::ref_var(state, self.1)
            .as_pointer()
            .expect("invalid pointer");

        match current_module
            .get_type_by_id(self.2)
            .expect("invalid type id")
        {
            Type::Pointer(inner_type) => match &**inner_type {
                Type::U64 => unsafe {
                    let val = state.heap.load::<u64>(ptr);
                    Self::set_var(state, self.0, Value::UI64(val));
                },
                Type::Bool => todo!(),
                Type::Pointer(_) => unsafe {
                    let val = state.heap.load::<Pointer>(ptr);
                    Self::set_var(state, self.0, Value::Pointer(val));
                },
                t => panic!("{:?}", t),
            },
            _ => panic!(),
        }

        Self::inc_ip(state);

        true
    }
}

impl InstrToBytecode for Load {
    fn op_size(&self) -> u32 {
        std::convert::TryInto::try_into(std::mem::size_of::<Self>()).unwrap()
    }

    fn emplace(&self, _bytes: &mut [u8]) {
        todo!();
    }
}

impl InstrToX64 for Load {
    fn emit(&self, _: &VM, _: &Module, state: &mut FunctionJITState, _: InstrLbl) {
        let ops = &mut state.ops;
        let memory = state.variable_locations.as_ref().unwrap();

        x64_asm!(ops
        ;; x64_asm_resolve_mem!(ops, memory; mov r(rsi), v(self.1))
        ; mov rsi, QWORD [rsi]
        ;; x64_asm_resolve_mem!(ops, memory; mov v(self.0), r(rsi))
        );
    }
}

impl InstrControlFlow for Load {
    fn behaviour(&self) -> ControlFlowBehaviour {
        ControlFlowBehaviour::Linear
    }
}

impl InstrTypecheck for Load {
    fn typecheck(
        &self,
        _: &VM,
        m: &Module,
        _: &Function,
        env: &mut TypeEnvironment,
    ) -> Result<(), TypeError> {
        let value_type = m.get_type_by_id(self.2).unwrap();

        match env.get(self.1) {
            None => return Err(TypeError::Missing { var: self.1 }),
            Some(t) => {
                if t != value_type {
                    return Err(TypeError::Mismatch {
                        expected: value_type.clone(),
                        was: t.clone(),
                        var: self.1,
                    });
                }
            }
        }
        match value_type {
            Type::Pointer(inner) => {
                if !env.insert_or_check(self.0, (**inner).clone()) {
                    return Err(TypeError::Mismatch {
                        expected: *inner.clone(),
                        was: env.get(self.0).unwrap().clone(),
                        var: self.0,
                    });
                }

                Ok(())
            }
            _ => {
                return Err(TypeError::Mismatch {
                    expected: Type::Pointer(Box::from(Type::Any)),
                    was: value_type.clone(),
                    var: self.1,
                })
            }
        }
    }
}

impl Display for Load {
    fn fmt(&self, f: &mut Formatter<'_>) -> Result<(), std::fmt::Error> {
        write!(f, "load {}, {}, {}", self.0, self.1, "<?>")
    }
}

/*
* Call
*/

#[derive(Debug, Clone)]
struct CallArgs(Vec<Var>);

impl FromTerm for CallArgs {
    fn construct(
        _module_loader: &ModuleLoader,
        _module: &mut Module,
        p: &Term,
    ) -> Result<Self, InstructionParseError>
    where
        Self: Sized,
    {
        match p {
            Term::Tuple(ts) => {
                let mut args: Vec<Var> = vec![];
                for t in ts {
                    match t.as_variable() {
                        Some(v) => args.push(*v),
                        None => return Err(InstructionParseError {}),
                    }
                }
                Ok(Self(args))
            }
            // hack to make parsing work here - should be fixed in bytecode parser
            Term::Type(TypeTerm::Unit) => Ok(Self(vec![])),
            _ => Err(InstructionParseError {}),
        }
    }
}

impl Display for CallArgs {
    fn fmt(&self, f: &mut Formatter<'_>) -> std::fmt::Result {
        let mut i = 0;
        write!(f, "[")?;
        for n in self.0.iter() {
            write!(f, "{}", n)?;

            i += 1;
            if i < self.0.len() {
                write!(f, ", ")?;
            }
        }
        write!(f, "]")?;

        Ok(())
    }
}

#[derive(Debug, Clone, FromTerm)]
//struct Call(Var, Name, CallArgs);
struct Call {
    result: Var,
    target: Name,
    args: CallArgs,
}

impl Call {
    fn resolve<'a>(&self, vm: &'a VM, current_module: u16) -> (&'a Module, &'a Function) {
        let (callee_module, callee_function) = if self.target.0.len() == 2 {
            let module = vm
                .module_loader
                .get_module_by_name(&self.target.0[0])
                .unwrap();
            let callee = module
                .get_function_by_name(self.target.0[1].as_str())
                .expect("invalid callee");
            (module, callee)
        } else if self.target.0.len() == 1 {
            let module = vm.module_loader.get_module_by_id(current_module).unwrap();
            let callee = module
                .get_function_by_name(self.target.0[0].as_str())
                .expect("invalid callee");
            (module, callee)
        } else {
            panic!("unsupported");
        };

        (callee_module, callee_function)
    }
}

impl Instruction for Call {
    define_instr_common!("std", "call");

    fn reads(&self) -> HashSet<Var> {
        let mut r = HashSet::new();
        r.extend(self.args.0.iter());
        r
    }

    fn writes(&self) -> HashSet<Var> {
        let mut r = HashSet::new();
        r.insert(self.result);
        r
    }
}

impl InstrInterpret for Call {
    fn interpret(&self, vm: &VM, state: &mut VMState) -> bool {
        let (callee_module, callee_function) = self.resolve(vm, state.interpreter_state.module);

        if callee_function.has_intrinsic_impl() {
            let native_impl = callee_function.intrinsic_impl().unwrap();

            // Native functions currently only have one 64 bit argument
            if self.target.0.len() > 1 {
                panic!("native function called with more than 1 arg");
            }

            let arg_value = unsafe { Self::ref_var(state, self.args.0[0]).value_pointer() };
            let arg_value_pointer = unsafe { std::mem::transmute::<_, Pointer>(arg_value) };

            // Signature of native functions is (vm state, pointer to arg)
            let res = unsafe {
                let unsafe_fn_ptr = std::mem::transmute::<
                    _,
                    extern "C" fn(&mut VMState, Pointer) -> u64,
                >(native_impl.fn_ptr);
                (unsafe_fn_ptr)(state, arg_value_pointer)
            };

            // Store result
            Self::set_var(state, self.result, Value::UI64(res));
            Self::inc_ip(state);
        } else if callee_function.has_ast_impl() {
            let ast_impl = callee_function.ast_impl().unwrap();
            let stack_size = ast_impl.varcount;
            let new_base = state.interpreter_state.stack.len();

            // Allocate stack frame
            state.interpreter_state.stack.alloc(stack_size.into());

            // Create new call info
            let new_ci = CallInfo {
                called_by_native: false,
                var_base: new_base.try_into().unwrap(),
                result_var: self.result,
                framesize: stack_size as usize,
                prev_ip: state.interpreter_state.ip,
                prev_fn: state.interpreter_state.func,
                prev_mod: state.interpreter_state.module,
            };

            // Write arguments into new frame
            for (idx, arg) in self.args.0.iter().enumerate() {
                let val = Self::clone_var(state, *arg);
                state
                    .interpreter_state
                    .set_stack_var_raw_index(new_base as usize + Into::<usize>::into(idx), val);
            }

            state.interpreter_state.call_info.push(new_ci);
            state.interpreter_state.ip = 0;
            state.interpreter_state.func = callee_function.location;
            state.interpreter_state.module = callee_module.id();
        } else {
            panic!("no available implementation");
        }
        true
    }
}

impl InstrToBytecode for Call {
    fn op_size(&self) -> u32 {
        std::convert::TryInto::try_into(std::mem::size_of::<Self>()).unwrap()
    }

    fn emplace(&self, _bytes: &mut [u8]) {
        todo!();
    }
}

impl InstrToX64 for Call {
    fn emit(&self, vm: &VM, module: &Module, state: &mut FunctionJITState, lbl: InstrLbl) {
        assert!(self.args.0.len() == 1 || self.args.0.len() == 0);
        let (resolved_module, resolved_function) = self.resolve(vm, module.id());
        let memory = state.variable_locations.as_ref().unwrap();
        let current_module_id = module.id();
        let resolved_module_id = resolved_module.id();
        let res = self.result;
        let ops = &mut state.ops;

        let trampoline_address: i64 = unsafe {
            std::mem::transmute::<_, i64>(
                jit::trampoline as extern "win64" fn(&VM, &mut VMState, CallTarget, Pointer) -> u64,
            )
        };

        let saved = store_volatiles_except(ops, memory, Some(self.result), true, lbl);

        let has_arg = self.args.0.len() == 1;

        // Recursive call
        // We know we can call the native impl. directly
        if resolved_module.id() == current_module_id
            && resolved_function.location == state.function_location
        {
            let mut stack_space = 0x20;

            if has_arg {
                let arg = self.args.0[0];
                x64_asm_resolve_mem!(ops, memory; mov r(r8), v(arg));
                x64_asm!(ops; push r8; mov r8, rsp);
                // align
                stack_space += 8;
            } else {
                // nullptr for no-arg functions
                x64_asm!(ops; mov r8, 0);
            }

            x64_asm!(ops
                    ; mov rcx, _vm
                    ; mov rdx, _vm_state
                    ; sub rsp, BYTE stack_space
                    ; call ->_self
                    ; add rsp, BYTE stack_space
                    ; add rsp, 8
                    ;; x64_asm_resolve_mem!(ops, memory; mov v(res), r(rax)));
        }
        // Unknown target
        // Conservatively call trampoline
        else {
            let mut stack_space = 0x20;

            if has_arg {
                let arg = self.args.0[0];
                x64_asm_resolve_mem!(ops, memory; mov r(r9), v(arg));
                x64_asm!(ops
                ; push r9
                ; mov r9, rsp);

                // Align because we are pushing r9 onto stack
                stack_space += 0x8;
            } else {
                x64_asm!(ops; mov r9, 0);
            }

            x64_asm!(ops
                ; mov rcx, _vm
                ; mov rdx, _vm_state
                ; mov r8w, resolved_function.location as i16
                ; shl r8, 16
                ; mov r8w, resolved_module_id as i16
                ; sub rsp, BYTE stack_space
                ; mov r10, QWORD trampoline_address
                ; call r10
                ; add rsp, BYTE stack_space
                ;; x64_asm_resolve_mem!(ops, memory; mov v(res), r(rax)));

            if has_arg {
                x64_asm!(ops; add rsp, 8);
            }
        }

        load_registers(ops, &saved, true);
    }
}

impl InstrTypecheck for Call {
    fn typecheck(
        &self,
        vm: &VM,
        m: &Module,
        _: &Function,
        env: &mut TypeEnvironment,
    ) -> Result<(), TypeError> {
        let (module, function) = self.resolve(vm, m.id());
        let callee_type = module.get_type_by_id(function.type_id);

        match callee_type {
            Some(Type::Fn(from, to)) => {
                if !env.insert_or_check(self.result, *to.clone()) {
                    return Err(TypeError::Mismatch {
                        expected: *to.clone(),
                        was: env.get(self.result).unwrap().clone(),
                        var: self.result,
                    });
                }

                if let Type::Tuple(params) = &**from {
                    for (param, arg) in params.iter().zip(self.args.0.iter()) {
                        if !env.insert_or_check(*arg, param.clone()) {
                            return Err(TypeError::Mismatch {
                                expected: param.clone(),
                                was: env.get(*arg).unwrap().clone(),
                                var: *arg,
                            });
                        }
                    }
                } else {
                    // this is a hacky because signatures don't map 1-1 to arguments

                    if self.args.0.len() == 1 {
                        let arg: Var = self.args.0[0];

                        if !env.insert_or_check(arg, (**from).clone()) {
                            return Err(TypeError::Mismatch {
                                expected: (**from).clone(),
                                was: env.get(arg).unwrap().clone(),
                                var: arg,
                            });
                        }
                    } else if self.args.0.len() == 0 {
                        match &**from {
                            Type::Unit => {
                                return Ok(());
                            }
                            _ => panic!(),
                        }
                    } else {
                        panic!("{:?}", from);
                    }
                }

                Ok(())
            }
            Some(_) => panic!(),
            None => todo!(),
        }
    }
}

impl InstrControlFlow for Call {
    fn behaviour(&self) -> ControlFlowBehaviour {
        ControlFlowBehaviour::Linear
    }
}

impl Display for Call {
    fn fmt(&self, f: &mut Formatter<'_>) -> Result<(), std::fmt::Error> {
        write!(f, "call {}, {}, {}", self.result, self.target, self.args)
    }
}

/*
* JmpIfNot
*/

#[derive(FromTerm, Clone, Debug)]
struct JmpIfNot(Var, Name);

impl Instruction for JmpIfNot {
    define_instr_common!("std", "jmpifn");

    fn reads(&self) -> HashSet<Var> {
        let mut r = HashSet::new();
        r.insert(self.0);
        r
    }

    fn writes(&self) -> HashSet<Var> {
        HashSet::new()
    }
}

impl InstrInterpret for JmpIfNot {
    fn interpret(&self, vm: &VM, state: &mut VMState) -> bool {
        assert!(self.1 .0.len() == 1);
        let i = vm
            .module_loader
            .get_module_by_id(state.interpreter_state.module)
            .unwrap()
            .get_function_by_id(state.interpreter_state.func)
            .expect("invalid callee")
            .ast_impl()
            .unwrap()
            .labels
            .get(&self.1 .0[0])
            .unwrap();

        match Self::ref_var(state, self.0) {
            Value::Bool(b) => {
                if *b {
                    Self::inc_ip(state);
                } else {
                    state.interpreter_state.ip = *i as isize;
                };
            }
            _ => panic!(),
        }
        true
    }
}

impl InstrToBytecode for JmpIfNot {
    fn op_size(&self) -> u32 {
        std::convert::TryInto::try_into(std::mem::size_of::<Self>()).unwrap()
    }

    fn emplace(&self, _bytes: &mut [u8]) {
        todo!();
    }
}

impl InstrToX64 for JmpIfNot {
    fn emit(&self, _: &VM, module: &Module, state: &mut FunctionJITState, _: InstrLbl) {
        let ops = &mut state.ops;
        let memory = state.variable_locations.as_ref().unwrap();

        let jmp_target = *module
            .get_function_by_id(state.function_location)
            .unwrap()
            .ast_impl()
            .unwrap()
            .labels
            .get(&self.1 .0[0])
            .unwrap();

        let dyn_lbl = state
            .dynamic_labels
            .entry(jmp_target as InstrLbl)
            .or_insert(ops.new_dynamic_label());

        x64_asm!(ops
                ;; x64_asm_resolve_mem!(ops, memory ; cmp v(self.0), e(0))
                ; je =>*dyn_lbl);
    }
}

impl InstrControlFlow for JmpIfNot {
    fn behaviour(&self) -> ControlFlowBehaviour {
        ControlFlowBehaviour::ConditionalJump(vec![self.1.clone()])
    }
}

impl InstrTypecheck for JmpIfNot {
    fn typecheck(
        &self,
        _: &VM,
        _: &Module,
        _: &Function,
        env: &mut TypeEnvironment,
    ) -> Result<(), TypeError> {
        match env.get(self.0) {
            Some(Type::Bool) => Ok(()),
            Some(t) => Err(TypeError::Mismatch {
                expected: Type::Bool,
                was: t.clone(),
                var: self.0,
            }),
            None => Err(TypeError::Missing { var: self.0 }),
        }
    }
}

impl Display for JmpIfNot {
    fn fmt(&self, f: &mut Formatter<'_>) -> Result<(), std::fmt::Error> {
        write!(f, "jmpifn {}, @{}", self.0, self.1)
    }
}

// match instr {
//     MulVarVar(r, a, b) => {
//         x64_asm!(ops; push rax);
//         x64_asm_resolve_mem!(ops, memory; mov rax, resolve(a));
//         x64_asm_resolve_mem!(ops, memory; mov rsi, resolve(b));
//         x64_asm!(ops; imul rsi);
//         x64_asm_resolve_mem!(ops, memory; mov resolve(r), rax);
//         x64_asm!(ops; pop rax);
//     }
//     EqVarVar(r, a, b) => {
//         x64_asm_resolve_mem!(ops, memory; cmp resolve(a), resolve(b));
//         x64_asm!(ops
//             ; mov rsi, 0
//             ; mov rdi, 1
//             ; cmove rsi, rdi);
//         x64_asm_resolve_mem!(ops, memory; mov resolve(r), rsi);
//     }
//     NotVar(r, a) => {
//         x64_asm_resolve_mem!(ops, memory; cmp resolve(a), 0);
//         x64_asm!(ops
//             ; mov rsi, 0
//             ; mov rdi, 1
//             ; cmove rsi, rdi);
//         x64_asm_resolve_mem!(ops, memory; mov resolve(r), rsi);
//     }
//     Copy(dest, src) => {
//         x64_asm_resolve_mem!(ops, memory; mov resolve(dest), resolve(src));
//     }
//     StoreVar(dst_ptr, src, ty) => {
//         let get_heap_address = unsafe {
//             std::mem::transmute::<_, i64>(
//                 VMState::heap_mut as unsafe extern "C" fn(&mut VMState) -> &mut Heap,
//             )
//         };

//         let store_address = unsafe {
//             std::mem::transmute::<_, i64>(
//                 Heap::store_u64 as unsafe extern "C" fn(&mut Heap, Pointer, u64),
//             )
//         };

//         let size = module.get_type_by_id(ty).unwrap().size() as i32;
//         assert_eq!(size, 8, "Can only store size 8 for now");

//         let saved = store_volatiles_except(ops, memory, None);
//         let stack_space = 0x20;

//         x64_asm!(ops
//             // Load first because they might be overriden
//             ;; x64_asm_resolve_mem!(ops, memory; mov r14, resolve(dst_ptr))
//             ;; x64_asm_resolve_mem!(ops, memory; mov r15, resolve(src))

//             ; mov rcx, _vm_state
//             ; sub rsp, BYTE stack_space
//             ; mov r10, QWORD get_heap_address
//             ; call r10
//             ; add rsp, BYTE stack_space

//             ; mov rcx, rax
//             ; mov rdx, r14
//             ; mov r8, r15
//             ; sub rsp, BYTE stack_space
//             ; mov r10, QWORD store_address
//             ; call r10
//             ; add rsp, BYTE stack_space);

//         load_volatiles(ops, &saved);
//     }
//     Alloc(r, t) => {
//         let saved = store_volatiles_except(ops, memory, Some(r));
//         let get_heap_address = unsafe {
//             std::mem::transmute::<_, i64>(
//                 VMState::heap_mut as unsafe extern "C" fn(&mut VMState) -> &mut Heap,
//             )
//         };

//         let alloc_address = unsafe {
//             std::mem::transmute::<_, i64>(
//                 Heap::allocate_type as unsafe extern "C" fn(&mut Heap, &Type) -> Pointer,
//             )
//         };

//         let ty = module.get_type_by_id(t).expect("type idx");

//         let stack_space = 0x20;
//         x64_asm!(ops
//                 ; mov rcx, _vm_state
//                 ; sub rsp, BYTE stack_space
//                 ; mov r10, QWORD get_heap_address
//                 ; call r10
//                 ; add rsp, BYTE stack_space

//                 ; mov rcx, rax
//                 ; mov rdx, QWORD ty as *const _ as i64
//                 ; sub rsp, BYTE stack_space
//                 ; mov r10, QWORD alloc_address
//                 ; call r10
//                 ; add rsp, BYTE stack_space

//                 ;;x64_asm_resolve_mem!(ops, memory; mov resolve(r), rax));

//         load_volatiles(ops, &saved);
//     }
//     LoadConst(res, idx) => {
//         let const_decl = module.get_constant_by_id(idx).unwrap();

//         let get_heap_address = unsafe {
//             std::mem::transmute::<_, i64>(
//                 VMState::heap_mut as unsafe extern "C" fn(&mut VMState) -> &mut Heap,
//             )
//         };

//         let write_to_heap_address = unsafe {
//             std::mem::transmute::<_, i64>(
//                 Module::write_constant_to_heap
//                     as unsafe extern "C" fn(&mut Heap, &Type, &Constant) -> Pointer,
//             )
//         };

//         let saved = store_volatiles_except(ops, memory, Some(res));
//         let stack_space = 0x20;
//         x64_asm!(ops
//             ; mov rcx, _vm_state
//             ; sub rsp, BYTE stack_space
//             ; mov r10, QWORD get_heap_address
//             ; call r10
//             ; add rsp, BYTE stack_space
//             ; mov rcx, rax
//             // TODO investigate, this wont work if the constdecl is moved, dangling ptr
//             ; mov rdx, QWORD &const_decl.type_ as *const _ as i64
//             ; mov r8, QWORD &const_decl.value as *const _ as i64
//             ; sub rsp, BYTE stack_space
//             ; mov r10, QWORD write_to_heap_address
//             ; call r10
//             ; add rsp, BYTE stack_space
//             ;; x64_asm_resolve_mem!(ops, memory; mov resolve(res), rax)
//         );

//         load_volatiles(ops, &saved);
//     }
//     Sizeof(res, src) => {
//         let get_heap_address = unsafe {
//             std::mem::transmute::<_, i64>(
//                 VMState::heap_mut as unsafe extern "C" fn(&mut VMState) -> &mut Heap,
//             )
//         };

//         let load_8_bytes_address = unsafe {
//             std::mem::transmute::<_, i64>(
//                 Heap::load_u64 as unsafe extern "C" fn(&mut Heap, Pointer) -> u64,
//             )
//         };

//         let saved = store_volatiles_except(ops, memory, Some(res));

//         let stack_space = 0x20;
//         x64_asm!(ops
//             // Load first because they might be overriden
//             ;; x64_asm_resolve_mem!(ops, memory; mov r14, resolve(src))

//             ; mov rcx, _vm_state
//             ; sub rsp, BYTE stack_space
//             ; mov r10, QWORD get_heap_address
//             ; call r10
//             ; add rsp, BYTE stack_space
//             // rax contains pointer to heap, mov into rcx (first arg)
//             ; mov rcx, rax
//             // load value
//             ; mov rdx, r14
//             ; sub rsp, BYTE stack_space
//             ; mov r10, QWORD load_8_bytes_address
//             ; call r10
//             ; add rsp, BYTE stack_space
//             ;; x64_asm_resolve_mem!(ops, memory; mov resolve(res), rax)
//         );

//         load_volatiles(ops, &saved);
//     }
//     Panic => {
//         let panic_address: i64 = unsafe {
//             std::mem::transmute::<_, i64>(
//                 JITEngine::internal_panic as extern "win64" fn(&VM, &mut VMState),
//             )
//         };

//         x64_asm!(ops
//             ; mov rcx, _vm
//             ; mov rdx, _vm_state
//             ; mov r10, QWORD panic_address
//             ; call r10);
//     }
//     instr => panic!("Unknown instr {:?}", instr),
// }
