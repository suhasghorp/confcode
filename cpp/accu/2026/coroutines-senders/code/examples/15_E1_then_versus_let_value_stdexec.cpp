#include <stdexec/execution.hpp>
#include <cstdio>
#include <string>
#include <utility>

int main() {
  // then: synchronously transform the incoming value into a new value.
  auto len1 = stdexec::just(std::string{"hello"})
            | stdexec::then([](std::string s) { return s.size(); });

  // let_value: the incoming value is stored in the operation state,
  // the function gets an lvalue reference to it and must return a SENDER.
  auto len2 = stdexec::just(std::string{"hello"})
            | stdexec::let_value([](std::string& s) {
                // s stays alive until this inner sender completes
                return stdexec::just()
                     | stdexec::then([&s] { return s.size(); });
              });

  auto [a] = stdexec::sync_wait(std::move(len1)).value();
  auto [b] = stdexec::sync_wait(std::move(len2)).value();
  std::printf("%zu %zu\n", a, b);  // prints: 5 5
}

