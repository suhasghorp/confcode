#include <memory>
#include <type_traits>

int main()
{
    const auto up = std::make_unique<int>(1);
    *up = 2;                                      // compiles: shallow const

    const std::indirect<int> ind(1);
    // *ind = 2;                                  // error: const propagates to the owned int
    static_assert(std::is_same_v<decltype(*ind), const int&>);

    std::indirect<int> copy = ind;               // deep copy: independent heap object
    *copy = 3;                                   // does not affect `ind`
    return *ind;                                 // still 1
}
