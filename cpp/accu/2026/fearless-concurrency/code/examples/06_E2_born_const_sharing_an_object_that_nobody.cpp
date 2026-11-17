#include <iostream>
#include <thread>
#include <vector>

void reader(const int& value, int id)
{
    std::cout << "reader " << id << " sees " << value << '\n';
}

int main()
{
    const int config = 42;   // born const: no thread can legally modify it

    std::vector<std::jthread> readers;
    for (int id = 0; id < 4; ++id) {
        readers.emplace_back(reader, std::cref(config), id);   // many readers, no writer
    }
}   // all readers join before `config` is destroyed
