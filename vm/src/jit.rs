use crate::bytecode::{FnLbl, Function, InstrLbl, Instruction, Var};
use crate::vm::{VMState, VM};
use dynasmrt::Assembler;
use dynasmrt::{
    dynasm, x64::Rq, x64::X64Relocation, AssemblyOffset, DynamicLabel, DynasmApi,
    DynasmLabelApi, ExecutableBuffer,
};
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

    pub fn compile(state: &mut JITState, func: &Function) {
        let mut fn_state = FunctionJITState::new(func.location);
        let native_fn_loc = fn_state.ops.offset();

        // RCX contains &VM
        // RDX contains &VMState
        // R8 contains arg

        let mut space_for_variables = func.meta.vars * 8;

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
            fn_state.register_allocator.pin_var_to_register(0 as Var, Rq::R8);
        }

        /*
        * FIXME
        * This is flawed as register allocation currently treats the control flow as linear
        * Allocation should happen over a CFG to account for jmps
        * Currently can go very wrong when the allocator thinks variables will be in registers
        * when they won't actually be during execution, or vice versa
        */
        for (idx, &instr) in func.instructions.iter().enumerate() {
            emit_instr_as_asm(&mut fn_state, idx as InstrLbl, instr);
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

struct LRURegisterAllocator {
    register_mapping: Vec<(Rq, Option<(Var, u64)>)>,
}

impl LRURegisterAllocator {
    pub fn new() -> Self {
        Self {
            register_mapping: vec![
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
    pub fn increment_register_ages(&mut self) {
        for mapping in self.register_mapping.iter_mut() {
            if let (_, Some((_, age))) = mapping {
                *age += 1;
            }
        }
    }

    fn reset_register_age(&mut self, reg: Rq) {
        if let Some(mapping) = self.register_mapping.iter_mut().find(|r| r.0.eq(&reg)) {
            if let Some(mut mapped) = mapping.1 {
                mapped.1 = 0;
            }
        }
    }

    fn get_variable_register(&self, var: Var) -> Option<Rq> {
        self.register_mapping
            .iter()
            .find(|mapping| {
                if let Some((reg_var, _)) = mapping.1 {
                    reg_var.eq(&var)
                } else {
                    false
                }
            })
            .map(|mapping| mapping.0)
    }

    // Returns the LRU register
    fn get_lru(&self) -> Rq {
        if let Some(lru) = self
            .register_mapping
            .iter()
            .enumerate()
            .max_by(|x, y| x.1 .1.unwrap().1.cmp(&y.1 .1.unwrap().1))
        {
            lru.1 .0
        } else {
            panic!("Failed to find LRU");
        }
    }

    fn find_empty_register(&mut self) -> Option<&mut (Rq, Option<(u16, u64)>)> {
        self.register_mapping
            .iter_mut()
            .find(|mapping| mapping.1.eq(&None))
    }

    pub fn pin_var_to_register(&mut self, var: Var, reg: Rq) {
        let mapping = self.register_mapping
            .iter_mut()
            .find(|mapping| {
                mapping.0.eq(&reg)
            }).expect("Invalid register specified");
        assert_eq!(mapping.1, None);
        mapping.1 = Some((var, 0));
    }


    fn alloc_register_impl(&mut self, ops: &mut Assembler<X64Relocation>, var: Var) -> (Rq, bool) {
        // See if var is already allocated
        let maybe_alloced_register = self.get_variable_register(var);
        if let Some(reg) = maybe_alloced_register {
            self.reset_register_age(reg);
            return (reg, false);
        }

        // See if we have an empty register
        if let Some(mapping) = self.find_empty_register() {
            mapping.1 = Some((var, 0)); 
            return (mapping.0, true);
        }

        // Evict oldest register
        let evicted = self.get_lru();
        self.spill_register(ops, evicted);
        let mapping = self.find_empty_register().expect("Evicting LRU failed");
        assert_eq!(mapping.0, evicted);

        mapping.1 = Some((var, 0));
        (mapping.0, true)
    }

    pub fn alloc_register_no_load(&mut self, ops: &mut Assembler<X64Relocation>, var: Var) -> Rq {
        let (allocated_register, _) = self.alloc_register_impl(ops, var);
        allocated_register
    }

    pub fn alloc_register(&mut self, ops: &mut Assembler<X64Relocation>, var: Var) -> Rq {
        let (allocated_register, is_new) = self.alloc_register_impl(ops, var);
        if is_new {
            x64_asm!(ops; mov Rq(allocated_register as u8), [rsp + (var as i32) * 8]);
        }
        allocated_register
    }

    pub fn spill_register<'a>(&mut self, ops: &mut Assembler<X64Relocation>, reg: Rq) {
        let mapping = self
            .register_mapping
            .iter_mut()
            .find(|mapping| mapping.0.eq(&reg));
        if let Some((reg, Some((var, _)))) = mapping {
            x64_asm!(ops ; mov QWORD [rsp + (*var as i32) * 8], Rq(*reg as u8));
            *mapping.unwrap() = (*reg, None);
        }
    }
}

struct FunctionJITState {
    ops: Assembler<X64Relocation>,
    function_location: FnLbl,
    dynamic_labels: HashMap<i16, DynamicLabel>,
    register_allocator: LRURegisterAllocator,
}

impl FunctionJITState {
    pub fn new(function_location: FnLbl) -> Self {
        Self {
            ops: Assembler::new().unwrap(),
            function_location,
            dynamic_labels: HashMap::new(),
            register_allocator: LRURegisterAllocator::new(),
        }
    }
}

fn emit_instr_as_asm<'a>(fn_state: &mut FunctionJITState, cur: InstrLbl, instr: Instruction) {
    // Ops cannot be referenced in x64_asm macros if it's inside fn_state
    let ops = &mut fn_state.ops;
    let reg_alloc = &mut fn_state.register_allocator;
    reg_alloc.increment_register_ages();

    use Instruction::*;
    match instr {
        ConstU32(r, c) => {
            let reg = reg_alloc.alloc_register_no_load(ops, r) as u8;
            x64_asm!(ops ; mov Rq(reg), c as i32);
        }
        LtVarVar(r, a, b) => {
            let reg_r = reg_alloc.alloc_register_no_load(ops, r) as u8;
            let reg_a = reg_alloc.alloc_register(ops, a) as u8;
            let reg_b = reg_alloc.alloc_register(ops, b) as u8;
            x64_asm!(ops
                ; cmp Rq(reg_a), Rq(reg_b)
                ; mov Rq(reg_r), 0
                ; mov rdi, 1
                ; cmovl Rq(reg_r), rdi);
        }
        AddVarVar(r, a, b) => {
            let reg_r = reg_alloc.alloc_register_no_load(ops, r) as u8;
            let reg_a = reg_alloc.alloc_register(ops, a) as u8;
            let reg_b = reg_alloc.alloc_register(ops, b) as u8;
            x64_asm!(ops
                ; mov rsi, Rq(reg_a)
                ; add rsi, Rq(reg_b)
                ; mov Rq(reg_r), rsi);
        }
        SubVarVar(r, a, b) => {
            let reg_r = reg_alloc.alloc_register_no_load(ops, r) as u8;
            let reg_a = reg_alloc.alloc_register(ops, a) as u8;
            let reg_b = reg_alloc.alloc_register(ops, b) as u8;
            x64_asm!(ops
                ; mov rsi, Rq(reg_a)
                ; sub rsi, Rq(reg_b)
                ; mov Rq(reg_r), rsi);
        }
        EqVarVar(r, a, b) => {
            let reg_r = reg_alloc.alloc_register_no_load(ops, r) as u8;
            let reg_a = reg_alloc.alloc_register(ops, a) as u8;
            let reg_b = reg_alloc.alloc_register(ops, b) as u8;
            x64_asm!(ops
                ; cmp Rq(reg_a), Rq(reg_b)
                ; mov Rq(reg_r), 0
                ; mov rdi, 1
                ; cmove Rq(reg_r), rdi);
        }
        Call(a, b, c) => {
            // Recursive call
            // We know we can call the native impl. directly
            if b == fn_state.function_location {
              
                reg_alloc.spill_register(ops, Rq::RCX);
                reg_alloc.spill_register(ops, Rq::RBX);
                reg_alloc.spill_register(ops, Rq::RDX);
                reg_alloc.spill_register(ops, Rq::RAX);
                reg_alloc.spill_register(ops, Rq::R8);
                reg_alloc.spill_register(ops, Rq::R9);
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

                reg_alloc.spill_register(ops, Rq::RCX);
                reg_alloc.spill_register(ops, Rq::RBX);
                reg_alloc.spill_register(ops, Rq::RDX);
                reg_alloc.spill_register(ops, Rq::R8);
                reg_alloc.spill_register(ops, Rq::R9);
                reg_alloc.spill_register(ops, Rq::RAX);
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
            let reg_c = reg_alloc.alloc_register(ops, c) as u8;
            x64_asm!(ops
                ; cmp Rq(reg_c), 0
                ; je =>*dyn_lbl);
        }
        Return(r) => {
            reg_alloc.spill_register(ops, Rq::RAX);
            let reg_r = reg_alloc.alloc_register(ops, r) as u8;
            x64_asm!(ops
                ; mov rax, Rq(reg_r)
                ; jmp ->_cleanup)
        }
        instr => panic!("Unknown instr {:?}", instr),
    }
}
