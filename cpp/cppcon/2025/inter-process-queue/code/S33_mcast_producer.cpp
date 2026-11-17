template <std::size_t Alignment>
void
Producer::Writer::
write(void const * data, std::size_t size) noexcept
{
    auto buffer = info_.prepare_buffer<Alignment>(size);
    std::memcpy(buffer.data(), data, size);
    info_.local_counter += size;
}
