#include <stdexec/execution.hpp>

// BROKEN: the sender is returned before it runs, so 'count' dangles.
auto bad_counter() {
  int count = 0;                                   // lives in bad_counter's stack frame
  return stdexec::just()
       | stdexec::then([&count] { return ++count; }); // reference to a soon-dead local
}                                                  // count destroyed here; nothing has run yet

// FIXED: lift the state into the asynchronous world and let let_value own it.
auto good_counter() {
  return stdexec::just(0)
       | stdexec::let_value([](int& count) {       // count lives in the operation state
           return stdexec::just()
                | stdexec::then([&count] { return ++count; });
         });
}

int main() {
  // auto [x] = stdexec::sync_wait(bad_counter()).value();  // undefined behaviour!
  auto [y] = stdexec::sync_wait(good_counter()).value();
  return y == 1 ? 0 : 1;
}

