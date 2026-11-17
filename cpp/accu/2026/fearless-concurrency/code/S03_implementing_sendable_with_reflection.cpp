template <typename T>
consteval bool is_sendable_impl();

template <typename T>
constexpr bool is_sendable_v = is_sendable_impl<T>();

template <typename T>
concept sendable = is_sendable_v<T>;
