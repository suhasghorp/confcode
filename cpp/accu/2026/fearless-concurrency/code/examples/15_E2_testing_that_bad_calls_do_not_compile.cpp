// Uses the sendable machinery from Listing 27, Listing 29, Listing 32 and Listing 33, and spawn_thread from Listing 36.
#include <string>
#include <string_view>
#include <utility>

template <typename... Ts>
concept spawnable = requires(Ts&&... ts) { spawn_thread(std::forward<Ts>(ts)...); };

struct print_view { std::string_view sv; void operator()() const {} };
struct overwrite  { std::string& str;    void operator()() const { str = "goodbye"; } };
void takes_view(std::string_view) {}

// Positive tests
static_assert(spawnable<print_view>);
static_assert(spawnable<void(*)(std::string_view), std::string_view>);

// Negative tests: each line would be a compile error if written as a call.
static_assert(not spawnable<overwrite>);
static_assert(not spawnable<void(*)(int*), int*>);

int main() {}
