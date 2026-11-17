task<void> print_lines(std::ostream& os, task<std::string>& in)
{
  for (;;) {
    auto line = co_await in;
    os << ':' << line << ":\n";
  }
}
