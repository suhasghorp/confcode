#include <chrono>
#include <iostream>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>

using namespace std::chrono_literals;

// Simulated remote query: takes `delay` unless cancelled first.
std::optional<std::string> query_replica(std::stop_token st, std::string name,
                                         std::chrono::milliseconds delay) {
    auto deadline = std::chrono::steady_clock::now() + delay;
    while (std::chrono::steady_clock::now() < deadline) {
        if (st.stop_requested()) return std::nullopt;  // someone else won
        std::this_thread::sleep_for(1ms);
    }
    return "answer from " + name;
}

int main() {
    std::stop_source race;                 // shared "cancel the losers" signal
    std::mutex m;
    std::optional<std::string> winner;

    auto contender = [&](std::string name, std::chrono::milliseconds delay) {
        if (auto r = query_replica(race.get_token(), std::move(name), delay)) {
            std::scoped_lock lock(m);
            if (!winner) {
                winner = std::move(r);
                race.request_stop();       // cancel everyone else
            }
        }
    };

    {
        std::jthread a(contender, "replica-A", 120ms);  // slow replica (tail event)
        std::jthread b(contender, "replica-B", 15ms);   // fast replica
    }   // both threads are joined here, so `winner` is safe to read

    std::cout << winner.value_or("no answer") << '\n';  // prints: answer from replica-B
}

