Int a = 0
std::atomic<int> ready(0)

// Thread 1
a = 42;
ready.store(1, memory_order_release)

// Thread 2
int r1 = ready.load(memory_order_acquire)
int r2 = a;
