B=./bench/out/broadcast_ring_buffer_benchmark

# Where does the time go? Sample the 8-byte zero-copy batch, by symbol and by library
perf record -g -o pzc8.data $B --benchmark_filter='PollZeroCopyBatch/8$' --benchmark_min_time=2s
perf report -i pzc8.data --no-children --sort symbol --stdio
perf report -i pzc8.data --no-children --sort dso --stdio

# Does the hot path make system calls? Compare two iteration counts
strace -c -f $B --benchmark_filter='RoundTripZeroCopy/8$' --benchmark_min_time=1000x
strace -c -f $B --benchmark_filter='RoundTripZeroCopy/8$' --benchmark_min_time=10000x
strace -c -f $B --benchmark_filter='PollZeroCopyBatch/8$' --benchmark_min_time=1000x

# Cost of the individual operations inside Advance()
taskset -c 2 ./bench/out/sync_costs_benchmark

# Real process-to-process latency, and the kernel baseline
./bench/out/ring_cross_process_latency
./bench/out/kernel_ipc_latency

# False sharing: the video's counters, then the tool that actually sees it
perf stat -e cycles,instructions,cache-misses,branch-misses ./bench/out/false_sharing packed rmw
perf stat -e cycles,instructions,cache-misses,branch-misses ./bench/out/false_sharing aligned rmw
perf c2c record ./bench/out/false_sharing packed rmw
perf c2c report --stdio
