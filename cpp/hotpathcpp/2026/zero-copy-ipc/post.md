---
title: "Zero-Copy IPC in C++20: A Shared-Memory Broadcast Ring Buffer, Measured"
date: 2026-11-17
tags: ["cpp", "cpp20", "ipc", "shared-memory", "lock-free", "linux", "mmap", "performance", "perf", "benchmarking"]
source: "https://www.youtube.com/watch?v=zkjz_v9E-Qg"
video_title: "Zero-Copy IPC at 286 GiB/s in C++20 (The Linux MMU Magic Mirror Hack!) ⚡"
channel: "Hot Path C++"
---

> This chapter is based on the talk [Zero-Copy IPC at 286 GiB/s in C++20 (The Linux MMU Magic Mirror Hack!) ⚡](https://www.youtube.com/watch?v=zkjz_v9E-Qg) (Hot Path C++). Code listings marked *from the talk* are transcribed from the slides; listings marked *illustrative* were written for this chapter to explain the ideas.

How fast can one process hand data to several others on the same machine? The usual answers, pipes and Unix domain sockets, go through the kernel for every message. This chapter follows an episode of the *Hot Path C++* YouTube series, "Zero-Copy IPC at 286 GiB/s in C++20", which builds a single-producer, multi-consumer (SPMC) broadcast ring buffer on POSIX shared memory and never enters the kernel on the hot path.

The design packs a remarkable number of systems ideas into a few hundred lines: two shared-memory segments with different permissions, a "magic mirror" double mapping that lets the MMU handle wraparound, cache-line padding against false sharing, a sequence lock, release fences, backpressure with heartbeat eviction, and a generation counter against ABA. We explain each one from first principles and walk through every listing from the video.

The code is open source, so we also did what the video invites: we built it (with CMake instead of Bazel), ran its tests and benchmark, ran the exact `perf stat` command from the video, and added a few measurements of our own. Some of the video's claims hold up very well. Others turn out to mean something different from what the narration says.

## What you will learn

- Explain what a system call and a context switch cost, and why shared memory avoids them.
- Set up POSIX shared memory with shm_open, ftruncate and mmap, and choose permissions per segment.
- Build a 'magic mirror' ring buffer by mapping the same pages twice with MAP_FIXED.
- Recognise false sharing, compute struct layouts with padding, and detect false sharing with perf c2c.
- Implement a sequence lock and explain the acquire/release orderings it needs.
- Design backpressure for a broadcast queue and handle dead consumers and the ABA problem.
- Read Google Benchmark output and perf stat counters critically, including what a benchmark does and does not measure.

## Before you start

- Comfortable C++ (classes, templates, std::atomic basics).
- Basic Linux: processes, file descriptors, virtual memory at a conceptual level.
- Helpful: a first look at std::memory_order.

## Contents

1. [Background: Processes, the Kernel, and the Cost of Talking](#1-background-processes-the-kernel-and-the-cost-of-talking) — [0:00](https://www.youtube.com/watch?v=zkjz_v9E-Qg&t=0s)
2. [Two Segments and a Magic Mirror](#2-two-segments-and-a-magic-mirror) — [1:09](https://www.youtube.com/watch?v=zkjz_v9E-Qg&t=69s)
3. [False Sharing and the Padding Moat](#3-false-sharing-and-the-padding-moat) — [3:27](https://www.youtube.com/watch?v=zkjz_v9E-Qg&t=207s)
4. [Sequence Locks and Release Fences](#4-sequence-locks-and-release-fences) — [3:57](https://www.youtube.com/watch?v=zkjz_v9E-Qg&t=237s)
5. [Backpressure, Dead Consumers and the ABA Trap](#5-backpressure-dead-consumers-and-the-aba-trap) — [4:52](https://www.youtube.com/watch?v=zkjz_v9E-Qg&t=292s)
6. [Benchmarks, perf stat, and What the Numbers Mean](#6-benchmarks-perf-stat-and-what-the-numbers-mean) — [6:13](https://www.youtube.com/watch?v=zkjz_v9E-Qg&t=373s)

## 1. Background: Processes, the Kernel, and the Cost of Talking

*Talk segment: [0:00](https://www.youtube.com/watch?v=zkjz_v9E-Qg&t=0s)*

### The problem: one writer, many readers, no time

The video opens with a scenario from trading systems. One CPU core receives market data (every price change on an exchange) and must hand each message to several other programs at once: a risk engine, an order router, perhaps a machine-learning model. Each of those runs as its own *process*, with its own memory. Messages arrive in bursts, and every microsecond of delay means trading on stale prices.

This shape of problem has a name: **single-producer, multi-consumer (SPMC) broadcast**. *Single producer*: one writer. *Multi-consumer*: several readers. *Broadcast*: every reader sees every message, unlike a work queue, where each message goes to exactly one worker.

The speaker's opening claim is bold: a C++20 SPMC broadcast ring buffer "that sustains over 280 GB/s with zero kernel crossings". The first slide's banner reads "Throughput: 286.9 GiB/s | Message latency: 6.1 ns | Kernel context switches: zero". We will spend the last section of this chapter on what those two numbers actually measure. First we need to understand why the obvious tools are slow.

### Processes and the kernel

On Linux, each process has its own **virtual address space**: the address `0x7000_0000` in one process has nothing to do with the same address in another. That isolation is what stops a bug in one program from corrupting another. It also means two processes cannot simply pass each other a pointer.

The traditional ways to move bytes between processes, **pipes** and **Unix domain sockets**, go through the operating system kernel:

1. The sender calls `write` or `send`. This is a **system call**: the CPU switches from user mode (the slide calls it "Ring 3") into kernel mode ("Ring 0").
2. The kernel copies the bytes from the sender's memory into a kernel buffer (for sockets, an `sk_buff`).
3. If the receiver is blocked waiting, the kernel wakes it up, which may mean a **context switch**: saving one thread's registers and restoring another's, possibly on another core.
4. The receiver calls `read` or `recv`, another system call, and the kernel copies the bytes into the receiver's memory.

Each step is cheap by human standards and expensive by CPU standards. The speaker summarises: the packet "sits in a kernel socket buffer before being copied back out to user space. That kernel context switch costs over 1,200 ns in TLB flushes and register saves." The slide itemises the bill:

| Step | Slide's cost |
|---|---|
| Syscall entry / trap | +250 ns |
| Kernel `sk_buff` double memcpy | +400 ns |
| Context switch & TLB shootdown | +590 ns |
| **Total kernel penalty** | **~1,240 ns per message** |

(A **TLB** is the CPU's cache of virtual-to-physical address translations; switching between processes can force entries to be discarded.) Treat the breakdown as illustrative rather than measured; the slide does not say how it was obtained, and the real cost varies a lot with the CPU, the kernel version and whether the receiver is spinning or sleeping. We measured it ourselves in the last section: on our laptop an 8-byte pipe message took about 3.1 µs one way.

The slide sketches the benchmark for this kernel path:

**Listing 1** — code from the talk (Anatomy of a Kernel Crossing), shown at [0:45](https://www.youtube.com/watch?v=zkjz_v9E-Qg&t=45s)

```cpp
// The Cost of Kernel Indirection (Unix Domain Sockets / Pipes)
void BenchmarkUnixSocketTransfer(benchmark::State& state) {
  for (auto _ : state) {
    // Syscall 1: Trap from User Space (Ring 3) into Kernel (Ring 0)
    ssize_t sent = send(socket_fd, payload.data(), payload.size(), 0);
    // Kernel copies data into sk_buff queue; CPU flushes TLB on context switch

    // Syscall 2: Kernel -> User space transition
    ssize_t recvd = recv(client_fd, buffer.data(), buffer.size(), 0);
    // Result: ~1,240 ns latency tax, store buffer stalls, cache thrashing!
  }
}
```

- `BenchmarkUnixSocketTransfer` follows the Google Benchmark convention: `benchmark::State& state` drives a timed loop, `for (auto _ : state)`.
- `send(socket_fd, ...)` is "Syscall 1": a trap from user space into the kernel, which copies the payload into its socket queue.
- `recv(client_fd, ...)` is "Syscall 2": back across the boundary to copy the data out.
- The final comment states the result the speaker attributes to this path: "~1,240 ns latency tax, store buffer stalls, cache thrashing!"

This listing is a sketch: `socket_fd`, `client_fd`, `payload` and `buffer` are not declared, and the file it names (`benchmark/traditional_ipc_bottleneck.cc`) is not in the video's repository. The speaker's verdict on the approach is characteristically blunt: "Kernel crossings are fantastic if your goal is hitting the server chassis while watching packet drops accumulate."

### Shared memory: let the processes share pages

The alternative is to keep the kernel out of the data path entirely. Virtual memory works by mapping pages of virtual addresses onto physical memory, and nothing stops the kernel from mapping *the same physical pages* into two processes. That is **shared memory**. Once it is set up, a store by one process is visible to the other through ordinary loads: no system call, no copy, no context switch. The kernel is involved only once, at setup.

POSIX provides a portable API for this:

- `shm_open(name, flags, mode)` creates or opens a named shared-memory object. On Linux it is a file in the `/dev/shm` memory filesystem, which is how other processes find it by name.
- `ftruncate(fd, size)` sets its size.
- `mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)` maps it into the caller's address space. `MAP_SHARED` means writes go to the shared object rather than to a private copy.

Here is the smallest complete program we could write that uses it, our own example rather than the video's:

**Listing 2** — Two processes sharing one page through POSIX shared memory *(illustrative example, not from the talk)*

```cpp
// A parent process writes into POSIX shared memory; a child process reads it.
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <new>

struct Mailbox {
  std::atomic<bool> ready{false};
  char text[56];
};

int main() {
  const char* name = "/confcode_hello";
  int fd = shm_open(name, O_CREAT | O_RDWR, 0600);   // creates /dev/shm/confcode_hello
  if (fd < 0 || ftruncate(fd, sizeof(Mailbox)) != 0) { std::perror("shm"); return 1; }
  void* p = mmap(nullptr, sizeof(Mailbox), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (p == MAP_FAILED) { std::perror("mmap"); return 1; }
  auto* box = new (p) Mailbox;  // construct the object in the shared pages

  if (fork() == 0) {            // child: same mapping, different process
    while (!box->ready.load(std::memory_order_acquire)) {}
    std::printf("child %d read: %s\n", getpid(), box->text);
    std::fflush(stdout);        // _exit() skips stdio flushing
    _exit(0);
  }
  std::strcpy(box->text, "hello through shared memory");
  box->ready.store(true, std::memory_order_release);   // publish
  wait(nullptr);
  munmap(p, sizeof(Mailbox));
  shm_unlink(name);
}
```

The parent creates the object, constructs a `Mailbox` in it with placement `new`, and forks a child, which inherits the mapping. The child spins until `ready` becomes true, then prints the text the parent wrote: `child 102844 read: hello through shared memory`. Two details matter for everything that follows:

- The data itself (`text`) is written with ordinary stores. What makes it safe to read is the `std::atomic<bool>` flag, stored with `memory_order_release` after the text and loaded with `memory_order_acquire` before reading it. This *publication* pattern, write the data and then publish a flag or cursor with release, reappears throughout the ring buffer.
- `std::fflush(stdout)` before `_exit`: `_exit` skips C stdio cleanup, so without it the child's output can vanish. (We hit exactly that bug in our own test programs.)

Shared memory gives us a channel. Turning it into a fast, safe broadcast queue is the rest of the design.

### The interface

The first code slide shows the public interface of the class the video builds:

**Listing 3** — code from the talk (Single-Producer Multi-Consumer Broadcast), shown at [0:09](https://www.youtube.com/watch?v=zkjz_v9E-Qg&t=9s)

```cpp
// Industrial Single-Producer Multi-Consumer (SPMC) Broadcast
// Lock-free shared memory fabric eliminating Unix socket overhead.

class BroadcastRingBuffer {
 public:
  static absl::StatusOr<std::unique_ptr<BroadcastRingBuffer>>
  Create(const std::string& name, size_t capacity);

  // Producer reserves contiguous zero-copy slot directly in shared RAM
  absl::StatusOr<uint8_t*> Reserve(uint32_t payload_size, uint32_t type_id);
  void Commit();

  // Wait-free consumer reads with direct pointer span return
  absl::StatusOr<absl::Span<const uint8_t>> PollZeroCopy(
      BroadcastReader& reader, FrameHeader& header_out);
};
```

- `BroadcastRingBuffer` is the shared-memory queue.
- `Create(name, capacity)` is a static factory. It returns `absl::StatusOr<std::unique_ptr<BroadcastRingBuffer>>`: either a buffer or an error. `absl::StatusOr` comes from Google's **Abseil** library; C++23's `std::expected` plays the same role in the standard library.
- `Reserve(payload_size, type_id)` gives the producer a pointer, `uint8_t*`, straight into shared memory where the next message will live. The producer writes the payload in place: there is no intermediate buffer to copy from. Then `Commit()` publishes it.
- `PollZeroCopy(reader, header_out)` gives a consumer an `absl::Span<const uint8_t>`, a pointer-and-length view of the message *in the shared buffer*. Nothing is copied. That is what **zero-copy** means here.
- The `BroadcastReader&` argument is the consumer's private bookkeeping (where it has read up to); `FrameHeader& header_out` receives the message's length and type.

The diagram on the same slide shows three consumers using three variants of the read API: `PollZeroCopy()`, `PollBatch()` (which the code later calls `PollCopyBatch`) and `PollCopy()`. The trade-offs between them are the subject of the final section.

The video's code lives in the repository `github.com/ktgorekore/hotpathcplusplus`, under `common/shared_memory/`. The slides' file paths say `cognitas/shared_memory/...`, and the repository's code uses the namespace `cognitas::trading`, but the file names and code match the repository. We built and tested that code for this chapter.

## 2. Two Segments and a Magic Mirror

*Talk segment: [1:09](https://www.youtube.com/watch?v=zkjz_v9E-Qg&t=69s)*

### Two segments, two levels of trust

Shared memory removes the kernel from the data path, but it also removes the kernel's protection: any process that maps the memory read-write can scribble over it. The speaker's design principle: "in production, you trust nobody, especially unprivileged consumer processes written in interpreted languages."

So the ring buffer is split into **two** shared-memory files:

| Segment | Contents | Consumers map it |
|---|---|---|
| `<name>_data` (`/dev/shm/feed_data` on the slide) | the message frames: headers and payloads | read-only (`PROT_READ`) |
| `<name>_meta` (`/dev/shm/feed_meta`) | `BroadcastHeader`: producer and commit cursors, plus a roster of 16 `ReaderSlot`s | read-write |

Consumers must be able to write *something*: each one records how far it has read (its cursor) so that the producer knows what it may overwrite. Putting that in a separate segment means the data segment can be mapped read-only by consumers. If a buggy consumer writes to a payload, the CPU's memory-management unit (MMU) refuses the store and the kernel delivers `SIGSEGV`: the consumer crashes, and the feed survives. The slide puts it this way: "Consumer Write Attempt -> Hardware MMU SIGSEGV!"

**Listing 4** — code from the talk (Dual POSIX Shared Memory Segments), shown at [1:35](https://www.youtube.com/watch?v=zkjz_v9E-Qg&t=95s)

```cpp
// Dual Shared Memory File Partitioning:
absl::StatusOr<std::unique_ptr<BroadcastRingBuffer>>
BroadcastRingBuffer::Attach(const std::string& name, bool writable) {
  // 1. Metadata Segment: MUST be read-write for consumer cursor registration
  auto meta_or = MemoryMap::Open(absl::StrCat(name, "_meta"), kHeaderSize,
                                 /*read_only=*/false);
  if (!meta_or.ok()) return meta_or.status();

  // 2. Data Segment: Consumer mapped strictly READ-ONLY to prevent corruption
  auto data_or = MemoryMap::Open(absl::StrCat(name, "_data"), capacity,
                                 /*read_only=*/!writable, /*use_magic_mirror=*/true);
  if (!data_or.ok()) return data_or.status();

  return std::unique_ptr<BroadcastRingBuffer>(
      new BroadcastRingBuffer(std::move(*met
```

- `Attach(name, writable)` is how a consumer process connects to an existing buffer.
- Step 1 opens `<name>_meta` with `read_only=false`, because consumers register and update cursors there.
- Step 2 opens `<name>_data` with `read_only = !writable` and `use_magic_mirror = true`. For a normal consumer `writable` is false, so the data is mapped `PROT_READ`. (The magic mirror is the subject of the rest of this section.)
- Each step returns early with the error status if mapping fails, the usual `absl::StatusOr` style.
- The listing is cut off at the end of the scene (the video types the code live). In the repository, the function goes on to read the capacity from the metadata header, sanity-check it (at most 1 GiB), and construct the object from the two mappings.

Two details from the repository are worth knowing. The slide's banner gives the data segment permission `0444` and the metadata `0666`; in the repository's `memory_map.cc`, both files are created with mode `0666`, and the read-only protection comes from the consumer *choosing* to map with `PROT_READ`. So the design protects against accidental writes, which is exactly what the speaker says ("A malfunctioning consumer cannot accidentally corrupt the live data stream"). It does not stop a hostile process from opening the file read-write; for that you would need different file ownership or permissions, as the slide's `0444` suggests.

### The wraparound problem

A **ring buffer** (circular buffer) is a fixed-size array used as a queue: the writer appends at a head position that wraps back to the start when it reaches the end. It needs no allocation, and old data is simply overwritten. The difficulty is the wrap itself.

**Listing 5** — code from the talk (Circular Buffer Boundary Overflow), shown at [1:45](https://www.youtube.com/watch?v=zkjz_v9E-Qg&t=105s)

```cpp
// The Naive Wraparound Flaw: 2x Memcpy + Conditional Branch
void WriteMessage(const uint8_t* msg, size_t len) {
  if (head_ + len <= capacity_) {
    // Fast path: Contiguous write
    std::memcpy(buf_ + head_, msg, len);
  } else {
    // SLOW PATH: Split across buffer boundary
    size_t first_part = capacity_ - head_;
    std::memcpy(buf_ + head_, msg, first_part);            // Write tail
    std::memcpy(buf_, msg + first_part, len - first_part); // Write head
    // Result: Branch predictor penalty + fragmented memory access!
  }
  head_ = (head_ + len) % capacity_;
}
```

- If the message fits before the end (`head_ + len <= capacity_`), one `memcpy` writes it.
- Otherwise the message is split: `first_part = capacity_ - head_` bytes go at the tail, and the remaining `len - first_part` bytes go at the start of the buffer.
- `head_ = (head_ + len) % capacity_` advances and wraps the head.

The slide's diagram makes it concrete: in a 65,536-byte buffer, a 128-byte message starting 48 bytes from the end is written as a 48-byte chunk at the tail and an 80-byte chunk at the head. The speaker identifies two costs:

1. **A branch on every access.** Every writer and every reader must check whether this message wraps. When it does, that is an unusual case, and the branch predictor guesses wrong; the slide quotes a pipeline squash of about 18 to 20 cycles. (In practice wraps are rare, so the branch is usually predicted correctly; the bigger cost is the next point.)
2. **No contiguous view.** A wrapped message is in two pieces, so the reader "Cannot return `std::span<const uint8_t>`" and must copy the message into a separate buffer to reassemble it. That rules out zero-copy reads.

The speaker's opinion of the branch: "On a modern branch predictor, that conditional branch is an insult."

### The magic mirror

The fix is an old trick that uses the MMU: map the buffer's memory **twice**, at two adjacent virtual address ranges.

| Virtual range | What it shows |
|---|---|
| `[base, base + size)` | the primary view of the physical buffer |
| `[base + size, base + 2*size)` | the mirror view of the *same* physical buffer |

Byte `base + size + k` is the *same physical byte* as `base + k`. So a message that starts near the end of the primary view simply continues into the mirror view, and the bytes land at the start of the physical buffer, which is exactly where a wrapped message should go. Writers and readers always see one contiguous range. No split, no reassembly, no wrap check: "Zero bounds checks. Zero memcpy splitting." The slide's banner: the MMU "automatically resolves pointer in silicon".

**Listing 6** — code from the talk (Virtual Memory Magic Mirror), shown at [2:13](https://www.youtube.com/watch?v=zkjz_v9E-Qg&t=133s)

```cpp
// Linux Virtual Memory Magic Mirror Double Mapping:
// Step 1: Reserve 2N contiguous virtual address space with PROT_NONE
size_t total_size = 2 * size;
void* base = mmap(nullptr, total_size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

// Step 2: Map primary buffer to physical shared memory at offset 0
void* first = mmap(base, size, prot, MAP_SHARED | MAP_FIXED, fd, 0);

// Step 3: Map second half (MAGIC MIRROR) to the SAME physical offset 0!
void* second = mmap(static_cast<uint8_t*>(base) + size, size, prot,
                    MAP_SHARED | MAP_FIXED, fd, 0);

// Zero-Copy Result: Any pointer dereference across 'size' wraps to byte
```

- **Step 1** reserves `2 * size` bytes of *address space* with `PROT_NONE` and `MAP_PRIVATE | MAP_ANONYMOUS`. This allocates no memory; it only claims a contiguous range of virtual addresses so nothing else can be placed there. Any access to it would fault.
- **Step 2** maps the shared-memory file descriptor at `base`, with `MAP_FIXED`. `MAP_FIXED` means "exactly at this address", replacing the reservation for that range.
- **Step 3** maps the *same* file, again at offset 0, at `base + size`. This is the mirror.
- The final comment, cut off as the scene ends, states the result: a pointer that runs past `size` wraps, in hardware, to byte 0.

The next slide shows the calls as a kernel sequence and adds the rule that makes it work:

**Listing 7** — code from the talk (Assembling the Mirror: 3x POSIX mmap), shown at [2:57](https://www.youtube.com/watch?v=zkjz_v9E-Qg&t=177s)

```cpp
// Page Alignment Enforcement & Validation:
absl::Status MemoryMap::DoMap(int fd, size_t size, int prot, bool use_mirror) {
  const size_t page_size = sysconf(_SC_PAGESIZE); // 4096 bytes on x86_64
  if (size % page_size != 0) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Buffer size %lu must be multiple of page size %lu",
                        size, page_size));
  }
  // Linux kernel requires strict page alignment for MAP_FIXED;
  // non-aligned offsets trigger instantaneous EINVAL error!
  return absl::OkStatus();
}
```

- `sysconf(_SC_PAGESIZE)` returns the page size, 4096 bytes on x86-64 Linux by default.
- If `size` is not a multiple of it, the function returns `absl::InvalidArgumentError` with a formatted message (`absl::StrFormat` is Abseil's type-safe `printf`).
- The comments explain why: `MAP_FIXED` mappings must start on a page boundary. The mirror starts at `base + size`, so if `size` is not a whole number of pages, the mirror's address is misaligned and the kernel rejects the call with `EINVAL`. The speaker's version: "Forget page alignment by a single byte and the Linux kernel will hand you an EINVAL faster than a compiler on missing semicolons."

On the slide this check is the body of `DoMap`; in the repository, the same check sits in `MemoryMap::Create` and `MemoryMap::Open`, and `DoMap` performs the three `mmap` calls with an error check after each.

Here is the whole trick as one runnable program of our own. It uses `memfd_create`, which creates an anonymous memory file without a name in `/dev/shm`, convenient for a demo:

**Listing 8** — A runnable magic mirror: one page mapped twice *(illustrative example, not from the talk)*

```cpp
// The "magic mirror": map the same pages twice, side by side.
#include <sys/mman.h>
#include <unistd.h>

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>

int main() {
  const size_t size = static_cast<size_t>(sysconf(_SC_PAGESIZE));  // one page = 4096
  int fd = memfd_create("ring", 0);                 // anonymous shared memory file
  if (fd < 0 || ftruncate(fd, size) != 0) { std::perror("memfd"); return 1; }

  // 1. Reserve 2 * size of address space; nothing is accessible yet.
  auto* base = static_cast<uint8_t*>(
      mmap(nullptr, 2 * size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
  // 2. and 3. Map the same file offset 0 into both halves.
  void* first = mmap(base, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, 0);
  void* second = mmap(base + size, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, 0);
  if (base == MAP_FAILED || first == MAP_FAILED || second == MAP_FAILED) { std::perror("mmap"); return 1; }

  // Write a 32-byte message that starts 12 bytes before the end of the buffer.
  const char msg[] = "this message wraps the ring end";  // 31 chars + '\0'
  size_t offset = size - 12;
  std::memcpy(base + offset, msg, sizeof msg);           // one memcpy, no split

  std::printf("read back as one span: \"%s\"\n", reinterpret_cast<char*>(base + offset));
  std::printf("first 20 bytes of the buffer: \"%.20s\"\n", reinterpret_cast<char*>(base));
  std::printf("same physical byte: base[0] == base[size]? %s\n", base[0] == base[size] ? "yes" : "no");

  // The kernel rejects a mirror at an address that is not page aligned.
  void* bad = mmap(base + size + 1, size, PROT_READ, MAP_SHARED | MAP_FIXED, fd, 0);
  std::printf("MAP_FIXED at an unaligned address: %s\n", bad == MAP_FAILED ? std::strerror(errno) : "mapped?!");
}
```

Its output on our machine, one line per `printf`:

- `read back as one span: "this message wraps the ring end"`
- `first 20 bytes of the buffer: " wraps the ring end"`
- `same physical byte: base[0] == base[size]? yes`
- `MAP_FIXED at an unaligned address: Invalid argument`

The message was written with a single `memcpy` starting 12 bytes before the end of a one-page buffer, and it reads back intact as one string. The second line shows where the overflow physically went: to the start of the buffer. The last line is the `EINVAL` the speaker warns about, triggered by mapping at `base + size + 1`.

Note what the mirror costs: twice the *virtual* address space (plentiful on 64-bit systems) and no extra *physical* memory. The slide: "2x Virtual Views -> 1x Physical Allocation -> ZERO Memory Bloat!" It also explains a constraint that is easy to miss: no single message may be larger than the buffer itself, or it would run off the end of the mirror. The repository's `Reserve` checks exactly that.

## 3. False Sharing and the Padding Moat

*Talk segment: [3:27](https://www.youtube.com/watch?v=zkjz_v9E-Qg&t=207s)*

### Cache lines and coherence

With the data path in place, the next problem appears in the metadata segment, where every consumer keeps a cursor. To see it, we need two facts about CPU caches.

First, caches do not store individual bytes or variables. They store **cache lines**, 64-byte aligned blocks of memory on x86-64. Reading one byte brings its whole line into the core's L1 cache; writing one byte requires the core to own the whole line.

Second, each core has its own L1 and L2 caches, so the same line can be cached in several places. A **cache-coherence protocol** keeps those copies consistent. The classic one is **MESI**, named after the four states a line can be in, in a given core's cache:

| State | Meaning |
|---|---|
| **M**odified | this core has the only copy, and has changed it |
| **E**xclusive | this core has the only copy, unchanged |
| **S**hared | several cores have read-only copies |
| **I**nvalid | this core's copy is stale and must not be used |

A core may write a line only in M or E. If another core holds a copy, it must first be invalidated, which means a message across the on-chip interconnect, and the other core's next access to that line misses and must fetch the line back.

**False sharing** happens when two cores repeatedly write *different* variables that happen to sit in the *same* line. Logically they share nothing. Physically the line bounces between their caches on every write, each core invalidating the other's copy: an "invalidation storm", in the slide's words.

**Listing 9** — code from the talk (The MESI Protocol Threat: False Sharing), shown at [3:17](https://www.youtube.com/watch?v=zkjz_v9E-Qg&t=197s)

```cpp
// The False Sharing Anti-Pattern: Unaligned Consumer Cursors
struct UnalignedRoster {
  // 8 consumers packed tightly into a single 64-byte cache line:
  std::atomic<uint64_t> cursors[8]; // sizeof = 64 bytes!
};

// When Core 1 and Core 2 update adjacent elements simultaneously:
// CPU 1 writes cursors[0] -> CPU 2 cache line is INVALIDATED!
// Bus traffic saturates L3 interconnect; throughput drops by 94%!
```

- `struct UnalignedRoster` holds eight `std::atomic<uint64_t>` cursors, 8 bytes each, 64 bytes in total: one cache line, if the struct starts on a line boundary.
- If consumer 0 on core 1 and consumer 1 on core 2 each update their own cursor, every update by one invalidates the other's copy of the line.
- The comments claim the bus traffic "saturates L3 interconnect; throughput drops by 94%!", and the slide's diagram puts the cost at about 140 cycles per write. The file named (`benchmark/false_sharing_reproducer.cc`) is not in the repository, so we wrote our own reproducer, described below.

The speaker's comparison: with false sharing, "Your 16-core Xeon is suddenly performing worse than an ancient single-core Pentium."

### The padding moat

The cure is to give each consumer's hot data its own cache line. The slide:

**Listing 10** — code from the talk (Cache Line Alignment & Padding Moats), shown at [3:53](https://www.youtube.com/watch?v=zkjz_v9E-Qg&t=233s)

```cpp
// Hardware Cache Line Isolation (64 Bytes):
constexpr size_t kRingBufferCacheLineSize = 64;

struct alignas(kRingBufferCacheLineSize) ReaderSlot {
  std::atomic<bool> active{false};          // 1 byte
  std::atomic<uint64_t> cursor{0};          // 8 bytes
  std::atomic<int64_t> heartbeat_ts{0};     // 8 bytes
  std::atomic<pid_t> pid{0};                // 4 bytes
  std::atomic<uint32_t> generation_id{0};   // 4 bytes
  char padding[35];  // Explicit padding moat: completes 64-byte line!
};
static_assert(sizeof(ReaderSlot) == 64, "ReaderSlot must exactly fill 64 bytes");
```

- `kRingBufferCacheLineSize = 64` names the line size.
- `alignas(kRingBufferCacheLineSize)` makes every `ReaderSlot` start on a 64-byte boundary, so in an array `ReaderSlot readers[16]` each slot begins a new line.
- The fields: `active` (is this slot in use), `cursor` (how far this consumer has read), `heartbeat_ts` (when it last made progress, used for eviction), `pid` (the consumer's process ID) and `generation_id` (used against the ABA problem, covered later). The comments give their sizes as 1, 8, 8, 4 and 4 bytes.
- `char padding[35];` is the "explicit padding moat", and `static_assert(sizeof(ReaderSlot) == 64, ...)` promises that the slot exactly fills one line.

In the narration, the speaker says "we use alignas 64 on our reader slot struct and add exactly 32 bytes of explicit char padding"; the slide and the repository both use `padding[35]`. The speaker's metaphor for it: "That padding array isn't wasted memory. It is a demilitarized zone that keeps your CPU cores from declaring war on each other."

### Checking the arithmetic

The slide's banner counts the fields as "29 bytes", 1 + 8 + 8 + 4 + 4 = 25, plus a 4-byte "gap", and adds 35 bytes of padding to reach 64. But that is not how the compiler lays out the struct. Every member is placed at an offset that is a multiple of its own alignment:

| Member | Size | Alignment | Offset |
|---|---|---|---|
| `active` | 1 | 1 | 0 |
| *(padding)* | 7 | | 1 |
| `cursor` | 8 | 8 | 8 |
| `heartbeat_ts` | 8 | 8 | 16 |
| `pid` | 4 | 4 | 24 |
| `generation_id` | 4 | 4 | 28 |
| `padding[35]` | 35 | 1 | 32 |

The last byte of `padding` is at offset 66, so the members need 67 bytes. `alignas(64)` then rounds the size up to the next multiple of 64: **`sizeof(ReaderSlot)` is 128**, two cache lines. The hidden 7 bytes after `active` are the gap that the "29 bytes" count misses (it allows 4).

So the slide's `static_assert` fails. GCC 13, GCC 16 and Clang 20 all reject it; Clang reports "static assertion failed due to requirement 'sizeof(ReaderSlot) == 64'" with the note "expression evaluates to '128 == 64'". The repository's `ReaderSlot` has the same fields and the same `padding[35]` but no `static_assert`, so it compiles, and on our build `sizeof(ReaderSlot)` printed 128, with `cursor` at offset 8 and `padding` at offset 32.

Does this matter? For false sharing, not really: each slot still starts on its own line, so no two consumers share one. It just wastes a line per slot (16 slots use 2 KiB instead of 1 KiB) and contradicts the comment. But it is a good lesson in why such `static_assert`s exist: this one would have caught the mistake at compile time.

The simplest correct version needs no manual padding at all. Order the members from largest to smallest so there is no internal padding, and let `alignas(64)` round the size up. This is our own fix:

**Listing 11** — A ReaderSlot that really is one cache line *(illustrative example, not from the talk)*

```cpp
#include <sys/types.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>

constexpr size_t kRingBufferCacheLineSize = 64;

// Largest fields first: no hidden padding between members.
struct alignas(kRingBufferCacheLineSize) ReaderSlot {
  std::atomic<uint64_t> cursor{0};         // offset  0
  std::atomic<int64_t> heartbeat_ts{0};    // offset  8
  std::atomic<pid_t> pid{0};               // offset 16
  std::atomic<uint32_t> generation_id{0};  // offset 20
  std::atomic<bool> active{false};         // offset 24
  // No explicit padding needed: alignas(64) rounds sizeof up to 64.
};

static_assert(sizeof(ReaderSlot) == kRingBufferCacheLineSize);
static_assert(alignof(ReaderSlot) == kRingBufferCacheLineSize);
static_assert(offsetof(ReaderSlot, active) == 24);  // 25 bytes used, 39 bytes of tail padding

int main() {
  ReaderSlot roster[16];
  std::printf("sizeof %zu, slot 1 starts %td bytes after slot 0\n", sizeof(ReaderSlot),
              reinterpret_cast<char*>(&roster[1]) - reinterpret_cast<char*>(&roster[0]));
}
```

It prints `sizeof 64, slot 1 starts 64 bytes after slot 0`. The `static_assert`s on `sizeof`, `alignof` and `offsetof` turn the layout into a compile-time contract. (C++17 also offers `std::hardware_destructive_interference_size` in `<new>` as a portable name for "the distance that avoids false sharing"; GCC warns that its value can vary between compiler versions and flags, so many projects hard-code 64 as this code does.)

### Measuring false sharing

The repository does not include the slide's false-sharing reproducer, so we wrote one (it is in the `bench/` folder next to this post). Two threads, pinned to two different physical cores, each update their own counter 100 million times. In the "packed" layout both counters share a line; in the "aligned" layout each has its own. We ran each case under the video's own `perf stat` command, with two update patterns:

| Update pattern | Packed (same line) | Aligned (own line) | perf `cache-misses`, packed / aligned |
|---|---|---|---|
| load, then `store(release)` | 2.26 ns per update | 1.51 ns | 92K / 76K |
| `fetch_add` (an atomic read-modify-write) | 27.4 ns | 5.1 ns | 393K / 117K |

Two lessons:

- **False sharing is real, and read-modify-write operations feel it most.** With `fetch_add`, which needs exclusive ownership of the line on every operation, the packed layout is 5.3 times slower, and IPC (instructions per cycle) drops from 0.33 to 0.07. With plain stores, the cost is much smaller, because each core's **store buffer** lets it keep running while its stores wait for line ownership. The ring buffer's consumers do both: `Advance` performs a compare-and-swap and two stores on the slot.
- **`cache-misses` is the wrong counter for it.** On Intel CPUs, perf's generic `cache-misses` event counts last-level-cache misses. When one core needs a line that another core has modified, the line is supplied from inside the chip, not from DRAM, so it is not an LLC miss. The tool built for this is `perf c2c` ("cache to cache"), which samples loads that hit a line *Modified in another core's cache*, called **HITM**:

`perf c2c record ./false_sharing packed rmw`
`perf c2c report --stdio`

On the packed run it reported 13,050 local HITM loads, 99.98% of them on a single cache line, and it names that line's address and the code that touches it. On the aligned run: 0 HITM loads. That is the measurement that proves false sharing, and the one to reach for when you suspect it.

## 4. Sequence Locks and Release Fences

*Talk segment: [3:57](https://www.youtube.com/watch?v=zkjz_v9E-Qg&t=237s)*

### Writers must never wait for readers

The producer writes each message in place, inside the shared buffer, while consumers may be reading nearby. How does a reader know that the frame it is looking at is complete and not half-written? A mutex would answer that, but the speaker rules it out: "Mutexes are for developers with plenty of time on their hands." A mutex shared with consumer processes would also let a stalled consumer block the producer, the one thing a market-data feed cannot allow.

The video's answer is a **sequence lock** (seqlock). Each frame header carries a `version` counter:

| State | Version | Meaning |
|---|---|---|
| Stable / committed | even (104) | the frame is complete; readers may read it |
| Write in progress | odd (105) | the producer is writing; readers must retry |
| Write committed | even (106) | the new contents are complete |

The writer never waits. A reader checks the version before and after reading the frame; if the version was odd, or changed in between, the read may be torn (part old, part new), and the reader retries. The slide's reader loop, in pseudo-code: load `v1` with acquire, retry if `(v1 & 1) != 0`, copy the payload, load `v2` with acquire, and succeed if `v1 == v2`.

### The writer side

**Listing 12** — code from the talk (Lock-Free SeqLock Bit Masking), shown at [4:09](https://www.youtube.com/watch?v=zkjz_v9E-Qg&t=249s)

```cpp
// SeqLock Bit-Masking Protocol in Reserve() and Commit():

// In Reserve(): Set Bit 0 (Odd version marks active modification)
uint32_t current_ver = header_dest->version.load(std::memory_order_relaxed);
header_dest->version.store(current_ver | 1, std::memory_order_release);

// In Commit(): Increment version to next Even number (Marks frame valid)
header_dest->version.store(current_ver + 1, std::memory_order_release);

// Readers verify: version is even and unchanged acr
```

- In `Reserve()`, the producer loads the frame's current version and stores `current_ver | 1`: setting bit 0 makes it odd, marking the frame as being written.
- In `Commit()`, it stores the next even number, marking the frame valid.
- The last comment, cut off as the scene ends, states the reader's rule: "version is even and unchanged across" the read.

Read literally, this sketch has a small slip: `Commit()` stores `current_ver + 1`, where `current_ver` is the *even* value loaded in `Reserve()`, which would give 105, still odd. The real `Commit()`, the next listing, reloads the version (now odd) before adding 1, so it does produce the even value. Two listings later you can see that line.

There is a subtler point about orderings. The odd marker is stored with `memory_order_release`, and the payload is written afterwards with ordinary stores. But a release store only stops *earlier* memory operations from moving after it; it does not stop *later* ones (the payload writes) from moving before it. Under the C++ memory model, a reader could therefore see new payload bytes while the version still looks even. The textbook seqlock writer stores the odd version and then issues a release *fence* before writing the data, as in the example at the end of this section. On x86 the hardware never reorders two stores, so in practice the repository's code works there; the gap is in what the compiler is allowed to do, and on CPUs with weaker ordering (ARM, POWER), in what the hardware does.

### Commit and the release fence

**Listing 13** — code from the talk (Store Buffers & Release Fences), shown at [4:33](https://www.youtube.com/watch?v=zkjz_v9E-Qg&t=273s)

```cpp
// Enforcing Hardware Store Order with Release Fence:
void BroadcastRingBuffer::Commit() {
  auto* header_dest = reinterpret_cast<FrameHeader*>(
      data_buffer_ + current_reservation_.start_offset);
  uint32_t current_version = header_dest->version.load(std::memory_order_relaxed);
  header_dest->version.store(current_version + 1, std::memory_order_release);

  // CRITICAL: Ensure payload bytes are globally visible before cursor update!
  std::atomic_thread_fence(std::memory_order_release);

  header_->commit_cursor.store(current_reservation_.claimed_cursor,
                               std::memory_order_release);
}
```

- `header_dest` points at the frame header the producer reserved (`data_buffer_` plus the reservation's offset). `reinterpret_cast` turns the raw bytes into a `FrameHeader*`.
- The version is reloaded and incremented: odd becomes even, the frame is committed.
- `std::atomic_thread_fence(std::memory_order_release)` is marked "CRITICAL: Ensure payload bytes are globally visible before cursor update!"
- Finally, `commit_cursor` is stored with release. This is the value consumers poll: everything before it is safe to read.

The speaker explains the fence: "Modern CPUs execute instructions out of order. Without a release fence, your processor will cheerfully publish the commit cursor before the message payload has flushed to RAM. Your consumers will read uninitialized garbage, and your risk engine will panic."

The underlying rule is right: consumers must not be able to observe the new cursor without also observing the payload behind it. Three refinements make it precise.

1. **The release store already does it.** In C++, a store with `memory_order_release` guarantees that every memory write *before* it, in this thread, is visible to any thread that reads the stored value with `memory_order_acquire`. The `commit_cursor.store(..., release)` alone orders the payload and the version before the cursor. The fence just before it adds nothing; it is harmless but redundant.
2. **"Flushed to RAM" is not the issue.** Visibility between cores is handled by cache coherence (the previous section); data never has to reach DRAM to be seen by another core. What can reorder are the compiler and, on some CPUs, the core's **store buffer**, the queue in which a core's stores wait before they reach its cache.
3. **On x86 none of this produces an instruction.** x86 uses a memory model called TSO (total store order): stores become visible to other cores in program order. So release stores and release fences compile to plain `mov` instructions, and their only effect is to stop the *compiler* from reordering. We disassembled the repository's `Commit()` as built by GCC 13 at `-O3 -march=native`: it is three `mov` instructions (load the version, store version+1, store the commit cursor) and a `ret`, with no fence instruction. On ARM64 the same code would use `stlr` (store-release) instructions, and there the ordering does matter at the hardware level.

None of this makes the video's advice wrong: getting the orderings right is essential, and on x86 the compiler is a real source of reordering. It does mean you should not expect a `mfence` in the output, or a measurable cost from these orderings, on x86.

### The reader side, and zero-copy

The repository's `PollCopy` implements the reader loop: it loads `version_before` with acquire, spins with `_mm_pause()` while it is odd, copies the payload into the consumer's own buffer, then executes `std::atomic_thread_fence(std::memory_order_acquire)` and reloads the version. That fence is necessary, and the slide's pseudo-code leaves it out: an acquire *load* of `v2` would not stop the payload reads above it from being reordered after it, so without the fence the second check could run before the copy finished. The acquire fence pins the copy before the check.

Notice what a zero-copy read cannot do: `PollZeroCopy` hands the consumer a span *into* the shared buffer and returns. The consumer reads the bytes later, so there is no "after" check. Zero-copy reads are protected by something else: backpressure. As the next section shows, the producer never overwrites bytes that a registered, live consumer has not yet released by calling `Advance`. The seqlock matters most for readers that may have been lapped, for example after being evicted.

Here is a complete seqlock of our own, which follows the textbook rules and also avoids a data race in the C++ sense by storing the payload as relaxed atomic words rather than plain bytes:

**Listing 14** — A single-writer SeqLock<T> with correct fences *(illustrative example, not from the talk)*

```cpp
// A single-writer sequence lock. The payload is stored as relaxed atomic
// words, so concurrent reads are not a data race in the C++ memory model.
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <thread>
#include <type_traits>

template <typename T>
class SeqLock {
  static_assert(std::is_trivially_copyable_v<T> && sizeof(T) % 8 == 0);
  static constexpr size_t kWords = sizeof(T) / 8;

 public:
  void Store(const T& value) {                                    // one writer only
    uint64_t v = seq_.load(std::memory_order_relaxed);
    seq_.store(v + 1, std::memory_order_relaxed);                 // odd: write in progress
    std::atomic_thread_fence(std::memory_order_release);          // odd seq before data
    std::array<uint64_t, kWords> w;
    std::memcpy(w.data(), &value, sizeof(T));
    for (size_t i = 0; i < kWords; ++i) words_[i].store(w[i], std::memory_order_relaxed);
    seq_.store(v + 2, std::memory_order_release);                 // even: data published
  }

  bool TryLoad(T& out) const {                                    // any number of readers
    uint64_t v1 = seq_.load(std::memory_order_acquire);
    if (v1 & 1) return false;                                     // writer active
    std::array<uint64_t, kWords> w;
    for (size_t i = 0; i < kWords; ++i) w[i] = words_[i].load(std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_acquire);          // data reads before re-check
    if (seq_.load(std::memory_order_relaxed) != v1) return false; // torn: retry
    std::memcpy(&out, w.data(), sizeof(T));
    return true;
  }

 private:
  std::atomic<uint64_t> seq_{0};
  std::array<std::atomic<uint64_t>, kWords> words_{};
};

struct Quote { uint64_t bid, ask, bid_size, ask_size; };  // 32 bytes

int main() {
  SeqLock<Quote> lock;
  std::atomic<bool> done{false};
  std::thread writer([&] {
    for (uint64_t i = 1; i <= 20'000'000; ++i) lock.Store({i, i, i, i});
    done.store(true);
  });
  uint64_t good = 0, torn = 0, retries = 0;
  while (!done.load()) {
    Quote q;
    if (!lock.TryLoad(q)) { ++retries; continue; }
    (q.bid == q.ask && q.ask == q.bid_size && q.bid_size == q.ask_size) ? ++good : ++torn;
  }
  writer.join();
  std::printf("consistent reads %llu, torn reads %llu, retries %llu\n",
              (unsigned long long)good, (unsigned long long)torn, (unsigned long long)retries);
}
```

- `Store` makes the sequence odd, issues a release fence so that the odd value is ordered before the data, writes the words, then publishes an even sequence with a release store.
- `TryLoad` reads the sequence with acquire, bails out if it is odd, reads the words, issues an acquire fence, and rechecks the sequence.
- The test runs a writer storing 20 million quotes whose four fields are always equal and a reader that checks every successful read. Pinned to two cores, it reported `torn reads 0` with both GCC 13 and Clang 20, with tens of thousands of consistent reads and millions of retries: the reader frequently caught the writer mid-update and correctly refused those reads.

## 5. Backpressure, Dead Consumers and the ABA Trap

*Talk segment: [4:52](https://www.youtube.com/watch?v=zkjz_v9E-Qg&t=292s)*

### Backpressure: never lap the slowest reader

A ring buffer reuses its memory. If the producer runs a full lap ahead of a consumer, it overwrites messages that consumer has not read yet. There are two broad policies:

- **Lossy broadcast**: the producer never waits; slow consumers detect that they were lapped and lose data (many market-data multicast feeds work this way).
- **Reliable broadcast with backpressure**: the producer waits until the slowest consumer has moved on. No message is lost, at the price of letting consumers slow the producer.

The video's buffer is reliable. "The producer must enforce back pressure to ensure it never overrides the slowest active consumer." To make that work, positions are tracked as **cursors**: 64-bit byte counts that only ever increase (the physical offset is `cursor % capacity`). A 64-bit byte counter will not overflow in any realistic lifetime, so cursor arithmetic never has to worry about wrapping.

**Listing 15** — code from the talk (SPMC Flow Control & Backpressure), shown at [5:11](https://www.youtube.com/watch?v=zkjz_v9E-Qg&t=311s)

```cpp
// Backpressure Scanning in Reserve():
uint64_t target_cursor = current_cursor + required_space;

// Check if target cursor laps the slowest registered consumer
if (target_cursor - cached_min_cursor_ > capacity_) {
  while (true) {
    uint64_t min_c = target_cursor;
    for (size_t i = 0; i < kMaxReaders; ++i) {
      if (header_->readers[i].active.load(std::memory_order_acquire)) {
        uint64_t rc = header_->readers[i].cursor.load(std::memory_order_acquire);
        if (rc < min_c) min_c = rc;
      }
    }
    cached_min_cursor_ = min_c;
    if (target_cursor - cached_min_cursor_ <= capacity_) break;
    _mm_pause(); // Yield hyperthread to readers
  }
}
```

- `target_cursor` is where the producer's cursor will be after this message.
- The producer must not get more than `capacity_` bytes ahead of any active consumer. `cached_min_cursor_` is the slowest consumer's cursor *as last seen*. If even that stale value leaves enough room, the producer skips the scan entirely, so most calls to `Reserve` cost nothing extra.
- Otherwise it scans all `kMaxReaders` (16) slots, considers only those whose `active` flag is set, and takes the minimum cursor, with acquire loads so that it sees each consumer's latest published position.
- If there is now room, it breaks out; if not, `_mm_pause()` hints to the CPU that this is a spin-wait (on a hyper-threaded core, it lets the sibling thread run) and scans again.

The roster diagram on the slide shows why this is dangerous: three consumers lag by 120 to 420 bytes, and a fourth, "HUNG JAVA GC / GDB", sits at cursor 1,200, lagging by 64,120 bytes in a 64 KiB buffer. Available space: 0 bytes. "Deadlock Vulnerability: A single hung reader freezes the entire global trading venue!"

### Evicting dead consumers

"What happens if a downstream consumer freezes on a 2-second Java garbage collection pause, or a developer attaches GDB in production without telling anyone?" With pure backpressure, the producer would wait forever. "Industrial buffers assume consumers are unstable."

The fix is a **heartbeat**: every consumer stamps its slot's `heartbeat_ts` with the current time whenever it advances. While the producer is blocked, it evicts any active consumer whose heartbeat is older than a timeout, 500 ms by default in the repository.

**Listing 16** — code from the talk (Rate-Limited Consumer Eviction), shown at [5:51](https://www.youtube.com/watch?v=zkjz_v9E-Qg&t=351s)

```cpp
// Rate-Limited Syscall Optimization via Hardware TSC:
uint64_t current_tsc = __rdtsc();
if (current_tsc - last_tsc > 100000ULL) {
  now = absl::ToUnixNanos(absl::Now()); // Sample wall-clock
  last_tsc = current_tsc;
}

// SAFE RECLAMATION: Evict dead/hung nodes to prevent global deadlock
if (now - header_->readers[i].heartbeat_ts.load(std::memory_order_relaxed) >
    evict_timeout_ns_) {
  header_->readers[i].active.store(false, std::memory_order_release);
  continue; // Unblocks producer immediately!
}
```

- Reading the wall clock costs time, so the producer reads the CPU's **time-stamp counter** first. `__rdtsc()` is a compiler intrinsic for the `rdtsc` instruction, which returns a cycle count without any system call. Only when more than 100,000 TSC ticks (about 25 µs at 4 GHz) have passed since the last sample does it call `absl::Now()` for a real timestamp.
- The eviction itself: if `now - heartbeat_ts > evict_timeout_ns_`, the slot's `active` flag is cleared with a release store, and `continue` skips that slot when computing the minimum cursor. The producer is unblocked: "The dead consumer is purged and the venue stays alive."

The slide's title calls the `__rdtsc()` gate a "Rate-Limited Syscall Optimization". On Linux, `absl::Now()` ends up in `clock_gettime`, which for the normal clocks is served by the **vDSO**, a small kernel-provided library mapped into every process, without entering the kernel. So the wall-clock read is not a system call; it is a function call costing about 26 ns on our machine (we measured it, see the next section). The rate limit is still sensible, because this loop spins.

Two observations about where this code runs. In the repository, the eviction scan lives inside `Reserve`'s backpressure loop, so it only happens when the producer is actually blocked. And the other side of the protocol, the consumer refreshing its heartbeat, happens in `Advance` on *every* call, with an `absl::Now()` each time. That per-message clock read will turn out to be the biggest single cost in the benchmark.

### The ABA trap

Eviction introduces a race. The slide's timeline:

1. **T1**: reader A registers and claims slot 2, with generation ID 14.
2. **T2**: A hangs and is evicted after 500 ms.
3. **T3**: reader B registers and claims slot 2; the generation increments to 15.
4. **T4**: A wakes up and calls `Advance()`, still believing it owns slot 2.

Without protection, A would write its (stale) cursor into B's slot. The producer would then trust that cursor and might keep data B needs, or worse, overwrite data B has not read. This is a form of the **ABA problem**: a check that only asks "is this still slot 2?" cannot tell that the slot has changed hands in between. The standard fix is a counter that changes on every reuse, here the **generation ID**.

**Listing 17** — code from the talk (The Eviction ABA Race Condition Trap), shown at [6:01](https://www.youtube.com/watch?v=zkjz_v9E-Qg&t=361s)

```cpp
// ABA Prevention via 32-bit Generation ID Atomic CAS:
void BroadcastRingBuffer::Advance(BroadcastReader& reader, size_t frame_bytes) {
  int slot = reader.slot_index();
  if (slot >= 0) {
    auto& my_slot = header_->readers[slot];
    uint32_t expected_gen = reader.generation_id();

    // Verify reader's local epoch matches slot's epoch before updating cursor!
    if (my_slot.generation_id.compare_exchange_strong(
            expected_gen, expected_gen, std::memory_order_acquire)) {
      my_slot.cursor.store(new_cursor, std::memory_order_release);
      my_slot.heartbeat_ts.store(absl::ToUnixNanos(absl::Now()), std::memory_order_relaxed);
    } else {
      reader.set_slot_index(-1); // Evicted & replaced! Unbind cleanly.
    }
  }
}
```

- `slot_index()` is -1 if the reader has been unbound; otherwise `my_slot` is its roster entry.
- `expected_gen` is the generation the reader was given at registration.
- `compare_exchange_strong(expected_gen, expected_gen, acquire)` checks that the slot's generation still equals the reader's. If it does, the reader publishes its cursor (release) and refreshes its heartbeat.
- If not, the slot belongs to someone else: the reader unbinds itself with `set_slot_index(-1)`. "The zombie consumer's stale cursor update is rejected cleanly."

As printed, the function does not compile: it takes `frame_bytes` but stores `new_cursor`, which is never declared. In the repository, `Advance` starts with `uint64_t new_cursor = reader.cursor() + total_frame_length;` and `reader.set_cursor(new_cursor);`, and the rest matches the slide.

Two details from the repository refine the story. First, the generation is incremented when a reader **registers** (`RegisterReader` claims a free slot with a CAS on `active`, then does `generation_id.fetch_add(1)`), not at eviction as the narration says; the effect on the zombie is the same. Second, look closely at the check:

- `compare_exchange_strong(expected_gen, expected_gen, ...)` replaces the value with *the same value*. Its only effect is the comparison, which an acquire load followed by `==` would also do, without the cost of a locked instruction (`lock cmpxchg`, about 5 ns on our machine).
- More importantly, the check and the cursor store are **two separate atomic operations**. If A passes the check, then gets descheduled for long enough to be evicted and replaced by B, A's `cursor.store` still lands in B's slot afterwards. The window is tiny, but this is exactly the kind of race eviction exists for.

The usual way to close such a window is to make the check and the update one atomic step, by packing the generation and the cursor into a single 64-bit word and updating them with one compare-and-swap. Here is a self-contained sketch of that idea (ours, not the repository's):

**Listing 18** — Generation and cursor in one atomic word: check and update in a single CAS *(illustrative example, not from the talk)*

```cpp
// Closing the window between "check generation" and "store cursor":
// keep both in one 64-bit word and update them with a single CAS.
#include <atomic>
#include <cstdint>
#include <cstdio>

class SlotState {
 public:
  static constexpr int kCursorBits = 48;                    // 256 TiB of stream offset
  static constexpr uint64_t kCursorMask = (uint64_t{1} << kCursorBits) - 1;

  static uint64_t Pack(uint32_t gen, uint64_t cursor) {
    return (uint64_t{gen & 0xFFFF} << kCursorBits) | (cursor & kCursorMask);
  }
  static uint32_t Gen(uint64_t s) { return static_cast<uint32_t>(s >> kCursorBits); }
  static uint64_t Cursor(uint64_t s) { return s & kCursorMask; }

  // Consumer: publish a new cursor only if the slot still belongs to `my_gen`.
  bool Advance(uint32_t my_gen, uint64_t new_cursor) {
    uint64_t cur = state_.load(std::memory_order_acquire);
    do {
      if (Gen(cur) != (my_gen & 0xFFFF)) return false;     // evicted or replaced
    } while (!state_.compare_exchange_weak(cur, Pack(my_gen, new_cursor),
                                           std::memory_order_release,
                                           std::memory_order_acquire));
    return true;
  }

  // Producer or a new consumer: take the slot over by bumping the generation.
  uint32_t Claim(uint64_t start_cursor) {
    uint64_t cur = state_.load(std::memory_order_acquire);
    uint32_t next;
    do {
      next = (Gen(cur) + 1) & 0xFFFF;
    } while (!state_.compare_exchange_weak(cur, Pack(next, start_cursor),
                                           std::memory_order_acq_rel));
    return next;
  }

  uint64_t cursor() const { return Cursor(state_.load(std::memory_order_acquire)); }

 private:
  std::atomic<uint64_t> state_{0};
};

int main() {
  SlotState slot;
  uint32_t a = slot.Claim(0);           // consumer A registers
  slot.Advance(a, 4096);
  uint32_t b = slot.Claim(8192);        // A is evicted; B takes the slot
  bool zombie = slot.Advance(a, 5000);  // A wakes up and tries to publish
  bool live = slot.Advance(b, 9000);
  std::printf("gen A=%u B=%u | zombie update accepted: %s | B update accepted: %s | cursor %llu\n",
              a, b, zombie ? "yes" : "no", live ? "yes" : "no",
              static_cast<unsigned long long>(slot.cursor()));
}
```

- The top 16 bits of `state_` hold the generation, the low 48 bits the cursor (enough for 256 TiB of stream).
- `Advance` succeeds only if the CAS from "my generation, old cursor" to "my generation, new cursor" succeeds; if another thread has bumped the generation in between, the CAS fails and the check runs again, now failing.
- `Claim` bumps the generation and sets the new owner's starting cursor in one CAS.

The demo prints `gen A=1 B=2 | zombie update accepted: no | B update accepted: yes | cursor 9000`. A 16-bit generation can wrap after 65,536 reuses of a slot; a real implementation would size the fields for its use (or use a 128-bit CAS where available).

## 6. Benchmarks, perf stat, and What the Numbers Mean

*Talk segment: [6:13](https://www.youtube.com/watch?v=zkjz_v9E-Qg&t=373s)*

### The video's numbers

The benchmark scene reports Google Benchmark results "on a bare metal 32-thread AMD Ryzen 9 9950X running Linux" (6.8, per the slide):

**Listing 19** — code from the talk (Empirical Benchmark Audit), shown at [6:41](https://www.youtube.com/watch?v=zkjz_v9E-Qg&t=401s)

```cpp
// Google Benchmark Results on AMD Ryzen 9 9950X (Linux 6.8):
// ---------------------------------------------------------------------------
// Benchmark                    Payload   Time/Op     Throughput
// ---------------------------------------------------------------------------
// BM_Produce/8B                     8B    21.4 ns   356.57 MiB/s
// BM_Produce/8KB                 8192B    93.3 ns    81.77 GiB/s
// BM_RoundTripZeroCopy/8KB       8192B   159.0 ns    47.93 GiB/s
// BM_PollZeroCopyBatch/8KB(100)  8192B    26.6 ns   286.93 GiB/s  <-- PEAK
// BM_PollCopyBatch/8B(100)          8B     6.1 ns     1.22 GiB/s  <-- FASTEST
```

- `BM_Produce` is the producer alone: `Reserve`, copy the payload in, `Commit`. 21.4 ns for 8 bytes, 93.3 ns for 8 KB.
- `BM_RoundTripZeroCopy` is one produce followed by one `PollZeroCopy` and `Advance`: 159 ns for 8 KB.
- `BM_PollZeroCopyBatch/8KB(100)` reads 100 pre-written 8 KB messages with `PollZeroCopy` + `Advance` each: 26.6 ns per message, which the slide converts to **286.93 GiB/s**, "PEAK".
- `BM_PollCopyBatch/8B(100)` reads 100 pre-written 8-byte messages with one `PollCopyBatch` call: **6.1 ns** per message, "FASTEST".

The bar chart on the same slide draws the lesson the speaker emphasises: for 8 KB messages, zero-copy batching (286.93 GiB/s) is 7.1x faster than copying round trips (40.23 GiB/s, 190 ns); for 8-byte messages, copying in a batch (6.1 ns) is 4.3x faster than single zero-copy reads (26.0 ns). "Zero-copy destroys copy for >512B; Batched copy destroys zero-copy for tiny ticks!"

### The speaker's explanation: batching

**Listing 20** — code from the talk (Bus Contention & Batch Amortization), shown at [7:13](https://www.youtube.com/watch?v=zkjz_v9E-Qg&t=433s)

```cpp
// Batch Amortization in PollBatch():
while (messages_read < max_messages && current < commit) {
  // 1. Read message from memory mirror into preallocated buffer (No bus locks)
  msg.payload.assign(payload_src, payload_src + length);
  current += total_frame_size;
  messages_read++;
}

// 2. AMORTIZED COMMIT: Execute exactly ONE atomic CAS for the entire batch!
if (messages_read > 0) {
  Advance(reader, total_bytes_advanced); // 1 single atomic cursor write!
}
// Result: 99% reduction in bus locking overhead!
```

- The loop copies up to `max_messages` messages out of the mirrored buffer into a pre-allocated vector of messages (`msg.payload.assign(...)` reuses each message's existing capacity, so nothing is allocated), advancing a local `current` cursor.
- Only after the loop does it call `Advance` once, for the total number of bytes consumed: "Execute exactly ONE atomic CAS for the entire batch!"

The speaker's account of why this wins: "With individual zero-copy reads, updating the cursor for every single message requires an atomic lock compare and swap instruction that swamps the CPU interconnect bus. By reading 100 messages and executing just one single atomic commit, we amortized that bus synchronization cost by 99%." And on the slide: "For tiny payloads, copying 8 bytes in L1 cache is virtually free compared to a cross-core bus lock."

The core idea is sound and important: when the per-message bookkeeping costs more than the message, amortize the bookkeeping over a batch. We will see below that the bookkeeping that dominates is not quite the one the slide names.

### Every measurement command from the video

The video gives two versions of the reproduction commands. The video description:

- `git clone https://github.com/ktgorekore/hotpathcplusplus.git`
- `cd hotpathcplusplus`
- `bazel run -c opt //:benchmark`
- `perf stat -e cycles,instructions,cache-misses,branch-misses ./bazel-bin/common/shared_memory/broadcast_ring_buffer_benchmark`

And the terminal in the final scene:

**Listing 21** — code from the talk (Hardware Alignment & Silicon Realities), shown at [7:43](https://www.youtube.com/watch?v=zkjz_v9E-Qg&t=463s)

```bash
bazel run -c opt //common/shared_memory:broadcast_ring_buffer_benchmark
perf stat -e cycles,instructions,cache-misses,branch-misses ./broadcast_ring_buffer_benchmark
```

- `bazel run -c opt` builds the target in optimised mode and runs it. In the repository's `BUILD.bazel`, `//:benchmark` is an alias for `//common/shared_memory:broadcast_ring_buffer_benchmark`, so both forms run the same program. The repository's `.bazelrc` adds `-std=c++20 -O3 -march=native`.
- `perf stat -e cycles,instructions,cache-misses,branch-misses` runs the program and reports four hardware counters: CPU cycles, instructions retired, `cache-misses` (perf's generic cache-miss event, which the kernel maps to a CPU-specific event; on Intel it counts last-level-cache misses) and mispredicted branches.

The output shown on screen:

**Listing 22** — code from the talk (Hardware Alignment & Silicon Realities), shown at [7:43](https://www.youtube.com/watch?v=zkjz_v9E-Qg&t=463s)

```console
Performance counter stats for './broadcast_ring_buffer_benchmark':
    48,291,048,192      cycles                    #    5.120 GHz
   183,506,083,129      instructions              #    3.80  insn per cycle
           142,019      cache-misses              #    0.04% of all L1 cache
            52,810      branch-misses             #    0.02% of all branches

  10.001429810 seconds time elapsed (Throu
```

- 48.3 billion cycles at 5.12 GHz and 183.5 billion instructions: **3.80 instructions per cycle**, very high, meaning the core was rarely stalled.
- 142,019 cache misses, labelled "0.04% of all L1 cache", and 52,810 branch misses (0.02% of branches).
- 10.0 seconds elapsed. The last line is cut off as the scene ends.

The final slide's "four pillars" sum up the speaker's thesis: MMU double mapping (zero bounds checks), the `alignas(64)` moat (zero false sharing), SeqLock masking (zero writer locks) and batch amortization ("Single atomic CAS per batch drops message latency to 6.1 nanoseconds"). In the speaker's closing words, extreme throughput "isn't about clever algorithmic complexity. It's about aligning your code with the physical realities of the underlying silicon."

### Re-running it ourselves

We ran the same code and the same commands on a very different machine: an Intel Core i7-7820HQ laptop (Kaby Lake, 4 cores / 8 threads, 3.9 GHz turbo, 8 MiB L3), Linux 7.0, GCC 13.4. Two changes were needed:

- **CMake instead of Bazel.** Bazel was not installed, and it is only the build tool here, so we wrote a `CMakeLists.txt` with the same targets and the same flags as `.bazelrc`. The code depends on Abseil, GoogleTest and Google Benchmark. Ubuntu's packaged Abseil (20220623) predates the `absl/log` library the benchmark includes, so we installed Abseil 20260526.0, the version pinned in the repository's `MODULE.bazel`, into `/usr/local`, built with `-DCMAKE_CXX_STANDARD=20`.
- **The binary path.** The `perf stat` command is identical; only `./bazel-bin/...` becomes `./out/...`.

**Listing 23** — Building with CMake and running the video's commands *(illustrative example, not from the talk)*

```bash
# One-time: Abseil 20260526.0 (the version pinned in MODULE.bazel) into /usr/local
git clone --depth 1 --branch 20260526.0 https://github.com/abseil/abseil-cpp.git /tmp/abseil-cpp
cmake -S /tmp/abseil-cpp -B /tmp/abseil-cpp/build -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_STANDARD=20 -DABSL_PROPAGATE_CXX_STD=ON \
      -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DABSL_BUILD_TESTING=OFF \
      -DCMAKE_INSTALL_PREFIX=/usr/local
cmake --build /tmp/abseil-cpp/build -j"$(nproc)"
sudo cmake --install /tmp/abseil-cpp/build

# The repository, built with the CMakeLists.txt in bench/ (same flags as .bazelrc)
git clone https://github.com/ktgorekore/hotpathcplusplus.git
cmake -S bench -B bench/out -DHOTPATH_REPO=$PWD/hotpathcplusplus -DCMAKE_CXX_COMPILER=g++-13
cmake --build bench/out -j6
./bench/out/memory_map_test && ./bench/out/broadcast_ring_buffer_test

# The video's benchmark and its perf stat command, with our binary path
./bench/out/broadcast_ring_buffer_benchmark --benchmark_repetitions=3 --benchmark_report_aggregates_only=true
perf stat -e cycles,instructions,cache-misses,branch-misses ./bench/out/broadcast_ring_buffer_benchmark
```

Both test suites passed (3 `memory_map` tests, 7 ring-buffer tests), and the code built without warnings under the repository's `-Wall -Wextra -Wpessimizing-move`.

### What we measured

**The repository benchmark** (mean of 3 repetitions; per-message times for the batch benchmarks are the batch time divided by 100):

| Benchmark | Video (Ryzen 9 9950X) | Our laptop (i7-7820HQ) |
|---|---|---|
| Produce, 8 B | 21.4 ns | 39.2 ns |
| Produce, 8 KB | 93.3 ns (81.77 GiB/s) | 678 ns (11.3 GiB/s) |
| RoundTripZeroCopy, 8 KB | 159.0 ns (47.93 GiB/s) | 778 ns (9.8 GiB/s) |
| RoundTripCopy, 8 KB | 190 ns (40.23 GiB/s) | 872 ns (8.8 GiB/s) |
| PollZeroCopyBatch, 8 B, per message | 26.0 ns (293 MiB/s) | 49.5 ns (154 MiB/s) |
| PollZeroCopyBatch, 8 KB, per message | 26.6 ns (286.93 GiB/s) | 81.4 ns (95.8 GiB/s) |
| PollCopyBatch, 8 B, per message | 6.1 ns (1.22 GiB/s) | 23.7 ns (323 MiB/s) |
| PollCopyBatch, 8 KB, per message | not shown | 507 ns (15.1 GiB/s) |

The shape survives: zero-copy batching is fastest for 8 KB messages, and for 8-byte messages copying in a batch beats one zero-copy read at a time, by 2.1x here instead of 4.3x. The large-message gap between the machines has a simple cause: the benchmark's ring is 16 MiB. That fits in one of the 9950X's 32 MiB L3 caches, but not in this laptop's 8 MiB L3, so here the 8 KB benchmarks stream from DRAM.

**The video's `perf stat` command**, on the full benchmark binary:

| Counter | Video | Our laptop |
|---|---|---|
| cycles | 48,291,048,192 | 171,073,131,304 |
| instructions | 183,506,083,129 | 160,000,763,990 |
| instructions per cycle | 3.80 | 0.94 |
| cache-misses | 142,019 | 4,856,972,469 |
| branch-misses | 52,810 | 39,820,832 |
| elapsed | 10.0 s | 45.7 s (43.1 s user, 2.5 s system) |

These are whole-program totals, so they include benchmark setup and Google Benchmark's own calibration runs, and the video does not say exactly how its run was configured. The `cache-misses` figure is not comparable across vendors at all, since perf maps the generic event to different hardware events on AMD and Intel. On this laptop, with a 16 MiB ring and an 8 MiB L3, billions of last-level misses and an IPC below 1 are what we would expect from benchmarks that copy megabytes through DRAM.

### What the benchmark measures, and what it does not

Reading the benchmark source changes how its headline numbers should be read.

**It is one thread in one process.** The fixture creates the producer and the consumer in the same process, and each benchmark calls `Reserve`/`Commit` and then `PollZeroCopy`/`Advance` from the same thread. No message crosses a core, let alone a process. So the benchmark measures the *API overhead* of the ring buffer, which is useful, but it is not an IPC latency or bandwidth measurement.

**Zero-copy throughput counts bytes that are never read.** `PollZeroCopyBatch` calls `benchmark::DoNotOptimize(*status)` on the returned span, which keeps the call from being optimised away but never touches the payload, then reports `bytes_processed = payload size × messages`. Returning a span to 8 KB costs about the same as returning a span to 8 bytes, so the "GiB/s" figure grows with the payload size. 286.9 GiB/s means "the cost of handing out a pointer, divided into 8 KB". It is not data moved through memory: for scale, our whole laptop's DRAM manages about 14 GB/s from one core.

**There is no cross-core bus contention in a single thread.** A `lock cmpxchg` on a cache line that sits in the core's own L1 cache does not go out on the interconnect. We measured an uncontended compare-and-swap at **4.8 ns**. The bus storms of the false-sharing section require two cores fighting over one line.

So where does the per-message time go? We profiled the 8-byte zero-copy batch with `perf record`:

**Listing 24** — Our additional measurement commands *(illustrative example, not from the talk)*

```bash
B=./bench/out/broadcast_ring_buffer_benchmark

# Where does the time go? Sample the 8-byte zero-copy batch, by symbol and by library
perf record -g -o pzc8.data $B --benchmark_filter='PollZeroCopyBatch/8$' --benchmark_min_time=2s
perf report -i pzc8.data --no-children --sort symbol --stdio
perf report -i pzc8.data --no-children --sort dso --stdio

# Does the hot path make system calls? Compare two iteration counts
strace -c -f $B --benchmark_filter='RoundTripZeroCopy/8$' --benchmark_min_time=1000x
strace -c -f $B --benchmark_filter='RoundTripZeroCopy/8$' --benchmark_min_time=10000x
strace -c -f $B --benchmark_filter='PollZeroCopyBatch/8$' --benchmark_min_time=1000x

# Cost of the individual operations inside Advance()
taskset -c 2 ./bench/out/sync_costs_benchmark

# Real process-to-process latency, and the kernel baseline
./bench/out/ring_cross_process_latency
./bench/out/kernel_ipc_latency

# False sharing: the video's counters, then the tool that actually sees it
perf stat -e cycles,instructions,cache-misses,branch-misses ./bench/out/false_sharing packed rmw
perf stat -e cycles,instructions,cache-misses,branch-misses ./bench/out/false_sharing aligned rmw
perf c2c record ./bench/out/false_sharing packed rmw
perf c2c report --stdio
```

Grouped by library, **30% of samples are in the vDSO** (the kernel-provided page that implements `clock_gettime`), and the top symbols include `absl::Now()` (14%), `GetCurrentTimeNanos`, `ToUnixNanos` and `clock_gettime`. Together, timekeeping is about 40% of the run. The source of it is the heartbeat: every `Advance` executes `heartbeat_ts.store(absl::ToUnixNanos(absl::Now()))`. A small Google Benchmark of the individual operations (ours, in `bench/sync_costs_benchmark.cc`) puts numbers on it:

| Operation (single thread, our laptop) | Cost |
|---|---|
| `absl::ToUnixNanos(absl::Now())` | 25.9 ns |
| `clock_gettime(CLOCK_MONOTONIC)` (vDSO) | 16.1 ns |
| uncontended `compare_exchange_strong` | 4.8 ns |
| `store(release)` | 0.3 ns |
| `state.PauseTiming(); state.ResumeTiming();` | 709 ns |

An unbatched read pays one clock read and one CAS per message, about 31 ns; a batch of 100 pays them once. That is the real amortization, and it is mostly a *clock read* being amortized, not a bus lock.

The last row matters too. The batch benchmarks call `state.PauseTiming()` and `state.ResumeTiming()` once per batch (to exclude the producing of the 100 messages). Google Benchmark reads the process CPU-time clock for this, `clock_gettime(CLOCK_PROCESS_CPUTIME_ID)`, which unlike the monotonic clock is a real system call. `strace -c` confirms it: the 8-byte zero-copy batch benchmark made 2,002 `clock_gettime` system calls for 1,000 iterations and 20,002 for 10,000. That costs about 709 ns per batch on our laptop, which is **7.1 ns of every per-message number** in the batch rows above, and the Google Benchmark documentation warns that these calls carry significant overhead. Part of the "6.1 ns" is measuring the stopwatch.

**The hot path really makes no system calls.** The same `strace -c` run on `RoundTripZeroCopy` reported 295 system calls in total whether we ran 1,000 or 10,000 iterations: all of them setup (`shm_open`, `ftruncate`, `mmap` and so on, since the fixture creates a fresh 16 MiB segment for every benchmark run, which also accounts for the 2.5 s of system time in `perf stat`). Producing and consuming messages never entered the kernel. That is the video's central claim, and it holds.

### Measuring real inter-process latency

To see the design doing actual IPC, we wrote a small test (in `bench/`) that uses the repository's library unchanged: a producer process on one core stamps each 8-byte message with `__rdtsc()`, and a separate consumer process on another physical core attaches to the same shared memory, polls with `PollZeroCopy`, and records the delay. The TSC on this CPU is invariant and synchronized across cores, so timestamps from different cores compare directly. The producer sends a message every ~2 µs so that we measure latency rather than queueing.

**Listing 25** — Measuring real cross-process latency through the ring buffer *(illustrative example, not from the talk)*

```cpp
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
```

For the baseline the speaker contrasts it with, we also ping-ponged 8-byte messages between two processes over a pipe pair and over a Unix stream socket pair, also pinned to two cores, and halved the round trip:

| One-way, 8 bytes, process to process | p50 | p99 | p99.99 |
|---|---|---|---|
| Pipe | 3,110 ns | 11,100 ns | 25,500 ns |
| Unix domain socket | 3,470 ns | 14,000 ns | 63,000 ns |
| `BroadcastRingBuffer` (shared memory) | **91-108 ns** | 290-1,600 ns | 18,000-21,000 ns |

(Ranges are across three runs; the kernel figures varied by less than 1% at p50.) The shared-memory ring buffer delivers a message from one process to another in about 100 ns at the median, roughly **30 times faster than a pipe** on this machine, and its p99 is well under the pipe's median. That 100 ns is mostly the cache-line transfer between cores plus the clock read in `Advance`. It is the number to compare with the kernel's ~1.2 µs (video) or ~3.1 µs (our laptop), not the 6.1 ns, which is a single-thread, amortized API cost. The p99.99 tails of all three are dominated by the operating system interrupting the threads, which pinning alone does not prevent.

### Summing up

The video's design holds up well where it matters most. Shared memory with a mirrored ring removes the kernel from the data path, `strace` confirms zero system calls per message, and a real process-to-process hand-off costs about 100 ns instead of several microseconds. The engineering ideas, the two segments, the mirror, padding against false sharing, the seqlock, backpressure with eviction and generation counters, are the right ones.

The measurements need more care than the narration gives them. 286.9 GiB/s and 6.1 ns are single-thread API costs, one of them for bytes that are never read. The batch speedup comes mostly from fewer clock reads, not from relieving bus contention. And the padding moat is two cache lines, not one. None of this diminishes the design. It is what the speaker's own closing advice, "aligning your code with the physical realities of the underlying silicon", looks like when it is applied to the measurements as well.

## Key takeaways

- Shared memory removes the kernel from the data path: our cross-process latency was about 95 ns at the median versus about 3.1 us for a pipe.
- Mapping the same pages twice lets the MMU handle ring-buffer wraparound, so every message is one contiguous span.
- Cache-line padding only works if you count alignment padding too: check layouts with static_assert and offsetof.
- A sequence lock needs acquire on the reader side and release on the writer side; fences and orderings are the whole algorithm.
- A broadcast queue with backpressure needs a policy for dead consumers, and slot reuse needs ABA protection.
- Single-threaded micro-benchmarks measure API overhead, not IPC; zero-copy throughput figures count bytes that were never read.
- Pick perf events that can see the effect you care about: perf c2c finds false sharing that cache-misses does not.

## Glossary

| Term | Meaning |
|---|---|
| IPC | Inter-process communication: any mechanism for separate processes to exchange data. |
| System call (syscall) | A request from a user program to the kernel, which switches the CPU into kernel mode. |
| Context switch | The kernel stopping one thread or process and running another, saving and restoring registers. |
| vDSO | A small shared library the kernel maps into every process so some calls (like clock_gettime for most clocks) run without a syscall. |
| POSIX shared memory | Named memory objects created with shm_open (files under /dev/shm on Linux) that several processes can mmap. |
| mmap | The system call that maps a file or memory object into a process's virtual address space. |
| MAP_FIXED | An mmap flag that places the mapping at exactly the given address, replacing anything mapped there. |
| Page | The unit of virtual memory mapping, 4096 bytes by default on x86-64 Linux. |
| Magic mirror (double mapping) | Mapping the same physical pages at two adjacent virtual ranges so a ring buffer never needs to wrap. |
| SPMC | Single producer, multiple consumers; in a broadcast queue every consumer receives every message. |
| Cache line | The 64-byte unit in which CPU caches store and transfer memory. |
| MESI | A cache-coherence protocol whose line states are Modified, Exclusive, Shared and Invalid. |
| False sharing | Slowdown caused by threads writing different variables that happen to share one cache line. |
| HITM | A load that hits a cache line held Modified in another core's cache; the signature of sharing, counted by perf c2c. |
| Sequence lock (seqlock) | A writer-preferring synchronisation scheme where readers retry if a version counter is odd or changed during the read. |
| Acquire / release | Memory orderings: a release store publishes earlier writes; an acquire load that sees it makes them visible. |
| Store buffer | A per-core queue where stores wait before reaching the cache, letting the core continue executing. |
| Backpressure | Slowing or stopping a producer when consumers fall behind, instead of dropping data. |
| Heartbeat | A timestamp a consumer updates regularly to prove it is alive. |
| TSC / __rdtsc() | The CPU's time-stamp counter and the intrinsic that reads it, without a system call. |
| ABA problem | A value changes from A to B and back to A, fooling a check that only compares values; fixed with a version or generation counter. |
| CAS | Compare-and-swap: atomically replace a value only if it still equals an expected value (lock cmpxchg on x86). |
| Zero-copy | Handing the consumer a pointer or span into the shared buffer instead of copying the bytes out. |
| IPC (perf) | In perf output, instructions per cycle: how much useful work the core retires per clock. |

## Exercises

1. Why must the magic-mirror ring buffer's capacity be a multiple of the page size? What exactly fails if it is not?

   <details><summary>Hint</summary>mmap maps whole pages, and the mirror must start exactly `capacity` bytes after the base.</details>

2. Compute sizeof and the field offsets of the slide's ReaderSlot by hand, then fix the struct so the static_assert holds.

   <details><summary>Hint</summary>std::atomic<uint64_t> is 8-byte aligned, so the bool is followed by padding.</details>

3. In the seqlock reader, why is an acquire load of the version after the data reads not enough on its own? What does a correct reader need?

   <details><summary>Hint</summary>Acquire prevents later operations from moving earlier, not earlier ones from moving later.</details>

4. Describe an interleaving in which an evicted consumer overwrites the cursor of the consumer that took its slot, despite the generation check in Advance.

   <details><summary>Hint</summary>The CAS and the cursor store are two separate atomic operations.</details>

5. The zero-copy batch benchmark reports 286.9 GiB/s for 8 KB messages. What would you change so that the number measures data actually delivered to a consumer?

   <details><summary>Hint</summary>Touch the payload, and put the consumer in another process on another core.</details>

6. Why does `perf stat -e cache-misses` barely change when you introduce false sharing on an Intel CPU? Which tool or event would show it?

   <details><summary>Hint</summary>Where is a line found when another core holds it Modified?</details>

## Further reading

- Hot Path C++ companion repository: github.com/ktgorekore/hotpathcplusplus (common/shared_memory)
- POSIX shared memory and mmap(2) man pages (shm_open, MAP_FIXED, MAP_SHARED, PROT_NONE)
- Abseil (absl::Status, absl::StatusOr, absl::Span, absl::Now)
- Google Benchmark
- Linux perf (perf stat)
- Bazel
