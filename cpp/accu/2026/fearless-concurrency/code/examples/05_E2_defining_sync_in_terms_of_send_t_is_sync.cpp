#include <type_traits>

template <typename T>
constexpr bool is_send_v = std::is_arithmetic_v<T>;

// Sending a const reference is safe when the referred-to type is safe to share.
template <typename T>
constexpr bool is_send_v<T const&> = is_send_v<T>;

// Rust's rule: T is Sync if and only if &T is Send.
template <typename T>
constexpr bool is_sync_v = is_send_v<T const&>;

static_assert(is_sync_v<int>);
static_assert(!is_sync_v<int*>);

int main() {}
