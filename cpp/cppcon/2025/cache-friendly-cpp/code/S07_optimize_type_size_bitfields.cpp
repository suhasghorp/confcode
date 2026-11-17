enum class State : uint8_t
{
    A, B, C
};

struct Widget
{
    bool is_enabled : 1;
    bool is_visible : 1;
    State state : 2;
};
