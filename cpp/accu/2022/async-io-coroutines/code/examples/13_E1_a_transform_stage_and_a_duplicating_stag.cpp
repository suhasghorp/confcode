// Uses task<T> and promise<T> from Listing 20, Listing 21, Listing 23, Listing 24 and Listing 26 (with the promise<void> specialisation below).
// With the talk's yield_value (returning std::suspend_always) this prints 1 1 9.
template <typename T, typename F>
task<T> transform(F f, task<T>& in)
{
  for (;;) {
    auto v = co_await in;
    co_yield f(v);                 // exactly one out per in
  }
}

template <typename T>
task<T> duplicate(task<T>& in)
{
  for (;;) {
    auto v = co_await in;
    co_yield v;                    // two outs per in
    co_yield v;
  }
}

template <typename T>
task<void> print_each(task<T>& in)
{
  for (;;) {
    std::cout << co_await in << ' ';
  }
}

int main()
{
  auto incoming = task<int>::make();
  auto squared  = transform([](int v) { return v * v; }, incoming);
  auto twice    = duplicate(squared);
  auto printer  = print_each(twice);
  for (int i = 1; i <= 3; ++i) {
    incoming.get_promise().yield_value(i);   // intended: 1 1 4 4 9 9
  }
}
