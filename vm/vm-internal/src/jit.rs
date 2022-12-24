use crate::bytecode::{ASTImpl, CallTarget, ConstId, FnId, Function, InstrLbl, Module, TypeId, FunctionLocation};
use crate::cfg::{BasicBlock, ControlFlowGraph};
use crate::dialect::{Instruction, Var, InstrToX64};
use crate::memory::{self, Pointer};
use crate::typechecker::{TypeEnvironment, typecheck};
use crate::vm::{VMState, VM};
use dynasmrt::x64::Assembler;
use dynasmrt::DynasmLabelApi;
use dynasmrt::{dynasm, x64::Rq, AssemblyOffset, DynamicLabel, DynasmApi, ExecutableBuffer};

#[macro_export]
macro_rules! x64_asm {
    ($ops:ident $($t:tt)*) => {
        dynasm!($ops
            ; .arch x64
            ; .alias _vm, r12
            ; .alias _vm_state, r13
            $($t)*
        )
    }
}

#[macro_export]
macro_rules! x64_reg {
    (rax) => { Rq::RAX };
    (rcx) => { Rq::RCX };
    (rdx) => { Rq::RDX };
    (rbx) => { Rq::RBX };
    (rsp) => { Rq::RSP };
    (rbp) => { Rq::RBP };
    (rsi) => { Rq::RSI };
    (rdi) => { Rq::RDI };
    (r8) => { Rq::R8 };
    (r9) => { Rq::R9 };
    (r10) => { Rq::R10 };
    (r11) => { Rq::R11 };
    (r12) => { Rq::R12 };
    (r13) => { Rq::R13 };
    (r14) => { Rq::R14 };
    (r15) => { Rq::R15 };
}

