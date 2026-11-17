int main()
{
  auto is_odd = [](auto v) { return v & 1;};
  auto incoming = task<int>::make();
  auto &promise = incoming.get_promise();
  auto to_cout = print_all(std::cout);
  auto pipeline = to_cout(filter_in(is_odd));
  auto coro = pipeline(std::move(incoming));
  for (int i = 0; i < 10; ++i) {
    promise.yield_value(i);
  }
}
