#include "ctrack.hpp"
#include <chrono>
#include <thread>

void handle_request()
{
    CTRACK;
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
}

int main()
{
    using clock = std::chrono::steady_clock;
    auto next_report = clock::now() + std::chrono::seconds(1);

    for (int i = 0; i < 3000; ++i) {
        handle_request();
        if (clock::now() >= next_report) {
            ctrack::ctrack_result_settings s;
            s.min_percent_active_exclusive = 1.0;  // keep the report short
            ctrack::result_print(s);               // computing results also flushes the recorded events
            next_report += std::chrono::seconds(1);
        }
    }
}