#[macro_export]
macro_rules! x64_asm_resolve_mem {
    /*
    * Uses $src from a register if it is in a register, otherwise loads it from memory.
    */
    ($ops:ident, $mem:ident ; $op:tt e($reg:expr), v($src:expr)) => {
        match $mem.lookup($src) {
            VarLoc::Register(l) => 
                    x64_asm!($ops ; $op $reg, Rq(l as u8)),
            VarLoc::Stack => x64_asm!($ops ; $op $reg, QWORD [rbp + ($src.0 as i32) * 8]),
        }
    };
 
    ($ops:ident, $mem:ident ; $op:tt r($reg:tt), v($src:expr)) => {
        match $mem.lookup($src) {
            VarLoc::Register(l) => {
                if l != x64_reg!($reg) {
                    x64_asm!($ops ; $op $reg, Rq(l as u8));
                }
            },
            VarLoc::Stack => x64_asm!($ops ; $op $reg, QWORD [rbp + ($src.0 as i32) * 8]),
        }
    };
    /*
    * Writes to $dst in a register if it is in a register, otherwise writes into its memory location.
    */
   ($ops:ident, $mem:ident ; $op:tt v($dst:expr), e($reg:expr)) => {
        match $mem.lookup($dst) {
            VarLoc::Register(l) => 
                    x64_asm!($ops ; $op Rq(l as u8), $reg),
            VarLoc::Stack => x64_asm!($ops ; $op QWORD [rbp + ($dst.0 as i32) * 8], $reg),
        }
    };
   
    ($ops:ident, $mem:ident ; $op:tt v($dst:expr), r($reg:tt)) => {
        match $mem.lookup($dst) {
            VarLoc::Register(l) => {
                if l != x64_reg!($reg) {
                    x64_asm!($ops ; $op Rq(l as u8), $reg);
                }
            },
            VarLoc::Stack => x64_asm!($ops ; $op QWORD [rbp + ($dst.0 as i32) * 8], $reg),
        }
    };
    /*
    * Resolves the locations of both $src and $dst, using registers whenever possible.
    * If both are in memory, we need to use an intermediary register so RSI is used.
    * This overwrites whatever was in RSI!
    */
    ($ops:ident, $mem:ident ; $op:tt v($dst:expr), v($src:expr)) => {
        match ($mem.lookup($dst), $mem.lookup($src)) {
            (VarLoc::Register(reg_dst), VarLoc::Register(reg_src)) => x64_asm!($ops ; $op Rq(reg_dst as u8), Rq(reg_src as u8)),
            (VarLoc::Stack, VarLoc::Register(reg_src)) => x64_asm!($ops ; $op QWORD [rbp + ($dst.0 as i32) * 8], Rq(reg_src as u8)),
            (VarLoc::Register(reg_dst), VarLoc::Stack) => x64_asm!($ops ; $op Rq(reg_dst as u8), QWORD [rbp + ($src.0 as i32) * 8]),
            (VarLoc::Stack, VarLoc::Stack) => x64_asm!($ops
                ; mov rsi, QWORD [rbp + ($src.0 as i32) * 8]
                ; $op QWORD [rbp + ($dst.0 as i32) * 8], rsi),
        }
    };

    /*
    * Resolves the locations of both $src and $dst, using registers whenever possible.
    * This may overwrite whatever was in RSI and RDI, depending on which variables are in memory!
    */
    ($ops:ident, $mem:ident ; $op:tt QWORD [v($dst:expr)], v($src:expr)) => {
        match ($mem.lookup($dst), $mem.lookup($src)) {
            (VarLoc::Register(reg_dst), VarLoc::Register(reg_src)) => x64_asm!($ops ; $op QWORD [Rq(reg_dst as u8)], Rq(reg_src as u8)),
            (VarLoc::Stack, VarLoc::Register(reg_src)) => x64_asm!($ops
                ; mov rsi, QWORD [rbp + ($dst as i32) * 8]
                ; $op QWORD [rsi], Rq(reg_src as u8)),
            (VarLoc::Register(reg_dst), VarLoc::Stack) => x64_asm!($ops
                ; mov rsi, QWORD [rbp + ($src as i32) * 8]
                ; $op QWORD [Rq(reg_dst as u8)], rsi),
            (VarLoc::Stack, VarLoc::Stack) => x64_asm!($ops
                ; mov rsi, QWORD [rbp + ($src as i32) * 8]
                ; mov rdi, QWORD [rbp + ($dst as i32) * 8]
                ; $op QWORD [rdi], rsi),
        }
    };

    /*
    * Resolves the locations of both $src and $dst, using registers whenever possible.
    * This may overwrite whatever was in RSI and RDI, depending on which variables are in memory!
    */
    ($ops:ident, $mem:ident ; $op:tt v($dst:expr), QWORD [v($src:expr)]) => {
        match ($mem.lookup($dst), $mem.lookup($src)) {
            (Some(reg_dst), Some(reg_src)) => x64_asm!($ops ; $op Rq(reg_dst as u8), QWORD [Rq(reg_src as u8)]),
            (None, Some(reg_src)) => x64_asm!($ops
                ; mov rdi, QWORD [Rq(reg_src as u8)]
                ; $op QWORD [rbp + ($src as i32) * 8], rdi),
            (Some(reg_dst), None) => x64_asm!($ops
                ; mov rdi, QWORD [rbp + ($src as i32) * 8]
                ; $op Rq(reg_dst as u8), QWORD [rdi]),
            (None, None) => x64_asm!($ops
                ; mov rsi, QWORD [rbp + ($src as i32) * 8]
                ; mov rdi, QWORD [rbp + ($dst as i32) * 8]
                ; $op rdi, QWORD [rsi]),
        }
    };
}

use nom::AsBytes;
use std::any::Any;
use std::marker::PhantomPinned;
use std::{collections::HashMap, fmt::Debug, mem};

#[derive(Debug, Clone, Copy)]
pub enum Operand {
    Var(Var),
    Reg(u8),
}

impl From<Var> for Operand {
    fn from(v: Var) -> Self {
        Operand::Var(v)
    }
}

