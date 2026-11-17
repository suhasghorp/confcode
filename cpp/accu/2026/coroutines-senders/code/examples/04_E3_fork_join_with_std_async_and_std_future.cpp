#include <future>
#include <iostream>
#include <numeric>
#include <vector>

long long sum(const std::vector<int>& v, std::size_t lo, std::size_t hi) {
    return std::accumulate(v.begin() + lo, v.begin() + hi, 0LL);
}

int main() {
    std::vector<int> data(1'000'000, 1);
    const std::size_t mid = data.size() / 2;

    // Fork: two independent computations (launch::async forces a real thread).
    auto left  = std::async(std::launch::async, sum, std::cref(data), 0, mid);
    auto right = std::async(std::launch::async, sum, std::cref(data), mid, data.size());

    // Join: the total depends on both halves.
    long long total = left.get() + right.get();
    std::cout << "total = " << total << '\n';
}

