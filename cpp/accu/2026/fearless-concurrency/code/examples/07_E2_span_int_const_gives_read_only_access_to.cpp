#include <numeric>
#include <print>
#include <span>
#include <thread>
#include <vector>

int sum(std::span<int const> view)
{
    // view[0] = 1;   // error: cannot assign to a const int
    return std::accumulate(view.begin(), view.end(), 0);
}

int main()
{
    const std::vector<int> data{1, 2, 3, 4, 5};   // born const

    std::jthread a([view = std::span<int const>(data)] { std::println("{}", sum(view)); });
    std::jthread b([view = std::span<int const>(data)] { std::println("{}", sum(view)); });
}
