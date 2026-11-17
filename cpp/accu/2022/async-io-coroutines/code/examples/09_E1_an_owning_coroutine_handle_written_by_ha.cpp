#include <coroutine>
#include <utility>

template <typename Promise>
class unique_coroutine {
public:
    unique_coroutine() = default;
    explicit unique_coroutine(std::coroutine_handle<Promise> h) noexcept : h_(h) {}

    unique_coroutine(unique_coroutine&& o) noexcept : h_(std::exchange(o.h_, {})) {}
    unique_coroutine& operator=(unique_coroutine o) noexcept { std::swap(h_, o.h_); return *this; }
    ~unique_coroutine() { if (h_) h_.destroy(); }      // destroy the frame, never delete

    Promise& promise() const { return h_.promise(); }
    void resume() const { h_.resume(); }
    explicit operator bool() const noexcept { return static_cast<bool>(h_); }

private:
    std::coroutine_handle<Promise> h_{};
};

struct tiny_task {
    struct promise_type {
        tiny_task get_return_object()
        { return tiny_task{unique_coroutine<promise_type>{std::coroutine_handle<promise_type>::from_promise(*this)}}; }
        std::suspend_always initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() {}
    };
    unique_coroutine<promise_type> coro;
};

tiny_task work() { co_return; }

int main()
{
    tiny_task t = work();   // suspended at initial_suspend
    t.coro.resume();        // runs to final_suspend
}                           // frame destroyed by unique_coroutine
