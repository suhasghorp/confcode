int main()
{
  uint64_t num_bytes = 0;
  uint64_t num_packets = 0;
  bool done = false;

  ring r;
  r.add(port4000.fd(),
        init_task<std::span<char>>(count_packet_data(num_packets, num_bytes)
                                   | to_string
                                   | strip_trailing_newline
                                   | concatenate_to(40)
                                   | print_lines(std::cout)
                                   ));
…
