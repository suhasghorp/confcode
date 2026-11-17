#include <atomic>
#include <cstdio>
#include <memory>
#include <sys/mman.h>
#include <sys/wait.h>
#include <type_traits>
#include <unistd.h>

template <class T>
struct Atomic {                                       // an aggregate: no constructors at all
    static_assert(std::atomic_ref<T>::is_always_lock_free, "must be address-free across processes");
    alignas(std::atomic_ref<T>::required_alignment) T value_;

    T load(std::memory_order o = std::memory_order_seq_cst) const {
        return std::atomic_ref<T const>(value_).load(o);
    }
    void store(T v, std::memory_order o = std::memory_order_seq_cst) {
        std::atomic_ref<T>(value_).store(v, o);
    }
    T fetch_add(T v, std::memory_order o = std::memory_order_seq_cst) {
        return std::atomic_ref<T>(value_).fetch_add(v, o);
    }
};
static_assert(std::is_implicit_lifetime_v<Atomic<long>>);

int main() {
    void* mem = ::mmap(nullptr, sizeof(Atomic<long>), PROT_READ | PROT_WRITE,
                       MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED) return 1;
    auto* counter = std::start_lifetime_as<Atomic<long>>(mem);   // explicit, and allowed

    pid_t pid = ::fork();
    for (int i = 0; i < 100'000; ++i) counter->fetch_add(1, std::memory_order_relaxed);
    if (pid == 0) return 0;
    ::waitpid(pid, nullptr, 0);
    std::printf("shared counter = %ld (expected 200000)\n", counter->load());
}
