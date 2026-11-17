#include <cstdint>
#include <iostream>
#include <thread>
#include <vector>

int main()
{
    std::vector<std::uint64_t> results(2);                       // one slot per thread: no sharing
    {
        std::jthread a([&results] { std::uint64_t n = 0; for (int i = 0; i < 10; ++i) ++n; results[0] = n; });
        std::jthread b([&results] { std::uint64_t n = 0; for (int i = 0; i < 20; ++i) ++n; results[1] = n; });
    }                                                            // joined here: writes happen before reads
    std::cout << "a: " << results[0] << ", b: " << results[1] << '\n';   // a: 10, b: 20
}
