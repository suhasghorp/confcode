int main()
{
  auto port4000 = udp_socket("127.0.0.1", 4000);
  auto port4001 = udp_socket("127.0.0.1", 4001);

  uint64_t sum = 0;
  uint64_t packets = 0;
  bool done = false;

  auto calc_sum = [&](std::span<char> data) {
    for (auto c : data) {
      sum += c;
    }
    ++packets;
    return true;
  };
  auto print_and_exit = [&](auto&&) {
    std::cout << "packets=" << packets << " sum=" << sum << '\n';
    done = true;
    return false;
  };
  ring r;
  r.add(port4000.fd(), calc_sum);
  r.add(port4000.fd(), calc_sum);
  r.add(port4000.fd(), calc_sum);
  r.add(port4000.fd(), calc_sum);
  r.add(port4001.fd(), print_and_exit);
  while (!done) {
    r.wait();
  }
}
