#include <algorithm>
#include <chrono>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

enum class Age : short {};

namespace aos {
struct Person { std::string name; Age age; };
using People = std::vector<Person>;

Age average_age(const People& people)
{
    long long total = 0;
    for (const Person& p : people) total += static_cast<int>(p.age);
    return static_cast<Age>(total / static_cast<long long>(people.size()));
}
}

namespace soa {
enum class Person : std::size_t {};
struct People { std::vector<std::string> names; std::vector<Age> ages; };

Age average_age(const std::vector<Age>& ages)
{
    long long total = 0;
    for (Age age : ages) total += static_cast<int>(age);
    return static_cast<Age>(total / static_cast<long long>(ages.size()));
}

Person index_of_oldest_person(const std::vector<Age>& ages)
{
    auto it = std::ranges::max_element(ages);
    return static_cast<Person>(it - ages.begin());
}

const std::string& name_of_oldest_person(const std::vector<std::string>& names,
                                         const std::vector<Age>& ages)
{
    auto index = index_of_oldest_person(ages);
    return names[static_cast<std::size_t>(index)];
}
}

template <typename F>
void measure(const char* name, F f)
{
    auto start = std::chrono::steady_clock::now();
    int result = static_cast<int>(f());
    auto stop = std::chrono::steady_clock::now();
    std::printf("%s: average %d in %.2f ms\n", name, result,
                std::chrono::duration<double, std::milli>(stop - start).count());
}

int main()
{
    constexpr std::size_t n = 1'000'000;
    std::mt19937 engine(11);
    std::uniform_int_distribution<int> dist(0, 100);

    aos::People rows;
    soa::People columns;
    for (std::size_t i = 0; i < n; ++i) {
        std::string name = "person #" + std::to_string(i);
        Age age = static_cast<Age>(dist(engine));
        rows.push_back({name, age});
        columns.names.push_back(name);
        columns.ages.push_back(age);
    }

    measure("AoS", [&] { return aos::average_age(rows); });
    measure("SoA", [&] { return soa::average_age(columns.ages); });
    std::printf("oldest: %s\n", soa::name_of_oldest_person(columns.names, columns.ages).c_str());
}
