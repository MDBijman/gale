use crate::bytecode::{Function, ModuleLoader, TypeDecl, TypeId};
use crate::cfg::{ControlFlowBehaviour, InstrControlFlow};
use crate::interpreter::Value;
use crate::jit::FunctionJITState;
use crate::typechecker::{InstrTypecheck, TypeEnvironment, TypeError};
use crate::x64_asm;
use crate::{
    bytecode::{InstrLbl, Module},
    parser::Term,
    vm::{VMState, VM},
};
use dynasmrt::{dynasm, DynasmLabelApi};
use std::fmt::{Display, Formatter};
use std::{collections::HashSet, convert::TryInto, fmt::Debug};
extern crate proc_macro;

/*
* Dialects are the main extension mechanism for adding instructions to the virtual machine.
* A dialect is responsible for providing syntactic and semantic definitions for instructions.
* This includes instruction names and their parameters, and various semantic properties used for
* analysis, execution, and compilation.
*
* Syntactical extensions are not yet implemented. A dialect provides a mapping from generic instruction
* elements (values, variables, blocks) to concrete instructions through the `construct` function.
*
*/
pub trait Dialect {
    fn get_dialect_tag(&self) -> &'static str;

    fn make_instruction(
        &self,
        module_loader: &ModuleLoader,
        module: &mut Module,
        p: &Term,
    ) -> Result<Box<dyn Instruction>, InstructionParseError>;
}

const INSTR_TAG_SIZE: u8 = 1;
const DIALECT_TAG_SIZE: u8 = 1;

/*
* The Instruction trait indicates a term can be executed.
*/
pub trait Instruction:
    Debug + Display + InstrToBytecode + InstrClone + InstrToX64 + InstrControlFlow + InstrTypecheck
{
    fn reads(&self) -> HashSet<Var>;
    fn writes(&self) -> HashSet<Var>;
    fn interpret(&self, vm: &VM, state: &mut VMState) -> bool;

    /*
     * Interpreter helpers
     */

    fn inc_ip(s: &mut VMState)
    where
        Self: Sized,
    {
        s.interpreter_state.ip += 1;
    }

    fn ref_var<'a>(s: &'a VMState, i: Var) -> &'a Value
    where
        Self: Sized,
    {
        s.interpreter_state.get_stack_var(i)
    }

    fn clone_var(s: &VMState, i: Var) -> Value
    where
        Self: Sized,
    {
        s.interpreter_state.get_stack_var(i).clone()
    }

    fn set_var(s: &mut VMState, i: Var, v: Value)
    where
        Self: Sized,
    {
        s.interpreter_state.set_stack_var(i, v)
    }
}

pub trait InstrToX64 {
    fn emit(&self, vm: &VM, module: &Module, fn_state: &mut FunctionJITState, cur: InstrLbl);
}

pub trait InstrToBytecode {
    fn op_size(&self) -> u32;
    fn emplace(&self, bytes: &mut [u8]);
}

pub trait InstrClone {
    fn clone_instr(&self) -> Box<dyn Instruction>;
}

// https://users.rust-lang.org/t/solved-is-it-possible-to-clone-a-boxed-trait-object/1714/7
impl<T> InstrClone for T
where
    T: Instruction + Clone + 'static,
{
    fn clone_instr(&self) -> Box<dyn Instruction> {
        Box::new(self.clone())
    }
}

impl Clone for Box<dyn Instruction> {
    fn clone(&self) -> Box<dyn Instruction> {
        self.clone_instr()
    }
}

#[derive(Hash, PartialEq, Eq, Clone, Copy, Debug, PartialOrd, Ord)]
pub struct Var(pub u8);

impl Display for Var {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "${}", self.0)
    }
}

impl Into<usize> for Var {
    fn into(self) -> usize {
        return self.0 as usize;
    }
}

#[derive(Clone, Debug)]
pub struct Name(pub Vec<String>);

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
* The FromTerm trait allows construction from parsed Terms.
*/

pub trait FromTerm {
    fn construct(
        module_loader: &ModuleLoader,
        module: &mut Module,
        p: &Term,
    ) -> Result<Self, InstructionParseError>
    where
        Self: Sized;
}

impl FromTerm for u8 {
    fn construct(
        _module_loader: &ModuleLoader,
        _module: &mut Module,
        p: &Term,
    ) -> Result<Self, InstructionParseError>
    where
        Self: Sized,
    {
        match p {
            Term::Number(n) => Ok(TryInto::<u8>::try_into(*n).unwrap()),
            _ => panic!(),
        }
    }
}

