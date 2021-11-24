use std::collections::{HashMap, HashSet, VecDeque};
use std::hash::Hash;

// Abstract lattice definition

pub trait Lattice {
    type Data;

    fn data(&self) -> &Self::Data;
    fn data_mut(&mut self) -> &mut Self::Data;

    fn sink(&mut self);
    fn float(&mut self);
    fn lub(&mut self, other: &dyn Lattice<Data = Self::Data>);
    fn leq(&self, other: &dyn Lattice<Data = Self::Data>) -> bool;
}

// Abstract analysis definition

#[derive(PartialEq, Eq, PartialOrd, Ord, Debug)]
pub enum AnalysisDirection {
    Forward,
    Backward,
}

pub trait Analysis<L>
where
    L: Lattice,
{
    type Node;

    fn direction(&self) -> AnalysisDirection;

    fn bottom(&self) -> L;
    fn top(&self) -> L;

    fn transfer(&self, tn: &Self::Node, lat: &L) -> L;
}

// Concrete lattice definition

#[derive(Debug, Clone)]
pub struct MaySet<D: Hash + Eq + Clone> {
    pub values: HashSet<D>,
}

impl<D: Hash + Eq + Clone> Lattice for MaySet<D> {
    type Data = HashSet<D>;

    fn lub(&mut self, other: &dyn Lattice<Data = Self::Data>) {
        self.data_mut().extend(other.data().iter().cloned());
    }

    fn leq(&self, other: &dyn Lattice<Data = Self::Data>) -> bool {
        self.data().is_subset(other.data())
    }

    fn data(&self) -> &HashSet<D> {
        return &self.values;
    }

    fn data_mut(&mut self) -> &mut HashSet<D> {
        return &mut self.values;
    }

    fn sink(&mut self) {
        self.values = HashSet::new();
    }

    fn float(&mut self) {
        unimplemented!("No Top value for MaySet");
    }
}

pub trait ControlFlowGraph<'a>
where
    <Self as ControlFlowGraph<'a>>::Node: 'a,
{
    type Node;
    type NodeIterator: Iterator<Item = &'a Self::Node>;

    fn nodes(&'a self) -> Self::NodeIterator;
    fn successors(&'a self, n: &'a Self::Node) -> Self::NodeIterator;
    fn predecessors(&'a self, n: &'a Self::Node) -> Self::NodeIterator;
}

pub fn solve<'a, T: Lattice, N: Eq + Hash, I: std::iter::Iterator>(
    graph: &'a dyn ControlFlowGraph<'a, Node = N, NodeIterator = I>,
    analysis: &dyn Analysis<T, Node = N>,
) -> HashMap<&'a N, T>
where
    I: Iterator<Item = &'a N>,
    N: std::fmt::Debug,
    T: std::fmt::Debug,
{
    assert_eq!(analysis.direction(), AnalysisDirection::Backward);
    let mut worklist: VecDeque<&N> = graph.nodes().collect();
    let mut values: HashMap<&N, T> = HashMap::new();

    while !worklist.is_empty() {
        let node = worklist.pop_front().unwrap();
        let predecessors = graph.predecessors(&node);

        // do eval function
        let n_lat = values.remove(&node).unwrap_or_else(|| analysis.bottom());

        let e_lat = analysis.transfer(&node, &n_lat);

        for p in predecessors {
            let p_lat = values.entry(&p).or_insert(analysis.bottom());
            if !e_lat.leq(p_lat) {
                p_lat.lub(&e_lat);
                worklist.push_back(p);
            }
        }

        values.insert(&node, n_lat);
    }

    values
}

pub trait TransferFunction<N, L: Lattice> {
    fn eval(&self, n: &N, l: &L) -> L;
}

// Concrete analysis definition

#[derive(Default)]
struct TransferFunctionId {}
impl TransferFunction<TestNode, MaySet<String>> for TransferFunctionId {
    fn eval(&self, _: &TestNode, l: &MaySet<String>) -> MaySet<String> {
        (*l).clone()
    }
}

#[derive(Default)]
struct TransferFunctionWrites {}
impl TransferFunction<TestNode, MaySet<String>> for TransferFunctionWrites {
    fn eval(&self, _: &TestNode, l: &MaySet<String>) -> MaySet<String> {
        let mut n = (*l).clone();
        n.data_mut().remove(&String::from("a"));
        n
    }
}

#[derive(Default)]
struct TransferFunctionReads {}
impl TransferFunction<TestNode, MaySet<String>> for TransferFunctionReads {
    fn eval(&self, _: &TestNode, l: &MaySet<String>) -> MaySet<String> {
        let mut n = (*l).clone();
        n.data_mut().insert(String::from("a"));
        n
    }
}

struct LivenessAnalysis {
    transfer: HashMap<TestNode, Box<dyn TransferFunction<TestNode, MaySet<String>>>>,
}

impl LivenessAnalysis {
    pub fn new() -> Self {
        Self {
            transfer: HashMap::new(),
        }
    }
}

