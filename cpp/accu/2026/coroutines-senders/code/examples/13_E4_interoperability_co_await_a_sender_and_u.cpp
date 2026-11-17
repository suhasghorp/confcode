// Uses NVIDIA stdexec: stdexec:: mirrors C++26 std::execution,
// exec:: holds non-standard extras (task, static_thread_pool).
#include <stdexec/execution.hpp>
#include <exec/task.hpp>
#include <exec/static_thread_pool.hpp>

exec::task<int> compute(stdexec::scheduler auto sch) {
    // Senders are awaitables: this sender runs a step on the thread pool.
    int a = co_await (stdexec::schedule(sch)
                      | stdexec::then([] { return 40; }));
    co_return a + 2;
}

int main() {
    exec::static_thread_pool pool{4};

    // Awaitables are senders: the coroutine task is consumed by sync_wait
    // exactly like any other sender.
    auto [result] = stdexec::sync_wait(compute(pool.get_scheduler())).value();
    return result == 42 ? 0 : 1;
}
