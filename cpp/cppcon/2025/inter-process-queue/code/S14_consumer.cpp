static constexpr std::uint64_t no_data = 0;
static constexpr std::uint64_t lapped = -1;

tl::expected<std::size_t, std::uint64_t> try_read(
    std::span<std::byte> data);
