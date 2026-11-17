// Compile: g++ -std=c++17 -I<ctrack>/include  (add -ltbb, or -DCTRACK_DISABLE_EXECUTION_POLICY, with libstdc++ + TBB headers)
#include "ctrack.hpp"
#include <chrono>
#include <iostream>
#include <thread>

void important_function()
{
    CTRACK;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

int main()
{
    important_function();
    std::string report = ctrack::result_as_string();    // same text as result_print
    std::cout << report.substr(0, 200) << "...\n";
    // ctrack::result_get_tables() returns ctrack_result_tables with summary and detail rows
}
