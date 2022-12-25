use std::{collections::HashMap};

use dataflow::Lattice;

use crate::{
    bytecode::{FnId, Function, InstrLbl},
    dataflow::run,
    dialect::{Instruction, Name, Var}, jit::Interval,
};

pub enum ControlFlowBehaviour {
    Linear,
    // Jumps with list of jump targets
    Jump(Vec<Name>),
    ConditionalJump(Vec<Name>),
    Target,
}

pub trait InstrControlFlow {
    fn behaviour(&self) -> ControlFlowBehaviour;
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

    pub fn shrink_one(&mut self) {
        self.last -= 1;
    }

    pub fn add_child(&mut self, idx: usize) {
        self.children.push(idx);
    }

    pub fn add_parent(&mut self, idx: usize) {
        self.parents.push(idx);
    }
}

impl ControlFlowGraph {
    pub fn from_function_instructions(fun: &Function) -> Self {
        let implementation = fun.ast_impl().expect("hl impl");

        // Create basic blocks
        // and edges between basic blocks
        // Greedily eat instructions and push into current basic block
        // When encountering a jump or label:
        //   create new basic block and repeat

        let mut basic_blocks = Vec::new();

        let mut pc = 0;
        let mut current_block = BasicBlock::starting_from(pc);

        for instr in implementation.instructions.iter() {
            match instr.behaviour() {
                ControlFlowBehaviour::Linear => {
                    current_block.extend_one();
                }
                ControlFlowBehaviour::Jump(_) | ControlFlowBehaviour::ConditionalJump(_) => {
                    basic_blocks.push(current_block);
                    current_block = BasicBlock::starting_from(pc + 1);
                }
                ControlFlowBehaviour::Target => {
                    let first_in_block = current_block.first == pc;

                    if first_in_block {
                        current_block.extend_one();
                    } else {
                        // Exclude this instruction from current block
                        current_block.shrink_one();
                        // Current BB is successor to last one, because otherwise
                        // this instruction would be first_in_block
                        let mut new_block = BasicBlock::starting_from(pc);
                        current_block.add_child(basic_blocks.len() + 1);
                        new_block.add_parent(basic_blocks.len());

                        basic_blocks.push(current_block);
                        current_block = new_block;

                        current_block.extend_one();
                    }
                }
            }

            pc += 1;
        }

        // Resolve jumps
        for (pc, instr) in implementation.instructions.iter().enumerate() {
            match instr.behaviour() {
                ControlFlowBehaviour::Jump(targets) => {
                    let parent =
                        ControlFlowGraph::find_basic_block_ending_at(&basic_blocks, pc as i16);

                    for t in targets {
                        let target_instr = fun.ast_impl().unwrap().labels.get(&t.0[0]).unwrap();
                        let child =
                            ControlFlowGraph::find_basic_block_starting_at(&basic_blocks, *target_instr as i16);
                        basic_blocks[parent].add_child(child);
                        basic_blocks[child].add_parent(parent);
                    }
                }
                ControlFlowBehaviour::ConditionalJump(targets) => {
                    let parent =
                        ControlFlowGraph::find_basic_block_ending_at(&basic_blocks, pc as i16);

                    for t in targets {
                        let target_instr = fun.ast_impl().unwrap().labels.get(&t.0[0]).unwrap();
                        let child =
                            ControlFlowGraph::find_basic_block_starting_at(&basic_blocks, *target_instr as i16);
                        basic_blocks[parent].add_child(child);
                        basic_blocks[child].add_parent(parent);
                    }
 
                    let fallthrough_child =
                        ControlFlowGraph::find_basic_block_starting_at(&basic_blocks, pc as i16 + 1);
                        basic_blocks[parent].add_child(fallthrough_child);
                        basic_blocks[fallthrough_child].add_parent(parent);
                }
                _ => {}
            }
        }

        Self {
            blocks: basic_blocks,
            from_fn: fun.location,
        }
    }

    /// For each variable, computes the first and last instruction in which it is used (write or read).
    pub fn liveness_intervals(&self, func: &Function) -> HashMap<Var, Interval> {
        let mut intervals: HashMap<Var, Interval> = HashMap::new();

        let liveness_analysis_results = run(self, func);

        for (instr, live_at_instr) in liveness_analysis_results {
            for var in live_at_instr.data() {
                // We add one here (and in the first if) since liveness analysis gives results
                // that indicate if a variables is live _after_ executing a particular instruction
                // JIT expects the interval to end at the last instruction to use a variable, not 1 before it
                let v = intervals.entry(*var).or_insert(Interval { begin: instr, end: instr });
                if instr > v.end {
                    v.end = instr;
                };
                if instr < v.begin {
                    v.begin = instr;
                };
            }
        }

        intervals
    }

    /*
     * TODO optimize
     */
    fn find_basic_block_starting_at(blocks: &Vec<BasicBlock>, pc: i16) -> usize {
        blocks
            .iter()
            .position(|x| x.first == pc)
            .expect("Basic block doesn't exist")
    }

    /*
     * TODO optimize
     */
    fn find_basic_block_ending_at(blocks: &Vec<BasicBlock>, pc: i16) -> usize {
        blocks
            .iter()
            .position(|x| x.last == pc)
            .expect("Basic block doesn't exist")
    }

    pub fn print_dot(&self, src_fn: &Function) {
        fn print_block(bb: &BasicBlock, instructions: &Vec<Box<dyn Instruction>>) {
            print!("\"");

            for instr in instructions[(bb.first as usize)..=(bb.last as usize)].iter() {
                print!("{}\\n", instr);
            }

            print!("\"");
        }

        let implementation = src_fn.ast_impl().expect("bytecode impl");

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
