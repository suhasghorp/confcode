#include <chrono>
#include <cstdio>
#include <exception>
#include <mutex>
#include <stdexcept>
#include <stop_token>
#include <thread>
#include <utility>
#include <vector>

using namespace std::chrono_literals;

class task_scope {
public:
    task_scope() = default;
    task_scope(const task_scope&) = delete;             // children hold `this`:
    task_scope& operator=(const task_scope&) = delete;  // the scope must not move

    template <class F>
    void spawn(F f) {
        threads_.emplace_back([this, f = std::move(f)]() mutable {
            try {
                f(stop_.get_token());
            } catch (...) {
                {
                    std::lock_guard lock(m_);
                    if (!error_) error_ = std::current_exception();
                }
                stop_.request_stop();  // cancel all siblings
            }
        });
    }

    // Wait for every child, then surface the first error (if any).
    void join() {
        for (auto& t : threads_) t.join();
        threads_.clear();
        if (error_) std::rethrow_exception(error_);
    }

    ~task_scope() { stop_.request_stop(); }  // then threads_ joins below

private:
    std::stop_source stop_;
    std::mutex m_;
    std::exception_ptr error_;
    std::vector<std::jthread> threads_;  // declared last => destroyed (joined) first
};

// Run a block that spawns children; return only when all of them are done.
template <class Body>
void with_scope(Body body) {
    task_scope s;
    body(s);
    s.join();
}

int main() {
    try {
        with_scope([](task_scope& s) {
            s.spawn([](std::stop_token st) {  // task a: long-running
                while (!st.stop_requested()) std::this_thread::sleep_for(10ms);
                std::puts("a: cancelled, cleaning up");
            });
            s.spawn([](std::stop_token) {     // task b: fails
                std::this_thread::sleep_for(50ms);
                throw std::runtime_error("b failed");
            });
        });  // both a and b are finished here
    } catch (const std::exception& e) {
        std::printf("scope failed: %s\n", e.what());
    }
}
