#include <map>
#include <memory>
#include <print>
#include <string>
#include <thread>

int main()
{
    // Build the data, then freeze it.
    auto table = std::make_shared<const std::map<std::string, int>>(
        std::map<std::string, int>{{"one", 1}, {"two", 2}});

    std::jthread a([table] { std::println("{}", table->at("one")); });
    std::jthread b([table] { std::println("{}", table->at("two")); });
    // table->at("one") = 5;   // error: the map is const
}
