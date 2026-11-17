#include <coroutine>
#include <cstdio>
#include <exception>
#include <random>
#include <vector>

// A deterministic, single-threaded scheduler that resumes ready
// coroutines in a random order chosen by a seed.
class fuzz_scheduler {
public:
    explicit fuzz_scheduler(unsigned seed) : rng_(seed) {}

    auto schedule() {
        struct awaiter {
            fuzz_scheduler* s;
            bool await_ready() const noexcept { return false; }
            void await_suspend(std::coroutine_handle<> h) { s->ready_.push_back(h); }
            void await_resume() const noexcept {}
        };
        return awaiter{this};
    }

    void run() {
        while (!ready_.empty()) {
            std::uniform_int_distribution<std::size_t> pick(0, ready_.size() - 1);
            const auto i = pick(rng_);
            auto h = ready_[i];
            ready_[i] = ready_.back();
            ready_.pop_back();
            h.resume();
        }
    }

private:
    std::mt19937 rng_;
    std::vector<std::coroutine_handle<>> ready_;
};

// Test-only fire-and-forget coroutine type (NOT structured!).
// Safe here only because run() drains every task before the
// scheduler and the log go out of scope.
struct job {
    struct promise_type {
        job get_return_object() noexcept { return {}; }
        std::suspend_never initial_suspend() noexcept { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        void return_void() noexcept {}
        void unhandled_exception() { std::terminate(); }
    };
};

job worker(fuzz_scheduler& s, int id, std::vector<int>& log) {
    for (int step = 0; step < 3; ++step) {
        co_await s.schedule();          // a point where interleaving may change
        log.push_back(id * 10 + step);
    }
}

int main() {
    for (unsigned seed = 0; seed < 5; ++seed) {
        fuzz_scheduler s{seed};
        std::vector<int> log;
        worker(s, 1, log);
        worker(s, 2, log);
        s.run();

        // Property check: each worker's own steps must appear in order.
        int last[3] = {-1, -1, -1};
        bool ok = true;
        for (int e : log) {
            const int id = e / 10, step = e % 10;
            ok = ok && step == last[id] + 1;
            last[id] = step;
        }

        std::printf("seed %u:", seed);
        for (int e : log) std::printf(" %d", e);
        std::printf("  -> %s\n", ok ? "ok" : "VIOLATION");
    }
}

