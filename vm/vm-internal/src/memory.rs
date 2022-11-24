use crate::bytecode::{Size, Type};

///
/// This is the capacity in bytes with which the heap space is initialized.
///
const HEAP_INITIAL_CAPACITY: usize = 1000 * 1000 * 1000;

/*
* This is the factor by which the heap grows when memory is needed.
*/
//const HEAP_GROW_FACTOR: usize = 2;

/// Minimum size of an Array on the heap. <size, elements...>
pub const ARRAY_HEADER_SIZE: usize = std::mem::size_of::<u64>();

/// Minimum size of a String on the heap. <size, bytes...>.
pub const STRING_HEADER_SIZE: usize = std::mem::size_of::<u64>();

/// Size of a pointer on the heap. <adress>.
pub const POINTER_SIZE: usize = std::mem::size_of::<u64>();

#[derive(Debug, Clone)]
pub struct Heap {
    heap: Vec<u8>,
}

impl Heap {
    fn fits_bytes(&self, size: usize) -> bool {
        let spare_slots = self.heap.capacity() - self.heap.len();
        let spare_bytes = spare_slots;

        spare_bytes >= size
    }

    fn resize_heap(&mut self) -> bool {

        let heap_ptr: *const u8 = &self.heap[0];
        self.heap.reserve(self.heap.len() * 2);
        let heap_ptr_after_realloc: *const u8 = &self.heap[0];

        let resized = heap_ptr != heap_ptr_after_realloc;

        if resized {
            log::debug!(target: "memory", "resized heap to size {}", self.heap.capacity());
        }

        resized
    }

    pub extern "C" fn allocate_type(&mut self, t: &Type) -> Pointer {
        let size = t.size();
        log::trace!(target: "heap", "allocating {} bytes", size);

        let ptr = self.allocate(size);

        match t {
            Type::Array(_, Size::Sized(len)) => unsafe {
                *(self.raw(ptr) as usize as *mut u64) = *len as u64;
            },
            Type::Str(Size::Sized(len)) => unsafe {
                *(self.raw(ptr) as usize as *mut u64) = *len as u64;
                *(self
                    .raw(ptr)
                    .offset(STRING_HEADER_SIZE as isize + *len as isize) as usize
                    as *mut u8) = '\0' as u8;
            },
            Type::Str(_) | Type::Array(_, _) => panic!("Cannot alloc unsized types"),
            _ => {}
        };

        ptr
    }

    /*
     * Allocates size bytes on the heap.
     */
    fn allocate(&mut self, size: usize) -> Pointer {
        log::debug!(target: "memory", "allocating {} bytes", size);

        let heap_index = self.heap.len();
        if !self.fits_bytes(size) {
            self.resize_heap();
        }

        self.heap.resize(self.heap.len() + size, 0);

        Pointer::new(heap_index, size)
    }

    pub fn free(&mut self, ptr: Pointer) {
        if ptr.size == 0 {
            panic!("Null pointer dereference");
        }

        todo!("Freeing memory");
    }

    pub fn raw(&mut self, ptr: Pointer) -> *mut u8 {
        &mut self.heap[ptr.index] as *mut u8
    }

    pub unsafe fn raw_as<T>(&mut self, ptr: Pointer) -> *mut T {
        if ptr.size == 0 {
            panic!("Null ptr dereference");
        }

        &mut self.heap[ptr.index] as *mut u8 as *mut T
    }

    pub extern "C" fn index_bytes(&mut self, ptr: Pointer, bytes: usize) -> Pointer {
        log::debug!(target: "memory",
            "index {} bytes from {:?}",
            bytes, ptr);

        if ptr.index < self.heap.len() && ptr.size - bytes <= 0 {
            panic!("Out of bounds");
        }

        Pointer::new(ptr.index + bytes, ptr.size - bytes)
    }

    pub unsafe extern "C" fn load<T>(&mut self, ptr: Pointer) -> T
    where
        T: Sized + Copy + std::fmt::Debug,
    {
        let res = *std::mem::transmute::<_, *mut T>(self.raw(ptr));
        log::debug!(target: "memory",
            "load {} bytes from {:?}: {:?}",
            std::mem::size_of::<T>(),
            ptr,
            res);

        res
    }

    pub unsafe fn store<T>(&mut self, ptr: Pointer, value: T)
    where
        T: Sized,
    {
        *self.raw_as::<T>(ptr) = value;
    }

    pub unsafe fn store_string(&mut self, ptr: Pointer, value: &str) {
        let mut raw_str_ptr: *mut u8 = self.raw(ptr).offset(8);

        for b in value.as_bytes() {
            *raw_str_ptr = *b;
            raw_str_ptr = raw_str_ptr.add(1);
        }
    }

    pub fn index<T>(&mut self, ptr: Pointer, index: usize) -> Pointer
    where
        T: Sized,
    {
        self.index_bytes(ptr, index * std::mem::size_of::<T>())
    }

    pub fn heap_dump(&self) -> String {
        let mut res = String::with_capacity(self.heap.len() * 8);
        use std::fmt::Write;

        for (i, slot) in self.heap.iter().enumerate() {
            unsafe {
                if i % 8 == 0 {
                    if i != 0 {
                        writeln!(res).unwrap();
                    }
                    write!(res, "{:?}:", std::mem::transmute::<_, *mut u8>(slot)).unwrap();
                }
                write!(res, " {:02x}", *slot).unwrap();
            }
        }

        res
    }
}

impl Default for Heap {
    fn default() -> Self {
        Self {
            heap: Vec::with_capacity(HEAP_INITIAL_CAPACITY),
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct Pointer {
    index: usize,
    size: usize,
}

impl Pointer {
    pub fn new(index: usize, size: usize) -> Self {
        Self { index, size }
    }

    pub fn null() -> Self {
        Self {
            index: usize::MAX,
            size: 0,
        }
    }

    pub fn index(&self, by: usize) -> Self {
        Self {
            index: self.index + by,
            size: self.size - by,
        }
    }
}
