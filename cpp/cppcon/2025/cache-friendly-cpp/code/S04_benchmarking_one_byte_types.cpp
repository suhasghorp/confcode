template <typename T> requires (sizeof(T) == 1)
void increment(std::vector<T>& data)
{
    for (std::size_t i = 0; i < data.size(); ++i)
        data[i] = static_cast<T>(int(data[i]) + 1);
}
