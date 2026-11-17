#include <mutex>
#include <thread>

std::mutex a, b;

void deadlock_prone_1() { std::lock_guard la(a); std::lock_guard lb(b); /* ... */ }
void deadlock_prone_2() { std::lock_guard lb(b); std::lock_guard la(a); /* ... */ }  // opposite order!

// Fix: lock both at once; std::scoped_lock uses a deadlock-avoidance algorithm.
void safe_1() { std::scoped_lock lock(a, b); /* ... */ }
void safe_2() { std::scoped_lock lock(b, a); /* ... */ }

int main()
{
    std::jthread t1(safe_1);
    std::jthread t2(safe_2);
}
