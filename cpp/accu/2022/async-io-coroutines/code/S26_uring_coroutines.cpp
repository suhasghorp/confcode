task<std::string> to_string(task<std::span<char>>& in)
{
  for (;;) {
    auto packet = co_await in;
    co_yield std::string{packet.begin(), packet.end()};
  }
}
