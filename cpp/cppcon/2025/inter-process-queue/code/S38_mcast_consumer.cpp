inline tl::expected<std::size_t, std::int_fast64_t>
Consumer::
try_read(std::span<std::byte> data)
{
    auto read_counter = aligned<Alignment::Int64>(info_.local_counter);
    std::uint32_t size;
    if (read_counter + sizeof(size) >= info_.cached_counter) {
        info_.cached_counter = info_.shared->read_counter.load(
            std::memory_order_acquire);
        if (read_counter + sizeof(size) > info_.cached_counter) {
            return tl::unexpected(no_data);
        }
    }
    auto const buf = info_.buffer + (read_counter & info_.mask);
    std::memcpy(&size, buf, sizeof(size));
