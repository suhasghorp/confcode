bazel run -c opt //common/shared_memory:broadcast_ring_buffer_benchmark
perf stat -e cycles,instructions,cache-misses,branch-misses ./broadcast_ring_buffer_benchmark
