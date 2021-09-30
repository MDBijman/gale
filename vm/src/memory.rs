use std::mem::size_of;

///
/// This is the capacity in bytes with which the heap space is initialized.
///
const HEAP_INITIAL_CAPACITY: usize = 256;

/*
* This is the factor by which the heap grows when memory is needed.
*/
//const HEAP_GROW_FACTOR: usize = 2;

/// Minimum size of an Array on the heap. <size, elements...>
pub const ARRAY_HEADER_SIZE: usize = std::mem::size_of::<u64>() * 1;

/// Minimum size of a String on the heap. <size, bytes...>.
pub const STRING_HEADER_SIZE: usize = std::mem::size_of::<u64>() * 1;

/// Minimum size of a pointer on the heap. <adress>.
pub const POINTER_SIZE: usize = std::mem::size_of::<u64>() * 1;

pub struct Heap {
    heap: Vec<u64>,
    tombstones: Vec<Tombstone>,
}

impl Heap {
    fn fits_bytes(&self, size: usize) -> bool {
        let spare_slots = self.heap.capacity() - self.heap.len();
        let spare_bytes = spare_slots * 8;

        spare_bytes >= size
    }

    fn resize_heap(&mut self) {
        println!("resizing heap");
        let heap_ptr: *const u64 = &self.heap[0];
        self.heap.reserve(self.heap.len() * 2);
        let heap_ptr_after_realloc: *const u64 = &self.heap[0];

        if heap_ptr != heap_ptr_after_realloc {
            let offset = heap_ptr_after_realloc as usize - heap_ptr as usize;

            // Fix tombstones
            for stone in self.tombstones.iter_mut() {
                stone.adress = (stone.adress as usize + offset as usize) as *mut u8;
            }
        }
    }

    pub fn allocate(&mut self, size: usize) -> Pointer {
        println!("allocating {} bytes", size);
        let tombstone_index = self.tombstones.len();
        let heap_index = self.heap.len();
        if !self.fits_bytes(size) {
            self.resize_heap();
        }

        // Compute the lowest number of u64 elements we need to allocate
        // to fit 'size' bytes in there
        let size_as_u64_elements = size / 8 + (size % 8 != 0) as usize;
        self.heap.resize(self.heap.len() + size_as_u64_elements, 0);

        self.tombstones
            .push(Tombstone::new(size, &mut self.heap[heap_index] as *mut u64 as *mut u8));

        Pointer::new(tombstone_index)
    }

    pub fn allocate_array(&mut self, size: usize) -> Pointer {
        let total_size = size + ARRAY_HEADER_SIZE;
        let ptr = self.allocate(total_size);

        unsafe {
            *(self.raw(ptr) as *mut u64) = size as u64;
        }

        ptr
    }

    pub fn allocate_string(&mut self, size: usize) -> Pointer {
        let total_size = size + STRING_HEADER_SIZE + 1;
        let ptr = self.allocate(total_size);

        unsafe {
            *(self.raw(ptr) as *mut u64) = size as u64;
        }

        ptr
    }

    pub fn free(&mut self, ptr: Pointer) {
        if ptr.index == 0 {
            panic!("Null pointer dereference");
        }

        let tombstone = &mut self.tombstones[ptr.index];
        tombstone.size = 0;
        tombstone.adress = 0 as *mut u8;
    }

    pub fn raw(&mut self, ptr: Pointer) -> *mut u8 {
        let tombstone = &mut self.tombstones[ptr.index];

        if tombstone.adress == 0 as *mut u8 {
            panic!("Null tombstone dereference");
        }

        tombstone.adress
    }

    pub fn index(&mut self, ptr: Pointer, bytes: usize) -> Pointer {
        let tombstone = self.tombstones[ptr.index].clone();

        if tombstone.adress == 0 as *mut u8 {
            panic!("Null tombstone dereference");
        }

        self.tombstones.push(Tombstone::new(tombstone.size - bytes, (tombstone.adress as u64 + bytes as u64) as *mut u8));

        Pointer::new(self.tombstones.len() - 1)
    }

    pub fn heap_dump(&self) -> String {
        let mut res = String::with_capacity(self.heap.len() * 8 + self.tombstones.len() * 8);
        use std::fmt::Write;

        writeln!(res, "tombstones").unwrap();
        for (i, stone) in self.tombstones.iter().enumerate() {
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
            tombstones: Vec::new(),
        }
    }
}

#[derive(Clone)]
struct Tombstone {
    adress: *mut u8,
    size: usize,
}

impl Tombstone {
    pub fn new(size: usize, adress: *mut u8) -> Self {
        Self { adress, size }
    }
}

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
