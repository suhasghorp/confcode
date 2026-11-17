#include <coroutine>
#include <exception>
#include <optional>
#include <stdexcept>
#include <utility>
#include <variant>

// The value slot of a promise that can hold a value OR an exception.
template <typename T>
struct value_slot {
    std::variant<std::monostate, T, std::exception_ptr> v;

    bool is_ready() const noexcept { return v.index() != 0; }

    T get()
    {
        if (auto* e = std::get_if<std::exception_ptr>(&v)) {
            auto ex = *e;
            v = std::monostate{};
            std::rethrow_exception(ex);          // surfaces at the consumer's co_await
        }
        T t = std::move(std::get<T>(v));
        v = std::monostate{};
        return t;
    }

    void set_value(T t) { v = std::move(t); }
    void set_exception(std::exception_ptr e) { v = e; }   // call from unhandled_exception()
};

int main()
{
    value_slot<int> s;
    s.set_exception(std::make_exception_ptr(std::runtime_error("bad packet")));
    try { (void)s.get(); } catch (const std::runtime_error&) { return 0; }
    return 1;
}
