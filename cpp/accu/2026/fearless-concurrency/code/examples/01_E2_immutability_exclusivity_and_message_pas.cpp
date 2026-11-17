#include <condition_variable>
#include <iostream>
#include <memory>
#include <mutex>
#include <numeric>
#include <optional>
#include <queue>
#include <thread>
#include <vector>

// 1. Immutability: share data that nobody can modify.
void immutable_style()
{
    auto data = std::make_shared<const std::vector<int>>(std::vector{1, 2, 3, 4});
    std::jthread reader1([data] { std::cout << std::accumulate(data->begin(), data->end(), 0) << '\n'; });
    std::jthread reader2([data] { std::cout << data->size() << '\n'; });
}

// 2. Exclusivity: one writer at a time, enforced (by convention!) with a mutex.
void exclusive_style()
{
    int total = 0;
    std::mutex m;
    auto add = [&](int n) { std::scoped_lock lock(m); total += n; };
    {
        std::jthread a([&] { for (int i = 0; i < 1000; ++i) add(1); });
        std::jthread b([&] { for (int i = 0; i < 1000; ++i) add(1); });
    }
    std::cout << total << '\n';   // always 2000
}

// 3. Message passing: threads share nothing except a synchronised channel.
template <typename T>
class channel {
    std::queue<T> q_;
    std::mutex m_;
    std::condition_variable cv_;
public:
    void send(T value)
    {
        { std::scoped_lock lock(m_); q_.push(std::move(value)); }
        cv_.notify_one();
    }
    T receive()
    {
        std::unique_lock lock(m_);
        cv_.wait(lock, [this] { return !q_.empty(); });
        T value = std::move(q_.front());
        q_.pop();
        return value;
    }
};

void message_style()
{
    channel<std::optional<int>> ch;
    std::jthread consumer([&ch] {
        int sum = 0;
        while (auto msg = ch.receive()) { sum += *msg; }
        std::cout << sum << '\n';
    });
    for (int i = 1; i <= 10; ++i) ch.send(i);
    ch.send(std::nullopt);   // "no more messages"
}

int main()
{
    immutable_style();
    exclusive_style();
    message_style();
}
