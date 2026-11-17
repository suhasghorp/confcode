#include <iostream>

// Overall speed-up when a fraction p of the runtime becomes s times faster.
double amdahl(double p, double s) { return 1.0 / ((1.0 - p) + p / s); }

int main()
{
    std::cout << "5% of runtime, 10x faster:  " << amdahl(0.05, 10) << "x overall\n";   // ~1.05x
    std::cout << "5% of runtime, infinitely:  " << amdahl(0.05, 1e12) << "x overall\n"; // ~1.05x
    std::cout << "60% of runtime, 2x faster:  " << amdahl(0.60, 2) << "x overall\n";    // ~1.43x
}
