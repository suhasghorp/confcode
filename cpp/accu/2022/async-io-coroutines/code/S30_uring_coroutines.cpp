...
  auto print_and_exit = [&](auto&&) {
    std::cout << "packets=" << num_packets << " bytes=" << num_bytes << '\n';
    done = true;
    return false;
  };

  ring r;
  auto port4000 = udp_socket("127.0.0.1", 4000);
  auto port4001 = udp_socket("127.0.0.1", 4001);
  r.add(port4000.fd(), to_promise(in_4000.get_promise()));
  r.add(port4001.fd(), print_and_exit);
  while (!done) {
    r.wait();
  }
}
