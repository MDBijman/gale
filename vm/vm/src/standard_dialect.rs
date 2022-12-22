use std::{
    collections::HashSet,
    convert::TryInto,
    fmt::{Display, Formatter},
};

use dynasmrt::DynasmLabelApi;
use dynasmrt::{dynasm, DynasmApi};
use memoffset::offset_of;
use vm_internal::{
    bytecode::{CallTarget, Function, InstrLbl, Module, ModuleLoader, TypeId},
    cfg::{ControlFlowBehaviour, InstrControlFlow},
    dialect::{
        Dialect, FromTerm, InstrToBytecode, InstrToX64, Instruction, InstructionParseError, Name,
        Var,
    },
    interpreter::{CallInfo, InterpreterStatus, Value},
    jit::{self, FunctionJITState, VarLoc, load_volatiles, store_volatiles_except},
    memory::{Heap, Pointer},
    parser::{Term, TypeTerm},
    typechecker::{InstrTypecheck, Type, TypeEnvironment, TypeError},
    vm::{VMState, VM},
    x64_asm, x64_asm_resolve_mem,
};
use vm_proc_macros::FromTerm;

/*
* The standard dialect defines commonly used instructions.
*/
pub struct StandardDialect {}

impl Dialect for StandardDialect {
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
        match p.name.as_str() {
            "ui32" => Ok(Box::from(
                ConstU32::construct(module_loader, module, i).unwrap(),
            )),
            "bool" => Ok(Box::from(
                ConstBool::construct(module_loader, module, i).unwrap(),
            )),
            "ret" => Ok(Box::from(
                Return::construct(module_loader, module, i).unwrap(),
            )),
            "cadr" => Ok(Box::from(
                ComputeAddress::construct(module_loader, module, i).unwrap(),
            )),
            "load" => Ok(Box::from(
                Load::construct(module_loader, module, i).unwrap(),
            )),
            "add" => Ok(Box::from(
                AddVarVar::construct(module_loader, module, i).unwrap(),
            )),
            "sub" => Ok(Box::from(
                SubVarVar::construct(module_loader, module, i).unwrap(),
            )),
            "call" => Ok(Box::from(
                Call::construct(module_loader, module, i).unwrap(),
            )),
            "lt" => Ok(Box::from(
                LtVarVar::construct(module_loader, module, i).unwrap(),
            )),
            "jmpifn" => Ok(Box::from(
                JmpIfNot::construct(module_loader, module, i).unwrap(),
            )),
            i => panic!("unknown instruction: {}", i),
        }
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
    fn reads(&self) -> HashSet<Var> {
        HashSet::default()
    }

    fn writes(&self) -> HashSet<Var> {
        let mut r = HashSet::new();
        r.insert(self.0);
        r
    }

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
        x64_asm_resolve_mem!(ops, memory; mov resolve(r), c);
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
    fn reads(&self) -> HashSet<Var> {
        HashSet::default()
    }

    fn writes(&self) -> HashSet<Var> {
        let mut r = HashSet::new();
        r.insert(self.0);
        r
    }

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
* CopyAddress
*/

#[derive(FromTerm, Clone, Debug)]
struct CopyAddress(Var, Name);

impl Instruction for CopyAddress {
    fn reads(&self) -> HashSet<Var> {
        HashSet::new()
    }

    fn writes(&self) -> HashSet<Var> {
        let mut r = HashSet::new();
        r.insert(self.0);
        r
    }

    fn interpret(&self, _vm: &VM, state: &mut VMState) -> bool {
        Self::set_var(state, self.0, Value::CallTarget(todo!()));
        Self::inc_ip(state);
    }
}

impl InstrToBytecode for CopyAddress {
    fn op_size(&self) -> u32 {
        std::convert::TryInto::try_into(std::mem::size_of::<Self>()).unwrap()
    }

