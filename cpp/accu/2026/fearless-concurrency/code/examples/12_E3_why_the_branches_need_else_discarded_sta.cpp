#include <meta>

template <typename T>
consteval bool separate_ifs()
{
    if constexpr (not std::meta::is_complete_type(^^T)) { return false; }
    // Still instantiated for incomplete T, because it is not in an else branch:
    if constexpr (std::meta::is_class_type(^^T)) { return sizeof(T) > 0; }
    return true;
}

template <typename T>
consteval bool chained_ifs()
{
    if constexpr (not std::meta::is_complete_type(^^T)) { return false; }
    else if constexpr (std::meta::is_class_type(^^T)) { return sizeof(T) > 0; }   // discarded
    else { return true; }
}

struct forward_declared;

static_assert(not chained_ifs<forward_declared>());      // OK
// static_assert(not separate_ifs<forward_declared>());  // error: sizeof applied to incomplete type

int main() {}
