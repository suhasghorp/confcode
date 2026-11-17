int main()
{
  uint64_t num_bytes = 0;
  uint64_t num_packets = 0;
  bool done = false;

  auto in_4000 = task<std::span<char>>::make();
  auto counted_packets = count_packet_data(num_packets, num_bytes, in_4000);
  auto strings = to_string(counted_packets);
  auto stripped_strings = strip_trailing_newline(strings);
  auto lines = concatenate_to<40>(stripped_strings);
  auto print = print_lines(std::cout, lines);
…
