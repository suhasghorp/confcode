#include <type_traits>

struct Foo
{
    void operator = (Foo &&) = delete;     // user-declared move assignment
};

static_assert(std::is_default_constructible_v<Foo>);    // still defaulted
static_assert(!std::is_copy_constructible_v<Foo>);      // implicitly deleted
static_assert(!std::is_move_constructible_v<Foo>);      // not declared; falls back to deleted copy
static_assert(!std::is_copy_assignable_v<Foo>);         // implicitly deleted
static_assert(!std::is_move_assignable_v<Foo>);         // explicitly deleted
static_assert(std::is_destructible_v<Foo>);             // still defaulted

int main() {}
