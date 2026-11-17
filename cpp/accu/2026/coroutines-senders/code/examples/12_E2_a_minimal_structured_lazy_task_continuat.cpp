#include <coroutine>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <utility>

template <class T>
class task {
public:
    struct promise_type {
        T value{};
        std::exception_ptr error;
        std::coroutine_handle<> continuation = std::noop_coroutine();

        task get_return_object() {
            return task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        // Lazy: the child does nothing until a parent awaits it.
        std::suspend_always initial_suspend() noexcept { return {}; }

        struct final_awaiter {
            bool await_ready() noexcept { return false; }
            // When the child finishes, transfer control back to the parent.
            std::coroutine_handle<> await_suspend(
                std::coroutine_handle<promise_type> h) noexcept {
                return h.promise().continuation;
            }
            void await_resume() noexcept {}
        };
        final_awaiter final_suspend() noexcept { return {}; }

        void return_value(T v) { value = std::move(v); }
        void unhandled_exception() { error = std::current_exception(); }
    };

    task(task&& other) noexcept : h_(std::exchange(other.h_, {})) {}
    task& operator=(task&&) = delete;
    ~task() { if (h_) h_.destroy(); }   // RAII: the frame dies with its owner

    // --- awaiter interface: lets a parent coroutine co_await this task ---
    bool await_ready() const noexcept { return false; }
    std::coroutine_handle<> await_suspend(std::coroutine_handle<> parent) noexcept {
        h_.promise().continuation = parent;  // the child remembers its parent
        return h_;                           // start the child (symmetric transfer)
    }
    T await_resume() {                       // success OR error flows upward
        if (h_.promise().error) std::rethrow_exception(h_.promise().error);
        return std::move(h_.promise().value);
    }

    // Top-level entry point; fine here because everything below is synchronous.
    T sync_wait() {
        h_.resume();
        return await_resume();
    }

private:
    explicit task(std::coroutine_handle<promise_type> h) : h_(h) {}
    std::coroutine_handle<promise_type> h_;
};

task<int> child(int x) {
    if (x < 0) throw std::invalid_argument("negative input");
    co_return x * 2;
}

task<int> parent() {
    int a = co_await child(20);   // child frame owned by a temporary task
    int b = co_await child(1);
    try {
        co_await child(-1);       // the error surfaces via await_resume()
    } catch (const std::exception& e) {
        std::cout << "child failed: " << e.what() << '\n';
    }
    co_return a + b;              // every child is finished by now
}

int main() {
    std::cout << parent().sync_wait() << '\n';   // prints 42
}

