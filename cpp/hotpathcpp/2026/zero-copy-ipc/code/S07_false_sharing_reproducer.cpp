// The False Sharing Anti-Pattern: Unaligned Consumer Cursors
struct UnalignedRoster {
  // 8 consumers packed tightly into a single 64-byte cache line:
  std::atomic<uint64_t> cursors[8]; // sizeof = 64 bytes!
};

// When Core 1 and Core 2 update adjacent elements simultaneously:
// CPU 1 writes cursors[0] -> CPU 2 cache line is INVALIDATED!
// Bus traffic saturates L3 interconnect; throughput drops by 94%!
