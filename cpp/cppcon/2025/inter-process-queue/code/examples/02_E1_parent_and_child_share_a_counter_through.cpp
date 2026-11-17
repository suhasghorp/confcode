#include <atomic>
#include <cstdio>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

int main() {
    void* mem = ::mmap(nullptr, sizeof(long), PROT_READ | PROT_WRITE,
                       MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED) { std::perror("mmap"); return 1; }
    long* counter = static_cast<long*>(mem);   // zero-filled by the kernel

    pid_t pid = ::fork();
    for (int i = 0; i < 100'000; ++i)          // both processes run this loop
        std::atomic_ref<long>(*counter).fetch_add(1, std::memory_order_relaxed);
    if (pid == 0) return 0;                    // child is done

    ::waitpid(pid, nullptr, 0);
    std::printf("counter = %ld (expected 200000)\n",
                std::atomic_ref<long>(*counter).load());
    ::munmap(mem, sizeof(long));
}
