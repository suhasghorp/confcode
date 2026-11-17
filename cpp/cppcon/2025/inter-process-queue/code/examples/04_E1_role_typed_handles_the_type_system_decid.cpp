#include <condition_variable>
#include <cstdio>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <utility>

class MessageQueue {
    std::mutex m_;
    std::condition_variable cv_;
    std::deque<std::string> items_;
    void push(std::string s) {
        { std::lock_guard lk(m_); items_.push_back(std::move(s)); }
        cv_.notify_one();
    }
    std::string pop() {
        std::unique_lock lk(m_);
        cv_.wait(lk, [&] { return !items_.empty(); });
        auto s = std::move(items_.front());
        items_.pop_front();
        return s;
    }
public:
    class Producer {                       // can only write
        MessageQueue* q_;
        friend MessageQueue;
        explicit Producer(MessageQueue& q) : q_(&q) {}
    public:
        Producer(Producer&&) = default;
        Producer(Producer const&) = delete;
        void push(std::string s) { q_->push(std::move(s)); }
    };
    class Consumer {                       // can only read
        MessageQueue* q_;
        friend MessageQueue;
        explicit Consumer(MessageQueue& q) : q_(&q) {}
    public:
        Consumer(Consumer&&) = default;
        Consumer(Consumer const&) = delete;
        std::string pop() { return q_->pop(); }
    };
    Producer producer() { return Producer(*this); }
    Consumer consumer() { return Consumer(*this); }
};

int main() {
    MessageQueue q;
    auto prod = q.producer();
    auto cons = q.consumer();
    std::jthread t([p = std::move(prod)]() mutable {
        for (int i = 0; i < 3; ++i) p.push("message " + std::to_string(i));
    });
    for (int i = 0; i < 3; ++i) std::printf("%s\n", cons.pop().c_str());
    // cons.push("x");   // error: Consumer has no member named 'push'
}
