# One-time: Abseil 20260526.0 (the version pinned in MODULE.bazel) into /usr/local
git clone --depth 1 --branch 20260526.0 https://github.com/abseil/abseil-cpp.git /tmp/abseil-cpp
cmake -S /tmp/abseil-cpp -B /tmp/abseil-cpp/build -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_STANDARD=20 -DABSL_PROPAGATE_CXX_STD=ON \
      -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DABSL_BUILD_TESTING=OFF \
      -DCMAKE_INSTALL_PREFIX=/usr/local
cmake --build /tmp/abseil-cpp/build -j"$(nproc)"
sudo cmake --install /tmp/abseil-cpp/build

# The repository, built with the CMakeLists.txt in bench/ (same flags as .bazelrc)
git clone https://github.com/ktgorekore/hotpathcplusplus.git
cmake -S bench -B bench/out -DHOTPATH_REPO=$PWD/hotpathcplusplus -DCMAKE_CXX_COMPILER=g++-13
cmake --build bench/out -j6
./bench/out/memory_map_test && ./bench/out/broadcast_ring_buffer_test

# The video's benchmark and its perf stat command, with our binary path
./bench/out/broadcast_ring_buffer_benchmark --benchmark_repetitions=3 --benchmark_report_aggregates_only=true
perf stat -e cycles,instructions,cache-misses,branch-misses ./bench/out/broadcast_ring_buffer_benchmark
