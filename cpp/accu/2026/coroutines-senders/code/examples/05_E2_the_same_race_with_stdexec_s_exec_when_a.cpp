#include <stdexec/execution.hpp>
#include <exec/static_thread_pool.hpp>
#include <exec/when_any.hpp>   // NVIDIA stdexec extension, namespace exec::

#include <iostream>
#include <string>

// Stand-ins for real asynchronous queries to two mirrored servers.
auto query(stdexec::scheduler auto sched, std::string name) {
    return stdexec::schedule(sched)
         | stdexec::then([name] { return "answer from " + name; });
}

int main() {
    exec::static_thread_pool pool{2};
    auto sched = pool.get_scheduler();

    // when_any starts both children, completes with the first result,
    // requests stop on the other one, and waits for it to finish.
    auto race = exec::when_any(query(sched, "replica-A"),
                               query(sched, "replica-B"));

    auto [answer] = stdexec::sync_wait(std::move(race)).value();
    std::cout << answer << '\n';
}

