struct Foo
{
    void operator = (Foo &&) = delete;
};

static_assert(std::is_default_constructible_v<Foo>);
static_assert(std::is_copy_constructible_v<Foo>);
static_assert(std::is_move_constructible_v<Foo>);
static_assert(std::is_copy_assignable_v<Foo>);
static_assert(std::is_move_assignable_v<Foo>);
static_assert(std::is_destructible_v<Foo>);
