use crate::bytecode::{ControlFlowGraph as BytecodeCFG, Function, InstrLbl, Instruction, VarId};
use dataflow::{
    solve, Analysis, AnalysisDirection, ControlFlowGraph, Lattice, MaySet, TransferFunction,
};
use std::collections::{HashMap, HashSet};

#[derive(Default, Debug)]
struct CFG {
    pub nodes: Vec<InstrLbl>,
    pub successors: HashMap<InstrLbl, Vec<InstrLbl>>,
    pub predecessors: HashMap<InstrLbl, Vec<InstrLbl>>,
}

impl CFG {
    pub fn add_node(&mut self, a: InstrLbl) {
        self.nodes.push(a);
        self.successors.entry(a).or_default();
        self.predecessors.entry(a).or_default();
    }

    pub fn make_edge(&mut self, a: InstrLbl, b: InstrLbl) {
        self.successors.entry(a).or_insert(vec![]).push(b);
        self.predecessors.entry(b).or_insert(vec![]).push(a);
    }
}

impl<'a> ControlFlowGraph<'a> for CFG {
    type Node = InstrLbl;
    type NodeIterator = std::slice::Iter<'a, InstrLbl>;

    fn nodes(&'a self) -> Self::NodeIterator {
        self.nodes.iter()
    }

    fn successors(&'a self, n: &'a Self::Node) -> Self::NodeIterator {
        self.successors.get(n).unwrap().iter()
    }

    fn predecessors(&'a self, n: &'a Self::Node) -> Self::NodeIterator {
        self.predecessors.get(n).unwrap().iter()
    }
}

impl From<&BytecodeCFG> for CFG {
    fn from(g: &BytecodeCFG) -> Self {
        let mut out_graph = CFG::default();
        let basic_blocks = &g.blocks;

        for block in basic_blocks {
            for instr_idx in block.first..block.last {
                let new_node = instr_idx;
                out_graph.add_node(new_node);
                out_graph.make_edge(new_node, instr_idx + 1);
            }

            out_graph.add_node(block.last);

            for child in &block.children {
                out_graph.make_edge(block.last, g.blocks[*child].first);
            }
        }

        out_graph
    }
}

#[derive(Default)]
struct LivenessAnalysis {
    transfer: HashMap<InstrLbl, Box<dyn TransferFunction<InstrLbl, MaySet<VarId>>>>,
}

impl LivenessAnalysis {
    pub fn new() -> Self {
        Self {
            transfer: HashMap::new(),
        }
    }
}

impl Analysis<MaySet<VarId>> for LivenessAnalysis {
    type Node = InstrLbl;

    fn direction(&self) -> AnalysisDirection {
        AnalysisDirection::Backward
    }

    fn bottom(&self) -> MaySet<VarId> {
        MaySet {
            values: HashSet::new(),
        }
    }

    fn top(&self) -> MaySet<VarId> {
        unimplemented!("No top value for MaySet")
    }

    fn transfer(&self, tn: &InstrLbl, lat: &MaySet<VarId>) -> MaySet<VarId> {
        (self.transfer.get(tn).unwrap()).eval(tn, lat)
    }
}

struct GenericTransferFunction {
    instruction: Instruction,
}
impl TransferFunction<InstrLbl, MaySet<VarId>> for GenericTransferFunction {
    fn eval(&self, _: &InstrLbl, l: &MaySet<VarId>) -> MaySet<VarId> {
        let mut n = (*l).clone();
        let reads_from = self.instruction.read_variables();
        for var in reads_from {
            n.data_mut().insert(var);
        }

        let writes_to = self.instruction.write_variables();
        for var in writes_to {
            n.data_mut().remove(&var);
        }
        n
    }
}

pub fn run(cfg: &BytecodeCFG, function: &Function) -> HashMap<InstrLbl, MaySet<VarId>> {
    let my_cfg = CFG::from(cfg);
    let bytecode = &function
        .bytecode_impl()
        .expect("Can only run dataflow on bytecode")
        .instructions;
    let mut liveness = LivenessAnalysis::default();

    for node in my_cfg.nodes() {
        liveness.transfer.insert(
            *node,
            Box::from(GenericTransferFunction {
                instruction: bytecode[*node as usize],
            }),
        );
    }

    let liveness_results = solve(&my_cfg, &mut liveness);

    liveness_results.into_iter().map(|(k, v)| (*k, v)).collect()
}
