#include "task.hpp"

template <typename T, str name>
task<void, "print_all"> print_all(task<T, name>& values)
{
  for (;;) {
    auto v = co_await values;
    std::cout << v << '\n' << std::flush;
  }
}

template <typename T, str name, typename P>
task<T, "filter_in"> filter_in(P predicate, task<T, name>& in)
{
  for (;;) {
    auto v = co_await in;
    if (predicate(v)) {
      co_yield v;
    }
  }
}


int main()
{
  auto incoming = task<int, "incoming">::make();
  auto is_odd = [](auto v) { return v & 1;};
  auto odd_values = filter_in(is_odd, incoming);
  auto printer = print_all(odd_values);
  for (int i = 0; i < 10; ++i) {
    incoming.get_promise().yield_value(i);
  }
}
