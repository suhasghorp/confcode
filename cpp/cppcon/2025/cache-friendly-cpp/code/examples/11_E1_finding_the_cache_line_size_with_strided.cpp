#include <chrono>
#include <cstdio>
#include <new>
#include <vector>

int main()
{
    std::vector<int> data(16 << 20, 1);   // 64 MB, far bigger than the caches
    std::printf("hardware_constructive_interference_size = %zu\n",
                std::hardware_constructive_interference_size);

    for (std::size_t stride = 1; stride <= 64; stride *= 2) {
        long long sum = 0;
        std::size_t reads = 0;
        auto start = std::chrono::steady_clock::now();
        for (int rep = 0; rep < 4; ++rep)
            for (std::size_t i = 0; i < data.size(); i += stride, ++reads)
                sum += data[i];
        auto stop = std::chrono::steady_clock::now();
        double ns = std::chrono::duration<double, std::nano>(stop - start).count() / reads;
        std::printf("stride %2zu ints (%3zu bytes): %6.2f ns per read  (sum %lld)\n",
                    stride, stride * sizeof(int), ns, sum);
    }
}
