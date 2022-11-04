use std::{
    collections::HashSet,
    convert::TryInto,
    fmt::{Display, Formatter},
};

use vm_internal::{
    bytecode::{Module, ModuleLoader, Type, TypeId},
    dialect::{
        Dialect, FromTerm, InstrToBytecode, InstrToX64, Instruction, InstructionParseError, Var,
    },
    interpreter::{CallInfo, InterpreterStatus, Value},
    jit::FunctionJITState,
    parser::Term,
    vm::{VMState, VM},
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
        let p = match i {
            Term::Instruction(i) => i,
            Term::LabeledInstruction(_, i) => i,
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

#[derive(Clone, Debug)]
struct Name(Vec<String>);

impl FromTerm for Name {
    fn construct(
        _: &ModuleLoader,
        _: &mut Module,
        p: &Term,
    ) -> Result<Self, InstructionParseError> {
        match p {
            Term::Name(n) => Ok(Name(n.clone())),
            t => panic!("Cannot make Name from {:?}", t),
        }
    }
}

impl Display for Name {
    fn fmt(&self, f: &mut Formatter<'_>) -> std::fmt::Result {
        let mut i = 0;
        for n in self.0.iter() {
            write!(f, "{}", n)?;

            i += 1;
            if i < self.0.len() {
                write!(f, ":")?;
            }
        }

        Ok(())
    }
}

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

    fn interpret(&self, _: &VM, vm_state: &mut VMState) -> bool {
        vm_state
            .interpreter_state
            .set_stack_var(self.0, Value::UI64(self.1 as u64));
        vm_state.interpreter_state.ip += 1;
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
    fn emit(&self, _: &mut FunctionJITState, _: &Module, _: i16) {
        todo!()
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

    fn interpret(&self, _: &VM, vm_state: &mut VMState) -> bool {
        vm_state
            .interpreter_state
            .set_stack_var(self.0, Value::Bool(self.1));
        vm_state.interpreter_state.ip += 1;
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
        let val = state.interpreter_state.get_stack_var(self.1).clone();
        state.interpreter_state.set_stack_var(self.0, val);
        state.interpreter_state.ip += 1;
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
        state
            .interpreter_state
            .set_stack_var(self.0, Value::CallTarget(todo!()));
        state.interpreter_state.ip += 1;
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
        let val_a = *state
            .interpreter_state
            .get_stack_var(self.1)
            .as_ui64()
            .expect("invalid type");

        let val_b = *state
            .interpreter_state
            .get_stack_var(self.2)
            .as_ui64()
            .expect("invalid type");

        let res = val_a < val_b;

        state
            .interpreter_state
            .set_stack_var(self.0, Value::Bool(res));

        state.interpreter_state.ip += 1;

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
        let val_a = *state
            .interpreter_state
            .get_stack_var(self.1)
            .as_ui64()
            .expect("invalid type");
        let val_b = *state
            .interpreter_state
            .get_stack_var(self.2)
            .as_ui64()
            .expect("invalid type");
        let res = val_a - val_b;

        state
            .interpreter_state
            .set_stack_var(self.0, Value::UI64(res));
        state.interpreter_state.ip += 1;

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
        let val_a = *state
            .interpreter_state
            .get_stack_var(self.1)
            .as_ui64()
            .expect("invalid type");
        let val_b = *state
            .interpreter_state
            .get_stack_var(self.2)
            .as_ui64()
            .expect("invalid type");
        let res = val_a + val_b;

        state
            .interpreter_state
            .set_stack_var(self.0, Value::UI64(res));
        state.interpreter_state.ip += 1;

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
        let val = state.interpreter_state.get_stack_var(self.0).clone();
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

        state
            .interpreter_state
            .set_stack_var(ci.result_var, val.clone());

        // Native function
        if !ci.called_by_native {
            state.interpreter_state.ip += 1;
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

        let idx_value = state
            .interpreter_state
            .get_stack_var(self.2)
            .as_ui64()
            .expect("invalid index");

        match current_module
            .get_type_by_id(self.3)
            .expect("invalid type id")
        {
            Type::Tuple(_) => {
                let tuple_value = state
                    .interpreter_state
                    .get_stack_var(self.1)
                    .as_tuple()
                    .expect("value didn't match tuple type");

                let element_value = tuple_value
                    .get(*idx_value as usize)
                    .expect("index out of bounds")
                    .clone();

                state.interpreter_state.set_stack_var(self.0, element_value);
            }
            Type::Pointer(_) => {
                let ptr = *state
                    .interpreter_state
                    .get_stack_var(self.1)
                    .as_pointer()
                    .expect("value didn't match pointer type");

                let new_ptr = state
                    .heap
                    .index::<u64>(ptr, *idx_value as usize + 1 /* array header */);

                let loaded_value = unsafe { state.heap.load::<u64>(new_ptr) };

                state
                    .interpreter_state
                    .set_stack_var(self.0, Value::Pointer(loaded_value.into()));
            }
            _ => panic!("invalid operand"),
        }

        state.interpreter_state.ip += 1;

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
        _fn_state: &mut FunctionJITState,
        _module: &Module,
        _cur: vm_internal::bytecode::InstrLbl,
    ) {
        todo!();
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

            let arg_value: u64 = state
                .interpreter_state
                .get_stack_var(self.2 .0[0])
                .clone()
                .into();

            // Signature of native functions is (vm state, arg)
            let res = unsafe {
                let unsafe_fn_ptr =
                    std::mem::transmute::<_, fn(&mut VMState, u64) -> u64>(native_impl.fn_ptr);
                (unsafe_fn_ptr)(state, arg_value)
            };

            // Store result
            state
                .interpreter_state
                .set_stack_var(self.0, Value::UI64(res));

            state.interpreter_state.ip += 1;
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
                let val = state.interpreter_state.get_stack_var(*arg).clone();
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

        match state.interpreter_state.get_stack_var(self.0) {
            Value::Bool(b) => {
                if *b {
                    state.interpreter_state.ip += 1;
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
