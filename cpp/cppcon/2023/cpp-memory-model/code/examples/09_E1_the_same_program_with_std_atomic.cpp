#include <atomic>
#include <iostream>
#include <thread>

int main()
{
    std::atomic<int> x{1};

    auto w = std::thread([&x] { for (int i = 0; i < 50; i++) x.fetch_add(1); });   // lock add / lock xadd
    auto r = std::thread([&x] { for (int i = 0; i < 50; i++) std::cout << x.load() << '\n'; });

    r.join();
    w.join();
    std::cout << "final: " << x << '\n';   // always 51
}
