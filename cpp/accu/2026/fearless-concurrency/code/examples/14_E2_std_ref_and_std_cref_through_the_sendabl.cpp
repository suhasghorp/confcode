// Uses the sendable machinery from Listing 27, Listing 29, Listing 32 and Listing 33, and spawn_thread from Listing 36.
#include <functional>

void read(const int& x)  { (void)x; }
void write(int& x)       { ++x; }

static_assert(not sendable<std::reference_wrapper<int>>);      // stores int*: mutable
static_assert(sendable<std::reference_wrapper<const int>>);    // stores const int*

int main()
{
    int value = 0;
    auto r = spawn_thread(read, std::cref(value));    // OK: shared read-only
    // auto w = spawn_thread(write, std::ref(value)); // error: not sendable
    r.join();
}
