#include <string>
#include <type_traits>

template <typename T> constexpr bool is_safe_v = std::is_arithmetic_v<T>;
template <> constexpr bool is_safe_v<std::string> = true;

struct Person { std::string name; int age; };
struct Pair   { Person a; Person b; };

// Without reflection we must spell out the member-wise rule for each type:
template <> constexpr bool is_safe_v<Person> = is_safe_v<std::string> && is_safe_v<int>;
template <> constexpr bool is_safe_v<Pair>   = is_safe_v<Person> && is_safe_v<Person>;

static_assert(is_safe_v<Pair>);
// Tedious and error-prone: add a member and forget to update the trait, and it silently lies.
// The reflection sections later in the chapter automate exactly this.

int main() {}
