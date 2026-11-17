template <typename T> requires (sizeof(T) == 1)
void increment(std::vector<T>& data)
{
    auto size = data.size();
    for (std::size_t i = 0; i < size; ++i)
        data[i] = static_cast<T>(int(value) + 1);
}
