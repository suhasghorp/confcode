template <TriviallyCopyableC T>
T &
Producer::Writer::
emplace(auto &&... args) noexcept(
    std::is_nothrow_constructible_v<T, decltype(args)...>)
requires std::constructible_from<T, decltype(args)...>
{
    auto buffer = info_.prepare_buffer<alignof(T)>(sizeof(T));
    auto t = new (static_cast<void *>(buffer.data()))
        T(std::forward<decltype(args)>(args)...);
    info_.local_counter += sizeof(T);
    return *t;
}
