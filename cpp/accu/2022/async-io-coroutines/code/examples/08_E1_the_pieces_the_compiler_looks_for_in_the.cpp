#include <coroutine>
#include <iostream>

struct fire_and_forget {
    struct promise_type {
        fire_and_forget get_return_object() { return {}; }       // step 3
        std::suspend_never initial_suspend() noexcept { return {}; } // step 4: run immediately
        std::suspend_never final_suspend() noexcept { return {}; }   // frame frees itself
        void return_void() {}                                     // co_return;
        void unhandled_exception() { std::terminate(); }
    };
};

fire_and_forget hello(int n)       // a coroutine because it uses co_return
{
    for (int i = 0; i < n; ++i) std::cout << "hello " << i << '\n';
    co_return;
}

int main()
{
    hello(2);   // allocates a frame, runs to completion, frame destroyed at final_suspend
}