#[derive(Debug, Clone)]
pub enum LowInstruction {
    // Mirrors of Instruction, but with enum operands
    ConstU32(Operand, u32),
    Copy(Operand, Operand),
    EqVarVar(Operand, Operand, Operand),
    LtVarVar(Operand, Operand, Operand),
    SubVarVar(Operand, Operand, Operand),
    AddVarVar(Operand, Operand, Operand),
    MulVarVar(Operand, Operand, Operand),
    NotVar(Operand, Operand),
    Return(Operand),
    Print(Operand),
    Call1(Operand, FnId, Operand),
    CallN(Operand, FnId, Vec<Operand>),
    ModuleCall(Operand, CallTarget, Operand),
    Jmp(InstrLbl),
    JmpIf(InstrLbl, Operand),
    JmpIfNot(InstrLbl, Operand),
    Alloc(Operand, TypeId),
    LoadConst(Operand, ConstId),
    LoadVar(Operand, Operand, TypeId),
    StoreVar(Operand, Operand, TypeId),
    Index(Operand, Operand, Operand, TypeId),
    Sizeof(Operand, Operand),
    Lbl,
    Nop,
    Panic,

    // LowInstruction specific
    Push(Var),
    Pop(u8),
}

pub struct CompiledFn {
    code: ExecutableBuffer,
    fn_ptr: extern "win64" fn(&VM, &mut VMState, Pointer) -> u64,
    _pin: std::marker::PhantomPinned,
}

impl CompiledFn {
    unsafe fn from(buf: ExecutableBuffer, off: AssemblyOffset) -> CompiledFn {
        let fn_ptr: extern "win64" fn(&VM, &mut VMState, Pointer) -> u64 =
            mem::transmute(buf.ptr(off));
        CompiledFn {
            code: buf,
            fn_ptr,
            _pin: PhantomPinned,
        }
    }

    pub fn call(&self, vm: &VM, state: &mut VMState, arg: Pointer) -> u64 {
        (self.fn_ptr)(vm, state, arg)
    }
}

impl Debug for CompiledFn {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("CompiledFn")
            .field("code", &self.code)
            .field("fn_ptr", &String::from("<native>"))
            .finish()
    }
}

#[derive(Default)]
pub struct JITConfig {
    debug: bool,
}

#[derive(Default)]
pub struct JITState {
    compiled_fns: HashMap<String, Box<CompiledFn>>,
    config: JITConfig,
}

impl JITState {
    pub fn new(debug: bool) -> Self {
        JITState {
            compiled_fns: HashMap::default(),
            config: JITConfig { debug: debug },
        }
    }

    pub fn set_config(&mut self, new_config: JITConfig) {
        self.config = new_config;
    }
}

pub fn call_if_compiled(
    vm: &VM,
    state: &mut VMState,
    target: CallTarget,
    arg: memory::Pointer,
) -> Option<u64> {
    /*
     * We access the compiled function and call it.
     * This is unsafe because we pass the VMState as mutable reference to the callee,
     * while we are also immutably borrowing the state by accessing the compiled function.
     * Further down the callstack, the VMState may be mutated such that
     * the pointer to the function we have here becomes invalid.
     * Do not drop or move CompiledFns that are in our VMState!
     * They may be used somewhere in the call stack.
     * I don't see a way to do this without 'unsafe' currently.
     * JIT state must be mutable so that we can compile new functions.
     * Cloning CompiledFns to satisfy the borrowchecker is a no-go.
     */
    let name = &vm
        .module_loader
        .get_module_by_id(target.module)
        .expect("missing module impl")
        .get_function_by_id(target.function)
        .expect("function")
        .name;

    let opt_fn = state.jit_state.compiled_fns.get(name);

    match opt_fn {
        Some(compiled_fn) => {
            let compiled_fn: *const CompiledFn = &**compiled_fn;
            // Currently the parameter types of each JITted function are the same
            // But we need to support n-ary functions
            // From asm to asm we need to generate code with the proper calling conventions
            // But from rust to asm we probably need something like libffi
            unsafe { Some(compiled_fn.as_ref()?.call(vm, state, arg)) }
        }
        None => None,
    }
}

pub fn get_function_bytes<'a>(state: &'a JITState, func: &Function) -> &'a [u8] {
    let comped_fn = state.compiled_fns.get(&func.name).unwrap();
    comped_fn.code.as_bytes()
}

pub fn to_hex_string(state: &mut JITState, func: &Function) -> String {
    let mut out = String::new();
    let a = state.compiled_fns.get(&func.name).unwrap();
    let mut i = 0;
    for x in a.code.iter() {
        out.push_str(format!("{:02x} ", x).as_str());
        if i == 15 {
            out.push('\n');
        }

        i += 1;
        i %= 16;
    }

    out
}

