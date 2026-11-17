// Uses the sendable machinery from Listing 27, Listing 29, Listing 32 and Listing 33, and spawn_thread from Listing 36.
#include <print>
#include <span>
#include <vector>

void print_all(std::span<int const> s) { std::println("{}", s); }

int main()
{
    std::vector<int> vec{1, 2, 3, 4, 5};

    auto thread = spawn_thread(print_all, std::span<int const>(vec));
    thread.join();        // the borrow ends here ...

    vec.resize(100);      // ... so mutating now is safe
}
