#include <stdexec/execution.hpp>
#include <exec/repeat_effect_until.hpp>   // library extension, not in C++26
#include <cstdio>
#include <utility>

int main() {
  auto countdown =
      stdexec::just(3)
    | stdexec::let_value([](int& n) {      // n owned by let_value's operation state
        return exec::repeat_effect_until(
            stdexec::just()
          | stdexec::then([&n] {
              std::printf("tick %d\n", n);
              return --n == 0;             // true => stop repeating
            }));
      });

  stdexec::sync_wait(std::move(countdown)); // prints tick 3, tick 2, tick 1
}

