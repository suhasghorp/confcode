template <typename T> requires (sizeof(T) == 1)
void increment(std::vector<T>& data)
{
    for (auto& value : data)
        value = static_cast<T>(int(value) + 1);
}
