use crate::bytecode::{
     ASTImpl, CallTarget, ConstId, FnId, Function,
    InstrLbl,  Module, TypeId, 
};
use crate::cfg::{ControlFlowGraph, BasicBlock};
use crate::dialect::{Instruction, Var};
use crate::memory::{self, Pointer};
use crate::vm::{VMState, VM};
use dynasmrt::x64::Assembler;
use dynasmrt::{
    dynasm, x64::Rq, AssemblyOffset, DynamicLabel, DynasmApi, DynasmLabelApi, ExecutableBuffer,
};

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
macro_rules! x64_asm_resolve_mem {
    /*
    * Uses $src from a register if it is in a register, otherwise loads it from memory.
    */
    ($ops:ident, $mem:ident ; $op:tt $reg:tt, resolve($src:expr)) => {
        match $mem.lookup($src) {
            VarLoc::Register(l) => x64_asm!($ops ; $op $reg, Rq(l as u8)),
            VarLoc::Stack => x64_asm!($ops ; $op $reg, QWORD [rbp + ($src.0 as i32) * 8]),
        }
    };
    /*
    * Writes to $dst in a register if it is in a register, otherwise writes into its memory location.
    */
    ($ops:ident, $mem:ident ; $op:tt resolve($dst:expr), $reg:tt) => {
        match $mem.lookup($dst) {
            VarLoc::Register(l) => x64_asm!($ops ; $op Rq(l as u8), $reg),
            VarLoc::Stack => x64_asm!($ops ; $op QWORD [rbp + ($dst.0 as i32) * 8], $reg),
        }
    };
    /*
    * Resolves the locations of both $src and $dst, using registers whenever possible.
    * If both are in memory, we need to use an intermediary register so RSI is used.
    * This overwrites whatever was in RSI!
    */
    ($ops:ident, $mem:ident ; $op:tt resolve($dst:expr), resolve($src:expr)) => {
        match ($mem.lookup($dst), $mem.lookup($src)) {
            (VarLoc::Register(reg_dst), VarLoc::Register(reg_src)) => x64_asm!($ops ; $op Rq(reg_dst as u8), Rq(reg_src as u8)),
            (VarLoc::Stack, VarLoc::Register(reg_src)) => x64_asm!($ops ; $op QWORD [rbp + ($dst as i32) * 8], Rq(reg_src as u8)),
            (VarLoc::Register(reg_dst), VarLoc::Stack) => x64_asm!($ops ; $op Rq(reg_dst as u8), QWORD [rbp + ($src as i32) * 8]),
            (VarLoc::Stack, VarLoc::Stack) => x64_asm!($ops
                ; mov rsi, QWORD [rbp + ($src as i32) * 8]
                ; $op QWORD [rbp + ($dst as i32) * 8], rsi),
        }
    };

    /*
    * Resolves the locations of both $src and $dst, using registers whenever possible.
    * This may overwrite whatever was in RSI and RDI, depending on which variables are in memory!
    */
    ($ops:ident, $mem:ident ; $op:tt QWORD [resolve($dst:expr)], resolve($src:expr)) => {
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
    ($ops:ident, $mem:ident ; $op:tt resolve($dst:expr), QWORD [resolve($src:expr)]) => {
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
        let fn_ptr: extern "win64" fn(&VM, &mut VMState, Pointer) -> u64 = mem::transmute(buf.ptr(off));
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

pub struct JITEngine {}

impl Default for JITEngine {
    fn default() -> Self {
        Self {}
    }
}

impl JITEngine {
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

    pub fn compile(state: &mut JITState, module: &Module, func: &Function) {
        let implementation = func
            .ast_impl()
            .expect("Can only JIT function with bytecode implementation");
        // let low_instructions: Vec<LowInstruction> = implementation
        //     .instructions
        //     .iter()
        //     .map(|x| From::from(*x))
        //     .collect();
        let cfg = ControlFlowGraph::from_function_instructions(func);
        let mut fn_state = FunctionJITState::new(func.location);
        let native_fn_loc = fn_state.ops.offset();

        let liveness = cfg.liveness_intervals(func);
        let allocations = VariableLocations::from_liveness_intervals(&liveness);
        fn_state.variable_locations = Some(allocations);


        // RCX contains &VM
        // RDX contains &VMState
        // R8 contains arg

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
                ; mov _vm_state, rdx);

            // Move argument into expected location
            // FIXME: hardcoded that argument = var $0
            let memory = fn_state
                .variable_locations
                .as_ref()
                .expect("Memory actions must be initialized when emitting instructions");
            x64_asm_resolve_mem!(ops, memory; mov resolve(Var(0)), r8);
        }

        if state.config.debug {
            func.print_liveness(&liveness);
            println!("{:?}", fn_state.variable_locations);
        }

        for bb in cfg.blocks.iter() {
            Self::compile_basic_block(&mut fn_state, module, implementation, bb);
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

        let buf = fn_state
            .ops
            .finalize()
            .expect("Failed to finalize x64 asm buffer");

        state.compiled_fns.insert(func.name.clone(), unsafe {
            Box::from(CompiledFn::from(buf, native_fn_loc))
        });
    }

    fn compile_basic_block(
        state: &mut FunctionJITState,
        module: &Module,
        func: &ASTImpl,
        block: &BasicBlock,
    ) {
        let mut pc = block.first;
        for instr in func.instructions[(block.first as usize)..=(block.last as usize)].iter() {
            emit_instr_as_asm(state, module, pc as InstrLbl, instr.as_ref());
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
        let func = vm
            .module_loader
            .get_module_by_id(target.module)
            .expect("missing module impl")
            .get_function_by_id(target.function)
            .expect("function id");

        if func.has_native_impl() {
            unsafe {
                let unsafe_fn_ptr = std::mem::transmute::<_, extern "C" fn(&mut VMState, Pointer) -> u64>(
                    func.native_impl().unwrap().fn_ptr,
                );
                (unsafe_fn_ptr)(state, arg)
            }
        } else if func.has_ast_impl() {
            if let Some(res) = JITEngine::call_if_compiled(vm, state, target, arg) {
                res
            } else {
                vm.interpreter.push_native_caller_frame(
                    vm,
                    &mut state.interpreter_state,
                    target,
                    arg,
                );
                vm.interpreter.finish_function(vm, state);
                let res = *vm
                    .interpreter
                    .pop_native_caller_frame(&mut state.interpreter_state)
                    .as_ui64().expect("invalid return type");
                res
            }
        } else {
            panic!("Unknown implementation type");
        }
    }

    pub extern "win64" fn internal_panic(vm: &VM, state: &mut VMState) {
        panic!("Panic while executing");
    }
}

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

#[derive(Debug, Clone)]
struct Spill {
    location: InstrLbl,
    var: Var,
    reg: Rq,
}

#[derive(Debug, Clone)]
struct Load {
    location: InstrLbl,
    var: Var,
    reg: Rq,
}

#[derive(Clone)]
pub struct VariableLocations {
    locations: HashMap<Var, VarLoc>,
}

impl VariableLocations {
    pub fn lookup(&self, var: Var) -> VarLoc {
        self.locations.get(&var).cloned().unwrap_or(VarLoc::Stack)
    }
}

#[derive(Debug, Clone, PartialEq, Copy)]
pub enum VarLoc {
    Register(Rq),
    Stack,
}

impl Debug for VariableLocations {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str("VariableLocations {\n")?;

        let mut loc_copy = self.locations.iter().collect::<Vec::<_>>();
        loc_copy.sort_by(|a, b| a.0.cmp(b.0));

        for l in loc_copy {
            f.write_fmt(format_args!("  {} -> {:?}\n", l.0, l.1))?;
        }

        f.write_str("}")?;

        Ok(())
    }
}

impl VariableLocations {
    pub fn from_liveness_intervals(intervals: &HashMap<Var, (InstrLbl, InstrLbl)>) -> Self {
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
        }
    }
}

pub struct FunctionJITState {
    pub ops: Assembler,
    pub function_location: FnId,
    pub dynamic_labels: HashMap<i16, DynamicLabel>,
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
}


pub fn store_volatiles_except(
    ops: &mut Assembler,
    memory: &VariableLocations,
    var: Option<Var>,
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

    if stored.len() % 2 == 1 {
        x64_asm!(ops; sub rsp, 8);
    }

    stored
}

pub fn load_volatiles(ops: &mut Assembler, stored: &Vec<Rq>) {
    if stored.len() % 2 == 1 {
        x64_asm!(ops; add rsp, 8);
    }

    for reg in stored.iter().rev() {
        x64_asm!(ops; pop Rq(*reg as u8));
    }
}

fn emit_instr_as_asm<'a>(
    fn_state: &mut FunctionJITState,
    module: &'a Module,
    cur: InstrLbl,
    instr: &(dyn Instruction + 'a),
) {
    instr.emit(fn_state, module, cur);

    // match instr {
    //     ConstU32(r, c) => {
    //         let c: i32 = c as i32;
    //         x64_asm_resolve_mem!(ops, memory; mov resolve(r), c);
    //     },
    //     LtVarVar(r, a, b) => {
    //         x64_asm_resolve_mem!(ops, memory; cmp resolve(a), resolve(b));
    //         match memory.lookup(r) {
    //             VarLoc::Register(reg) => x64_asm!(ops
    //                     ; mov Rq(reg as u8), 0
    //                     ; mov rdi, 1
    //                     ; cmovl Rq(reg as u8), rdi),
    //             _ => x64_asm!(ops
    //                     ; mov rsi, 0
    //                     ; mov rdi, 1
    //                     ; cmovl rsi, rdi
    //                     ; mov [rbp + (r as i32) * 8], rsi),
    //         }
    //     }
    //     AddVarVar(r, a, b) => {
    //         x64_asm_resolve_mem!(ops, memory; mov rsi, resolve(a));
    //         x64_asm_resolve_mem!(ops, memory; add rsi, resolve(b));
    //         x64_asm_resolve_mem!(ops, memory; mov resolve(r), rsi);
    //     }
    //     SubVarVar(r, a, b) => {
    //         x64_asm_resolve_mem!(ops, memory; mov rsi, resolve(a));
    //         x64_asm_resolve_mem!(ops, memory; sub rsi, resolve(b));
    //         x64_asm_resolve_mem!(ops, memory; mov resolve(r), rsi);
    //     }
    //     MulVarVar(r, a, b) => {
    //         x64_asm!(ops; push rax);
    //         x64_asm_resolve_mem!(ops, memory; mov rax, resolve(a));
    //         x64_asm_resolve_mem!(ops, memory; mov rsi, resolve(b));
    //         x64_asm!(ops; imul rsi);
    //         x64_asm_resolve_mem!(ops, memory; mov resolve(r), rax);
    //         x64_asm!(ops; pop rax);
    //     }
    //     EqVarVar(r, a, b) => {
    //         x64_asm_resolve_mem!(ops, memory; cmp resolve(a), resolve(b));
    //         x64_asm!(ops
    //             ; mov rsi, 0
    //             ; mov rdi, 1
    //             ; cmove rsi, rdi);
    //         x64_asm_resolve_mem!(ops, memory; mov resolve(r), rsi);
    //     }
    //     NotVar(r, a) => {
    //         x64_asm_resolve_mem!(ops, memory; cmp resolve(a), 0);
    //         x64_asm!(ops
    //             ; mov rsi, 0
    //             ; mov rdi, 1
    //             ; cmove rsi, rdi);
    //         x64_asm_resolve_mem!(ops, memory; mov resolve(r), rsi);
    //     }
    //     Copy(dest, src) => {
    //         x64_asm_resolve_mem!(ops, memory; mov resolve(dest), resolve(src));
    //     }
    //     StoreVar(dst_ptr, src, ty) => {
    //         let get_heap_address = unsafe {
    //             std::mem::transmute::<_, i64>(
    //                 VMState::heap_mut as unsafe extern "C" fn(&mut VMState) -> &mut Heap,
    //             )
    //         };

    //         let store_address = unsafe {
    //             std::mem::transmute::<_, i64>(
    //                 Heap::store_u64 as unsafe extern "C" fn(&mut Heap, Pointer, u64),
    //             )
    //         };

    //         let size = module.get_type_by_id(ty).unwrap().size() as i32;
    //         assert_eq!(size, 8, "Can only store size 8 for now");

    //         let saved = store_volatiles_except(ops, memory, None);
    //         let stack_space = 0x20;

    //         x64_asm!(ops
    //             // Load first because they might be overriden
    //             ;; x64_asm_resolve_mem!(ops, memory; mov r14, resolve(dst_ptr))
    //             ;; x64_asm_resolve_mem!(ops, memory; mov r15, resolve(src))

    //             ; mov rcx, _vm_state
    //             ; sub rsp, BYTE stack_space
    //             ; mov r10, QWORD get_heap_address
    //             ; call r10
    //             ; add rsp, BYTE stack_space

    //             ; mov rcx, rax
    //             ; mov rdx, r14
    //             ; mov r8, r15
    //             ; sub rsp, BYTE stack_space
    //             ; mov r10, QWORD store_address
    //             ; call r10
    //             ; add rsp, BYTE stack_space);

    //         load_volatiles(ops, &saved);
    //     }
    //     LoadVar(dst, src_ptr, ty) => {
    //         let get_heap_address = unsafe {
    //             std::mem::transmute::<_, i64>(
    //                 VMState::heap_mut as unsafe extern "C" fn(&mut VMState) -> &mut Heap,
    //             )
    //         };

    //         let load_address = unsafe {
    //             std::mem::transmute::<_, i64>(
    //                 Heap::load_u64 as unsafe extern "C" fn(&mut Heap, Pointer) -> u64,
    //             )
    //         };

    //         let size = module.get_type_by_id(ty).unwrap().size() as i32;
    //         assert_eq!(size, 8, "Can only store size 8 for now");

    //         let saved = store_volatiles_except(ops, memory, Some(dst));
    //         let stack_space = 0x20;

    //         x64_asm!(ops
    //             // Load first because they might be overriden
    //             ;; x64_asm_resolve_mem!(ops, memory; mov r14, resolve(src_ptr))

    //             ; mov rcx, _vm_state
    //             ; sub rsp, BYTE stack_space
    //             ; mov r10, QWORD get_heap_address
    //             ; call r10
    //             ; add rsp, BYTE stack_space

    //             ; mov rcx, rax
    //             ; mov rdx, r14
    //             ; sub rsp, BYTE stack_space
    //             ; mov r10, QWORD load_address
    //             ; call r10
    //             ; add rsp, BYTE stack_space
    //             ;; x64_asm_resolve_mem!(ops, memory; mov resolve(dst), rax)
    //         );

    //         load_volatiles(ops, &saved);
    //     }
    //     Alloc(r, t) => {
    //         let saved = store_volatiles_except(ops, memory, Some(r));
    //         let get_heap_address = unsafe {
    //             std::mem::transmute::<_, i64>(
    //                 VMState::heap_mut as unsafe extern "C" fn(&mut VMState) -> &mut Heap,
    //             )
    //         };

    //         let alloc_address = unsafe {
    //             std::mem::transmute::<_, i64>(
    //                 Heap::allocate_type as unsafe extern "C" fn(&mut Heap, &Type) -> Pointer,
    //             )
    //         };

    //         let ty = module.get_type_by_id(t).expect("type idx");

    //         let stack_space = 0x20;
    //         x64_asm!(ops
    //                 ; mov rcx, _vm_state
    //                 ; sub rsp, BYTE stack_space
    //                 ; mov r10, QWORD get_heap_address
    //                 ; call r10
    //                 ; add rsp, BYTE stack_space

    //                 ; mov rcx, rax
    //                 ; mov rdx, QWORD ty as *const _ as i64
    //                 ; sub rsp, BYTE stack_space
    //                 ; mov r10, QWORD alloc_address
    //                 ; call r10
    //                 ; add rsp, BYTE stack_space

    //                 ;;x64_asm_resolve_mem!(ops, memory; mov resolve(r), rax));

    //         load_volatiles(ops, &saved);
    //     }
    //     ModuleCall(res, target, arg) => {
    //         let fn_ref: i64 = unsafe {
    //             std::mem::transmute::<_, i64>(
    //                 JITEngine::trampoline
    //                     as extern "win64" fn(&VM, &mut VMState, CallTarget, u64) -> u64,
    //             )
    //         };

    //         let func = target.function;
    //         let module = target.module;

    //         let saved = store_volatiles_except(ops, memory, Some(res));
    //         let stack_space = 0x20;
    //         x64_asm!(ops
    //                 ;;x64_asm_resolve_mem!(ops, memory; mov r9, resolve(arg))
    //                 ; mov rcx, _vm
    //                 ; mov rdx, _vm_state
    //                 ; mov r8w, func as i16
    //                 ; shl r8, 16
    //                 ; mov r8w, module as i16
    //                 ; sub rsp, BYTE stack_space
    //                 ; mov r10, QWORD fn_ref
    //                 ; call r10
    //                 ; add rsp, BYTE stack_space
    //                 ;;x64_asm_resolve_mem!(ops, memory; mov resolve(res), rax));
    //         load_volatiles(ops, &saved);
    //     }
    //     Lbl => {
    //         let dyn_lbl = fn_state
    //             .dynamic_labels
    //             .entry(cur)
    //             .or_insert(ops.new_dynamic_label());
    //         x64_asm!(ops
    //             ; =>*dyn_lbl);
    //     }
    //     Jmp(l) => {
    //         let jmp_target = cur as i64 + l as i64;
    //         let dyn_lbl = fn_state
    //             .dynamic_labels
    //             .entry(jmp_target as InstrLbl)
    //             .or_insert(ops.new_dynamic_label());

    //         x64_asm!(ops
    //             ; jmp =>*dyn_lbl);
    //     }
    //     JmpIfNot(l, c) => {
    //         let jmp_target = cur as i64 + l as i64;
    //         let dyn_lbl = fn_state
    //             .dynamic_labels
    //             .entry(jmp_target as InstrLbl)
    //             .or_insert(ops.new_dynamic_label());

    //         x64_asm!(ops
    //             ;;x64_asm_resolve_mem!(ops, memory ; cmp resolve(c), 0)
    //             ; je =>*dyn_lbl);
    //     }
    //     JmpIf(l, c) => {
    //         let jmp_target = cur as i64 + l as i64;
    //         let dyn_lbl = fn_state
    //             .dynamic_labels
    //             .entry(jmp_target as InstrLbl)
    //             .or_insert(ops.new_dynamic_label());

    //         x64_asm!(ops
    //             ;; x64_asm_resolve_mem!(ops, memory ; cmp resolve(c), 1)
    //             ; je =>*dyn_lbl);
    //     }
    //     Return(r) => {
    //         x64_asm!(ops
    //             ;; x64_asm_resolve_mem!(ops, memory; mov rax, resolve(r))
    //             ; jmp ->_cleanup)
    //     }
    //     LoadConst(res, idx) => {
    //         let const_decl = module.get_constant_by_id(idx).unwrap();

    //         let get_heap_address = unsafe {
    //             std::mem::transmute::<_, i64>(
    //                 VMState::heap_mut as unsafe extern "C" fn(&mut VMState) -> &mut Heap,
    //             )
    //         };

    //         let write_to_heap_address = unsafe {
    //             std::mem::transmute::<_, i64>(
    //                 Module::write_constant_to_heap
    //                     as unsafe extern "C" fn(&mut Heap, &Type, &Constant) -> Pointer,
    //             )
    //         };

    //         let saved = store_volatiles_except(ops, memory, Some(res));
    //         let stack_space = 0x20;
    //         x64_asm!(ops
    //             ; mov rcx, _vm_state
    //             ; sub rsp, BYTE stack_space
    //             ; mov r10, QWORD get_heap_address
    //             ; call r10
    //             ; add rsp, BYTE stack_space
    //             ; mov rcx, rax
    //             // TODO investigate, this wont work if the constdecl is moved, dangling ptr
    //             ; mov rdx, QWORD &const_decl.type_ as *const _ as i64
    //             ; mov r8, QWORD &const_decl.value as *const _ as i64
    //             ; sub rsp, BYTE stack_space
    //             ; mov r10, QWORD write_to_heap_address
    //             ; call r10
    //             ; add rsp, BYTE stack_space
    //             ;; x64_asm_resolve_mem!(ops, memory; mov resolve(res), rax)
    //         );

    //         load_volatiles(ops, &saved);
    //     }
    //     Index(res, src, idx, ty) => {
    //         let get_heap_address = unsafe {
    //             std::mem::transmute::<_, i64>(
    //                 VMState::heap_mut as unsafe extern "C" fn(&mut VMState) -> &mut Heap,
    //             )
    //         };

    //         let index_bytes_address = unsafe {
    //             std::mem::transmute::<_, i64>(
    //                 Heap::index_bytes as unsafe extern "C" fn(&mut Heap, Pointer, usize) -> Pointer,
    //             )
    //         };

    //         let index_bytes: i32 = match module.get_type_by_id(ty).expect("type idx") {
    //             Type::U64 | Type::Pointer(_) => 8,
    //             ty => panic!("Illegal index operand: {:?}", ty),
    //         };

    //         let load_8_bytes_address = unsafe {
    //             std::mem::transmute::<_, i64>(
    //                 Heap::load_u64 as unsafe extern "C" fn(&mut Heap, Pointer) -> u64,
    //             )
    //         };

    //         // call get_heap_address to get heap ptr
    //         // load idx, multiply with element size in bytes
    //         // load src
    //         // call heap.index_bytes on src value with idx
    //         // write res

    //         let saved = store_volatiles_except(ops, memory, Some(res));
    //         let stack_space = 0x20;
    //         x64_asm!(ops
    //             // Load first because they might be overriden
    //             ;; x64_asm_resolve_mem!(ops, memory; mov r14, resolve(src))
    //             ;; x64_asm_resolve_mem!(ops, memory; mov r15, resolve(idx))

    //             ; mov rcx, _vm_state
    //             ; sub rsp, BYTE stack_space
    //             ; mov r10, QWORD get_heap_address
    //             ; call r10
    //             ; add rsp, BYTE stack_space
    //             // rax contains pointer to heap, mov into rcx (first arg)
    //             ; mov rcx, rax
    //             ; mov rdi, rax // store in non-vol register for later
    //             // compute idx and put into r8
    //             ; mov rax, r15
    //             ; mov r8, index_bytes
    //             ; mul r8
    //             ; mov r8, rax
    //             // Offset for array header
    //             ; add r8, 8
    //             // put src into rdx
    //             ; mov rdx, r14
    //             ; sub rsp, BYTE stack_space
    //             ; mov r10, QWORD index_bytes_address
    //             ; call r10
    //             ; add rsp, BYTE stack_space
    //             // rax contains proxy
    //             // load value
    //             ; mov rcx, rdi
    //             ; mov rdx, rax
    //             ; sub rsp, BYTE stack_space
    //             ; mov r10, QWORD load_8_bytes_address
    //             ; call r10
    //             ; add rsp, BYTE stack_space
    //             ;; x64_asm_resolve_mem!(ops, memory; mov resolve(res), rax)
    //         );

    //         load_volatiles(ops, &saved);
    //     }
    //     Sizeof(res, src) => {
    //         let get_heap_address = unsafe {
    //             std::mem::transmute::<_, i64>(
    //                 VMState::heap_mut as unsafe extern "C" fn(&mut VMState) -> &mut Heap,
    //             )
    //         };

    //         let load_8_bytes_address = unsafe {
    //             std::mem::transmute::<_, i64>(
    //                 Heap::load_u64 as unsafe extern "C" fn(&mut Heap, Pointer) -> u64,
    //             )
    //         };

    //         let saved = store_volatiles_except(ops, memory, Some(res));


    //         let stack_space = 0x20;
    //         x64_asm!(ops
    //             // Load first because they might be overriden
    //             ;; x64_asm_resolve_mem!(ops, memory; mov r14, resolve(src))

    //             ; mov rcx, _vm_state
    //             ; sub rsp, BYTE stack_space
    //             ; mov r10, QWORD get_heap_address
    //             ; call r10
    //             ; add rsp, BYTE stack_space
    //             // rax contains pointer to heap, mov into rcx (first arg)
    //             ; mov rcx, rax
    //             // load value
    //             ; mov rdx, r14
    //             ; sub rsp, BYTE stack_space
    //             ; mov r10, QWORD load_8_bytes_address
    //             ; call r10
    //             ; add rsp, BYTE stack_space
    //             ;; x64_asm_resolve_mem!(ops, memory; mov resolve(res), rax)
    //         );

    //         load_volatiles(ops, &saved);
    //     }
    //     Panic => {
    //         let panic_address: i64 = unsafe {
    //             std::mem::transmute::<_, i64>(
    //                 JITEngine::internal_panic as extern "win64" fn(&VM, &mut VMState),
    //             )
    //         };

    //         x64_asm!(ops
    //             ; mov rcx, _vm
    //             ; mov rdx, _vm_state
    //             ; mov r10, QWORD panic_address
    //             ; call r10);
    //     }
    //     instr => panic!("Unknown instr {:?}", instr),
    // }
}

// fn emit_low_instr_as_asm<'a>(
//     fn_state: &mut FunctionJITState,
//     module: &Module,
//     cur: InstrLbl,
//     instr: LowInstruction,
// ) {
//     // Ops cannot be referenced in x64_asm macros if it's inside fn_state
//     let ops = &mut fn_state.ops;
//     let get_heap_address = unsafe {
//         std::mem::transmute::<_, i64>(
//             VMState::heap_mut as unsafe extern "C" fn(&mut VMState) -> &mut Heap,
//         )
//     };

//     let store_address = unsafe {
//         std::mem::transmute::<_, i64>(
//             Heap::store_u64 as unsafe extern "C" fn(&mut Heap, Pointer, u64),
//         )
//     };

//     let load_address = unsafe {
//         std::mem::transmute::<_, i64>(
//             Heap::load_u64 as unsafe extern "C" fn(&mut Heap, Pointer) -> u64,
//         )
//     };

//     let load_const_to_heap_address = unsafe {
//         std::mem::transmute::<_, i64>(
//             Module::write_constant_to_heap
//                 as unsafe extern "C" fn(&mut Heap, &Type, &Constant) -> Pointer,
//         )
//     };

//     let alloc_address = unsafe {
//         std::mem::transmute::<_, i64>(
//             Heap::allocate_type as unsafe extern "C" fn(&mut Heap, &Type) -> Pointer,
//         )
//     };

//     let trampoline_address: i64 = unsafe {
//         std::mem::transmute::<_, i64>(
//             JITEngine::trampoline as extern "win64" fn(&VM, &mut VMState, CallTarget, u64) -> u64,
//         )
//     };

//     let index_bytes_address = unsafe {
//         std::mem::transmute::<_, i64>(
//             Heap::index_bytes as unsafe extern "C" fn(&mut Heap, Pointer, usize) -> Pointer,
//         )
//     };

//     use LowInstruction::*;
//     use Operand::*;
//     match instr {
//         ConstU32(Reg(res), val) => {
//             x64_asm!(ops; mov Rq(res), val as i32);
//         }
//         LtVarVar(Reg(res), Reg(lhs), Reg(rhs)) => {
//             x64_asm!(ops
//                 ; cmp Rq(lhs), Rq(rhs)
//                 ; mov Rq(res), 0
//                 ; mov rdi, 1
//                 ; cmovl Rq(res), rdi);
//         }
//         AddVarVar(Reg(a), Reg(b), Reg(c)) => {
//             x64_asm!(ops
//                 ; xor Rq(a), Rq(a)
//                 ; mov Rq(a), Rq(b)
//                 ; add Rq(a), Rq(c));
//         }
//         SubVarVar(Reg(a), Reg(b), Reg(c)) => {
//             x64_asm!(ops
//                 ; xor Rq(a), Rq(a)
//                 ; mov Rq(a), Rq(b)
//                 ; sub Rq(a), Rq(c));
//         }
//         EqVarVar(Reg(res), Reg(lhs), Reg(rhs)) => {
//             x64_asm!(ops
//                 ; cmp Rq(lhs), Rq(rhs)
//                 ; mov Rq(res), 0
//                 ; mov rdi, 1
//                 ; cmove Rq(res), rdi);
//         }
//         NotVar(Reg(res), Reg(val)) => {
//             x64_asm!(ops
//                 ; cmp Rq(val), 0
//                 ; mov Rq(res), 0
//                 ; mov rdi, 1
//                 ; cmove Rq(res), rdi)
//         }
//         Copy(Reg(res), Reg(src)) => {
//             x64_asm!(ops; mov Rq(res), Rq(src));
//         }
//         StoreVar(Reg(res_ptr), Reg(src), ty) => {
//             let size = module.get_type_by_id(ty).unwrap().size() as i32;
//             assert_eq!(size, 8, "Can only store size 8 for now");

//             let stack_space = 0x20;

//             x64_asm!(ops
//                 ; mov rcx, _vm_state
//                 ; sub rsp, BYTE stack_space
//                 ; mov r10, QWORD get_heap_address
//                 ; call r10
//                 ; add rsp, BYTE stack_space

//                 ; mov rcx, rax
//                 ; mov rdx, Rq(res_ptr)
//                 ; mov r8, Rq(src)
//                 ; sub rsp, BYTE stack_space
//                 ; mov r10, QWORD store_address
//                 ; call r10
//                 ; add rsp, BYTE stack_space);
//         }
//         LoadVar(Reg(dst), Reg(src_ptr), ty) => {
//             let size = module.get_type_by_id(ty).unwrap().size() as i32;
//             assert_eq!(size, 8, "Can only store size 8 for now");
//             let stack_space = 0x20;

//             x64_asm!(ops
//                 ; mov rcx, _vm_state
//                 ; sub rsp, BYTE stack_space
//                 ; mov r10, QWORD get_heap_address
//                 ; call r10
//                 ; add rsp, BYTE stack_space

//                 ; mov rcx, rax
//                 ; mov rdx, Rq(src_ptr)
//                 ; sub rsp, BYTE stack_space
//                 ; mov r10, QWORD load_address
//                 ; call r10
//                 ; add rsp, BYTE stack_space

//                 ; mov Rq(dst), rax);
//         }
//         Alloc(Reg(res), t) => {
//             let ty = module.get_type_by_id(t).expect("type idx");
//             let stack_space = 0x20;

//             x64_asm!(ops
//                     ; mov rcx, _vm_state
//                     ; sub rsp, BYTE stack_space
//                     ; mov r10, QWORD get_heap_address
//                     ; call r10
//                     ; add rsp, BYTE stack_space

//                     ; mov rcx, rax
//                     ; mov rdx, QWORD ty as *const _ as i64
//                     ; sub rsp, BYTE stack_space
//                     ; mov r10, QWORD alloc_address
//                     ; call r10
//                     ; add rsp, BYTE stack_space

//                     ; mov Rq(res), rax);
//         }
//         Call1(Reg(res), func, Reg(arg)) => {
//             let module = module.id;

//             // Recursive call
//             // We know we can call the native impl. directly
//             if func == fn_state.function_location {
//                 let stack_space = 0x20;
//                 x64_asm!(ops
//                     ; mov rcx, _vm
//                     ; mov rdx, _vm_state
//                     ; mov r8, Rq(arg)
//                     ; sub rsp, BYTE stack_space
//                     ; call ->_self
//                     ; add rsp, BYTE stack_space
//                     ; mov Rq(res), rax);
//             }
//             // Unknown target
//             // Conservatively call trampoline
//             else {
//                 let stack_space = 0x20;
//                 x64_asm!(ops
//                     ; mov rcx, _vm
//                     ; mov rdx, _vm_state
//                     ; mov r8w, func as i16
//                     ; shl r8, 16
//                     ; mov r8w, module as i16
//                     ; mov r9, Rq(arg)
//                     ; sub rsp, BYTE stack_space
//                     ; mov r10, QWORD trampoline_address
//                     ; call r10
//                     ; add rsp, BYTE stack_space
//                     ; mov Rq(res), rax);
//             }
//         }
//         ModuleCall(Reg(res), target, Reg(arg)) => {
//             let func = target.function;
//             let module = target.module;

//             let stack_space = 0x20;
//             x64_asm!(ops
//                     ; mov rcx, _vm
//                     ; mov rdx, _vm_state
//                     ; mov r8w, func as i16
//                     ; shl r8, 16
//                     ; mov r8w, module as i16
//                     ; mov r9, Rq(arg)
//                     ; sub rsp, BYTE stack_space
//                     ; mov r10, QWORD trampoline_address
//                     ; call r10
//                     ; add rsp, BYTE stack_space
//                     ; mov Rq(res), rax);
//         }
//         Lbl => {
//             let dyn_lbl = fn_state
//                 .dynamic_labels
//                 .entry(cur)
//                 .or_insert(ops.new_dynamic_label());

//             x64_asm!(ops; =>*dyn_lbl);
//         }
//         Jmp(l) => {
//             let jmp_target = cur as i64 + l as i64;
//             let dyn_lbl = fn_state
//                 .dynamic_labels
//                 .entry(jmp_target as InstrLbl)
//                 .or_insert(ops.new_dynamic_label());

//             x64_asm!(ops; jmp =>*dyn_lbl);
//         }
//         JmpIfNot(l, Reg(c)) => {
//             let jmp_target = cur as i64 + l as i64;
//             let dyn_lbl = fn_state
//                 .dynamic_labels
//                 .entry(jmp_target as InstrLbl)
//                 .or_insert(ops.new_dynamic_label());

//             x64_asm!(ops
//                 ; cmp Rq(c), 0
//                 ; je =>*dyn_lbl);
//         }
//         JmpIf(l, Reg(c)) => {
//             let jmp_target = cur as i64 + l as i64;
//             let dyn_lbl = fn_state
//                 .dynamic_labels
//                 .entry(jmp_target as InstrLbl)
//                 .or_insert(ops.new_dynamic_label());

//             x64_asm!(ops
//                 ; cmp Rq(c), 0
//                 ; jne =>*dyn_lbl);
//         }
//         Return(Reg(r)) => {
//             x64_asm!(ops
//                 ; mov rax, Rq(r)
//                 ; jmp ->_cleanup)
//         }
//         LoadConst(Reg(res), idx) => {
//             let const_decl = module.get_constant_by_id(idx).unwrap();

//             let stack_space = 0x20;
//             x64_asm!(ops
//                 ; mov rcx, _vm_state
//                 ; sub rsp, BYTE stack_space
//                 ; mov r10, QWORD get_heap_address
//                 ; call r10
//                 ; add rsp, BYTE stack_space
//                 ; mov rcx, rax
//                 // TODO investigate, this wont work if the constdecl is moved, dangling ptr
//                 ; mov rdx, QWORD &const_decl.type_ as *const _ as i64
//                 ; mov r8, QWORD &const_decl.value as *const _ as i64
//                 ; sub rsp, BYTE stack_space
//                 ; mov r10, QWORD load_const_to_heap_address
//                 ; add rsp, BYTE stack_space
//                 ; mov Rq(res), rax);
//         }
//         Index(Reg(res), Reg(src), Reg(idx), ty) => {
//             let index_bytes: i32 = match module.get_type_by_id(ty).expect("type idx") {
//                 Type::U64 | Type::Pointer(_) => 8,
//                 ty => panic!("Illegal index operand: {:?}", ty),
//             };
//             // call get_heap_address to get heap ptr
//             // load idx, multiply with element size in bytes
//             // load src
//             // call heap.index_bytes on src value with idx
//             // write res

//             let stack_space = 0x20;
//             x64_asm!(ops
//                 // Load first because they might be overriden
//                 ; mov r14, Rq(src)
//                 ; mov r15, Rq(idx)

//                 ; mov rcx, _vm_state
//                 ; sub rsp, BYTE stack_space
//                 ; mov r10, QWORD get_heap_address
//                 ; call r10
//                 ; add rsp, BYTE stack_space
//                 // rax contains pointer to heap, mov into rcx (first arg)
//                 ; mov rcx, rax
//                 ; mov rdi, rax // store in non-vol register for later
//                 // compute idx and put into r8
//                 ; mov rax, r15
//                 ; mov r8, index_bytes
//                 ; imul r8
//                 ; mov r8, rax
//                 // put src into rdx
//                 ; mov r14, rdx
//                 ; sub rsp, BYTE stack_space
//                 ; mov r10, QWORD index_bytes_address
//                 ; call r10
//                 ; add rsp, BYTE stack_space
//                 // rax contains proxy
//                 // load value
//                 ; mov rcx, rdi
//                 ; mov rdx, rax
//                 ; sub rsp, BYTE stack_space
//                 ; mov r10, QWORD load_address
//                 ; call r10
//                 ; add rsp, BYTE stack_space

//                 ; mov Rq(res), rax
//             );
//         }
//         instr => panic!("Unknown instr {:?}", instr),
//     }
// }
