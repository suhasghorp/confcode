#include <chrono>
#include <iostream>

int dead_code(int size)
{
    int i{}, j{};
    for (; i < size; i++) j++;
    return i;
}

int main()
{
    auto t0 = std::chrono::steady_clock::now();
    long long total = 0;
    for (int n = 0; n < 1'000'000; ++n) total += dead_code(n % 1000);   // the result is used...
    auto t1 = std::chrono::steady_clock::now();

    volatile long long sink = total;                                     // ...and made observable
    (void)sink;
    std::cout << std::chrono::duration<double, std::milli>(t1 - t0).count() << " ms\n";
}