pub fn compile(vm: &mut VM, func: FunctionLocation, state: &mut JITState) {
    let tenv = typecheck(vm, func).unwrap();

    vm.module_loader.lookup_function_mut(func).unwrap().variable_types = Some(tenv);
    let (module, func) = vm.module_loader.lookup(func).unwrap();

    let implementation = func
        .ast_impl()
        .expect("Can only JIT function with bytecode implementation");

    let types = func
        .variable_types()
        .expect("Can only JIT function that has been typechecked");

    let cfg = ControlFlowGraph::from_function_instructions(func);
    let mut fn_state = FunctionJITState::new(func.location);
    let native_fn_loc = fn_state.ops.offset();

    let liveness = cfg.liveness_intervals(func);
    let allocations = VariableLocations::from_liveness_intervals(&liveness, &types);
    fn_state.variable_locations = Some(allocations);

    // RCX contains &VM
    // RDX contains &VMState
    // R8 contains arg ptr

    let mut space_for_variables = implementation.varcount as i32 * 8;

    // Align stack at the same time
    // At the beginning of the function, stack is unaligned (return address + 6 register saves)
    // Check if the the space for variables makes stack aligned, otherwise add 8
    if space_for_variables % 16 == 0 {
        space_for_variables += 8;
    }

    {
        // Prelude, reserve stack space for all variables
        // Create small scope for 'ops' so we don't borrow mutably twice when we emit instructions
        let ops = &mut fn_state.ops;
        x64_asm!(ops
                ; ->_self:
                ; push rbp
                ; push rbx
                ; push rdi
                ; push rsi
                ; push _vm
                ; push _vm_state
                ; push r14
                ; push r15
                ; sub rsp, space_for_variables.into()
                ; mov rbp, rsp
                ; mov _vm, rcx
                ; mov _vm_state, rdx
                ; mov r8, [r8]);

        // Move argument into expected location
        // FIXME: hardcoded that argument = var $0
        let memory = fn_state
            .variable_locations
            .as_ref()
            .expect("Memory actions must be initialized when emitting instructions");
        x64_asm_resolve_mem!(ops, memory; mov v(Var(0)), r(r8));
    }

    if state.config.debug {
        func.print_liveness(&liveness);
        println!("{:?}", fn_state.variable_locations);
    }

    for bb in cfg.blocks.iter() {
        compile_basic_block(vm, module, bb, implementation, &mut fn_state);
    }

    {
        // Postlude - free space
        let ops = &mut fn_state.ops;
        x64_asm!(ops
                ; ->_cleanup:
                ; add rsp, space_for_variables.into()
                ; pop r15
                ; pop r14
                ; pop _vm_state
                ; pop _vm
                ; pop rsi
                ; pop rdi
                ; pop rbx
                ; pop rbp 
                ; ret);
    }

    fn_state.ops.commit().unwrap();
    let buf = fn_state
        .ops
        .finalize()
        .expect("Failed to finalize x64 asm buffer");

    state.compiled_fns.insert(func.name.clone(), unsafe {
        Box::from(CompiledFn::from(buf, native_fn_loc))
    });
}

fn compile_basic_block(
    vm: &VM,
    module: &Module,
    block: &BasicBlock,
    func: &ASTImpl,
    state: &mut FunctionJITState,
) {
    let mut pc = block.first;
    for instr in func.instructions[(block.first as usize)..=(block.last as usize)].iter() {
        vm.module_loader.get_dialect("std").unwrap().compile(vm, module, state, &**instr, pc as InstrLbl);
        pc += 1;
    }
}

