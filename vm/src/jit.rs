use crate::bytecode::{BasicBlock, ControlFlowGraph, FnLbl, Function, InstrLbl, Instruction, Var, Module};
use crate::vm::{VMState, VM};
use dynasmrt::x64::Assembler;
use dynasmrt::{
    dynasm, x64::Rq, AssemblyOffset, DynamicLabel, DynasmApi, DynasmLabelApi, ExecutableBuffer,
};
use nom::AsBytes;
use std::marker::PhantomPinned;
use std::{collections::HashMap, fmt::Debug, mem};


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

pub struct CompiledFn {
    code: ExecutableBuffer,
    fn_ptr: extern "win64" fn(&VM, &mut VMState, u64) -> u64,
    _pin: std::marker::PhantomPinned,
}

impl CompiledFn {
    unsafe fn from(buf: ExecutableBuffer, off: AssemblyOffset) -> CompiledFn {
        let fn_ptr: extern "win64" fn(&VM, &mut VMState, u64) -> u64 = mem::transmute(buf.ptr(off));
        CompiledFn {
            code: buf,
            fn_ptr,
            _pin: PhantomPinned,
        }
    }

    pub fn call(&self, vm: &VM, state: &mut VMState, arg: u64) -> u64 {
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
    pub fn call_if_compiled(vm: &VM, state: &mut VMState, name: &str, arg: u64) -> Option<u64> {
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
        let opt_fn = state.jit_state.compiled_fns.get(&String::from(name));
        match opt_fn {
            Some(compiled_fn) => {
                let compiled_fn: *const CompiledFn = &**compiled_fn;
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
        let cfg = ControlFlowGraph::from_function(func);
        let mut fn_state = FunctionJITState::new(func.location);
        let native_fn_loc = fn_state.ops.offset();

        // RCX contains &VM
        // RDX contains &VMState
        // R8 contains arg

        let mut space_for_variables = func.varcount * 8;

        // Align stack at the same time
        // At the beginning of the function, stack is aligned (return address + 5 register saves)
        // Check if the the space for variables keeps stack aligned, otherwise add 8
        if space_for_variables % 16 == 8 {
            space_for_variables += 8;
        }

        {
            // Prelude, reserve stack space for all variables
            // Create small scope for 'ops' so we don't borrow mutably twice when we emit instructions
            let ops = &mut fn_state.ops;
            x64_asm!(ops
                ; ->_self:
                ; push rbx
                ; push rdi
                ; push rsi
                ; push _vm
                ; push _vm_state
                ; sub rsp, space_for_variables.into()
                ; mov QWORD [rsp], r8
                ; mov _vm, rcx
                ; mov _vm_state, rdx);
        }

        let liveness = func.liveness_intervals();
        let allocations = MemoryActions::from_liveness_intervals(&liveness);

        if state.config.debug {
            func.print_liveness(&liveness);
            println!("{:?}", allocations);
        }

        fn_state.memory_actions = Some(allocations);
        fn_state.register_state = Some(RegisterAllocation::new());

        for bb in cfg.blocks.iter() {
            Self::compile_basic_block(&mut fn_state, module, func, bb);
        }

        {
            // Postlude - free space
            let ops = &mut fn_state.ops;
            x64_asm!(ops
                ; ->_cleanup:
                ; add rsp, space_for_variables.into()
                ; pop _vm_state
                ; pop _vm
                ; pop rsi
                ; pop rdi
                ; pop rbx
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

    fn compile_basic_block(state: &mut FunctionJITState, module: &Module, func: &Function, block: &BasicBlock) {
        let mut pc = block.first;
        for &instr in func.instructions[(block.first as usize)..=(block.last as usize)].iter() {
            emit_instr_as_asm(state, module, pc as InstrLbl, instr);
            pc += 1;
        }
    }

    // Jump from native to bytecode or native
    pub extern "win64" fn trampoline(vm: &VM, state: &mut VMState, func: FnLbl, arg: u64) -> u64 {
        let func_name = &vm
            .module
            .get_function_by_id(func)
            .expect("Called unknown function")
            .name;

        if let Some(res) = JITEngine::call_if_compiled(vm, state, func_name.as_str(), arg) {
            res
        } else {
            vm.interpreter
                .push_native_caller_frame(&mut state.interpreter_state, func, arg);
            vm.interpreter.finish_function(&mut state.interpreter_state);
            let res = vm
                .interpreter
                .pop_native_caller_frame(&mut state.interpreter_state);
            res
        }
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
struct MemoryActions {
    spills: Vec<Spill>,
    loads: Vec<Load>,
}

impl Debug for MemoryActions {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str("MemoryActions {")?;

        for spill in self.spills.iter() {
            write!(
                f,
                "spill {2:?} into {1} at {0}",
                spill.location, spill.var, spill.reg
            )?;
        }
        for load in self.loads.iter() {
            write!(
                f,
                "load {1} into {2:?} at {0}",
                load.location, load.var, load.reg
            )?;
        }

        f.write_str("}")?;

        Ok(())
    }
}

impl MemoryActions {
    fn from_liveness_intervals(intervals: &HashMap<Var, (InstrLbl, InstrLbl)>) -> Self {
        let mut as_vec: Vec<LivenessInterval> = intervals
            .iter()
            .map(|(v, (a, b))| LivenessInterval {
                var: *v,
                interval: Interval { begin: *a, end: *b },
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
        let mut allocations = Self {
            spills: Vec::new(),
            loads: Vec::new(),
        };

        // as_vec contains the liveness intervals in ascending start point
        for live_interval in as_vec.iter() {
            // Retire all variables with intervals that have ended
            active
                .iter_mut()
                .filter(|x| {
                    x.1.is_some() && x.1.unwrap().interval.end < live_interval.interval.begin
                })
                .for_each(|x| x.1 = None);

            // Consider loading the variable into a reg
            match active.iter_mut().find(|x| x.1.is_none()) {
                // There is an empty register
                Some(r) => {
                    r.1 = Some(*live_interval);
                    allocations.loads.push(Load {
                        location: live_interval.interval.begin,
                        var: live_interval.var,
                        reg: r.0,
                    });
                }
                // There is no empty register
                None => {
                    // Find an interval that ends later than the current one
                    match active.iter_mut().find(|(_, x)| {
                        x.is_some() && x.unwrap().interval.end > live_interval.interval.end
                    }) {
                        Some(r) => {
                            let var = r.1.unwrap().var;
                            // Spill, load into register
                            r.1 = Some(*live_interval);
                            allocations.spills.push(Spill {
                                location: live_interval.interval.begin,
                                var,
                                reg: r.0,
                            });
                            allocations.loads.push(Load {
                                location: live_interval.interval.begin,
                                var: live_interval.var,
                                reg: r.0,
                            });
                        }
                        None => {
                            // Do nothing, variable remains in memory
                        }
                    }
                }
            }
        }

        allocations
    }

    fn spills_at(&self, instr: InstrLbl) -> Vec<&Spill> {
        self.spills
            .iter()
            .filter(|spill| spill.location == instr)
            .collect()
    }

    fn loads_at(&self, instr: InstrLbl) -> Vec<&Load> {
        self.loads
            .iter()
            .filter(|load| load.location == instr)
            .collect()
    }
}

struct FunctionJITState {
    ops: Assembler,
    function_location: FnLbl,
    dynamic_labels: HashMap<i16, DynamicLabel>,
    memory_actions: Option<MemoryActions>,
    register_state: Option<RegisterAllocation>,
}

impl FunctionJITState {
    pub fn new(function_location: FnLbl) -> Self {
        Self {
            ops: Assembler::new().unwrap(),
            function_location,
            dynamic_labels: HashMap::new(),
            memory_actions: None,
            register_state: None,
        }
    }
}

#[derive(Debug)]
struct RegisterAllocation {
    registers: [(Rq, Option<Var>); 7],
}

impl RegisterAllocation {
    pub fn new() -> Self {
        Self {
            registers: [
                (Rq::RAX, None),
                (Rq::RCX, None),
                (Rq::RDX, None),
                (Rq::R8, None),
                (Rq::R9, None),
                (Rq::R10, None),
                (Rq::R11, None),
            ],
        }
    }

    fn load(&mut self, var: Var, reg: Rq) {
        self.registers.iter_mut().find(|a| a.0 == reg).unwrap().1 = Some(var);
    }

    fn spill(&mut self, reg: Rq) {
        self.registers.iter_mut().find(|a| a.0 == reg).unwrap().1 = None;
    }

    fn lookup(&self, var: Var) -> Option<Rq> {
        self.registers
            .iter()
            .find_map(|a| if a.1 == Some(var) { Some(a.0) } else { None })
    }

    fn lookup_register(&self, reg: Rq) -> Option<Var> {
        let r = self.registers.iter().find(|a| a.0 == reg)?;
        r.1
    }
}

macro_rules! x64_asm_resolve_mem {
    /*
    * Uses $src from a register if it is in a register, otherwise loads it from memory.
    */
    ($ops:ident, $mem:ident ; $op:tt $reg:tt, resolve($src:expr)) => {
        match $mem.lookup($src) {
            Some(l) => x64_asm!($ops ; $op $reg, Rq(l as u8)),
            None => x64_asm!($ops ; $op $reg, QWORD [rsp + ($src as i32) * 8]),
        }
    };
    /*
    * Writes to $dst in a register if it is in a register, otherwise writes into its memory location.
    */
    ($ops:ident, $mem:ident ; $op:tt resolve($dst:expr), $reg:tt) => {
        match $mem.lookup($dst) {
            Some(l) => x64_asm!($ops ; $op Rq(l as u8), $reg),
            None => x64_asm!($ops ; $op QWORD [rsp + ($dst as i32) * 8], $reg),
        }
    };
    /*
    * Resolves the locations of both $src and $dst, using registers whenever possible.
    * If both are in memory, we need to use an intermediary register so RSI is used.
    * This overwrites whatever was in RSI!
    */
    ($ops:ident, $mem:ident ; $op:tt resolve($dst:expr), resolve($src:expr)) => {
        match ($mem.lookup($dst), $mem.lookup($src)) {
            (Some(reg_dst), Some(reg_src)) => x64_asm!($ops ; $op Rq(reg_dst as u8), Rq(reg_src as u8)),
            (None, Some(reg_src)) => x64_asm!($ops ; $op QWORD [rsp + ($dst as i32) * 8], Rq(reg_src as u8)),
            (Some(reg_dst), None) => x64_asm!($ops ; $op Rq(reg_dst as u8), QWORD [rsp + ($dst as i32) * 8]),
            (None, None) => x64_asm!($ops
                ; mov rsi, QWORD [rsp + ($src as i32) * 8]
                ; $op QWORD [rsp + ($dst as i32) * 8], rsi),
        }
    };

    /*
    * Resolves the locations of both $src and $dst, using registers whenever possible.
    * This may overwrite whatever was in RSI and RDI, depending on which variables are in memory!
    */
    ($ops:ident, $mem:ident ; $op:tt QWORD [resolve($dst:expr)], resolve($src:expr)) => {
        match ($mem.lookup($dst), $mem.lookup($src)) {
            (Some(reg_dst), Some(reg_src)) => x64_asm!($ops ; $op QWORD [Rq(reg_dst as u8)], Rq(reg_src as u8)),
            (None, Some(reg_src)) => x64_asm!($ops 
                ; mov rsi, QWORD [rsp + ($dst as i32) * 8] 
                ; $op QWORD [rsi], Rq(reg_src as u8)),
            (Some(reg_dst), None) => x64_asm!($ops
                ; mov rsi, QWORD [rsp + ($src as i32) * 8]
                ; $op QWORD [Rq(reg_dst as u8)], rsi),
            (None, None) => x64_asm!($ops
                ; mov rsi, QWORD [rsp + ($src as i32) * 8]
                ; mov rdi, QWORD [rsp + ($dst as i32) * 8]
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
                ; $op QWORD [rsp + ($src as i32) * 8], rdi),
            (Some(reg_dst), None) => x64_asm!($ops
                ; mov rdi, QWORD [rsp + ($src as i32) * 8]
                ; $op Rq(reg_dst as u8), QWORD [rdi]),
            (None, None) => x64_asm!($ops
                ; mov rsi, QWORD [rsp + ($src as i32) * 8]
                ; mov rdi, QWORD [rsp + ($dst as i32) * 8]
                ; $op rdi, QWORD [rsi]),
        }
    };
}

macro_rules! store_if_loaded {
    ($ops:ident, $mem:ident, $src:expr) => {
        match $mem.lookup_register($src) {
            Some(v) => { x64_asm!($ops ; mov QWORD [rsp + (v as i32) * 8], Rq($src as u8)); Some(v) },
            None => { None },
        }
    };
}

fn store_volatiles_except(
    ops: &mut Assembler,
    memory: &RegisterAllocation,
    var: Var,
) -> Vec<(Rq, Var)> {
    let mut stored = Vec::new();

    let mut do_register = |reg: Rq| {
        if memory.lookup_register(reg) != Some(var) {
            if let Some(v) = store_if_loaded!(ops, memory, reg) {
                stored.push((reg, v));
            }
        }
    };

    do_register(Rq::RAX);
    do_register(Rq::RCX);
    do_register(Rq::RDX);
    do_register(Rq::R8);
    do_register(Rq::R9);
    do_register(Rq::R10);
    do_register(Rq::R11);

    stored
}

fn load_volatiles(ops: &mut Assembler, stored: &Vec<(Rq, Var)>) {
    for pair in stored.iter().rev() {
        x64_asm!(ops ; mov Rq(pair.0 as u8), [rsp + (pair.1 as i32) * 8]);
    }
}

use libc::malloc;

fn emit_instr_as_asm<'a>(fn_state: &mut FunctionJITState, module: &Module, cur: InstrLbl, instr: Instruction) {
    // Ops cannot be referenced in x64_asm macros if it's inside fn_state
    let ops = &mut fn_state.ops;
    let memory = fn_state
        .register_state
        .as_mut()
        .expect("Register state must be initialized when emitting instructions");
    let mem_actions = fn_state
        .memory_actions
        .as_ref()
        .expect("Memory actions must be initialized when emitting instructions");

    // Put spills into a closure so it can be used by individual instruction emitters
    fn emit_spills(
        ops: &mut Assembler,
        mem_actions: &MemoryActions,
        memory: &mut RegisterAllocation,
        cur: InstrLbl,
    ) {
        for spill in mem_actions.spills_at(cur).iter() {
            memory.spill(spill.reg);
            x64_asm!(ops
                ; mov QWORD [rsp + (spill.var as i32) * 8], Rq(spill.reg as u8));
        }
    }

    // We do loads here, before the instruction
    for load in mem_actions.loads_at(cur).iter() {
        memory.load(load.var, load.reg);
        x64_asm!(ops
            ; mov Rq(load.reg as u8), QWORD [rsp + (load.var as i32) * 8]);
    }

    use Instruction::*;
    match instr {
        ConstU32(r, c) => match memory.lookup(r) {
            Some(reg) => x64_asm!(ops
                ; mov Rq(reg as u8), c as i32
                ;; emit_spills(ops, &mem_actions, memory, cur)),
            None => x64_asm!(ops
                ; mov QWORD [rsp + (r as i32) * 8], c as i32
                ;; emit_spills(ops, &mem_actions, memory, cur)),
        },
        LtVarVar(r, a, b) => {
            x64_asm_resolve_mem!(ops, memory; cmp resolve(a), resolve(b));
            match memory.lookup(r) {
                Some(reg_r) => x64_asm!(ops
                        ; mov Rq(reg_r as u8), 0
                        ; mov rdi, 1
                        ; cmovl Rq(reg_r as u8), rdi
                        ;; emit_spills(ops, &mem_actions, memory, cur)),
                None => x64_asm!(ops
                        ; mov rsi, 0
                        ; mov rdi, 1
                        ; cmovl rsi, rdi
                        ; mov [rsp + (r as i32) * 8], rsi
                        ;; emit_spills(ops, &mem_actions, memory, cur)),
            }
        }
        AddVarVar(r, a, b) => {
            x64_asm_resolve_mem!(ops, memory; mov rsi, resolve(a));
            x64_asm_resolve_mem!(ops, memory; add rsi, resolve(b));
            x64_asm_resolve_mem!(ops, memory; mov resolve(r), rsi);
            emit_spills(ops, &mem_actions, memory, cur);
        }
        SubVarVar(r, a, b) => {
            x64_asm_resolve_mem!(ops, memory; mov rsi, resolve(a));
            x64_asm_resolve_mem!(ops, memory; sub rsi, resolve(b));
            x64_asm_resolve_mem!(ops, memory; mov resolve(r), rsi);
            emit_spills(ops, &mem_actions, memory, cur);
        }
        EqVarVar(r, a, b) => {
            x64_asm_resolve_mem!(ops, memory; cmp resolve(a), resolve(b));
            x64_asm!(ops
                ; mov rsi, 0
                ; mov rdi, 1
                ; cmove rsi, rdi);
            x64_asm_resolve_mem!(ops, memory; mov resolve(r), rsi);
            emit_spills(ops, &mem_actions, memory, cur);
        }
        StoreVar(dst_ptr, src, ty) => {
            let size = module.get_type_by_id(ty).unwrap().size() as i32;
            assert_eq!(size, 8, "Can only store size 8 for now");
            x64_asm_resolve_mem!(ops, memory; mov QWORD [resolve(dst_ptr)], resolve(src));
        }
        LoadVar(dst, src_ptr, ty) => {
            let size = module.get_type_by_id(ty).unwrap().size() as i32;
            assert_eq!(size, 8, "Can only load size 8 for now");
            x64_asm_resolve_mem!(ops, memory; mov resolve(dst), QWORD [resolve(src_ptr)]);

        }
        Alloc(r, t) => {
            let saved = store_volatiles_except(ops, memory, r);
            let address = unsafe {
                std::mem::transmute::<_, i64>(
                    malloc as unsafe extern "C" fn(usize) -> *mut libc::c_void,
                )
            };
            
            let stack_space = if saved.len() % 2 == 1 { 0x28 } else { 0x20 };
            let size = module.get_type_by_id(t).unwrap().size() as i32;
            x64_asm!(ops
                    ; mov rcx, size
                    ; mov r10, QWORD address
                    ; sub rsp, BYTE stack_space
                    ; call r10
                    ; add rsp, BYTE stack_space
                    ;;x64_asm_resolve_mem!(ops, memory; mov resolve(r), rax));
            load_volatiles(ops, &saved);
                // TODO avoid loading if it is spilled V here  
            emit_spills(ops, &mem_actions, memory, cur);
        }
        Call(a, b, c) => {
            // Recursive call
            // We know we can call the native impl. directly
            if b == fn_state.function_location {
                let saved = store_volatiles_except(ops, memory, a);
                let stack_space = if saved.len() % 2 == 1 { 0x28 } else { 0x20 };
                x64_asm!(ops
                    ; mov r8, [rsp + (c as i32) * 8]
                    ; mov rcx, _vm
                    ; mov rdx, _vm_state
                    ; sub rsp, BYTE stack_space
                    ; call ->_self
                    ; add rsp, BYTE stack_space
                    ;; x64_asm_resolve_mem!(ops, memory; mov resolve(a), rax));
                load_volatiles(ops, &saved); 
                // TODO avoid loading if it is spilled V here  
                emit_spills(ops, &mem_actions, memory, cur);
            }
            // Unknown target
            // Conservatively call trampoline
            else {
                let fn_ref: i64 = unsafe {
                    std::mem::transmute::<_, i64>(
                        JITEngine::trampoline
                            as extern "win64" fn(&VM, &mut VMState, FnLbl, u64) -> u64,
                    )
                };

                let saved = store_volatiles_except(ops, memory, a);
                x64_asm!(ops
                    ;;x64_asm_resolve_mem!(ops, memory; mov r9, resolve(c))
                    ; mov rcx, _vm
                    ; mov rdx, _vm_state
                    ; mov r8w, b as i16
                    ; sub rsp, BYTE 0x20
                    ; mov r10, QWORD fn_ref
                    ; call r10
                    ; add rsp, BYTE 0x20
                    ;;x64_asm_resolve_mem!(ops, memory; mov resolve(a), rax));
                load_volatiles(ops, &saved);
                // TODO avoid loading if it is spilled V here  
                emit_spills(ops, &mem_actions, memory, cur);
            }
        }
        Lbl => {
            let dyn_lbl = fn_state
                .dynamic_labels
                .entry(cur)
                .or_insert(ops.new_dynamic_label());
            emit_spills(ops, &mem_actions, memory, cur);
            x64_asm!(ops
                ; =>*dyn_lbl);
        }
        JmpIfNot(l, c) => {
            let jmp_target = cur as i64 + l as i64;
            let dyn_lbl = fn_state
                .dynamic_labels
                .entry(jmp_target as InstrLbl)
                .or_insert(ops.new_dynamic_label());
            x64_asm!(ops
                ;;x64_asm_resolve_mem!(ops, memory ; cmp resolve(c), 0)
                ;; emit_spills(ops, &mem_actions, memory, cur)
                ; je =>*dyn_lbl);
        }
        Return(r) => {
            x64_asm!(ops
                ;;x64_asm_resolve_mem!(ops, memory; mov rax, resolve(r))
                ;; emit_spills(ops, &mem_actions, memory, cur)
                ; jmp ->_cleanup)
        }
        instr => panic!("Unknown instr {:?}", instr),
    }
}
