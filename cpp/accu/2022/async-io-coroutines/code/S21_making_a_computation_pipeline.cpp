int main()
{
  auto is_odd = [](auto v) { return v & 1;};
  auto incoming = task<int>::make();
  auto odd_values = filter_in(is_odd, incoming);
  auto printer = print_all(odd_values);
  for (int i = 0; i < 10; ++i) {
    incoming.get_promise().yield_value(i);
  }
}
