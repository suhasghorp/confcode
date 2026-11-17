#include <cstdio>
#include <pthread.h>
#include <sys/wait.h>
#include <unistd.h>

pid_t naive_current() {
    static pid_t id = ::getpid();          // computed once, copied into children by fork
    return id;
}

pid_t current() {
    static pid_t id = [] {
        ::pthread_atfork(nullptr, nullptr, [] { id = ::getpid(); });   // runs in the child
        return ::getpid();
    }();
    return id;
}

int main() {
    std::printf("parent pid %d\n", static_cast<int>(current()));
    naive_current();                       // make sure both caches are filled before forking
    std::fflush(stdout);
    if (pid_t pid = ::fork(); pid == 0) {
        // printf is not async-signal-safe; this demo is single-threaded, so it is tolerable here
        std::printf("child: real pid %d, naive cache says %d, atfork cache says %d\n",
                    static_cast<int>(::getpid()), static_cast<int>(naive_current()),
                    static_cast<int>(current()));
        return 0;
    } else {
        ::waitpid(pid, nullptr, 0);
    }
}
