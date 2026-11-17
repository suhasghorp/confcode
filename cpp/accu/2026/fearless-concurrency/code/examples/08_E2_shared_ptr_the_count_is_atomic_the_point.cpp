#include <memory>
#include <thread>

int main()
{
    auto p = std::make_shared<int>(0);

    auto work = [p] {                    // copying p: atomic refcount increment, safe
        for (int i = 0; i < 1000; ++i) {
            ++*p;                        // unsynchronised write to the shared int: data race
        }
    };

    std::jthread a(work);
    std::jthread b(work);
}   // last owner frees the int: safe, thanks to the atomic count
