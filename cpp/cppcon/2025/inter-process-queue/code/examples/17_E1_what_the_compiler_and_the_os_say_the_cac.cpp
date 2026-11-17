#include <cstdio>
#include <new>
#include <unistd.h>

int main() {
#ifdef __cpp_lib_hardware_interference_size
    std::printf("hardware_destructive_interference_size  = %zu\n", std::hardware_destructive_interference_size);
    std::printf("hardware_constructive_interference_size = %zu\n", std::hardware_constructive_interference_size);
#endif
    std::printf("sysconf(_SC_LEVEL1_DCACHE_LINESIZE)     = %ld\n", ::sysconf(_SC_LEVEL1_DCACHE_LINESIZE));
}
