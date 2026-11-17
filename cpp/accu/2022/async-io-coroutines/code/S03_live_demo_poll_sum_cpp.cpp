int main()
{
  auto port4000 = udp_socket("127.0.0.1", 4000);
  auto port4001 = udp_socket("127.0.0.1", 4001);

  uint64_t packets = 0;
  uint64_t sum = 0;
  bool done = false;

  auto calc_sum = [&](std::span<char> data) {
    ++packets;
    for (auto c : data) {
      sum += c;
    }
  };
  auto print_and_exit = [&](std::span<char>) {
    std::cout << "packets=" << packets << " sum=" << sum << '\n';
    done = true;
  };
  poller poll;
  poll.add(port4000.fd(), calc_sum);
  poll.add(port4001.fd(), print_and_exit);
  while (!done) {
    poll.wait();
  }
}
