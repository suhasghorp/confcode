#include "ctrack.hpp"
#include <chrono>
#include <thread>

using namespace std::chrono;

void complex_algorithm()
{
    CTRACK;                                        // whole function
    {
        CTRACK_NAME("algorithm_init");
        std::this_thread::sleep_for(milliseconds(10));
    }
    for (int i = 0; i < 3; ++i) {
        CTRACK_NAME("algorithm_iteration");        // one entry, 3 calls
        std::this_thread::sleep_for(milliseconds(15));
    }
    CTRACK_DEV;                                    // second tracker in the same scope: unique name via __COUNTER__
}

int main()
{
    for (int i = 0; i < 10; ++i) complex_algorithm();

    ctrack::ctrack_result_settings s;
    s.non_center_percent = 5;                      // centre = [5-95]
    s.min_percent_active_exclusive = 1.0;          // hide entries below 1% exclusive time
    ctrack::result_print(s);
}
