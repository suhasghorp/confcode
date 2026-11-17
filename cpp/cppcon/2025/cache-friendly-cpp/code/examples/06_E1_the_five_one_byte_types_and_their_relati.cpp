#include <cstddef>
#include <cstdint>
#include <type_traits>

// three distinct character types...
static_assert(!std::is_same_v<char, signed char>);
static_assert(!std::is_same_v<char, unsigned char>);
// ...while 'signed' is redundant for every other integer type
static_assert(std::is_same_v<signed int, int>);
static_assert(std::is_same_v<signed short, short>);
// plus two more distinct one-byte types
static_assert(!std::is_same_v<char8_t, unsigned char>);
static_assert(std::is_unsigned_v<char8_t>);
static_assert(std::is_enum_v<std::byte>);
// the fixed-width aliases are the signed/unsigned char types
static_assert(std::is_same_v<std::int8_t, signed char>);
static_assert(std::is_same_v<std::uint8_t, unsigned char>);
static_assert(sizeof(char) == 1 && sizeof(char8_t) == 1 && sizeof(std::byte) == 1);

int main() {}
