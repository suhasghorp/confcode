#include <coroutine>
#include <iostream>

struct traced {
    struct promise_type {
        promise_type() { std::cout << "promise()\n"; }
        ~promise_type() { std::cout << "~promise()\n"; }
        traced get_return_object()
        {
            std::cout << "get_return_object()\n";
            return traced{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        std::suspend_never initial_suspend() noexcept { std::cout << "initial_suspend()\n"; return {}; }
        std::suspend_always final_suspend() noexcept { std::cout << "final_suspend()\n"; return {}; }
        void return_void() { std::cout << "return_void()\n"; }
        void unhandled_exception() {}
    };
    std::coroutine_handle<promise_type> h;
    ~traced() { if (h) h.destroy(); }
};

traced body()
{
    std::cout << "body: start\n";
    co_await std::suspend_always{};
    std::cout << "body: resumed\n";
}

int main()
{
    traced t = body();
    std::cout << "main: resuming\n";
    t.h.resume();
    std::cout << "main: done\n";
}
