#include <pthread.h>
#include <sys/wait.h>
#include <unistd.h>

// write(2) is async-signal-safe, so it is allowed in the child handlers
template <char N> void prepare() { char m[] = {'p','r','e','p','a','r','e',' ',N,'\n'}; if (::write(1, m, sizeof m) < 0) {} }
template <char N> void child()   { char m[] = {'c','h','i','l','d',' ',N,'\n'};         if (::write(1, m, sizeof m) < 0) {} }

int main() {
    ::pthread_atfork(prepare<'1'>, nullptr, child<'1'>);
    ::pthread_atfork(prepare<'2'>, nullptr, child<'2'>);
    ::pthread_atfork(prepare<'3'>, nullptr, child<'3'>);
    pid_t pid = ::fork();
    if (pid == 0) _exit(0);
    ::waitpid(pid, nullptr, 0);
}
