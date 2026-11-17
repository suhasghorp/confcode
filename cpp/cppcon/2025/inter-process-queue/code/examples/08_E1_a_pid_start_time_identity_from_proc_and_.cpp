#include <cstdio>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

struct ProcessIdentity {
    pid_t pid;
    unsigned long long start_ticks;      // field 22 of /proc/<pid>/stat
    bool operator==(ProcessIdentity const&) const = default;
};

std::optional<ProcessIdentity> identity_of(pid_t pid) {
    std::ifstream in("/proc/" + std::to_string(pid) + "/stat");
    std::string line;
    if (!std::getline(in, line)) return std::nullopt;        // no such process
    auto close = line.rfind(')');                              // field 2 (comm) may contain spaces
    std::istringstream rest(line.substr(close + 2));           // starts at field 3 (state)
    std::string field;
    for (int i = 3; i < 22; ++i) rest >> field;                // skip fields 3..21
    ProcessIdentity id{pid, 0};
    if (!(rest >> id.start_ticks)) return std::nullopt;
    return id;
}

bool is_alive(ProcessIdentity const& owner) {
    auto now = identity_of(owner.pid);          // who has this PID today?
    return now && *now == owner;                // same PID *and* same start time
}

int main() {
    auto me = *identity_of(::getpid());
    std::printf("me: pid %d start %llu -> %s\n", me.pid, me.start_ticks, is_alive(me) ? "alive" : "dead");

    pid_t child = ::fork();
    if (child == 0) { ::pause(); _exit(0); }                   // child waits to be killed
    ::usleep(10'000);
    auto c = *identity_of(child);
    std::printf("child: pid %d start %llu -> %s while running\n", c.pid, c.start_ticks,
                is_alive(c) ? "alive" : "dead");
    ::kill(child, SIGKILL);                                    // simulate a crash
    ::waitpid(child, nullptr, 0);
    std::printf("child after exit: %s\n", is_alive(c) ? "alive" : "dead");
}
