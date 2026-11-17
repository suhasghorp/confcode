#include <coroutine>
#include <deque>
#include <exception>
#include <iostream>

// A toy "reactor": suspended coroutines waiting to be resumed.
std::deque<std::coroutine_handle<>> ready_queue;

// An awaiter: co_await next_tick{v} suspends, and later yields v.
struct next_tick {
    int value;
    bool await_ready() const noexcept { return false; }   // 1. always suspend
    void await_suspend(std::coroutine_handle<> h) {      // 2. park ourselves
        ready_queue.push_back(h);
    }
    int await_resume() const noexcept { return value; }  // 3. result of co_await
};

// Deliberately UNstructured coroutine type: nobody owns the frame,
// it destroys itself at the end (like std::thread::detach()).
struct detached {
    struct promise_type {
        detached get_return_object() noexcept { return {}; }
        std::suspend_never initial_suspend() noexcept { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        void return_void() noexcept {}
        void unhandled_exception() noexcept { std::terminate(); }
    };
};

detached worker(const char* name) {
    int a = co_await next_tick{1};   // locals live in the coroutine frame
    std::cout << name << " got " << a << '\n';
    int b = co_await next_tick{2};
    std::cout << name << " got " << b << '\n';
}

int main() {
    worker("A");   // runs until its first co_await, then returns here
    worker("B");
    while (!ready_queue.empty()) {   // the event loop
        auto h = ready_queue.front();
        ready_queue.pop_front();
        h.resume();
    }
    // Prints: A got 1, B got 1, A got 2, B got 2
}

