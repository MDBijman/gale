use std::collections::HashMap;

use dataflow::Lattice;

use crate::{
    bytecode::{FnId, Function, InstrLbl},
    dialect::{Instruction, Var},
    dataflow::run
};

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
    pub fn from_function_instructions(fun: &Function) -> Self {
        let implementation = fun.ast_impl().expect("bytecode impl");

        // Create basic blocks
        // and edges between basic blocks
        // Greedily eat instructions and push into current basic block
        // When encountering a jump or label:
        //   create new basic block and repeat

        let mut basic_blocks = Vec::new();

        let mut pc = 0;
        let mut current_block = BasicBlock::starting_from(pc);

        for instr in implementation.instructions.iter() {
            panic!();
            // match instr {
            //     Instruction::Return(_) => {
            //         basic_blocks.push(current_block);
            //         current_block = BasicBlock::starting_from(pc + 1);
            //     }
            //     Instruction::Jmp(_) => {
            //         basic_blocks.push(current_block);
            //         current_block = BasicBlock::starting_from(pc + 1);
            //     }
            //     Instruction::JmpIf(_, _) => {
            //         basic_blocks.push(current_block);
            //         current_block = BasicBlock::starting_from(pc + 1);
            //     }
            //     Instruction::JmpIfNot(_, _) => {
            //         basic_blocks.push(current_block);
            //         current_block = BasicBlock::starting_from(pc + 1);
            //     }
            //     Instruction::Panic => {
            //         basic_blocks.push(current_block);
            //         current_block = BasicBlock::starting_from(pc + 1);
            //     }
            //     Instruction::Lbl => {
            //         // If this is not the first in the block, we need to make a new block
            //         if current_block.first != pc {
            //             // Remove this label from this current_block
            //             current_block.last -= 1;

            //             // Create a new block starting from this lbl
            //             // do not replace current_block yet, need to update its children
            //             let mut new_block = BasicBlock::starting_from(pc);

            //             // We were not the first in the block, so previous instruction
            //             // was not a jump, so this is child of last block
            //             // current_block will be at basic_blocks.len, new block adds 1
            //             current_block.add_child(basic_blocks.len() + 1);
            //             new_block.add_parent(basic_blocks.len());

            //             // Commit the current_block
            //             basic_blocks.push(current_block);

            //             // Replace the current with the new
            //             current_block = new_block;

            //             // Extend one
            //             current_block.extend_one();
            //         } else {
            //             current_block.extend_one();
            //         }
            //     }
            //     _ => {
            //         current_block.extend_one();
            //     }
            // }

            pc += 1;
        }

        for (pc, instr) in implementation.instructions.iter().enumerate() {
            panic!();
            // match instr {
            //     Instruction::Jmp(l) => {
            //         let target = l + pc as i16;
            //         let child =
            //             ControlFlowGraph::find_basic_block_starting_at(&basic_blocks, target);
            //         let parent =
            //             ControlFlowGraph::find_basic_block_ending_at(&basic_blocks, pc as i16);
            //         basic_blocks[parent].add_child(child);
            //         basic_blocks[child].add_parent(parent);
            //     }
            //     Instruction::JmpIf(l, _) => {
            //         let target = l + pc as i16;
            //         let child =
            //             ControlFlowGraph::find_basic_block_starting_at(&basic_blocks, target);
            //         let child_no_jump = ControlFlowGraph::find_basic_block_starting_at(
            //             &basic_blocks,
            //             pc as i16 + 1,
            //         );
            //         let parent =
            //             ControlFlowGraph::find_basic_block_ending_at(&basic_blocks, pc as i16);
            //         basic_blocks[parent].add_child(child);
            //         basic_blocks[parent].add_child(child_no_jump);
            //         basic_blocks[child].add_parent(parent);
            //         basic_blocks[child_no_jump].add_parent(parent);
            //     }
            //     Instruction::JmpIfNot(l, _) => {
            //         let target = l + pc as i16;
            //         let child =
            //             ControlFlowGraph::find_basic_block_starting_at(&basic_blocks, target);
            //         let child_no_jump = ControlFlowGraph::find_basic_block_starting_at(
            //             &basic_blocks,
            //             pc as i16 + 1,
            //         );
            //         let parent =
            //             ControlFlowGraph::find_basic_block_ending_at(&basic_blocks, pc as i16);
            //         basic_blocks[parent].add_child(child);
            //         basic_blocks[parent].add_child(child_no_jump);
            //         basic_blocks[child].add_parent(parent);
            //         basic_blocks[child_no_jump].add_parent(parent);
            //     }
            //     _ => {}
            // }
        }

        Self {
            blocks: basic_blocks,
            from_fn: fun.location,
        }
    }

    pub fn from_function_low_instructions(fun: &Function) -> Self {
        let implementation = fun.ast_impl().expect("bytecode impl");

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
            panic!();
            // match instr {
            //     Instruction::Return(_) => {
            //         basic_blocks.push(current_block);
            //         current_block = BasicBlock::starting_from(pc + 1);
            //     }
            //     Instruction::Jmp(_) => {
            //         basic_blocks.push(current_block);
            //         current_block = BasicBlock::starting_from(pc + 1);
            //     }
            //     Instruction::JmpIf(_, _) => {
            //         basic_blocks.push(current_block);
            //         current_block = BasicBlock::starting_from(pc + 1);
            //     }
            //     Instruction::JmpIfNot(_, _) => {
            //         basic_blocks.push(current_block);
            //         current_block = BasicBlock::starting_from(pc + 1);
            //     }
            //     Instruction::Panic => {
            //         basic_blocks.push(current_block);
            //         current_block = BasicBlock::starting_from(pc + 1);
            //     }
            //     Instruction::Lbl => {
            //         // If this is not the first in the block, we need to make a new block
            //         if current_block.first != pc {
            //             // Remove this label from this current_block
            //             current_block.last -= 1;

            //             // Create a new block starting from this lbl
            //             // do not replace current_block yet, need to update its children
            //             let mut new_block = BasicBlock::starting_from(pc);

            //             // We were not the first in the block, so previous instruction
            //             // was not a jump, so this is child of last block
            //             // current_block will be at basic_blocks.len, new block adds 1
            //             current_block.add_child(basic_blocks.len() + 1);
            //             new_block.add_parent(basic_blocks.len());

            //             // Commit the current_block
            //             basic_blocks.push(current_block);

            //             // Replace the current with the new
            //             current_block = new_block;

            //             // Extend one
            //             current_block.extend_one();
            //         } else {
            //             current_block.extend_one();
            //         }
            //     }
            //     _ => {
            //         current_block.extend_one();
            //     }
            // }

            pc += 1;
        }

        for (pc, instr) in implementation.instructions.iter().enumerate() {
            panic!();
            // match instr {
            //     Instruction::Jmp(l) => {
            //         let target = l + pc as i16;
            //         let child =
            //             ControlFlowGraph::find_basic_block_starting_at(&basic_blocks, target);
            //         let parent =
            //             ControlFlowGraph::find_basic_block_ending_at(&basic_blocks, pc as i16);
            //         basic_blocks[parent].add_child(child);
            //         basic_blocks[child].add_parent(parent);
            //     }
            //     Instruction::JmpIf(l, _) => {
            //         let target = l + pc as i16;
            //         let child =
            //             ControlFlowGraph::find_basic_block_starting_at(&basic_blocks, target);
            //         let child_no_jump = ControlFlowGraph::find_basic_block_starting_at(
            //             &basic_blocks,
            //             pc as i16 + 1,
            //         );
            //         let parent =
            //             ControlFlowGraph::find_basic_block_ending_at(&basic_blocks, pc as i16);
            //         basic_blocks[parent].add_child(child);
            //         basic_blocks[parent].add_child(child_no_jump);
            //         basic_blocks[child].add_parent(parent);
            //         basic_blocks[child_no_jump].add_parent(parent);
            //     }
            //     Instruction::JmpIfNot(l, _) => {
            //         let target = l + pc as i16;
            //         let child =
            //             ControlFlowGraph::find_basic_block_starting_at(&basic_blocks, target);
            //         let child_no_jump = ControlFlowGraph::find_basic_block_starting_at(
            //             &basic_blocks,
            //             pc as i16 + 1,
            //         );
            //         let parent =
            //             ControlFlowGraph::find_basic_block_ending_at(&basic_blocks, pc as i16);
            //         basic_blocks[parent].add_child(child);
            //         basic_blocks[parent].add_child(child_no_jump);
            //         basic_blocks[child].add_parent(parent);
            //         basic_blocks[child_no_jump].add_parent(parent);
            //     }
            //     _ => {}
            // }
        }

        Self {
            blocks: basic_blocks,
            from_fn: fun.location,
        }
    }

    /// For each variable, computes the first and last instruction in which it is used (write or read).
    pub fn liveness_intervals(&self, func: &Function) -> HashMap<Var, (InstrLbl, InstrLbl)> {
        let mut intervals: HashMap<Var, (InstrLbl, InstrLbl)> = HashMap::new();

        let liveness_analysis_results = run(self, func);

        for (instr, live_at_instr) in liveness_analysis_results {
            for var in live_at_instr.data() {
                // We add one here (and in the first if) since liveness analysis gives results
                // that indicate if a variables is live _after_ executing a particular instruction
                // JIT expects the interval to end at the last instruction to use a variable, not 1 before it
                let v = intervals.entry(*var).or_insert((instr, instr));
                if instr > v.1 {
                    v.1 = instr;
                };
                if instr < v.0 {
                    v.0 = instr;
                };
            }
        }

        intervals
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