// Jump from native to bytecode or native
pub extern "win64" fn trampoline(
    vm: &VM,
    state: &mut VMState,
    target: CallTarget,
    arg: Pointer,
) -> u64 {
    // println!("trampoline {:?} {:?}", target, arg);
    let func = vm
        .module_loader
        .get_module_by_id(target.module)
        .expect("missing module impl")
        .get_function_by_id(target.function)
        .expect("function id");

    // println!("{}",func.name);
    if func.has_intrinsic_impl() {
        unsafe {
            let unsafe_fn_ptr = std::mem::transmute::<_, extern "C" fn(&mut VMState, Pointer) -> u64>(
                func.intrinsic_impl().unwrap().fn_ptr,
            );
            (unsafe_fn_ptr)(state, arg)
        }
    } else if func.has_ast_impl() {
        if let Some(res) = call_if_compiled(vm, state, target, arg) {
            res
        } else {
            vm.interpreter
                .push_native_caller_frame(vm, &mut state.interpreter_state, target, arg);
            vm.interpreter.finish_function(vm, state);
            let res = *vm
                .interpreter
                .pop_native_caller_frame(&mut state.interpreter_state)
                .as_ui64()
                .expect("invalid return type");
            res
        }
    } else {
        panic!("Unknown implementation type");
    }
}

/*
* The calling convention allows us to abstract code generation over the calling conventions used.
*/
trait CallingConvention {

}

struct Win64CallingConvention {}

impl CallingConvention for Win64CallingConvention {

}

struct InterpreterCallingConvention {}

impl CallingConvention for InterpreterCallingConvention {

}

// Register allocation business below

#[derive(Clone, Copy, Debug)]
struct Interval {
    begin: InstrLbl,
    end: InstrLbl,
}

#[derive(Clone, Copy, Debug)]
struct LivenessInterval {
    var: Var,
    interval: Interval,
}

#[derive(Clone)]
pub struct VariableLocations {
    locations: HashMap<Var, VarLoc>,
    liveness_intervals: HashMap<Var, (InstrLbl, InstrLbl)>,
}

impl VariableLocations {
    pub fn lookup(&self, var: Var) -> VarLoc {
        self.locations.get(&var).cloned().unwrap_or(VarLoc::Stack)
    }

    // TODO fix complexity of this
    pub fn is_register_used_at(&self, register: Rq, location: InstrLbl) -> Option<Var> {
        for (var, interval) in self.liveness_intervals.iter() {
            if location < interval.0 || location > interval.1 {
                continue;
            }

            match self.locations.get(var) {
                Some(VarLoc::Register(r)) => if *r == register {
                    return Some(*var);
                },
                _ => continue,
            }
        }

        None
    }
}

#[derive(Debug, Clone, PartialEq, Copy)]
pub enum VarLoc {
    Register(Rq),
    Stack,
}

impl VarLoc {
    pub fn as_register(&self) -> Option<&Rq> {
        if let Self::Register(v) = self {
            Some(v)
        } else {
            None
        }
    }
}

impl Debug for VariableLocations {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str("VariableLocations {\n")?;

        let mut loc_copy = self.locations.iter().collect::<Vec<_>>();
        loc_copy.sort_by(|a, b| a.0.cmp(b.0));

        for l in loc_copy {
            f.write_fmt(format_args!("  {} -> {:?}\n", l.0, l.1))?;
        }

        f.write_str("}")?;

        Ok(())
    }
}

