enum class Age : int {};

Age average_age(const std::vector<Age>& ages)
{
    int total = 0;
    for (Age age : ages) {
        total += static_cast<int>(age);
    }
    return static_cast<Age>(total / ages.size());
}
