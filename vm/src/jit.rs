use crate::bytecode::{BasicBlock, ControlFlowGraph, FnLbl, Function, InstrLbl, Instruction, Var};
use crate::vm::{VMState, VM};
use dynasmrt::x64::Assembler;
use dynasmrt::{
    dynasm, x64::Rq, x64::X64Relocation, AssemblyOffset, DynamicLabel, DynasmApi, DynasmLabelApi,
    ExecutableBuffer,
};
use dynasmrt::{Register};
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

pub struct JITState {
    compiled_fns: HashMap<String, Box<CompiledFn>>,
}

impl JITState {
    pub fn new() -> Self {
        Self {
            compiled_fns: HashMap::new(),
        }
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
        let opt_fn = state.jitter_state.compiled_fns.get(&String::from(name));
        match opt_fn {
            Some(compiled_fn) => {
                let compiled_fn: *const CompiledFn = &**compiled_fn;
                unsafe { Some(compiled_fn.as_ref()?.call(vm, state, arg)) }
            }
            None => None,
        }
    }

    pub fn dump_raw(state: &mut JITState, func: &Function) {
        let a = state.compiled_fns.get(&func.name).unwrap();
        let mut i = 0;
        for x in a.code.iter() {
            print!("{:02x} ", x);
            if i == 15 {
                println!();
            }

            i += 1;
            i %= 16;
        }
        println!();
    }

