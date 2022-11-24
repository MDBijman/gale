use std::{
    collections::HashSet,
    convert::TryInto,
    fmt::{Display, Formatter},
};

use dynasmrt::{dynasm, DynasmApi};
use vm_internal::{
    bytecode::{Module, ModuleLoader, Type, TypeId},
    cfg::{ControlFlowBehaviour, InstrControlFlow},
    dialect::{
        Dialect, FromTerm, InstrToBytecode, InstrToX64, Instruction, InstructionParseError, Name,
        Var,
    },
    interpreter::{CallInfo, InterpreterStatus, Value},
    jit::{FunctionJITState, VarLoc, VariableLocations, store_volatiles_except, load_volatiles},
    memory::{Pointer, ARRAY_HEADER_SIZE, Heap},
    parser::Term,
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
            "idx" => Ok(Box::from(
                Index::construct(module_loader, module, i).unwrap(),
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
    fn emit(&self, fun: &mut FunctionJITState, _: &Module, _: i16) {
        // Ops cannot be referenced in x64_asm macros if it's inside fn_state
        let ops = &mut fun.ops;
        let memory = fun
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
    fn emit(
        &self,
        _fn_state: &mut FunctionJITState,
        _module: &Module,
        _cur: vm_internal::bytecode::InstrLbl,
    ) {
        todo!()
    }
}

impl InstrControlFlow for ConstBool {
    fn behaviour(&self) -> ControlFlowBehaviour {
        ControlFlowBehaviour::Linear
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
    fn emit(
        &self,
        _fn_state: &mut FunctionJITState,
        _module: &Module,
        _cur: vm_internal::bytecode::InstrLbl,
    ) {
        todo!()
    }
}

impl InstrControlFlow for Copy {
    fn behaviour(&self) -> ControlFlowBehaviour {
        ControlFlowBehaviour::Linear
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
    fn emit(
        &self,
        _fn_state: &mut FunctionJITState,
        _module: &Module,
        _cur: vm_internal::bytecode::InstrLbl,
    ) {
        todo!()
    }
}

impl InstrControlFlow for CopyAddress {
    fn behaviour(&self) -> ControlFlowBehaviour {
        ControlFlowBehaviour::Linear
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
    fn emit(
        &self,
        _fn_state: &mut FunctionJITState,
        _module: &Module,
        _cur: vm_internal::bytecode::InstrLbl,
    ) {
        todo!();
    }
}

impl InstrControlFlow for CopyIntoIndex {
    fn behaviour(&self) -> ControlFlowBehaviour {
        ControlFlowBehaviour::Linear
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
    fn emit(
        &self,
        _fn_state: &mut FunctionJITState,
        _module: &Module,
        _cur: vm_internal::bytecode::InstrLbl,
    ) {
        todo!();
    }
}

impl InstrControlFlow for CopyAddressIntoIndex {
    fn behaviour(&self) -> ControlFlowBehaviour {
        ControlFlowBehaviour::Linear
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
    fn emit(
        &self,
        _fn_state: &mut FunctionJITState,
        _module: &Module,
        _cur: vm_internal::bytecode::InstrLbl,
    ) {
        todo!();
    }
}

impl InstrControlFlow for EqVarVar {
    fn behaviour(&self) -> ControlFlowBehaviour {
        ControlFlowBehaviour::Linear
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
    fn emit(
        &self,
        _fn_state: &mut FunctionJITState,
        _module: &Module,
        _cur: vm_internal::bytecode::InstrLbl,
    ) {
        todo!();
    }
}

impl InstrControlFlow for LtVarVar {
    fn behaviour(&self) -> ControlFlowBehaviour {
        ControlFlowBehaviour::Linear
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
    fn emit(
        &self,
        _fn_state: &mut FunctionJITState,
        _module: &Module,
        _cur: vm_internal::bytecode::InstrLbl,
    ) {
        todo!();
    }
}

impl InstrControlFlow for SubVarVar {
    fn behaviour(&self) -> ControlFlowBehaviour {
        ControlFlowBehaviour::Linear
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
    fn emit(
        &self,
        _fn_state: &mut FunctionJITState,
        _module: &Module,
        _cur: vm_internal::bytecode::InstrLbl,
    ) {
        todo!();
    }
}

impl InstrControlFlow for AddVarVar {
    fn behaviour(&self) -> ControlFlowBehaviour {
        ControlFlowBehaviour::Linear
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
    fn emit(
        &self,
        _fn_state: &mut FunctionJITState,
        _module: &Module,
        _cur: vm_internal::bytecode::InstrLbl,
    ) {
        todo!();
    }
}

impl InstrControlFlow for MulVarVar {
    fn behaviour(&self) -> ControlFlowBehaviour {
        ControlFlowBehaviour::Linear
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
    fn emit(
        &self,
        _fn_state: &mut FunctionJITState,
        _module: &Module,
        _cur: vm_internal::bytecode::InstrLbl,
    ) {
        todo!();
    }
}

impl InstrControlFlow for NotVar {
    fn behaviour(&self) -> ControlFlowBehaviour {
        ControlFlowBehaviour::Linear
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
    fn emit(
        &self,
        _fn_state: &mut FunctionJITState,
        _module: &Module,
        _cur: vm_internal::bytecode::InstrLbl,
    ) {
        todo!();
    }
}

impl Display for Return {
    fn fmt(&self, f: &mut Formatter<'_>) -> Result<(), std::fmt::Error> {
        write!(f, "ret {}", self.0)
    }
}

impl InstrControlFlow for Return {
    fn behaviour(&self) -> ControlFlowBehaviour {
        ControlFlowBehaviour::Jump(vec![])
    }
}

/*
* Index
*/

#[derive(FromTerm, Debug, Clone)]
struct Index(Var, Var, Var, TypeId);

impl Instruction for Index {
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

    fn interpret(&self, vm: &VM, state: &mut VMState) -> bool {
        let current_module = vm
            .module_loader
            .get_module_by_id(state.interpreter_state.module)
            .unwrap();

        let idx_value = Self::ref_var(state, self.2)
            .as_ui64()
            .expect("invalid index");

        match current_module
            .get_type_by_id(self.3)
            .expect("invalid type id")
        {
            Type::Tuple(_) => {
                let tuple_value = Self::ref_var(state, self.1)
                    .as_tuple()
                    .expect("value didn't match tuple type");

                let element_value = tuple_value
                    .get(*idx_value as usize)
                    .expect("index out of bounds")
                    .clone();

                Self::set_var(state, self.0, element_value);
            }
            Type::Array(t, _) => {
                let ptr = Self::ref_var(state, self.1)
                    .as_pointer()
                    .expect("value didn't match pointer type");

                // increment ptr
                let elem_ptr = ptr.index(ARRAY_HEADER_SIZE + t.size() * (*idx_value as usize));

                match **t {
                    Type::Pointer(_) => unsafe {
                        let v = state.heap.load::<Pointer>(elem_ptr);
                        Self::set_var(state, self.0, Value::Pointer(v));
                    },
                    _ => panic!(),
                }
            }
            Type::Pointer(_) => {
                let ptr = *Self::ref_var(state, self.1)
                    .as_pointer()
                    .expect("value didn't match pointer type");

                Self::set_var(
                    state,
                    self.0,
                    Value::Pointer(ptr.index(*idx_value as usize)),
                );

                panic!("should idx also deref?");
            }
            _ => panic!("invalid operand"),
        }

        Self::inc_ip(state);

        true
    }
}

impl InstrToBytecode for Index {
    fn op_size(&self) -> u32 {
        std::convert::TryInto::try_into(std::mem::size_of::<Self>()).unwrap()
    }

    fn emplace(&self, _bytes: &mut [u8]) {
        todo!();
    }
}

impl InstrToX64 for Index {
    fn emit(
        &self,
        state: &mut FunctionJITState,
        module: &Module,
        _cur: vm_internal::bytecode::InstrLbl,
    ) {
        let mut ops = &mut state.ops;
        let memory = state.variable_locations.as_ref().unwrap();

        let get_heap_address = unsafe {
            std::mem::transmute::<_, i64>(
                VMState::heap_mut as unsafe extern "C" fn(&mut VMState) -> &mut Heap,
            )
        };

        let index_bytes_address = unsafe {
            std::mem::transmute::<_, i64>(
                Heap::index_bytes as unsafe extern "C" fn(&mut Heap, Pointer, usize) -> Pointer,
            )
        };

        let index_bytes: i32 = match module.get_type_by_id(self.3).expect("type idx") {
            Type::U64 | Type::Pointer(_) => 8,
            ty => panic!("Illegal index operand: {:?}", ty),
        };

        let load_8_bytes_address = unsafe {
            std::mem::transmute::<_, i64>(
                Heap::load::<u64> as unsafe extern "C" fn(&mut Heap, Pointer) -> u64,
            )
        };

        // call get_heap_address to get heap ptr
        // load idx, multiply with element size in bytes
        // load src
        // call heap.index_bytes on src value with idx
        // write res

        let saved = store_volatiles_except(&mut ops, &memory, Some(self.0));
        let stack_space = 0x20;
        x64_asm!(ops
            // Load first because they might be overriden
            ;; x64_asm_resolve_mem!(ops, memory; mov r14, resolve(self.1))
            ;; x64_asm_resolve_mem!(ops, memory; mov r15, resolve(self.2))

            ; mov rcx, _vm_state
            ; sub rsp, BYTE stack_space
            ; mov r10, QWORD get_heap_address
            ; call r10
            ; add rsp, BYTE stack_space
            // rax contains pointer to heap, mov into rcx (first arg)
            ; mov rcx, rax
            ; mov rdi, rax // store in non-vol register for later
            // compute idx and put into r8
            ; mov rax, r15
            ; mov r8, index_bytes
            ; mul r8
            ; mov r8, rax
            // Offset for array header
            ; add r8, 8
            // put src into rdx
            ; mov rdx, r14
            ; sub rsp, BYTE stack_space
            ; mov r10, QWORD index_bytes_address
            ; call r10
            ; add rsp, BYTE stack_space
            // rax contains proxy
            // load value
            ; mov rcx, rdi
            ; mov rdx, rax
            ; sub rsp, BYTE stack_space
            ; mov r10, QWORD load_8_bytes_address
            ; call r10
            ; add rsp, BYTE stack_space
            ;; x64_asm_resolve_mem!(ops, memory; mov resolve(self.0), rax)
        );

        load_volatiles(&mut ops, &saved);
    }
}

impl InstrControlFlow for Index {
    fn behaviour(&self) -> ControlFlowBehaviour {
        ControlFlowBehaviour::Linear
    }
}

impl Display for Index {
    fn fmt(&self, f: &mut Formatter<'_>) -> Result<(), std::fmt::Error> {
        write!(f, "idx {}, {}, {}, {}", self.0, self.1, self.2, "<?>")
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
        let (callee_module, callee_function) = if self.1 .0.len() == 2 {
            let module = vm.module_loader.get_module_by_name(&self.1 .0[0]).unwrap();
            let callee = vm
                .module_loader
                .get_module_by_id(module.id)
                .unwrap()
                .get_function_by_name(self.1 .0[1].as_str())
                .expect("invalid callee");
            (module, callee)
        } else if self.1 .0.len() == 1 {
            let module = vm
                .module_loader
                .get_module_by_id(state.interpreter_state.module)
                .unwrap();
            let callee = vm
                .module_loader
                .get_module_by_id(state.interpreter_state.module)
                .unwrap()
                .get_function_by_name(self.1 .0[0].as_str())
                .expect("invalid callee");
            (module, callee)
        } else {
            panic!("unsupported");
        };

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
            state.interpreter_state.module = callee_module.id;
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
    fn emit(
        &self,
        _fn_state: &mut FunctionJITState,
        _module: &Module,
        _cur: vm_internal::bytecode::InstrLbl,
    ) {
        todo!();
    }
}

impl Display for Call {
    fn fmt(&self, f: &mut Formatter<'_>) -> Result<(), std::fmt::Error> {
        write!(f, "call {}, {}, {}", self.0, self.1, self.2)
    }
}

impl InstrControlFlow for Call {
    fn behaviour(&self) -> ControlFlowBehaviour {
        ControlFlowBehaviour::Linear
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
    fn emit(&self, _: &mut FunctionJITState, _: &Module, _: i16) {
        todo!()
    }
}

impl Display for JmpIfNot {
    fn fmt(&self, f: &mut Formatter<'_>) -> Result<(), std::fmt::Error> {
        write!(f, "jmpifn {}, @{}", self.0, self.1)
    }
}

impl InstrControlFlow for JmpIfNot {
    fn behaviour(&self) -> ControlFlowBehaviour {
        ControlFlowBehaviour::ConditionalJump(vec![self.1.clone()])
    }
}
