// Uses task<T> and promise<T> from Listing 20, Listing 21, Listing 23, Listing 24 and Listing 26, and filter_in from Listing 33.
template <typename T>
task<void> print_all(task<T>& values)
{
  for (;;) std::cout << co_await values << '\n';
}

int main()
{
  auto is_odd = [](auto v) { return v & 1; };
  auto incoming = task<int>::make();

  // BUG: filter_in(...) returns a temporary task. print_all keeps a *reference*
  // to it, but the temporary is destroyed at the end of this full-expression,
  // destroying the filter's coroutine frame while print_all still awaits it.
  // auto printer = print_all(filter_in(is_odd, incoming));   // does not even compile:
  //                                  ^ a temporary cannot bind to task<T>&
  // ...but the same bug compiles if print_all takes `const task<T>&`, or if the
  // named variable holding the filter goes out of scope early:
  auto printer = [&] {
    auto odd = filter_in(is_odd, incoming);
    return print_all(odd);         // returns a task that refers to `odd`...
  }();                             // ...which is destroyed here
  // incoming.get_promise().yield_value(1);   // would resume into a dead frame
}