    pub fn compile(state: &mut JITState, func: &Function) {
        let cfg = ControlFlowGraph::from_function(func);
        let mut fn_state = FunctionJITState::new(func.location, &cfg);
        let native_fn_loc = fn_state.ops.offset();

        // RCX contains &VM
        // RDX contains &VMState
        // R8 contains arg

        let mut space_for_variables = func.varcount * 8;

        // Align stack at the same time
        // At the beginning of the function, stack is not aligned (due to return address having been pushed)
        // Check if the the space for variables aligns the stack, otherwise add 8
        if space_for_variables % 16 == 0 {
            space_for_variables += 8;
        }

        {
            // Prelude, reserve stack space for all variables
            // Create small scope for 'ops' so we don't borrow mutably twice when we emit instructions
            let ops = &mut fn_state.ops;
            x64_asm!(ops
                ; ->_self:
                ; push _vm
                ; push _vm_state
                ; sub rsp, space_for_variables.into()
                ; mov _vm, rcx
                ; mov _vm_state, rdx);
        }

        for bb in cfg.blocks.iter() {
            Self::allocate_registers_basic_block(&mut fn_state, func, bb);
        }

        fn_state
            .register_allocations
            .get_mut(&0)
            .expect("There is no basic block starting at 0?")
            .0
            .pin_var_to_register(0 as Var, Rq::R8);

        for bb in cfg.blocks.iter() {
            Self::compile_basic_block(&mut fn_state, func, bb);
        }

        {
            // Postlude - free space
            let ops = &mut fn_state.ops;
            x64_asm!(ops
                ; ->_cleanup:
                ; add rsp, space_for_variables.into()
                ; pop _vm_state
                ; pop _vm
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

    fn allocate_registers_basic_block(
        state: &mut FunctionJITState,
        func: &Function,
        block: &BasicBlock,
    ) {
        let mut alloc_begin = LRURegisterAllocation::empty();

        for parent in block.parents.iter() {
            let parent_block = &state.cfg.blocks[*parent];
            let parent_alloc = &state.register_allocations.get(&parent_block.first);

            if parent_alloc.is_none() {
                continue;
            }

            alloc_begin.join(&parent_alloc.unwrap().1);
        }

        let mut alloc_end = alloc_begin.clone();

        for &instr in func.instructions.iter() {
            alloc_instr(&mut alloc_end, instr);
        }

        state.register_allocations.insert(block.first, (alloc_begin, alloc_end));
    }

    fn compile_basic_block(state: &mut FunctionJITState, func: &Function, block: &BasicBlock) {
        let mut pc = block.first;
        for &instr in func.instructions[(block.first as usize)..=(block.last as usize)].iter() {
            emit_instr_as_asm(state, block, pc as InstrLbl, instr);
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

#[derive(PartialEq, Debug, Clone)]
enum RegisterAllocation {
    Has(Var, u64),
    None,
    Any,
}

#[derive(Debug, Clone)]
struct LRURegisterAllocation {
    register_mapping: [(Rq, RegisterAllocation); 7],
}

impl Default for LRURegisterAllocation {
    fn default() -> Self {
        use RegisterAllocation::Any;
        Self {
            register_mapping: [
                (Rq::RAX, Any),
                (Rq::RBX, Any),
                (Rq::RCX, Any),
                (Rq::R8, Any),
                (Rq::R9, Any),
                (Rq::R10, Any),
                (Rq::R11, Any),
            ],
        }
    }
}

impl LRURegisterAllocation {
    fn empty() -> Self {
        use RegisterAllocation::None;
        Self {
            register_mapping: [
                (Rq::RAX, None),
                (Rq::RBX, None),
                (Rq::RCX, None),
                (Rq::R8, None),
                (Rq::R9, None),
                (Rq::R10, None),
                (Rq::R11, None),
            ],
        }
    }

    pub fn new(maps: [(Rq, RegisterAllocation); 7]) -> Self {
        Self {
            register_mapping: maps,
        }
    }

    fn get_register_mut(&mut self, reg: Rq) -> Option<&mut (Rq, RegisterAllocation)> {
        self.register_mapping.iter_mut().find(|r| r.0.eq(&reg))
    }

    pub fn increment_register_ages(&mut self) {
        for mapping in self.register_mapping.iter_mut() {
            if let (_, RegisterAllocation::Has(_, age)) = mapping {
                *age += 1;
            }
        }
    }

    fn reset_register_age(&mut self, reg: Rq) {
        if let Some(mapping) = self.get_register_mut(reg) {
            if let (_, RegisterAllocation::Has(_, age)) = mapping {
                *age = 0;
            }
        }
    }

    fn get_register_variable(&self, needle: Rq) -> Option<Var> {
        self.register_mapping.iter().find_map(|mapping| {
            if mapping.0 == needle {
                if let RegisterAllocation::Has(v, _) = mapping.1 {
                    Some(v)
                } else {
                    None
                }
            } else {
                None
            }
        })
    }

    fn get_variable_register(&self, needle: Var) -> Option<Rq> {
        self.register_mapping
            .iter()
            .find(|mapping| {
                if let RegisterAllocation::Has(var, _) = mapping.1 {
                    var.eq(&needle)
                } else {
                    false
                }
            })
            .map(|mapping| mapping.0)
    }

    // Returns the LRU register
    fn get_lru(&self) -> Rq {
        let mut oldest: Option<(Rq, u64)> = None;
        // Kinda messy
        for mapping in self.register_mapping.iter() {
            if let RegisterAllocation::Has(_, age) = mapping.1 {
                if oldest.is_none() || age > oldest.unwrap().1 {
                    oldest = Some((mapping.0, age));
                }
            }
        }
        println!("{:?}", &self);
        oldest.expect("No LRU register to be found").0
    }

    fn find_empty_register(&mut self) -> Option<&mut (Rq, RegisterAllocation)> {
        self.register_mapping
            .iter_mut()
            .find(|mapping| mapping.1.eq(&RegisterAllocation::None))
    }

    pub fn pin_var_to_register(&mut self, var: Var, reg: Rq) {
        let mapping = self
            .get_register_mut(reg)
            .expect("Invalid register specified");
        //assert_eq!(mapping.1, RegisterAllocation::None);
        mapping.1 = RegisterAllocation::Has(var, 0);
    }

    fn alloc_register(&mut self, var: Var) -> (Rq, bool) {
        // See if var is already allocated
        let maybe_alloced_register = self.get_variable_register(var);
        if let Some(reg) = maybe_alloced_register {
            self.reset_register_age(reg);
            return (reg, false);
        }

        // See if we have an empty register
        if let Some(mapping) = self.find_empty_register() {
            mapping.1 = RegisterAllocation::Has(var, 0);
            return (mapping.0, true);
        }

        // Evict oldest register
        let evicted = self.get_lru();
        self.spill_register(evicted);
        let mapping = self.find_empty_register().expect("Evicting LRU failed");
        assert_eq!(mapping.0, evicted);

        mapping.1 = RegisterAllocation::Has(var, 0);
        (mapping.0, true)
    }

    pub fn spill_register<'a>(&mut self, reg: Rq) -> Option<Var> {
        let mapping = self.get_register_mut(reg);
        if let Some((reg, RegisterAllocation::Has(var, _))) = mapping {
            let var = var.clone();
            *mapping.unwrap() = (*reg, RegisterAllocation::None);
            Some(var)
        } else {
            None
        }
    }

    pub fn join(&mut self, r: &LRURegisterAllocation) {
        fn join_register(lhs: &RegisterAllocation, rhs: &RegisterAllocation) -> RegisterAllocation {
            match (lhs, rhs) {
                (RegisterAllocation::Any, _) => RegisterAllocation::Any,
                (_, RegisterAllocation::Any) => RegisterAllocation::Any,
                (RegisterAllocation::None, RegisterAllocation::None) => RegisterAllocation::None,
                (RegisterAllocation::Has(v1, a1), RegisterAllocation::Has(v2, a2)) if v1 == v2 => {
                    RegisterAllocation::Has(*v1, u64::min(*a1, *a2))
                }
                (_, _) => RegisterAllocation::Any,
            }
        }

        for (r, mapping) in r.register_mapping.iter() {
            let reg = self.get_register_mut(*r).unwrap();
            let joined = join_register(&reg.1, mapping);
            reg.1 = joined;
        }
    }
}

struct FunctionJITState<'a> {
    cfg: &'a ControlFlowGraph,
    ops: Assembler,
    function_location: FnLbl,
    dynamic_labels: HashMap<i16, DynamicLabel>,
    register_allocations: HashMap<i16, (LRURegisterAllocation, LRURegisterAllocation)>,
}

impl<'a> FunctionJITState<'a> {
    pub fn new(function_location: FnLbl, cfg: &'a ControlFlowGraph) -> Self {
        Self {
            cfg,
            ops: Assembler::new().unwrap(),
            function_location,
            dynamic_labels: HashMap::new(),
            register_allocations: HashMap::new(),
        }
    }
}

fn alloc_instr(
    reg_alloc: &mut LRURegisterAllocation,
    instr: Instruction,
) {
    use Instruction::*;
    match instr {
        ConstU32(r, _) => {
            reg_alloc.alloc_register(r);
        }
        LtVarVar(r, a, b) => {
            reg_alloc.alloc_register(r);
            reg_alloc.alloc_register(a);
            reg_alloc.alloc_register(b);
        }
        AddVarVar(r, a, b) => {
            reg_alloc.alloc_register(r);
            reg_alloc.alloc_register(a);
            reg_alloc.alloc_register(b);
        }
        SubVarVar(r, a, b) => {
            reg_alloc.alloc_register(r);
            reg_alloc.alloc_register(a);
            reg_alloc.alloc_register(b);
        }
        EqVarVar(r, a, b) => {
            reg_alloc.alloc_register(r);
            reg_alloc.alloc_register(a);
            reg_alloc.alloc_register(b);
        }
        Call(_, _, _) => {
            reg_alloc.spill_register(Rq::RCX);
            reg_alloc.spill_register(Rq::RBX);
            reg_alloc.spill_register(Rq::RDX);
            reg_alloc.spill_register(Rq::RAX);
            reg_alloc.spill_register(Rq::R8);
            reg_alloc.spill_register(Rq::R9);
        }
        Lbl => {}
        JmpIfNot(l, c) => {
            reg_alloc.alloc_register(c);
        }
        Return(r) => {
            reg_alloc.spill_register(Rq::RAX);
            reg_alloc.alloc_register(r);
        }
        instr => panic!("Unknown instr {:?}", instr),
    }
}



fn emit_instr_as_asm<'a>(
    fn_state: &mut FunctionJITState,
    block: &BasicBlock,
    cur: InstrLbl,
    instr: Instruction,
) {
    // Ops cannot be referenced in x64_asm macros if it's inside fn_state
    let ops = &mut fn_state.ops;
    let reg_alloc = &mut fn_state
        .register_allocations
        .get_mut(&block.first)
        .unwrap()
        .0;

    use Instruction::*;
    match instr {
        ConstU32(r, c) => {
            let reg_r = alloc_no_load(reg_alloc, r);
            x64_asm!(ops ; mov Rq(reg_r as u8), c as i32);
        }
        LtVarVar(r, a, b) => {
            let reg_r = alloc_no_load(reg_alloc, r);
            let reg_a = alloc_and_load(ops, reg_alloc, a);
            let reg_b = alloc_and_load(ops, reg_alloc, b);

            x64_asm!(ops
                ; cmp Rq(reg_a as u8), Rq(reg_b as u8)
                ; mov Rq(reg_r as u8), 0
                ; mov rdi, 1
                ; cmovl Rq(reg_r as u8), rdi);
        }
        AddVarVar(r, a, b) => {
            let reg_r = alloc_no_load(reg_alloc, r);
            let reg_a = alloc_and_load(ops, reg_alloc, a);
            let reg_b = alloc_and_load(ops, reg_alloc, b);
            x64_asm!(ops
                ; mov rsi, Rq(reg_a as u8)
                ; add rsi, Rq(reg_b as u8)
                ; mov Rq(reg_r as u8), rsi);
        }
        SubVarVar(r, a, b) => {
            let reg_r = alloc_no_load(reg_alloc, r);
            let reg_a = alloc_and_load(ops, reg_alloc, a);
            let reg_b = alloc_and_load(ops, reg_alloc, b);
            x64_asm!(ops
                ; mov rsi, Rq(reg_a as u8)
                ; sub rsi, Rq(reg_b as u8)
                ; mov Rq(reg_r as u8), rsi);
        }
        EqVarVar(r, a, b) => {
            let reg_r = alloc_no_load(reg_alloc, r);
            let reg_a = alloc_and_load(ops, reg_alloc, a);
            let reg_b = alloc_and_load(ops, reg_alloc, b);
            x64_asm!(ops
                ; cmp Rq(reg_a as u8), Rq(reg_b as u8)
                ; mov Rq(reg_r as u8), 0
                ; mov rdi, 1
                ; cmove Rq(reg_r as u8), rdi);
        }
        Call(a, b, c) => {
            // Recursive call
            // We know we can call the native impl. directly
            if b == fn_state.function_location {
                spill_and_store(ops, reg_alloc, Rq::RCX);
                spill_and_store(ops, reg_alloc, Rq::RBX);
                spill_and_store(ops, reg_alloc, Rq::RDX);
                spill_and_store(ops, reg_alloc, Rq::RAX);
                spill_and_store(ops, reg_alloc, Rq::R8);
                spill_and_store(ops, reg_alloc, Rq::R9);
                x64_asm!(ops
                    ; mov r8, [rsp + (c as i32) * 8]
                    ; mov rcx, _vm
                    ; mov rdx, _vm_state
                    ; sub rsp, BYTE 0x20
                    ; call ->_self
                    ; add rsp, BYTE 0x20
                    ; ; reg_alloc.pin_var_to_register(a, Rq::RAX));
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

                spill_and_store(ops, reg_alloc, Rq::RCX);
                spill_and_store(ops, reg_alloc, Rq::RBX);
                spill_and_store(ops, reg_alloc, Rq::RDX);
                spill_and_store(ops, reg_alloc, Rq::RAX);
                spill_and_store(ops, reg_alloc, Rq::R8);
                spill_and_store(ops, reg_alloc, Rq::R9);
                x64_asm!(ops
                    ; mov rcx, _vm
                    ; mov rdx, _vm_state
                    ; mov r8w, b as i16
                    ; mov r9, QWORD [rsp + (c as i32) * 8]
                    ; sub rsp, BYTE 0x20
                    ; mov r10, QWORD fn_ref
                    ; call r10
                    ; add rsp, BYTE 0x20
                    ; ; reg_alloc.pin_var_to_register(a, Rq::RAX));
            }
        }
        Lbl => {
            let dyn_lbl = fn_state
                .dynamic_labels
                .entry(cur)
                .or_insert(ops.new_dynamic_label());
            x64_asm!(ops
                ; =>*dyn_lbl);
        }
        JmpIfNot(l, c) => {
            let jmp_target = cur as i64 + l as i64;
            let dyn_lbl = fn_state
                .dynamic_labels
                .entry(jmp_target as InstrLbl)
                .or_insert(ops.new_dynamic_label());
            let reg_c = alloc_and_load(ops, reg_alloc, c);
            x64_asm!(ops
                ; cmp Rq(reg_c as u8), 0
                ; je =>*dyn_lbl);
        }
        Return(r) => {
            spill_and_store(ops, reg_alloc, Rq::RAX);
            let reg_r = alloc_and_load(ops, reg_alloc, r);
            x64_asm!(ops
                ; mov rax, Rq(reg_r as u8)
                ; jmp ->_cleanup)
        }
        instr => panic!("Unknown instr {:?}", instr),
    }
}

fn spill_and_store(ops: &mut Assembler, reg_alloc: &mut LRURegisterAllocation, reg: Rq) {
    let spill = reg_alloc.spill_register(reg);
    if spill.is_some() {
        let var = spill.unwrap();
        x64_asm!(ops ; mov [rsp + (var as i32) * 8], Rq(reg as u8));
    }
}

fn alloc_and_load(ops: &mut Assembler, reg_alloc: &mut LRURegisterAllocation, var: Var) -> Rq {
    let (reg, load) = reg_alloc.alloc_register(var);
    if load {
        x64_asm!(ops ; mov Rq(reg as u8), [rsp + (var as i32) * 8])
    }
    reg
}

fn alloc_no_load(reg_alloc: &mut LRURegisterAllocation, var: Var) -> Rq {
    let (reg, _) = reg_alloc.alloc_register(var);
    reg
}
