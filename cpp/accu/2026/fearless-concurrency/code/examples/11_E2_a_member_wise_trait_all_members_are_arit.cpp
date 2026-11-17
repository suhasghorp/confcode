#include <meta>
#include <type_traits>

template <typename T>
consteval bool all_members_arithmetic()
{
    constexpr auto ctx = std::meta::access_context::unchecked();
    template for (constexpr auto m :
                  std::define_static_array(std::meta::nonstatic_data_members_of(^^T, ctx))) {
        if constexpr (not std::is_arithmetic_v<typename[: std::meta::type_of(m) :]>) {
            return false;
        }
    }
    return true;
}

struct point { int x; int y; };
struct named_point { const char* name; int x; int y; };

static_assert(all_members_arithmetic<point>());
static_assert(not all_members_arithmetic<named_point>());

int main() {}
