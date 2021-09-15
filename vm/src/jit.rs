use crate::bytecode::{FnLbl, Function, InstrLbl, Instruction};
use crate::vm::{VMState, VM};
use dynasmrt::{
    dynasm, x64::Assembler, AssemblyOffset, DynasmApi, DynasmLabelApi, ExecutableBuffer,
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
        let mut ops = Assembler::new().unwrap();
        let fn_loc = ops.offset();

        // RCX contains &VM
        // RDX contains &VMState
        // R8 contains arg

        // Prelude, reserve stack space for all variables
        let mut space_for_variables = func.meta.vars * 8;

        // Align stack at the same time
        // At the beginning of the function, stack is not aligned (due to return address having been pushed)
        // Check if the the space for variables aligns the stack, otherwise add 8
        if space_for_variables % 16 == 0 {
            space_for_variables += 8;
        }

        x64_asm!(ops
            ; ->_self:
            ; push _vm
            ; push _vm_state
            ; sub rsp, space_for_variables.into()
            ; mov _vm, rcx
            ; mov _vm_state, rdx
            ; mov [rsp], r8);

        let mut dynamic_labels = HashMap::new();
        for (idx, &instr) in func.instructions.iter().enumerate() {
            emit_instr_as_asm(
                &mut ops,
                &mut dynamic_labels,
                func.location as FnLbl,
                idx as InstrLbl,
                instr,
            );
        }

        // Postlude - free space
        x64_asm!(ops
            ; ->_cleanup:
            ; add rsp, space_for_variables.into()
            ; pop _vm_state
            ; pop _vm
            ; ret);

        let buf = ops.finalize().expect("Failed to finalize x64 asm buffer");

        state.compiled_fns.insert(func.name.clone(), unsafe {
            Box::from(CompiledFn::from(buf, fn_loc))
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

fn emit_instr_as_asm<'a>(
    ops: &mut Assembler,
    dynamic_labels: &mut HashMap<InstrLbl, dynasmrt::DynamicLabel>,
    cur_fn: FnLbl,
    cur: InstrLbl,
    instr: Instruction,
) {
    use Instruction::*;
    match instr {
        ConstU32(r, c) => {
            x64_asm!(ops
                ; mov QWORD [rsp + (r as i32) * 8], c as i32);
        }
        LtVarVar(r, a, b) => {
            x64_asm!(ops
                ; mov rax, [rsp + (a as i32) * 8]
                ; mov rcx, [rsp + (b as i32) * 8]
                ; mov rdx, 0
                ; cmp rax, rcx
                ; mov rax, 1
                ; cmovle rdx, rax
                ; mov [rsp + (r as i32) * 8], rdx);
        }
        AddVarVar(r, a, b) => {
            x64_asm!(ops
                ; mov rax, [rsp + (a as i32) * 8]
                ; mov rcx, [rsp + (b as i32) * 8]
                ; add rax, rcx
                ; mov [rsp + (r as i32) * 8], rax);
        }
        SubVarVar(r, a, b) => {
            x64_asm!(ops
                ; mov rax, [rsp + (a as i32) * 8]
                ; mov rcx, [rsp + (b as i32) * 8]
                ; sub rax, rcx
                ; mov [rsp + (r as i32) * 8], rax);
        }
        EqVarVar(r, a, b) => {
            x64_asm!(ops
                ; mov rax, [rsp + (a as i32) * 8]
                ; mov rcx, [rsp + (b as i32) * 8]
                ; mov rdx, 0
                ; cmp rax, rcx
                ; mov rax, 1
                ; cmove rdx, rax
                ; mov [rsp + (r as i32) * 8], rdx);
        }
        Call(a, b, c) => {
            // Recursive call
            // We know we can call the native impl. directly
            if b == cur_fn {
                x64_asm!(ops
                    ; mov rcx, _vm
                    ; mov rdx, _vm_state
                    ; mov r8, QWORD [rsp + (c as i32) * 8]
                    ; sub rsp, BYTE 0x20
                    ; call ->_self
                    ; add rsp, BYTE 0x20
                    ; mov [rsp + (a as i32) * 8], rax);
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

                x64_asm!(ops
                    ; mov rcx, _vm
                    ; mov rdx, _vm_state
                    ; mov r8w, b as i16
                    ; mov r9, QWORD [rsp + (c as i32) * 8]
                    ; sub rsp, BYTE 0x20
                    ; mov r10, QWORD fn_ref
                    ; call r10
                    ; add rsp, BYTE 0x20
                    ; mov [rsp + (a as i32) * 8], rax);
            }
        }
        Lbl => {
            let dyn_lbl = dynamic_labels.entry(cur).or_insert(ops.new_dynamic_label());
            x64_asm!(ops
                ; =>*dyn_lbl);
        }
        JmpIfNot(l, c) => {
            let jmp_target = cur as i64 + l as i64;
            let dyn_lbl = dynamic_labels
                .entry(jmp_target as InstrLbl)
                .or_insert(ops.new_dynamic_label());
            x64_asm!(ops
                ; mov rax, [rsp + (c as i32) * 8]
                ; cmp rax, 0
                ; je =>*dyn_lbl);
        }
        Return(r) => {
            x64_asm!(ops
                ; mov rax, [rsp + (r as i32) * 8]
                ; jmp ->_cleanup)
        }
        instr => panic!("Unknown instr {:?}", instr),
    }
}
