#include <asio.hpp>
#include <iostream>

int main() {
    asio::io_context ctx;

    for (int i = 0; i < 3; ++i)
        asio::post(ctx, [i] { std::cout << "task " << i << '\n'; });

    std::size_t a = ctx.poll_one();  // runs task 0 only -> 1
    std::size_t b = ctx.poll();      // runs tasks 1 and 2 -> 2

    // The last outstanding work finished, so ctx is now *stopped*.
    asio::post(ctx, [] { std::cout << "late task\n"; });
    std::size_t c = ctx.poll();      // stopped: runs nothing -> 0

    ctx.restart();                   // clear the stopped state
    std::size_t d = ctx.poll();      // runs "late task" -> 1

    std::cout << a << ' ' << b << ' ' << c << ' ' << d << '\n';  // 1 2 0 1

    // In a hybrid loop, holding asio::make_work_guard(ctx) avoids this
    // trap: the context never runs out of work, so it never auto-stops.
}

