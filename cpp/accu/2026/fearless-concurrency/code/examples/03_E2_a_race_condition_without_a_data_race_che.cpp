#include <iostream>
#include <mutex>
#include <thread>

class account {
    mutable std::mutex m_;
    int balance_ = 100;
public:
    int balance() const { std::scoped_lock lock(m_); return balance_; }
    void withdraw(int amount) { std::scoped_lock lock(m_); balance_ -= amount; }
};

int main()
{
    account acc;
    auto try_withdraw = [&acc] {
        if (acc.balance() >= 100) {   // check ...
            acc.withdraw(100);        // ... then act: another thread may get in between
        }
    };
    {
        std::jthread a(try_withdraw);
        std::jthread b(try_withdraw);
    }
    // Every access was locked, so there is no data race,
    // yet the balance can end up at -100.
    std::cout << acc.balance() << '\n';
}
