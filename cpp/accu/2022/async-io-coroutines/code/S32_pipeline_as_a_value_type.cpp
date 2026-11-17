template <typename T>
task<void> print_all(task<T> in)
{
  for (;;) {
    auto v = co_await in;
    std::cout << v << '\n';
  }
}
