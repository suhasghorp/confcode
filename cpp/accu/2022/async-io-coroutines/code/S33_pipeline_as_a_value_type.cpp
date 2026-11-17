int main()
{
  auto is_odd = [](auto v) { return v & 1;};
  auto incoming = task<int>::make();
  auto &promise = incoming.get_promise();
  auto pipeline = print_all(filter_in(is_odd,
                                      std::move(incoming));
  for (int i = 0; i < 10; ++i) {
    promise.yield_value(i);
  }
}
