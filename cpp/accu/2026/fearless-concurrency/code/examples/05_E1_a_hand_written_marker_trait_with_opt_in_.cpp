#include <concepts>
#include <memory>
#include <string>

// 1. Primary template: the default answer.
template <typename T>
constexpr bool is_send_v = std::is_arithmetic_v<T>;   // a crude default for now

// 2. Specialisations: library or user opts types in or out.
template <>
constexpr bool is_send_v<std::string> = true;

template <typename T>
constexpr bool is_send_v<std::unique_ptr<T>> = is_send_v<T>;

// 3. A concept for constraining templates.
template <typename T>
concept send = is_send_v<T>;

template <send T>
void give_to_worker(T value) { /* ... move value into another thread ... */ }

int main()
{
    give_to_worker(42);                                 // OK: int
    give_to_worker(std::string("hello"));               // OK: opted in
    give_to_worker(std::make_unique<double>(1.5));      // OK: unique_ptr<double>
    // give_to_worker(std::make_unique<int*>(nullptr)); // error: int* is not send
}
