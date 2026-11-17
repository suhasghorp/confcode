#include <iostream>
#include <thread>

void reader(const int& value)   // "I promise not to modify it" -- but someone else might
{
    for (int i = 0; i < 3; ++i) {
        std::cout << value << '\n';   // races with the writes below
    }
}

int main()
{
    int shared = 0;
    std::jthread t(reader, std::cref(shared));

    for (int i = 0; i < 1000; ++i) {
        ++shared;   // the owning thread still writes: data race (UB)
    }
}
