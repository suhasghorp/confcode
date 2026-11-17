#include <cstdio>
#include <sys/wait.h>
#include <unistd.h>

int counter = 1;   // an ordinary global variable

int main() {
    pid_t pid = ::fork();
    if (pid == 0) {                       // child process
        counter = 2;
        std::printf("child sees %d at %p\n", counter, static_cast<void*>(&counter));
        return 0;
    }
    ::waitpid(pid, nullptr, 0);           // parent: wait for the child to finish
    std::printf("parent sees %d at %p\n", counter, static_cast<void*>(&counter));
}
