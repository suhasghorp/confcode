// Uses the sendable machinery from Listing 27, Listing 29, Listing 32 and Listing 33, and spawn_thread from Listing 36.

int hits = 0;   // global mutable state: invisible to the sendable check

void count() { for (int i = 0; i < 1000; ++i) ++hits; }

int main()
{
    // A function pointer is sendable, so both calls are accepted ...
    auto a = spawn_thread(count);
    auto b = spawn_thread(count);
    // ... and the two threads race on `hits`.
}