    fn emplace(&self, _bytes: &mut [u8]) {
        todo!();
    }
}

impl InstrToX64 for CopyAddress {
    fn emit(&self, _: &VM, _: &Module, _: &mut FunctionJITState, _: InstrLbl) {
        todo!()
    }
}

impl InstrControlFlow for CopyAddress {
    fn behaviour(&self) -> ControlFlowBehaviour {
        ControlFlowBehaviour::Linear
    }
}

impl InstrTypecheck for CopyAddress {
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

impl Display for CopyAddress {
    fn fmt(&self, f: &mut Formatter<'_>) -> Result<(), std::fmt::Error> {
        write!(f, "movi ${}, {}", self.0, self.1)
    }
}

/*
* CopyIntoIndex
*/

#[derive(FromTerm, Debug, Clone)]
struct CopyIntoIndex(Var, u8, Var);

impl Instruction for CopyIntoIndex {
    fn reads(&self) -> HashSet<Var> {
        let mut r = HashSet::new();
        r.insert(self.2);
        r
    }

    fn writes(&self) -> HashSet<Var> {
        let mut r = HashSet::new();
        r.insert(self.0);
        r
    }

    fn interpret(&self, _vm: &VM, _state: &mut VMState) -> bool {
        todo!()
    }
}

impl InstrToBytecode for CopyIntoIndex {
    fn op_size(&self) -> u32 {
        std::convert::TryInto::try_into(std::mem::size_of::<Self>()).unwrap()
    }

    fn emplace(&self, _bytes: &mut [u8]) {
        todo!();
    }
}

impl InstrToX64 for CopyIntoIndex {
    fn emit(&self, _: &VM, _: &Module, _: &mut FunctionJITState, _: InstrLbl) {
        todo!();
    }
}

impl InstrControlFlow for CopyIntoIndex {
    fn behaviour(&self) -> ControlFlowBehaviour {
        ControlFlowBehaviour::Linear
    }
}

impl InstrTypecheck for CopyIntoIndex {
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

impl Display for CopyIntoIndex {
    fn fmt(&self, _f: &mut Formatter<'_>) -> Result<(), std::fmt::Error> {
        todo!()
    }
}

/*
* CopyAddressIntoIndex
*/

#[derive(FromTerm, Debug, Clone)]
struct CopyAddressIntoIndex(Var, u8, Name);

impl Instruction for CopyAddressIntoIndex {
    fn reads(&self) -> HashSet<Var> {
        todo!()
    }

    fn writes(&self) -> HashSet<Var> {
        todo!()
    }

    fn interpret(&self, _vm: &VM, _state: &mut VMState) -> bool {
        todo!()
    }
}

impl InstrToBytecode for CopyAddressIntoIndex {
    fn op_size(&self) -> u32 {
        std::convert::TryInto::try_into(std::mem::size_of::<Self>()).unwrap()
    }

    fn emplace(&self, _bytes: &mut [u8]) {
        todo!();
    }
}

impl InstrToX64 for CopyAddressIntoIndex {
    fn emit(&self, _: &VM, _: &Module, _: &mut FunctionJITState, _: InstrLbl) {
        todo!();
    }
}

impl InstrControlFlow for CopyAddressIntoIndex {
    fn behaviour(&self) -> ControlFlowBehaviour {
        ControlFlowBehaviour::Linear
    }
}

impl InstrTypecheck for CopyAddressIntoIndex {
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

impl Display for CopyAddressIntoIndex {
    fn fmt(&self, _f: &mut Formatter<'_>) -> Result<(), std::fmt::Error> {
        todo!()
    }
}

/*
* EqVarVar
*/

#[derive(FromTerm, Debug, Clone)]
struct EqVarVar(Var, Var, Var);

impl Instruction for EqVarVar {
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
                ;; x64_asm_resolve_mem!(ops, memory; cmp resolve(self.1), resolve(self.2))
                ;; x64_asm_resolve_mem!(ops, memory; mov resolve(self.0), 0)
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
        x64_asm_resolve_mem!(ops, memory; mov rsi, resolve(self.1));
        x64_asm_resolve_mem!(ops, memory; sub rsi, resolve(self.2));
        x64_asm_resolve_mem!(ops, memory; mov resolve(self.0), rsi);
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
        x64_asm_resolve_mem!(ops, memory; mov rsi, resolve(self.1));
        x64_asm_resolve_mem!(ops, memory; add rsi, resolve(self.2));
        x64_asm_resolve_mem!(ops, memory; mov resolve(self.0), rsi);
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
    fn reads(&self) -> HashSet<Var> {
        let mut r = HashSet::new();
        r.insert(self.0);
        r
    }

