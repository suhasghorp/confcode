// Uses the sendable machinery from Listing 27, Listing 29, Listing 32 and Listing 33, and spawn_thread from Listing 36.
#include <print>
#include <string>
#include <utility>

void show(const std::string* s) { std::println("{}", *s); }

template <>
constexpr bool is_sendable_v<std::string> = true;   // as discussed: a value type

int main()
{
    std::string message = "Hello";

    // const std::string* is sendable: pointer to const, sendable pointee.
    auto t = spawn_thread(show, &std::as_const(message));

    message = "a much longer string that forces a reallocation";   // the spawning thread cheats

    t.join();   // the worker may read freed memory: data race + use-after-free
}
