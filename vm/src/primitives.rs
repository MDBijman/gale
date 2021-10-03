use crate::memory::{Heap, Pointer};
use crate::vm::VMState;

pub fn alloc(state: &mut VMState, bytes: usize) -> Pointer {
    state.heap.allocate(bytes)
}

pub fn load<T>(state: &mut VMState, ptr: Pointer) -> T
where
    T: Sized + Copy,
{
    unsafe {
        *std::mem::transmute::<_, *mut T>(state.heap.raw(ptr))
    }
}

pub fn store<T>(state: &mut VMState, ptr: Pointer, value: T) 
where
    T: Sized,
{
    unsafe {
        *state.heap.raw_as::<T>(ptr) = value;
    }
}

pub fn index<T>(state: &mut VMState, ptr: Pointer, index: usize) -> Pointer
where
    T: Sized,
{
    state.heap.index_bytes(ptr, index * std::mem::size_of::<T>())
}


