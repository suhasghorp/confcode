// SeqLock Bit-Masking Protocol in Reserve() and Commit():

// In Reserve(): Set Bit 0 (Odd version marks active modification)
uint32_t current_ver = header_dest->version.load(std::memory_order_relaxed);
header_dest->version.store(current_ver | 1, std::memory_order_release);

// In Commit(): Increment version to next Even number (Marks frame valid)
header_dest->version.store(current_ver + 1, std::memory_order_release);

// Readers verify: version is even and unchanged acr
