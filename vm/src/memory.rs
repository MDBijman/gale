use crate::bytecode::{Size, Type};
use std::mem::size_of;

///
/// This is the capacity in bytes with which the heap space is initialized.
///
const HEAP_INITIAL_CAPACITY: usize = 1000 * 1000 * 1000;

const DEBUG_HEAP: bool = false;

/*
* This is the factor by which the heap grows when memory is needed.
*/
//const HEAP_GROW_FACTOR: usize = 2;

/// Minimum size of an Array on the heap. <size, elements...>
pub const ARRAY_HEADER_SIZE: usize = std::mem::size_of::<u64>() * 1;

/// Minimum size of a String on the heap. <size, bytes...>.
pub const STRING_HEADER_SIZE: usize = std::mem::size_of::<u64>() * 1;

/// Size of a pointer on the heap. <adress>.
pub const POINTER_SIZE: usize = std::mem::size_of::<u64>() * 1;

#[derive(Debug, Clone)]
pub struct Heap {
    heap: Vec<u64>,
    proxies: Vec<Proxy>,
    debug: bool,
}

impl Heap {
    fn fits_bytes(&self, size: usize) -> bool {
        let spare_slots = self.heap.capacity() - self.heap.len();
        let spare_bytes = spare_slots * 8;

        spare_bytes >= size
    }

    fn resize_heap(&mut self) {
        if self.debug {
            println!("resizing heap");
        }

        let heap_ptr: *const u64 = &self.heap[0];
        self.heap.reserve(self.heap.len() * 2);
        let heap_ptr_after_realloc: *const u64 = &self.heap[0];

        if heap_ptr != heap_ptr_after_realloc {
            let offset = heap_ptr_after_realloc as usize - heap_ptr as usize;

            // Fix proxies
            for stone in self.proxies.iter_mut() {
                stone.adress = (stone.adress as usize + offset as usize) as *mut u8;
            }
        }
    }

    pub extern "C" fn allocate_type(&mut self, t: &Type) -> Pointer {
        let size = t.size();
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

    fn allocate(&mut self, size: usize) -> Pointer {
        if self.debug {
            println!("allocating {} bytes", size);
        }

        let proxy_index = self.proxies.len();
        let heap_index = self.heap.len();
        if !self.fits_bytes(size) {
            self.resize_heap();
        }

        // Compute the lowest number of u64 elements we need to allocate
        // to fit 'size' bytes in there
        let size_as_u64_elements = size / 8 + (size % 8 != 0) as usize;
        self.heap.resize(self.heap.len() + size_as_u64_elements, 0);

        self.proxies.push(Proxy::new(
            size,
            &mut self.heap[heap_index] as *mut u64 as *mut u8,
        ));

        Pointer::new(proxy_index)
    }

    pub fn free(&mut self, ptr: Pointer) {
        if ptr.index == 0 {
            panic!("Null pointer dereference");
        }

        let proxy = &mut self.proxies[ptr.index];
        proxy.size = 0;
        proxy.adress = 0 as *mut u8;
    }

    pub fn raw(&mut self, ptr: Pointer) -> *mut u8 {
        let proxy = &mut self.proxies[ptr.index];

        if proxy.adress == 0 as *mut u8 {
            panic!("Null proxy dereference");
        }

        proxy.adress
    }

    pub unsafe fn raw_as<T>(&mut self, ptr: Pointer) -> *mut T {
        let proxy = &mut self.proxies[ptr.index];

        if proxy.adress == 0 as *mut u8 {
            panic!("Null proxy dereference");
        }

        proxy.adress as *mut T
    }

    pub extern "C" fn index_bytes(&mut self, ptr: Pointer, bytes: usize) -> Pointer {
        if self.debug {
            println!("index {} bytes from {:?}", bytes, ptr);
        }

        let proxy = self.proxies[ptr.index].clone();

        if proxy.adress == 0 as *mut u8 {
            panic!("Null proxy dereference");
        }

        self.proxies.push(Proxy::new(
            proxy.size - bytes,
            (proxy.adress as u64 + bytes as u64) as *mut u8,
        ));

        Pointer::new(self.proxies.len() - 1)
    }

    pub unsafe extern "C" fn load_u64(&mut self, ptr: Pointer) -> u64
    {
        let res = *std::mem::transmute::<_, *mut u64>(self.raw(ptr));

        if self.debug {
            println!("load u64 from {:?}: {}", ptr, res);
        }

        res
    }
    
    pub unsafe extern "C" fn store_u64(&mut self, ptr: Pointer, value: u64)
    {
        if self.debug {
            println!("store u64 {} to {:?}", value, ptr);
        }

        *self.raw_as::<u64>(ptr) = value;
    }


    pub unsafe fn load<T>(&mut self, ptr: Pointer) -> T
    where
        T: Sized + Copy + std::fmt::Debug,
    {
        let res = *std::mem::transmute::<_, *mut T>(self.raw(ptr));
        if self.debug {
            println!("load {} bytes from {:?}: {:?}", std::mem::size_of::<T>(), ptr, res);
        }

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
        let mut res = String::with_capacity(self.heap.len() * 8 + self.proxies.len() * 8);
        use std::fmt::Write;

        writeln!(res, "proxies").unwrap();
        for (i, stone) in self.proxies.iter().enumerate() {
            writeln!(res, "{0} = {2}@{1:?}", i, stone.adress, stone.size).unwrap();
        }

        writeln!(res, "heap").unwrap();
        for slot in self.heap.iter() {
            unsafe {
                write!(res, "{:?}:", std::mem::transmute::<_, *mut u64>(slot)).unwrap();
                let bytes: [u8; 8] = std::mem::transmute(*slot);
                for b in &bytes {
                    write!(res, " {:02x}", b).unwrap();
                }
                writeln!(res).unwrap();
            }
        }

        res
    }
}

impl Default for Heap {
    fn default() -> Self {
        Self {
            heap: Vec::with_capacity(HEAP_INITIAL_CAPACITY / size_of::<u64>()),
            proxies: Vec::new(),
            debug: false || DEBUG_HEAP,
        }
    }
}

#[derive(Debug, Clone)]
struct Proxy {
    adress: *mut u8,
    size: usize,
}

impl Proxy {
    pub fn new(size: usize, adress: *mut u8) -> Self {
        Self { adress, size }
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct Pointer {
    index: usize,
}

impl Pointer {
    pub fn new(index: usize) -> Self {
        Self { index }
    }

    pub fn null() -> Self {
        Self { index: usize::MAX }
    }
}

impl Into<u64> for Pointer {
    fn into(self) -> u64 {
        self.index as u64
    }
}

impl Into<usize> for Pointer {
    fn into(self) -> usize {
        self.index
    }
}

impl From<u64> for Pointer {
    fn from(index: u64) -> Self {
        Self {
            index: index as usize,
        }
    }
}

impl From<usize> for Pointer {
    fn from(index: usize) -> Self {
        Self { index }
    }
}
