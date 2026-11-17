#include <atomic>
#include <cstdint>
#include <cstdio>
#include <map>
#include <optional>

struct PID {                                     // 64-bit packed (pid, start time)
    std::uint64_t v = 0;
    static PID null() { return {}; }
    static PID make(std::uint32_t pid, std::uint32_t start) { return {std::uint64_t(pid) << 32 | start}; }
    std::uint32_t pid() const { return std::uint32_t(v >> 32); }
    bool operator==(PID const&) const = default;
    static std::map<std::uint32_t, std::uint32_t> running;   // fake process table: pid -> start
    static std::optional<PID> maybe(std::uint32_t pid) {     // identity of the process with this pid now
        if (auto it = running.find(pid); it != running.end()) return make(pid, it->second);
        return std::nullopt;
    }
};
std::map<std::uint32_t, std::uint32_t> PID::running;

bool exchange(std::atomic<PID>& pid, PID& expected, PID desired) {
    return pid.compare_exchange_strong(expected, desired, std::memory_order_acq_rel,
                                       std::memory_order_acquire);
}

bool try_lock_impl(std::atomic<PID>& pid, PID const& me) {     // same logic as the talk's slide
    auto expected = PID::null();
    if (exchange(pid, expected, me)) return true;
    if (expected != me) {
        if (auto p = PID::maybe(expected.pid()); not p || expected != *p) {
            exchange(pid, expected, PID::null());              // clear the dead owner
            expected = PID::null();
            if (exchange(pid, expected, me)) return true;      // compete like everyone else
        }
    }
    return false;
}

int main() {
    std::atomic<PID> lock{PID::null()};
    PID::running = {{200, 5}, {300, 7}, {400, 9}};
    auto const p200 = PID::make(200, 5), p300 = PID::make(300, 7), p400 = PID::make(400, 9);

    std::printf("free lock, 200 tries: %s\n", try_lock_impl(lock, p200) ? "acquired" : "refused");
    std::printf("live owner 200, 300 tries: %s\n", try_lock_impl(lock, p300) ? "acquired" : "refused");
    PID::running.erase(200);                                   // 200 crashes holding the lock
    std::printf("owner 200 died, 300 tries: %s\n",
                try_lock_impl(lock, p300) ? "acquired (recovered)" : "refused");
    PID::running[300] = 8;                                     // 300 dies, its pid is reused
    std::printf("owner 300 recycled (same pid, new start), 400 tries: %s\n",
                try_lock_impl(lock, p400) ? "acquired (recovered)" : "refused");
}