impl FromTerm for u16 {
    fn construct(
        _module_loader: &ModuleLoader,
        _module: &mut Module,
        p: &Term,
    ) -> Result<Self, InstructionParseError>
    where
        Self: Sized,
    {
        match p {
            Term::Number(n) => Ok(TryInto::<u16>::try_into(*n).unwrap()),
            t => panic!("{:?}", t),
        }
    }
}

impl FromTerm for u32 {
    fn construct(
        _module_loader: &ModuleLoader,
        _module: &mut Module,
        p: &Term,
    ) -> Result<Self, InstructionParseError>
    where
        Self: Sized,
    {
        match p {
            Term::Number(n) => Ok(TryInto::<u32>::try_into(*n).unwrap()),
            _ => panic!(),
        }
    }
}

impl FromTerm for bool {
    fn construct(
        _module_loader: &ModuleLoader,
        _module: &mut Module,
        p: &Term,
    ) -> Result<Self, InstructionParseError>
    where
        Self: Sized,
    {
        match p {
            Term::Bool(b) => Ok(*b),
            _ => panic!(),
        }
    }
}

impl FromTerm for Var {
    fn construct(
        _module_loader: &ModuleLoader,
        _module: &mut Module,
        p: &Term,
    ) -> Result<Self, InstructionParseError>
    where
        Self: Sized,
    {
        match p {
            Term::Variable(b) => Ok(*b),
            _ => panic!(),
        }
    }
}

impl FromTerm for TypeId {
    fn construct(
        module_loader: &ModuleLoader,
        module: &mut Module,
        p: &Term,
    ) -> Result<Self, InstructionParseError>
    where
        Self: Sized,
    {
        match p {
            Term::Type(t) => {
                let idx: u16 = module.types.len().try_into().unwrap();
                module
                    .types
                    .push(TypeDecl::new(idx, module_loader.make_type_from_term(t)));
                Ok(TypeId(idx))
            }
            _ => panic!(),
        }
    }
}

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

#[derive(Debug)]
pub struct InstructionParseError {}

impl Display for InstructionParseError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "parse error")
    }
}

/*
* The instruction buffer holds a flat vector into which instructions are allocated.
*/
#[derive(Debug, Clone)]
pub struct InstructionBuffer {
    buffer: Vec<u8>,
}

pub struct InstructionOffset(usize);

impl InstructionBuffer {
    pub fn alloc(&mut self, instr: &dyn Instruction) -> InstructionOffset {
        let idx = self.buffer.len();
        self.buffer.resize(
            idx + instr.op_size() as usize + DIALECT_TAG_SIZE as usize + INSTR_TAG_SIZE as usize,
            0,
        );
        let last_idx = self.buffer.len();
        instr.emplace(&mut self.buffer[idx..last_idx]);
        InstructionOffset(idx)
    }
}

#[derive(Clone, Debug)]
pub struct LabelInstruction(pub Vec<String>);

impl Instruction for LabelInstruction {
    fn reads(&self) -> HashSet<Var> {
        HashSet::new()
    }

    fn writes(&self) -> HashSet<Var> {
        HashSet::new()
    }

    fn interpret(&self, _vm: &VM, state: &mut VMState) -> bool {
        state.interpreter_state.ip += 1;
        true
    }
}

impl InstrToBytecode for LabelInstruction {
    fn op_size(&self) -> u32 {
        todo!()
    }

    fn emplace(&self, bytes: &mut [u8]) {
        todo!()
    }
}

impl InstrToX64 for LabelInstruction {
    fn emit(&self, vm: &VM, module: &Module, fn_state: &mut FunctionJITState, cur: InstrLbl) {
        let ops = &mut fn_state.ops;
        let dyn_lbl = fn_state
            .dynamic_labels
            .entry(cur)
            .or_insert(ops.new_dynamic_label());

        x64_asm!(ops
                ; =>*dyn_lbl);
    }
}

impl InstrControlFlow for LabelInstruction {
    fn behaviour(&self) -> ControlFlowBehaviour {
        ControlFlowBehaviour::Target
    }
}

impl InstrTypecheck for LabelInstruction {
    fn typecheck(
        &self,
        _: &VM,
        _: &Module,
        _: &Function,
        _: &mut TypeEnvironment,
    ) -> Result<(), TypeError> {
        Ok(())
    }
}

impl Display for LabelInstruction {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "@")?;
        for (i, n) in self.0.iter().enumerate() {
            if i == 0 {
                write!(f, "{}", n)?;
            } else {
                write!(f, ":{}", n)?;
            }
        }
        Ok(())
    }
}
