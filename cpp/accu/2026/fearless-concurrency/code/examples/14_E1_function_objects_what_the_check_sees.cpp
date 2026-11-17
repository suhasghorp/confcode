// Uses the sendable machinery from Listing 27, Listing 29, Listing 32 and Listing 33, and spawn_thread from Listing 36.
#include <string>
#include <string_view>

// A hand-written equivalent of  [sv] { std::println("{}", sv); }
struct print_view {
    std::string_view sv;             // by-value capture of a view of const chars
    void operator()() const { /* print sv */ }
};

// A hand-written equivalent of  [&str] { str = "goodbye"; }
struct overwrite {
    std::string& str;                // by-reference capture of a mutable string
    void operator()() const { str = "goodbye"; }
};

static_assert(sendable<print_view>);       // string_view: sendable
static_assert(not sendable<overwrite>);    // std::string&: mutable reference

int main()
{
    std::string s = "Hello";
    auto t = spawn_thread(print_view{s});  // OK
    // auto u = spawn_thread(overwrite{s}); // error: constraints not satisfied
    t.join();
}
