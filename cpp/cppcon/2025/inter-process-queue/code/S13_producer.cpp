class Writer
{
    template <std::size_t N>
    void write(std::span<std::byte const, N> data) noexcept;

    template <TriviallyCopyableC T>
    T & emplace(auto &&... args) noexcept(
        std::is_nothrow_constructible_v<T, decltype(args)...>)
    requires std::constructible_from<T, decltype(args)...>;

    template <std::size_t Alignment = 1>
    void write(void const * data, std::size_t size) noexcept;
};
