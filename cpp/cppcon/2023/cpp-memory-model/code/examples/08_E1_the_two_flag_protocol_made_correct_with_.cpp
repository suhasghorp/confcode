#include <atomic>
#include <iostream>
#include <thread>

std::atomic<bool> flag1{false}, flag2{false};
int in_critical = 0, overlaps = 0;

void enter(std::atomic<bool>& mine, std::atomic<bool>& other)
{
    mine.store(true);                       // seq_cst by default
    if (!other.load()) {                    // seq_cst: cannot be reordered before the store
        if (++in_critical > 1) ++overlaps;  // the critical section
        --in_critical;
    }
    mine.store(false);
}

int main()
{
    for (int i = 0; i < 100'000; ++i) {
        std::thread a(enter, std::ref(flag1), std::ref(flag2));
        std::thread b(enter, std::ref(flag2), std::ref(flag1));
        a.join(); b.join();
    }
    std::cout << "overlapping entries: " << overlaps << '\n';   // always 0 with seq_cst
}
