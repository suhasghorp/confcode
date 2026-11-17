task<std::string>
strip_trailing_newline(task<std::string>& in)
{
  for (;;) {
    auto s = co_await in;
    while (s.ends_with("\n")) {
      s.resize(s.length() - 1);
    }
    co_yield s;
  }
}
