class Person
{
public:
    std::string_view name() const;
    Age age() const;
};

using People = std::vector<Person>;

Age average_age(const People& people);
std::string_view name_of_oldest_person(const People& people);
