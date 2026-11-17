// Uses the sendable machinery from Listing 27, Listing 29, Listing 32 and Listing 33, and spawn_thread from Listing 36.
#include <array>
#include <atomic>
#include <print>
#include <thread>
#include <utility>

template <typename First, typename... Rest>
    requires sendable<std::decay_t<First>> && (sendable<std::decay_t<Rest>> && ...)
void fork_all(First&& first, Rest&&... rest)
{
    // Spawn one thread per extra callable ...
    std::array<std::jthread, sizeof...(Rest)> threads{ spawn_thread(std::forward<Rest>(rest))... };
    // ... run the first one here ...
    auto{first}();
    // ... and join them all before returning.
    for (auto& t : threads) t.join();
}

struct hello { int id; void operator()() const { std::println("hello from task {}", id); } };

int main()
{
    fork_all(hello{0}, hello{1}, hello{2}, hello{3});
}
