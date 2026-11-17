#include <atomic>

std::atomic<int> a;
int data;

void store_seq_cst(int x) { a.store(x); }                                // x86: xchg
void store_release(int x) { a.store(x, std::memory_order_release); }     // x86: mov
int  load_seq_cst()       { return a.load(); }                           // x86: mov
int  load_acquire()       { return a.load(std::memory_order_acquire); }  // x86: mov

void publish_with_fence(int x)
{
    data = x;
    std::atomic_thread_fence(std::memory_order_release);   // x86: no instruction, but a compiler barrier
    a.store(1, std::memory_order_relaxed);
}

int main() { publish_with_fence(42); return load_acquire() == 1 ? 0 : 1; }
