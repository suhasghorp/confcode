// Linux Virtual Memory Magic Mirror Double Mapping:
// Step 1: Reserve 2N contiguous virtual address space with PROT_NONE
size_t total_size = 2 * size;
void* base = mmap(nullptr, total_size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

// Step 2: Map primary buffer to physical shared memory at offset 0
void* first = mmap(base, size, prot, MAP_SHARED | MAP_FIXED, fd, 0);

// Step 3: Map second half (MAGIC MIRROR) to the SAME physical offset 0!
void* second = mmap(static_cast<uint8_t*>(base) + size, size, prot,
                    MAP_SHARED | MAP_FIXED, fd, 0);

// Zero-Copy Result: Any pointer dereference across 'size' wraps to byte
