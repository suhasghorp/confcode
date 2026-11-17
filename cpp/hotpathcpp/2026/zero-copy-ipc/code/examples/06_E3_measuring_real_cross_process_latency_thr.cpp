// Real inter-process latency through BroadcastRingBuffer: the producer process
// (CPU 1) stamps each 8-byte message with __rdtsc(); a separate consumer process
// (CPU 2) attaches to the same shared memory, polls, and measures the delay.
// The TSC is invariant and synchronized across cores on this CPU.
#include <sched.h>
#include <sys/wait.h>
#include <unistd.h>
#include <x86intrin.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include "common/shared_memory/broadcast_ring_buffer.h"

using cognitas::trading::BroadcastReader;
using cognitas::trading::BroadcastRingBuffer;
using cognitas::trading::FrameHeader;

namespace {

void PinTo(int cpu) {
  cpu_set_t set;
  CPU_ZERO(&set);
  CPU_SET(cpu, &set);
  sched_setaffinity(0, sizeof(set), &set);
}

double TscPerNs() {
  auto t0 = std::chrono::steady_clock::now();
  uint64_t c0 = __rdtsc();
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  uint64_t c1 = __rdtsc();
  auto t1 = std::chrono::steady_clock::now();
  return double(c1 - c0) / std::chrono::duration<double, std::nano>(t1 - t0).count();
}

constexpr int kWarmup = 20'000, kMessages = 200'000;

}  // namespace

int main() {
  const std::string name = "/zc_xproc_" + std::to_string(getpid());
  BroadcastRingBuffer::Unlink(name).IgnoreError();
  const size_t capacity = 16 << 20;  // 16 MiB, page aligned
  auto producer = BroadcastRingBuffer::Create(name, capacity);
  if (!producer.ok()) { std::fprintf(stderr, "%s\n", producer.status().ToString().c_str()); return 1; }

  int ready[2];
  if (pipe(ready) != 0) return 1;

  pid_t pid = fork();
  if (pid == 0) {  // consumer process
    PinTo(2);
    auto consumer = BroadcastRingBuffer::Attach(name);
    BroadcastReader reader;
    if (!consumer.ok() || !(*consumer)->RegisterReader(reader).ok()) _exit(1);
    char go = 1;
    if (write(ready[1], &go, 1) != 1) _exit(1);  // tell the producer we are registered

    std::vector<uint64_t> cycles;
    cycles.reserve(kMessages);
    for (int received = 0; received < kWarmup + kMessages;) {
      FrameHeader header;
      auto span = (*consumer)->PollZeroCopy(reader, header);
      if (!span.ok()) { _mm_pause(); continue; }  // nothing new yet
      uint64_t sent;
      std::memcpy(&sent, span->data(), sizeof sent);
      uint64_t now = __rdtsc();
      (*consumer)->Advance(reader, header.TotalFrameSize());
      if (received++ >= kWarmup) cycles.push_back(now - sent);
    }
    std::sort(cycles.begin(), cycles.end());
    double per_ns = TscPerNs();
    auto pct = [&](double p) { return cycles[size_t(p * (cycles.size() - 1))] / per_ns; };
    std::printf("ring buffer, process->process one-way 8B: p50 %.0f ns | p99 %.0f ns | p99.99 %.0f ns\n",
                pct(0.50), pct(0.99), pct(0.9999));
    std::fflush(stdout);  // _exit() does not flush stdio buffers
    (*consumer)->UnregisterReader(reader);
    _exit(0);
  }

  PinTo(1);
  char go;
  if (read(ready[0], &go, 1) != 1) return 1;
  for (int i = 0; i < kWarmup + kMessages; ++i) {
    // Pace the producer (~2 us apart) so we measure latency, not queueing.
    uint64_t until = __rdtsc() + 6000;
    while (__rdtsc() < until) _mm_pause();
    auto buf = (*producer)->Reserve(sizeof(uint64_t), 1);
    if (!buf.ok()) { --i; continue; }
    uint64_t stamp = __rdtsc();
    std::memcpy(*buf, &stamp, sizeof stamp);
    (*producer)->Commit();
  }
  waitpid(pid, nullptr, 0);
  BroadcastRingBuffer::Unlink(name).IgnoreError();
}
