#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <thread>

class scope_timer {
public:
    explicit scope_timer(std::string_view name) : name_(name), start_(std::chrono::steady_clock::now()) {}
    ~scope_timer()
    {
        auto us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start_);
        std::cout << name_ << " took " << us.count() << " us\n";
    }
private:
    std::string_view name_;
    std::chrono::steady_clock::time_point start_;
};

int work(bool fail, bool early)
{
    scope_timer t("work");                         // one line, every exit path is measured
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    if (early) return 1;                           // still timed
    if (fail) throw std::runtime_error("oops");    // still timed (during stack unwinding)
    return 0;
}

int main()
{
    work(false, true);
    try { work(true, false); } catch (const std::exception&) {}
}
