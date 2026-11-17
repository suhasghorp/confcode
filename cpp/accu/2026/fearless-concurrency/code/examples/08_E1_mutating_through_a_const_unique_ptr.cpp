#include <memory>
#include <thread>

void bump(const std::unique_ptr<int>& p)
{
    // p.reset();   // error: reset() is non-const -- the pointer itself is protected
    ++*p;           // fine: operator* is const but returns int& -- the pointee is NOT
}

int main()
{
    auto p = std::make_unique<int>(0);
    {
        // Sharing one unique_ptr by const reference with two threads:
        std::jthread a(bump, std::cref(p));
        std::jthread b(bump, std::cref(p));   // data race on *p
    }
}
