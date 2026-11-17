#include <iostream>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

template <typename T>
class synchronized {
    T value_;
    mutable std::mutex m_;
public:
    template <typename... Args>
    explicit synchronized(Args&&... args) : value_(std::forward<Args>(args)...) {}

    // The ONLY way to reach the value: run a function while the lock is held.
    template <typename Fn>
    decltype(auto) with_lock(Fn&& fn)
    {
        std::scoped_lock lock(m_);
        return std::forward<Fn>(fn)(value_);
    }

    template <typename Fn>
    decltype(auto) with_lock(Fn&& fn) const
    {
        std::scoped_lock lock(m_);
        return std::forward<Fn>(fn)(std::as_const(value_));
    }
};

int main()
{
    synchronized<std::vector<int>> shared;

    {
        std::jthread a([&] { for (int i = 0; i < 100; ++i) shared.with_lock([&](auto& v) { v.push_back(i); }); });
        std::jthread b([&] { for (int i = 0; i < 100; ++i) shared.with_lock([&](auto& v) { v.push_back(-i); }); });
    }

    std::cout << shared.with_lock([](auto const& v) { return v.size(); }) << '\n';   // 200
}
