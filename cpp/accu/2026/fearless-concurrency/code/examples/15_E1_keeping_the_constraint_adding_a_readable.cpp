// Uses the sendable machinery from Listing 27, Listing 29, Listing 32 and Listing 33, and spawn_thread from Listing 36.
#include <thread>
#include <type_traits>

// Chosen only when the constrained spawn_thread is not viable.
template <typename Fn, typename... Args>
    requires (not (sendable<std::decay_t<Fn>> && (sendable<std::decay_t<Args>> && ...)))
auto spawn_thread(Fn&&, Args&&...) -> std::jthread
{
    static_assert(sendable<std::decay_t<Fn>>,
                  "spawn_thread: the function object (or one of its captures) is not sendable");
    static_assert((sendable<std::decay_t<Args>> && ...),
                  "spawn_thread: an argument is not sendable");
    return {};
}

struct overwrite { int& target; void operator()() const { ++target; } };

int main()
{
    int x = 0;
    (void)x;
    // auto t = spawn_thread(overwrite{x});
    // error: static assertion failed: spawn_thread: the function object
    //        (or one of its captures) is not sendable
}
