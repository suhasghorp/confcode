#include <iostream>
#include <thread>

int main()
{
    int counter = 0;   // shared, unsynchronised

    auto work = [&counter] {
        for (int i = 0; i < 1'000'000; ++i) {
            ++counter;   // read-modify-write: NOT atomic
        }
    };

    {
        std::jthread t1(work);
        std::jthread t2(work);
    }   // jthreads join automatically here

    // Undefined behaviour: the two threads race on `counter`.
    std::cout << counter << '\n';
}
