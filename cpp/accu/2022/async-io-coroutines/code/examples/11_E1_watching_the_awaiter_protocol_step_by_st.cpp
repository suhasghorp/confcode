#include <coroutine>
#include <iostream>

struct logging_awaiter {
    std::coroutine_handle<>* stash;

    bool await_ready() const noexcept { std::cout << "await_ready -> false\n"; return false; }
    void await_suspend(std::coroutine_handle<> h) const noexcept
    {
        std::cout << "await_suspend: stashing handle\n";
        *stash = h;                                  // someone else will resume us
    }
    int await_resume() const noexcept { std::cout << "await_resume -> 42\n"; return 42; }
};

struct detached {
    struct promise_type {
        detached get_return_object() { return {}; }
        std::suspend_never initial_suspend() noexcept { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() {}
    };
};

std::coroutine_handle<> waiting;

detached consumer()
{
    std::cout << "consumer: before co_await\n";
    int v = co_await logging_awaiter{&waiting};
    std::cout << "consumer: got " << v << '\n';
}

int main()
{
    consumer();                                      // runs until it suspends
    std::cout << "main: resuming consumer\n";
    waiting.resume();                                // like m_continuation.resume()
}
