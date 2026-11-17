// Uses task<T> and promise<T> from Listing 20, Listing 21, Listing 23, Listing 24 and Listing 26,
// with yield_value returning std::suspend_never (see the pipelines section).
#include <sstream>

task<std::string> split_words(task<std::string>& in)
{
  for (;;) {
    auto line = co_await in;
    std::istringstream words(line);
    for (std::string w; words >> w; ) {
      co_yield w;                 // zero or more outputs per input
    }
  }
}

template <typename T>
task<void> print_each(task<T>& in)
{
  for (;;) std::cout << '[' << co_await in << ']';
}

int main()
{
  auto incoming = task<std::string>::make();
  auto words    = split_words(incoming);
  auto printer  = print_each(words);
  incoming.get_promise().yield_value(std::string("hello coroutine world"));
  std::cout << '\n';             // [hello][coroutine][world]
}
