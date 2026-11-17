#include <array>
#include <cassert>
#include <list>
#include <vector>

struct read_work { int fd; std::array<char, 1500> buffer; };

int main()
{
    // With a vector, growing may move every element:
    std::vector<read_work> v;
    v.push_back({3, {}});
    char* kernel_target = v[0].buffer.data();   // what we would hand to io_uring_prep_read
    v.push_back({4, {}});                       // may reallocate...
    bool moved = kernel_target != v[0].buffer.data();
    (void)moved;                                // ...and the kernel would write into freed memory

    // With a list, element addresses are stable for the element's lifetime:
    std::list<read_work> l;
    auto& w = l.emplace_back(read_work{3, {}});
    char* stable = w.buffer.data();
    for (int i = 0; i < 100; ++i) l.emplace_back(read_work{i, {}});
    assert(stable == l.front().buffer.data());
}
