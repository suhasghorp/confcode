// Baseline for the talk's "kernel crossing tax": ping-pong an 8-byte message
// between two processes over a pipe pair and over a Unix domain socket pair.
// One-way latency = round trip / 2. Pin with: taskset -c 1,2 ./kernel_ipc_latency
#include <sched.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {

void PinTo(int cpu) {
  cpu_set_t set;
  CPU_ZERO(&set);
  CPU_SET(cpu, &set);
  sched_setaffinity(0, sizeof(set), &set);
}

void Must(bool ok, const char* what) {
  if (!ok) { std::perror(what); std::exit(1); }
}

// to_child/from_child: write end and read end used by the parent;
// child_in/child_out: read end and write end used by the child.
void PingPong(const char* label, int to_child, int from_child, int child_in,
              int child_out) {
  constexpr int kWarmup = 10'000, kRounds = 200'000;
  pid_t pid = fork();
  Must(pid >= 0, "fork");
  if (pid == 0) {
    PinTo(2);
    uint64_t msg;
    for (int i = 0; i < kWarmup + kRounds; ++i) {
      Must(read(child_in, &msg, sizeof msg) == sizeof msg, "child read");
      Must(write(child_out, &msg, sizeof msg) == sizeof msg, "child write");
    }
    _exit(0);
  }
  PinTo(1);
  std::vector<double> one_way_ns;
  one_way_ns.reserve(kRounds);
  for (int i = 0; i < kWarmup + kRounds; ++i) {
    uint64_t msg = i;
    auto t0 = std::chrono::steady_clock::now();
    Must(write(to_child, &msg, sizeof msg) == sizeof msg, "write");
    Must(read(from_child, &msg, sizeof msg) == sizeof msg, "read");
    auto t1 = std::chrono::steady_clock::now();
    if (i >= kWarmup) {
      one_way_ns.push_back(std::chrono::duration<double, std::nano>(t1 - t0).count() / 2);
    }
  }
  waitpid(pid, nullptr, 0);
  std::sort(one_way_ns.begin(), one_way_ns.end());
  auto pct = [&](double p) { return one_way_ns[static_cast<size_t>(p * (one_way_ns.size() - 1))]; };
  std::printf("%-22s one-way 8B: p50 %7.0f ns | p99 %7.0f ns | p99.99 %7.0f ns\n", label,
              pct(0.50), pct(0.99), pct(0.9999));
}

}  // namespace

int main() {
  int a[2], b[2];
  Must(pipe(a) == 0 && pipe(b) == 0, "pipe");
  PingPong("pipe", a[1], b[0], a[0], b[1]);

  int sv[2];
  Must(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0, "socketpair");
  PingPong("unix socket (stream)", sv[0], sv[0], sv[1], sv[1]);
}
