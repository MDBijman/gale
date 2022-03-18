use crate::bytecode::{ModuleLoader, TypeDecl, TypeId};
use crate::jit::FunctionJITState;
use crate::{
    bytecode::{InstrLbl, Module},
    parser::Term,
    vm::{VMState, VM},
};
use std::fmt::Display;
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
pub trait Instruction: Debug + Display + InstrToBytecode + InstrClone + InstrToX64 {
    fn reads(&self) -> HashSet<Var>;
    fn writes(&self) -> HashSet<Var>;
    fn interpret(&self, vm: &VM, state: &mut VMState) -> bool;
}

pub trait InstrToX64 {
    fn emit(&self, fn_state: &mut FunctionJITState, module: &Module, cur: InstrLbl);
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
        module_loader: &ModuleLoader,
        module: &mut Module,
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
        module_loader: &ModuleLoader,
        module: &mut Module,
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
        module_loader: &ModuleLoader,
        module: &mut Module,
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
        module_loader: &ModuleLoader,
        module: &mut Module,
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
        module_loader: &ModuleLoader,
        module: &mut Module,
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
