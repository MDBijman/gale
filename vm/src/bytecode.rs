use std::collections::HashMap;
use std::convert::TryInto;
use std::fmt::{self, Display};

#[derive(Debug, Clone)]
pub struct Module {
    pub constants: Vec<Value>,
    pub functions: Vec<Function>,
}

impl Module {
    pub fn new() -> Module {
        Module {
            constants: Vec::new(),
            functions: Vec::new(),
        }
    }

    pub fn from(consts: Vec<Value>, fns: Vec<Function>) -> Module {
        let mut module = Module::new();
        module.functions = fns;
        for (idx, func) in module.functions.iter_mut().enumerate() {
            func.location = idx.try_into().expect("Too many functions!");
        }

        module.constants = consts;

        module
    }

    pub fn from_long_functions(consts: Vec<Value>, fns: Vec<ParsedFunction>) -> Module {
        let mut compact_fns = Vec::new();

        let mut mapping = HashMap::new();
        for (idx, f) in fns.iter().enumerate() {
            mapping.insert(f.name.clone(), idx);
        }

        for f in fns.into_iter() {
            compact_fns.push(Function::from_long_instr(f, &mapping));
        }

        Module::from(consts, compact_fns)
    }

    pub fn find_function_id(&self, name: &str) -> Option<FnLbl> {
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

    pub fn get_function_by_id(&self, lbl: FnLbl) -> Option<&Function> {
        Some(self.functions.iter().find(|f| f.location == lbl.into())?)
    }
}

#[derive(Debug, Clone)]
pub enum Type {
    U64(),
    Array(Box<Type>),
    Pointer(Box<Type>),
    Null,
    Str,
}

#[derive(Debug, Clone, PartialEq, PartialOrd)]
pub enum Value {
    U64(u64),
    Array(Vec<Value>),
    Pointer(u64),
    Null,
    Str(String),
}

impl Value {
    pub fn as_u64(&self) -> Option<&u64> {
        if let Self::U64(v) = self {
            Some(v)
        } else {
            None
        }
    }
}

impl fmt::Display for Value {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Value::U64(v) => write!(f, "{}", v),
            Value::Array(values) => {
                write!(f, "[")?;
                let mut first = true;
                for v in values {
                    if !first {
                        write!(f, ", ")?;
                    }
                    write!(f, "{}", v)?;
                    first = false;
                }
                write!(f, "]")
            }
            Value::Null => write!(f, "null"),
            Value::Pointer(b) => write!(f, "&{}", b),
            Value::Str(s) => write!(f, "\"{}\"", s),
        }
    }
}

pub type Var = u16;
pub type InstrLbl = i16;
pub type FnLbl = u16;
pub type ConstLbl = u16;

#[derive(Debug, Clone, Copy)]
pub enum Instruction {
    ConstU32(Var, u32),
    Copy(Var, Var),
    EqVarVar(Var, Var, Var),
    LtVarVar(Var, Var, Var),
    SubVarVar(Var, Var, Var),
    AddVarVar(Var, Var, Var),
    NotVar(Var, Var),

    Return(Var),
    Print(Var),
    Call(Var, FnLbl, Var),
    Jmp(InstrLbl),
    JmpIf(InstrLbl, Var),
    JmpIfNot(InstrLbl, Var),

    //Alloc(Var, Type),
    LoadConst(Var, ConstLbl),
    LoadVar(Var, Var),
    StoreVar(Var, Var),
    Index(Var, Var, Var),
    Lbl,
    Nop,
}

impl Display for Instruction {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Instruction::ConstU32(a, b) => write!(f, "movc ${}, {}", a, b),
            Instruction::Copy(a, b) => write!(f, "cp ${}, {}", a, b),
            Instruction::EqVarVar(_, _, _) => todo!(),
            Instruction::LtVarVar(a, b, c) => write!(f, "lt ${}, ${}, ${}", a, b, c),
            Instruction::SubVarVar(a, b, c) => write!(f, "sub ${}, ${}, ${}", a, b, c),
            Instruction::AddVarVar(a, b, c) => write!(f, "add ${}, ${}, ${}", a, b, c),
            Instruction::NotVar(_, _) => todo!(),
            Instruction::Return(a) => write!(f, "ret ${}", a),
            Instruction::Print(_) => todo!(),
            Instruction::Call(a, b, c) => write!(f, "call ${}, @{} (${})", a, b, c),
            Instruction::Jmp(_) => todo!(),
            Instruction::JmpIf(_, _) => todo!(),
            Instruction::JmpIfNot(l, c) => write!(f, "jmpifn ${} @{}", c, l),
            Instruction::LoadConst(_, _) => todo!(),
            Instruction::LoadVar(_, _) => todo!(),
            Instruction::StoreVar(_, _) => todo!(),
            Instruction::Index(_, _, _) => todo!(),
            Instruction::Lbl => write!(f, "label:"),
            Instruction::Nop => todo!(),
        }
    }
}