    fn writes(&self) -> HashSet<Var> {
        HashSet::default()
    }

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
                ;; x64_asm_resolve_mem!(ops, memory; mov rax, resolve(self.0))
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
                    let offset = t.size() * (*idx_value as usize);

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
    fn emit(&self, _: &VM, module: &Module, state: &mut FunctionJITState, _: InstrLbl) {
        let ops = &mut state.ops;
        let memory = state.variable_locations.as_ref().unwrap();

        let element_size: i32 = match module.get_type_by_id(self.2).expect("type idx") {
            Type::Pointer(inner_type) => match &**inner_type {
                Type::Array(element_type, _) => element_type.size() as i32,
                _ => panic!("Illegal cadr operand: {:?}", inner_type),
            },
            ty => panic!("Illegal cadr operand: {:?}", ty),
        };

        let saved = store_volatiles_except(ops, memory, Some(self.0));

        x64_asm!(ops
            // Load first because they might be overriden
            ;; x64_asm_resolve_mem!(ops, memory; mov r14, resolve(self.1))
            ;; x64_asm_resolve_mem!(ops, memory; mov r15, resolve(self.3))

            // 2) compute pointer to element
            // base + element_size * index (r15) + 8 (array header)
            ; mov rax, element_size
            ; mov r8, r15
            ; mul r8
            // Offset for array header
            ; add rax, 8
            // Offset with base pointer
            ; add rax, r14

            ;; x64_asm_resolve_mem!(ops, memory; mov resolve(self.0), rax)
        );

        load_volatiles(ops, &saved);
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
                Type::Pointer(_) => todo!(),
                _ => panic!(),
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

        let load_fn_address: i64 = unsafe {
            std::mem::transmute::<_, i64>(
                Heap::load::<u64> as unsafe extern "C" fn(&mut Heap, Pointer) -> u64,
            )
        };

        let heap_offset_in_vm_state: i32 = offset_of!(VMState, heap).try_into().unwrap();
        let stack_space = 0x20;

        let saved = store_volatiles_except(ops, memory, Some(self.0));

        x64_asm!(ops
        // Pointer to value in rdx (do this first to avoid invalidation)
        ;; x64_asm_resolve_mem!(ops, memory; mov rdx, resolve(self.1))
        // Pointer to heap in rcx
        ; mov rcx, _vm_state
        ; add rcx, heap_offset_in_vm_state
        ; mov rcx, [rcx]
        ; sub rsp, BYTE stack_space
        ; mov r10, QWORD load_fn_address
        ; call r10
        ; add rsp, BYTE stack_space
        ;; x64_asm_resolve_mem!(ops, memory; mov resolve(self.0), rax)
        );

        load_volatiles(ops, &saved);
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
struct Call(Var, Name, CallArgs);

impl Call {
    fn resolve<'a>(&self, vm: &'a VM, current_module: u16) -> (&'a Module, &'a Function) {
        let (callee_module, callee_function) = if self.1 .0.len() == 2 {
            let module = vm.module_loader.get_module_by_name(&self.1 .0[0]).unwrap();
            let callee = module
                .get_function_by_name(self.1 .0[1].as_str())
                .expect("invalid callee");
            (module, callee)
        } else if self.1 .0.len() == 1 {
            let module = vm.module_loader.get_module_by_id(current_module).unwrap();
            let callee = module
                .get_function_by_name(self.1 .0[0].as_str())
                .expect("invalid callee");
            (module, callee)
        } else {
            panic!("unsupported");
        };

        (callee_module, callee_function)
    }
}

impl Instruction for Call {
    fn reads(&self) -> HashSet<Var> {
        let mut r = HashSet::new();
        r.extend(self.2 .0.iter());
        r
    }

    fn writes(&self) -> HashSet<Var> {
        let mut r = HashSet::new();
        r.insert(self.0);
        r
    }

