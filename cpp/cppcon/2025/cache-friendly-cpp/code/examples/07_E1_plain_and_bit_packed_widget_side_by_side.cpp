#include <cstdint>
#include <cstdio>

enum class State : std::uint8_t { A, B, C };

struct PlainWidget
{
    bool is_enabled;
    bool is_visible;
    State state;
};

struct Widget
{
    bool is_enabled : 1 = false;   // default member initializers: C++20
    bool is_visible : 1 = false;
    State state : 2 = State::A;
};

static_assert(sizeof(PlainWidget) == 3);
static_assert(sizeof(Widget) == 1);

int main()
{
    Widget w;
    w.is_enabled = true;
    w.state = State::C;   // read-modify-write of one byte
    std::printf("plain: %zu bytes, packed: %zu byte\n", sizeof(PlainWidget), sizeof(Widget));
    std::printf("enabled=%d visible=%d state=%d\n",
                w.is_enabled, w.is_visible, static_cast<int>(w.state));
}
