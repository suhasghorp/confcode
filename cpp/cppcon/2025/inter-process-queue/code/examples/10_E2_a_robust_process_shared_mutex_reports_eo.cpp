#include <cerrno>
#include <cstdio>
#include <new>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

int main() {
    void* mem = ::mmap(nullptr, sizeof(pthread_mutex_t), PROT_READ | PROT_WRITE,
                       MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    auto* m = static_cast<pthread_mutex_t*>(mem);
    pthread_mutexattr_t attr;
    ::pthread_mutexattr_init(&attr);
    ::pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    ::pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST);
    ::pthread_mutex_init(m, &attr);

    if (::fork() == 0) {                 // child: take the lock and die without unlocking
        ::pthread_mutex_lock(m);
        _exit(1);
    }
    ::wait(nullptr);

    int rc = ::pthread_mutex_lock(m);
    if (rc == EOWNERDEAD) {
        std::puts("previous owner died; repairing state and marking the mutex consistent");
        ::pthread_mutex_consistent(m);
    }
    ::pthread_mutex_unlock(m);
    std::printf("lock again: %s\n", ::pthread_mutex_lock(m) == 0 ? "ok" : "failed");
    ::pthread_mutex_unlock(m);
}
