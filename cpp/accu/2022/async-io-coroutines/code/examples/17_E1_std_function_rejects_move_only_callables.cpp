#include <functional>
#include <memory>
#include <span>

int main()
{
    auto state = std::make_unique<int>(0);
    auto callback = [s = std::move(state)](std::span<char> data) mutable {
        *s += static_cast<int>(data.size());
        return true;
    };

    // std::function<bool(std::span<char>)> f = std::move(callback);   // error: lambda is not copyable
    std::move_only_function<bool(std::span<char>)> g = std::move(callback);   // OK (C++23)

    char buf[4]{};
    return g(buf) ? 0 : 1;
}
