#include <generator>
#include <iostream>

std::generator<int> producer()
{
    for (int x = 1; x <= 3; ++x) {
        std::cout << "  producer: computed " << x << ", suspending\n";
        co_yield x;                                   // suspend, hand x over
        std::cout << "  producer: resumed\n";
    }
}

int main()
{
    for (int x : producer()) {                        // each step resumes the producer
        std::cout << "consumer: working with " << x << '\n';
    }
}
