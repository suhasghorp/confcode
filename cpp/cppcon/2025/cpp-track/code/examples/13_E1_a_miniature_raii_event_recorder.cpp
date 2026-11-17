#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <vector>

using clock_type = std::chrono::steady_clock;

struct Event {
    clock_type::time_point start_time, end_time;
    std::string_view function;   // points at a string literal: no allocation
    int line;
};

std::vector<Event> g_events;

class EventHandler {
public:
    EventHandler(int line, std::string_view fn) : line_(line), fn_(fn), start_(clock_type::now()) {}
    ~EventHandler() { g_events.push_back({start_, clock_type::now(), fn_, line_}); }   // runs on every exit
private:
    int line_;
    std::string_view fn_;
    clock_type::time_point start_;
};

#define MINI_TRACK EventHandler mini_track_instance{__LINE__, __func__}

int risky(int x)
{
    MINI_TRACK;
    if (x == 0) return -1;                       // early return: recorded
    if (x < 0) throw std::invalid_argument("x"); // exception: recorded during unwinding
    return x * 2;
}

int main()
{
    risky(0); risky(5);
    try { risky(-1); } catch (...) {}
    for (const auto& e : g_events)
        std::cout << e.function << ':' << e.line << " took "
                  << std::chrono::duration_cast<std::chrono::nanoseconds>(e.end_time - e.start_time).count() << " ns\n";
}
