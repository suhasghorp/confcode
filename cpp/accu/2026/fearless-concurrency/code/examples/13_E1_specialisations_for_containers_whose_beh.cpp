// Uses the sendable machinery from Listing 27, Listing 29, Listing 32 and Listing 33.
#include <string>
#include <vector>

// From the speaker's repository: a vector is sendable if its elements are.
template <typename T, typename A>
constexpr bool is_sendable_v<std::vector<T, A>> = is_sendable_v<T>;

// The same reasoning applies to std::basic_string, whose internal
// non-const char pointer otherwise makes the default answer "no".
template <typename C, typename Tr, typename A>
constexpr bool is_sendable_v<std::basic_string<C, Tr, A>> = is_sendable_v<C>;

struct Person { std::string name; int age; };

static_assert(sendable<std::vector<int>>);
static_assert(sendable<std::string>);
static_assert(sendable<Person>);                   // derived member-wise
static_assert(not sendable<std::vector<int*>>);    // elements are not sendable

int main() {}
