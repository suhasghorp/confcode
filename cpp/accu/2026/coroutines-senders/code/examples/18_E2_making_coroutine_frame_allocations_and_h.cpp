#include <coroutine>
#include <cstddef>
#include <cstdio>
#include <exception>
#include <new>
#include <utility>

inline std::size_t frame_allocs = 0;

struct lazy_int {
    struct promise_type {
        int value{};

        // Called for every frame allocation that is NOT elided by HALO.
        static void* operator new(std::size_t n) {
            ++frame_allocs;
            return ::operator new(n);
        }
        static void operator delete(void* p, std::size_t n) noexcept {
            ::operator delete(p, n);
        }

        lazy_int get_return_object() {
            return lazy_int{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        std::suspend_always initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        void return_value(int v) noexcept { value = v; }
        void unhandled_exception() { std::terminate(); }
    };

    explicit lazy_int(std::coroutine_handle<promise_type> h) : h_(h) {}
    lazy_int(lazy_int&& o) noexcept : h_(std::exchange(o.h_, {})) {}
    lazy_int& operator=(lazy_int&&) = delete;
    ~lazy_int() { if (h_) h_.destroy(); }

    int get() {
        h_.resume();
        return h_.promise().value;
    }

private:
    std::coroutine_handle<promise_type> h_;
};

lazy_int answer() { co_return 42; }

int main() {
    int sum = 0;
    for (int i = 0; i < 1000; ++i)
        sum += answer().get();   // frame lifetime is strictly nested: a HALO candidate

    // Typically 1000 at -O0; may drop to 0 with optimizations on some compilers.
    std::printf("sum=%d frame allocations=%zu\n", sum, frame_allocs);
}

