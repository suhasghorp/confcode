#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <vector>

int main()
{
    std::shared_mutex m;
    int config = 0;

    auto reader = [&](int id) {
        for (int i = 0; i < 3; ++i) {
            std::shared_lock lock(m);            // many readers at once
            std::cout << "reader " << id << " sees " << config << '\n';
        }
    };
    auto writer = [&] {
        for (int i = 1; i <= 3; ++i) {
            std::unique_lock lock(m);            // exclusive
            config = i;
        }
    };

    std::vector<std::jthread> ts;
    ts.emplace_back(writer);
    for (int id = 0; id < 3; ++id) ts.emplace_back(reader, id);
}
