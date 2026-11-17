template <typename T>
consteval bool is_sendable_impl()
{
    using namespace std::meta;

    constexpr auto info = ^^T;

    // Incomplete types are not sendable
    if constexpr (not is_complete_type(info)) {
        return false;
    }

    // Fundamental types are sendable
    if constexpr (is_fundamental_type(info)) {
        return true;
    }

    // Enum types are sendable
    if constexpr (is_enum_type(info)) {
        return true;
    }
