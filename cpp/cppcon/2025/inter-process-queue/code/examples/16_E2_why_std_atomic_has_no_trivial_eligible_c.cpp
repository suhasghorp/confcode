#include <atomic>
#include <type_traits>

using A = std::atomic<int>;
static_assert(!std::is_trivially_default_constructible_v<A>);   // value-initialises since C++20
static_assert(!std::is_copy_constructible_v<A>);                // copy constructor deleted
static_assert(!std::is_move_constructible_v<A>);                // move constructor not declared
static_assert(!std::is_aggregate_v<A>);
static_assert(std::is_trivially_destructible_v<A>);             // the destructor is not the problem
static_assert(A::is_always_lock_free);                          // lock-free and address-free, yet...

int main() {}
