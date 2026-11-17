template <typename T, str name>
task<void, "printer"> print_all(task<T,name>& values)
{
  trace("print_all");
  for (;;) {
    auto v = co_await values;
    std::cout << v << '\n' << std::flush;
  }
  trace("print_all done");
}


int main()
{
  auto incoming = task<int,"incoming">::make();
  auto h = print_all(incoming);
  auto& promise = incoming.get_promise();
  promise.yield_value(4);
  promise.yield_value(2);
}
