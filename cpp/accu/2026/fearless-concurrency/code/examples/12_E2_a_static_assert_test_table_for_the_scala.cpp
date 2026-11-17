// Uses the sendable machinery from Listing 27, Listing 29, Listing 32 and Listing 33.
#include <cstddef>

enum class colour { red, green, blue };

static_assert(sendable<int>);
static_assert(sendable<double>);
static_assert(sendable<char8_t>);
static_assert(sendable<bool>);
static_assert(sendable<std::nullptr_t>);
static_assert(sendable<colour>);
// Incomplete types: see the else-if-constexpr discussion above.

int main() {}
