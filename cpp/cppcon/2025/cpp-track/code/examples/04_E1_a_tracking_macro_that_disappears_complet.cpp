#include <chrono>
#include <cstdio>

#ifndef MYTRACK_DISABLE
struct track_scope {
    const char* fn;
    std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
    ~track_scope()
    {
        auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - t0).count();
        std::printf("%s: %lld ns\n", fn, static_cast<long long>(ns));
    }
};
#define MYTRACK_CONCAT2(a, b) a##b
#define MYTRACK_CONCAT(a, b) MYTRACK_CONCAT2(a, b)
#define MYTRACK track_scope MYTRACK_CONCAT(mytrack_, __COUNTER__){__func__}
#else
#define MYTRACK               // expands to nothing: zero cost, nothing in the binary
#endif

int compute(int n)
{
    MYTRACK;
    int s = 0;
    for (int i = 0; i < n; ++i) s += i;
    return s;
}

int main() { return compute(1000) > 0 ? 0 : 1; }
// Build normally to track; build with -DMYTRACK_DISABLE for a binary identical to untracked code.
