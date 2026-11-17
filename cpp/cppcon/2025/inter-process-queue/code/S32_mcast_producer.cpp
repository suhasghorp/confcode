template <std::size_t N>
void
Producer::Writer::
write(std::span<std::byte const, N> data) noexcept
{
    auto const size = std::uint32_t(data.size());
    assert(size == data.size());
    auto buffer = info_.prepare_buffer<8>(sizeof(std::uint64_t) + size);
    std::memcpy(buffer.data(), &size, sizeof(size));
    std::memcpy(
        buffer.data() + sizeof(std::uint64_t), data.data(), size);
    info_.local_counter += buffer.size();
}
