// Uses the sendable machinery from Listing 27, Listing 29, Listing 32 and Listing 33, and spawn_thread from Listing 36.
#include <mutex>
#include <print>
#include <utility>

template <typename T>
class synchronized {
    T value_;
    mutable std::mutex m_;
public:
    explicit synchronized(T v) : value_(std::move(v)) {}
    template <typename Fn>
    auto with_lock(Fn fn) const { std::scoped_lock lock(m_); return fn(value_); }
    template <typename Fn>
    auto with_lock(Fn fn)       { std::scoped_lock lock(m_); return fn(value_); }
};

// The default algorithm rejects it (mutable member), so we vouch for it --
// the C++ counterpart of Rust's `unsafe impl Sync`. Sharing it by reference
// is then allowed, as long as the value type itself is sendable.
template <typename T>
constexpr bool is_sendable_v<synchronized<T>&> = sendable<T>;

struct add_one {
    synchronized<int>& counter;
    void operator()() const { counter.with_lock([](int& n) { ++n; }); }
};

int main()
{
    synchronized<int> counter(0);
    static_assert(sendable<synchronized<int>&>);
    {
        auto a = spawn_thread(add_one{counter});
        auto b = spawn_thread(add_one{counter});
    }
    std::println("{}", counter.with_lock([](int n) { return n; }));   // 2
}