#[derive(Debug, Clone)]
pub struct Param {
    pub var: Var,
    pub type_: Type,
}

#[derive(Debug, Clone)]
pub struct Function {
    pub name: String,
    pub location: FnLbl,
    pub varcount: u16,
    pub instructions: Vec<Instruction>,
    pub has_native_impl: bool,
}

impl Function {
    pub fn new(name: String, varcount: u16, instructions: Vec<Instruction>) -> Function {
        Function {
            name,
            location: FnLbl::MAX,
            varcount,
            instructions,
            has_native_impl: false,
        }
    }

    pub fn from_long_instr(long_fn: ParsedFunction, fns: &HashMap<String, usize>) -> Function {
        let mut jump_table: HashMap<String, usize> = HashMap::new();
        let mut pc: usize = 0;
        for instr in long_fn.instructions.iter() {
            match instr {
                ParsedInstruction::Lbl(name) => {
                    jump_table.insert(name.clone(), pc);
                }
                _ => {}
            }

            pc += 1;
        }

        let mut compact_instructions = Vec::new();
        pc = 0;
        for instr in long_fn.instructions.iter() {
            match instr {
                ParsedInstruction::IndirectJmp(n) => {
                    compact_instructions.push(Instruction::Jmp(
                        (jump_table[n] as isize - pc as isize).try_into().unwrap(),
                    ));
                }
                ParsedInstruction::IndirectJmpIf(n, e) => {
                    compact_instructions.push(Instruction::JmpIf(
                        (jump_table[n] as isize - pc as isize).try_into().unwrap(),
                        e.clone(),
                    ));
                }
                ParsedInstruction::IndirectJmpIfNot(n, e) => {
                    compact_instructions.push(Instruction::JmpIfNot(
                        (jump_table[n] as isize - pc as isize).try_into().unwrap(),
                        e.clone(),
                    ));
                }
                ParsedInstruction::Lbl(_) => {
                    compact_instructions.push(Instruction::Lbl);
                }
                ParsedInstruction::Instr(instr) => compact_instructions.push(instr.clone()),
                ParsedInstruction::IndirectCall(res, n, vars) => compact_instructions.push(
                    Instruction::Call(res.clone(), fns[n].try_into().unwrap(), vars[0].clone()),
                ),
            }
            pc += 1;
        }

        assert_eq!(compact_instructions.len(), long_fn.instructions.len());

        Function::new(long_fn.name, long_fn.varcount, compact_instructions)
    }
}

#[derive(Debug, Clone)]
pub enum ParsedInstruction {
    Instr(Instruction),
    IndirectCall(Var, String, Vec<Var>),
    IndirectJmp(String),
    IndirectJmpIf(String, Var),
    IndirectJmpIfNot(String, Var),
    Lbl(String),
}

#[derive(Debug, Clone)]
pub struct ParsedFunction {
    pub name: String,
    pub varcount: u16,
    pub instructions: Vec<ParsedInstruction>,
}

impl ParsedFunction {
    pub fn new(name: String, varcount: u16, instructions: Vec<ParsedInstruction>) -> Self {
        Self {
            name,
            varcount,
            instructions,
        }
    }
}

pub struct ControlFlowGraph {
    pub blocks: Vec<BasicBlock>,
    pub from_fn: FnLbl,
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
            parents: Vec::new()
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
        // Create basic blocks
        // and edges between basic blocks
        // Greedily eat instructions and push into current basic block
        // When encountering a jump or label:
        //   create new basic block and repeat
        // Link up basic blocks - how?

        let mut basic_blocks = Vec::new();

        let mut pc = 0;
        let mut current_block = BasicBlock::starting_from(pc);

        for instr in fun.instructions.iter() {
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
            blocks.iter().position(|x| x.first == pc).expect("Basic block doesn't exist")
        }

        fn find_basic_block_ending_at(blocks: &Vec<BasicBlock>, pc: i16) -> usize {
            blocks.iter().position(|x| x.last == pc).expect("Basic block doesn't exist")
        }

        for (pc, instr) in fun.instructions.iter().enumerate() {
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
                },
                _ => {}
            }  
        }

        Self {
            blocks: basic_blocks,
            from_fn: fun.location,
        }
    }

    pub fn print_dot(&self, src_fn: &Function) {
        fn print_block(bb: &BasicBlock, fun: &Function) {
            print!("\"");

            for instr in fun.instructions[(bb.first as usize)..=(bb.last as usize)].iter() {
                print!("{}\\n", instr);
            }

            print!("\"");
        }

        assert_eq!(self.from_fn, src_fn.location);

        println!("digraph g {{");
        for (i, bb) in self.blocks.iter().enumerate() {
            print!("\t{} [label=", i);
            print_block(bb, src_fn);
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
