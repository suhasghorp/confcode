#include <meta>
#include <print>
#include <string>

struct Person {
    std::string name;
    int age;
private:
    double secret = 0.0;
};

int main()
{
    constexpr auto ctx = std::meta::access_context::unchecked();   // include private members

    template for (constexpr auto m :
                  std::define_static_array(std::meta::nonstatic_data_members_of(^^Person, ctx))) {
        std::println("{} : {}", std::meta::identifier_of(m),
                     std::meta::display_string_of(std::meta::type_of(m)));
    }
}
// Build: g++ -std=c++26 -freflection