    fn interpret(&self, vm: &VM, state: &mut VMState) -> bool {
        let (callee_module, callee_function) = self.resolve(vm, state.interpreter_state.module);

        if callee_function.has_native_impl() {
            let native_impl = callee_function.native_impl().unwrap();

            // Native functions currently only have one 64 bit argument
            if self.2 .0.len() > 1 {
                panic!("native function called with more than 1 arg");
            }

            let arg_value = Self::ref_var(state, self.2 .0[0]).as_pointer().unwrap();

            // Signature of native functions is (vm state, arg)
            let res = unsafe {
                let unsafe_fn_ptr = std::mem::transmute::<
                    _,
                    extern "C" fn(&mut VMState, Pointer) -> u64,
                >(native_impl.fn_ptr);
                (unsafe_fn_ptr)(state, *arg_value)
            };

            // Store result
            Self::set_var(state, self.0, Value::UI64(res));
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
                result_var: self.0,
                framesize: stack_size as usize,
                prev_ip: state.interpreter_state.ip,
                prev_fn: state.interpreter_state.func,
                prev_mod: state.interpreter_state.module,
            };

            // Write arguments into new frame
            for (idx, arg) in self.2 .0.iter().enumerate() {
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
    fn emit(&self, vm: &VM, module: &Module, state: &mut FunctionJITState, _: InstrLbl) {
        assert!(self.2 .0.len() == 1 || self.2 .0.len() == 0);
        let (resolved_module, resolved_function) = self.resolve(vm, module.id());
        let memory = state.variable_locations.as_ref().unwrap();
        let current_module_id = module.id();
        let resolved_module_id = resolved_module.id();
        let res = self.0;
        let ops = &mut state.ops;

        let trampoline_address: i64 = unsafe {
            std::mem::transmute::<_, i64>(
                jit::trampoline as extern "win64" fn(&VM, &mut VMState, CallTarget, Pointer) -> u64,
            )
        };

        let saved = store_volatiles_except(ops, memory, Some(self.0));

        let has_arg = self.2 .0.len() == 1;

        // Recursive call
        // We know we can call the native impl. directly
        if resolved_module.id() == current_module_id
            && resolved_function.location == state.function_location
        {
            let mut stack_space = 0x20;

            if has_arg {
                let arg = self.2 .0[0];
                x64_asm_resolve_mem!(ops, memory; mov r8, resolve(arg));
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
                    ;; x64_asm_resolve_mem!(ops, memory; mov resolve(res), rax));
        }
        // Unknown target
        // Conservatively call trampoline
        else {
            let mut stack_space = 0x20;

            if has_arg {
                let arg = self.2 .0[0];
                x64_asm_resolve_mem!(ops, memory; mov r9, resolve(arg));
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
                ;; x64_asm_resolve_mem!(ops, memory; mov resolve(res), rax));

            if has_arg {
                x64_asm!(ops; add rsp, 8);
            }
        }

        load_volatiles(ops, &saved);
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
                if !env.insert_or_check(self.0, *to.clone()) {
                    return Err(TypeError::Mismatch {
                        expected: *to.clone(),
                        was: env.get(self.0).unwrap().clone(),
                        var: self.0,
                    });
                }

                if let Type::Tuple(params) = &**from {
                    for (param, arg) in params.iter().zip(self.2 .0.iter()) {
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

                    if self.2 .0.len() == 1 {
                        let arg: Var = self.2 .0[0];

                        if !env.insert_or_check(arg, (**from).clone()) {
                            return Err(TypeError::Mismatch {
                                expected: (**from).clone(),
                                was: env.get(arg).unwrap().clone(),
                                var: arg,
                            });
                        }
                    } else if self.2 .0.len() == 0 {
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
        write!(f, "call {}, {}, {}", self.0, self.1, self.2)
    }
}

/*
* JmpIfNot
*/

#[derive(FromTerm, Clone, Debug)]
struct JmpIfNot(Var, Name);

impl Instruction for JmpIfNot {
    fn reads(&self) -> HashSet<Var> {
        let mut r = HashSet::new();
        r.insert(self.0);
        r
    }

    fn writes(&self) -> HashSet<Var> {
        HashSet::new()
    }

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
                ;; x64_asm_resolve_mem!(ops, memory ; cmp resolve(self.0), 0)
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
