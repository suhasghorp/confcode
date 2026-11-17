// Uses the sendable machinery from Listing 27, Listing 29, Listing 32 and Listing 33, and spawn_thread from Listing 36.
#include <numeric>
#include <optional>
#include <print>
#include <span>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

template <typename Fn1, typename Fn2>
    requires sendable<std::decay_t<Fn1>> && sendable<std::decay_t<Fn2>>
auto fork_results(Fn1&& fn1, Fn2&& fn2)
{
    using R2 = std::invoke_result_t<std::decay_t<Fn2>&>;
    std::optional<R2> r2;
    {
        // The worker writes into r2, which only this function can see,
        // and the join below happens before anyone reads it.
        std::jthread worker([&r2, fn = std::decay_t<Fn2>(std::forward<Fn2>(fn2))]() mutable { r2.emplace(fn()); });
        auto r1 = auto{fn1}();
        worker.join();
        return std::pair{std::move(r1), std::move(*r2)};
    }
}

struct sum_of {
    std::span<const int> data;
    long operator()() const { return std::accumulate(data.begin(), data.end(), 0L); }
};

template <> constexpr bool is_sendable_v<std::vector<int>> = true;

int main()
{
    const std::vector<int> v{1, 2, 3, 4, 5, 6, 7, 8};
    std::span<const int> all(v);
    auto [left, right] = fork_results(sum_of{all.first(4)}, sum_of{all.last(4)});
    std::println("{} + {} = {}", left, right, left + right);
}
