task<std::span<char>>
count_packet_data(uint64_t& packets,
                  uint64_t& bytes,
                  task<std::span<char>>& in)
{
  for (;;) {
    auto packet = co_await in;
    ++packets;
    bytes += packet.size();
    co_yield packet;
  }
}
