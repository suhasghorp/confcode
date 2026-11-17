// Google Benchmark Results on AMD Ryzen 9 9950X (Linux 6.8):
// ---------------------------------------------------------------------------
// Benchmark                    Payload   Time/Op     Throughput
// ---------------------------------------------------------------------------
// BM_Produce/8B                     8B    21.4 ns   356.57 MiB/s
// BM_Produce/8KB                 8192B    93.3 ns    81.77 GiB/s
// BM_RoundTripZeroCopy/8KB       8192B   159.0 ns    47.93 GiB/s
// BM_PollZeroCopyBatch/8KB(100)  8192B    26.6 ns   286.93 GiB/s  <-- PEAK
// BM_PollCopyBatch/8B(100)          8B     6.1 ns     1.22 GiB/s  <-- FASTEST
