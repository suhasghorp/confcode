#include <span>
#include <vector>

void worker(const std::span<int> view)   // const span ...
{
    view[0] = 99;                 // ... still allows writing the elements
    std::span<int> copy = view;   // and a copy drops even the top-level const
    copy[1] = 100;
}

int main()
{
    std::vector<int> data{1, 2, 3};
    worker(data);
    // data is now {99, 100, 3}: shallow const protected nothing.
}
