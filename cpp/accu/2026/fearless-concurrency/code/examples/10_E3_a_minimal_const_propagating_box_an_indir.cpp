#include <memory>
#include <utility>

template <typename T>
class box {
    std::unique_ptr<T> p_;
public:
    template <typename... Args>
    explicit box(Args&&... args) : p_(std::make_unique<T>(std::forward<Args>(args)...)) {}

    box(const box& other) : p_(std::make_unique<T>(*other.p_)) {}   // deep copy
    box& operator=(const box& other) { *p_ = *other.p_; return *this; }
    box(box&&) noexcept = default;
    box& operator=(box&&) noexcept = default;

    T&       operator*()       { return *p_; }    // mutable access only through non-const box
    const T& operator*() const { return *p_; }    // const box => const T: deep const
};

int main()
{
    const box<int> b(41);
    // *b = 42;          // error, unlike const std::unique_ptr<int>
    box<int> c = b;
    *c = 42;            // c is an independent copy
    return *b;          // 41
}