impl VariableLocations {
    pub fn from_liveness_intervals(
        intervals: &HashMap<Var, (InstrLbl, InstrLbl)>,
        types: &TypeEnvironment,
    ) -> Self {
        let mut as_vec: Vec<LivenessInterval> = intervals
            .iter()
            .map(|(v, (a, b))| LivenessInterval {
                var: *v,
                interval: Interval {
                    begin: *a,
                    end: *b + 1,
                },
            })
            .collect();

        as_vec.sort_by(|a, b| a.interval.begin.cmp(&b.interval.begin));

        let mut active: [(Rq, Option<LivenessInterval>); 7] = [
            (Rq::RCX, None),
            (Rq::RDX, None),
            (Rq::R8, None),
            (Rq::R9, None),
            (Rq::R10, None),
            (Rq::R11, None),
            (Rq::RAX, None),
        ];

        let mut variable_locations = HashMap::<Var, VarLoc>::new();

        // as_vec contains the liveness intervals in ascending start point
        for live_interval in as_vec.iter() {
            // Retire all variables with intervals that have ended
            active
                .iter_mut()
                .filter(|x| {
                    x.1.is_some() && x.1.unwrap().interval.end < live_interval.interval.begin
                })
                .for_each(|x| {
                    // Confirm that variable should be kept in register
                    // since it was not evicted
                    variable_locations.insert(x.1.unwrap().var, VarLoc::Register(x.0));
                    // Empty the register
                    x.1 = None
                });

            // Consider loading the variable into a reg
            match active.iter_mut().find(|x| x.1.is_none()) {
                // There is an empty register
                Some(r) => {
                    r.1 = Some(*live_interval);
                }
                // There is no empty register
                None => {
                    // Find an interval that ends later than the current one
                    match active.iter_mut().find(|(_, x)| {
                        x.is_some() && x.unwrap().interval.end > live_interval.interval.end
                    }) {
                        Some(r) => {
                            // Spilled variable must be kept in memory
                            variable_locations.insert(r.1.unwrap().var, VarLoc::Stack);
                            // Replace register slot
                            r.1 = Some(*live_interval);
                        }
                        None => {
                            // Do nothing, variable remains in memory
                            variable_locations.insert(live_interval.var, VarLoc::Stack);
                        }
                    }
                }
            }
        }

        // Everything still in active can be kept in registers
        for a in active {
            if a.1.is_none() {
                continue;
            }

            variable_locations.insert(a.1.unwrap().var, VarLoc::Register(a.0));
        }

        VariableLocations {
            locations: variable_locations,
            liveness_intervals: intervals.clone()
        }
    }
}

pub struct FunctionJITState {
    /*
     * Contains the assembler operations.
     */
    pub ops: Assembler,

    /*
     * Id of the current function in the surrounding module.
     */
    pub function_location: FnId,

    /*
     * Todo
     */
    pub dynamic_labels: HashMap<i16, DynamicLabel>,

    /*
     * Todo
     */
    pub variable_locations: Option<VariableLocations>,
}

impl FunctionJITState {
    pub fn new(function_location: FnId) -> Self {
        Self {
            ops: Assembler::new().unwrap(),
            function_location,
            dynamic_labels: HashMap::new(),
            variable_locations: None,
        }
    }

    pub fn lookup(&self, v: Var) -> Option<VarLoc> {
        match &self.variable_locations {
            Some(locs) => Some(locs.lookup(v)),
            None => None,
        }
    }
}

pub fn store_registers(
    ops: &mut Assembler,
    memory: &VariableLocations,
    registers: &Vec<Rq>,
    except: &Vec<Var>,
    location: InstrLbl,
    align: bool
) -> Vec<Rq> {
    let mut stored = Vec::new();

    let mut do_register = |reg: Rq| {
        if let Some(v) = memory.is_register_used_at(reg, location) {
            if except.contains(&v) {
                return;
            }
        } else {
            return;
        }

        x64_asm!(ops; push Rq(reg as u8));
        stored.push(reg);
    };

    for r in registers {
        do_register(*r);
    }

    if align && stored.len() % 2 == 1 {
        x64_asm!(ops; sub rsp, 8);
    }

    stored
}


pub fn store_volatiles_except(
    ops: &mut Assembler,
    memory: &VariableLocations,
    var: Option<Var>,
    align: bool
) -> Vec<Rq> {
    let mut stored = Vec::new();

    let mut do_register = |reg: Rq| {
        if var.is_some() && memory.lookup(var.unwrap()) == VarLoc::Register(reg) {
            return;
        }

        x64_asm!(ops; push Rq(reg as u8));
        stored.push(reg);
    };

    do_register(Rq::RAX);
    do_register(Rq::RCX);
    do_register(Rq::RDX);
    do_register(Rq::R8);
    do_register(Rq::R9);
    do_register(Rq::R10);
    do_register(Rq::R11);

    if align && stored.len() % 2 == 1 {
        x64_asm!(ops; sub rsp, 8);
    }

    stored
}

pub fn load_registers(ops: &mut Assembler, stored: &Vec<Rq>, aligned: bool) {
    if aligned &&  stored.len() % 2 == 1 {
        x64_asm!(ops; add rsp, 8);
    }

    for reg in stored.iter().rev() {
        x64_asm!(ops; pop Rq(*reg as u8));
    }
}