impl Analysis<MaySet<String>> for LivenessAnalysis {
    type Node = TestNode;

    fn direction(&self) -> AnalysisDirection {
        AnalysisDirection::Backward
    }

    fn bottom(&self) -> MaySet<String> {
        MaySet {
            values: HashSet::new(),
        }
    }

    fn top(&self) -> MaySet<String> {
        unimplemented!("No top value for MaySet")
    }

    fn transfer(&self, tn: &TestNode, lat: &MaySet<String>) -> MaySet<String> {
        (self.transfer.get(tn).unwrap()).eval(tn, lat)
    }
}

#[derive(PartialEq, Eq, PartialOrd, Ord, Hash, Clone, Copy, Debug)]
struct TestNode {
    pub id: u64,
}

impl TestNode {
    pub fn new(id: u64) -> Self {
        Self { id: id }
    }
}

struct TestGraph {
    pub nodes: Vec<TestNode>,
    pub successors: HashMap<TestNode, Vec<TestNode>>,
    pub predecessors: HashMap<TestNode, Vec<TestNode>>,
}

impl TestGraph {
    pub fn new() -> Self {
        Self {
            nodes: vec![],
            successors: HashMap::new(),
            predecessors: HashMap::new(),
        }
    }

    pub fn add_node(&mut self, a: TestNode) {
        self.nodes.push(a);
        self.successors.insert(a, vec![]);
        self.predecessors.insert(a, vec![]);
    }

    pub fn make_edge(&mut self, a: TestNode, b: TestNode) {
        self.successors.entry(a).or_insert(vec![]).push(b);
        self.predecessors.entry(b).or_insert(vec![]).push(a);
    }
}

impl<'a> ControlFlowGraph<'a> for TestGraph {
    type Node = TestNode;
    type NodeIterator = std::slice::Iter<'a, TestNode>;

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

#[test]
fn test_basic_block() {
    let mut tg = TestGraph::new();

    let r = TestNode::new(0);
    let a = TestNode::new(1);
    let b = TestNode::new(2);
    let c = TestNode::new(3);
    let d = TestNode::new(4);

    tg.add_node(r);
    tg.add_node(a);
    tg.add_node(b);
    tg.add_node(c);
    tg.add_node(d);

    tg.make_edge(r, a);
    tg.make_edge(a, b);
    tg.make_edge(b, c);
    tg.make_edge(c, d);

    let mut la: LivenessAnalysis = LivenessAnalysis::new();

    la.transfer
        .insert(r, Box::from(TransferFunctionId::default()));
    la.transfer
        .insert(a, Box::from(TransferFunctionWrites::default()));
    la.transfer
        .insert(b, Box::from(TransferFunctionId::default()));
    la.transfer
        .insert(c, Box::from(TransferFunctionReads::default()));
    la.transfer
        .insert(d, Box::from(TransferFunctionId::default()));

    let values = solve(&tg, &mut la);

    assert!(!values.get(&r).unwrap().data().contains(&String::from("a")));
    assert!(values.get(&a).unwrap().data().contains(&String::from("a")));
    assert!(values.get(&b).unwrap().data().contains(&String::from("a")));
    assert!(!values.get(&c).unwrap().data().contains(&String::from("a")));
    assert!(!values.get(&d).unwrap().data().contains(&String::from("a")));
}

#[test]
fn test_with_loop() {
    let mut tg = TestGraph::new();

    let r = TestNode::new(0);
    let a = TestNode::new(1);
    let b = TestNode::new(2);
    let c = TestNode::new(3);
    let d = TestNode::new(4);
    let e = TestNode::new(5);

    tg.add_node(r);
    tg.add_node(a);
    tg.add_node(b);
    tg.add_node(c);
    tg.add_node(d);
    tg.add_node(e);

    tg.make_edge(r, a);
    tg.make_edge(a, b);
    tg.make_edge(b, c);
    tg.make_edge(c, d);
    tg.make_edge(d, b);
    tg.make_edge(d, e);

    let mut la: LivenessAnalysis = LivenessAnalysis::new();

    la.transfer
        .insert(r, Box::from(TransferFunctionId::default()));
    la.transfer
        .insert(a, Box::from(TransferFunctionWrites::default()));
    la.transfer
        .insert(b, Box::from(TransferFunctionId::default()));
    la.transfer
        .insert(c, Box::from(TransferFunctionId::default()));
    la.transfer
        .insert(d, Box::from(TransferFunctionReads::default()));
    la.transfer
        .insert(e, Box::from(TransferFunctionId::default()));

    let values = solve(&tg, &mut la);

    assert!(!values.get(&r).unwrap().data().contains(&String::from("a")));
    assert!(values.get(&a).unwrap().data().contains(&String::from("a")));
    assert!(values.get(&b).unwrap().data().contains(&String::from("a")));
    assert!(values.get(&c).unwrap().data().contains(&String::from("a")));
    assert!(values.get(&d).unwrap().data().contains(&String::from("a")));
    assert!(!values.get(&e).unwrap().data().contains(&String::from("a")));
}
