#include <barrier>
#include <cstddef>
#include <print>
#include <string_view>
#include <thread>
#include <vector>

int main()
{
    std::vector<std::string_view> ppl{"Alex", "Dani", "Benny", "Guy"};
    auto on_complete = []() noexcept {
        static int x = 0;
        not x++ ? std::print("Go Home!\n") : std::print("Done!\n");
    };
    std::barrier sync{static_cast<std::ptrdiff_t>(ppl.size()), on_complete};   // no narrowing
    auto job = [&sync](std::string_view name) {
        std::print("{} is doing a job\n", name);
        sync.arrive_and_wait();
        std::print("{} is going home\n", name);
        sync.arrive_and_wait();
    };

    std::vector<std::jthread> workers;          // declared AFTER sync: joined before sync is destroyed
    for (const auto& p : ppl) {
        workers.push_back(std::jthread(job, p));
    }
}
