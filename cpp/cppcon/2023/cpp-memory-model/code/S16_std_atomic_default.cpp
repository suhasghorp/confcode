Int a = 0
std::atomic<int> ready(0)

// Thread 1
a = 42;
ready.store(1)

// Thread 2
int r1 = ready
int r2 = a;
