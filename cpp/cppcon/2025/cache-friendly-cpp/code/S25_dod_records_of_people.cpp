Age average_age(const std::vector<Age>& ages);

std::size_t index_of_oldest_person(const std::vector<Age>& ages);
std::string_view name_of_oldest_person(
    const std::vector<std::string_view>& names,
    const std::vector<Age>& ages
)
{
    auto index = index_of_oldest_person(ages);
    return names[index];
}
