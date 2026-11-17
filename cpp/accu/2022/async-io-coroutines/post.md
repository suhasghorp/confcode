---
title: "Asynchronous I/O and Coroutines: Streaming Data with io_uring and C++20"
date: 2026-11-14
tags: ["C++", "C++20", "coroutines", "io_uring", "Linux", "asynchronous I/O", "networking", "ACCU"]
source: "https://www.youtube.com/watch?v=54oGgmMAUNk"
video_title: "Asynchronous I/O and Coroutines for Smooth Data Streaming - Björn Fahller - ACCU 2022"
channel: "ACCU Conference"
---

> This chapter is based on the talk [Asynchronous I/O and Coroutines for Smooth Data Streaming - Björn Fahller - ACCU 2022](https://www.youtube.com/watch?v=54oGgmMAUNk) (ACCU Conference). Code listings marked *from the talk* are transcribed from the slides; listings marked *illustrative* were written for this chapter to explain the ideas.

A network service spends most of its life waiting: for packets to arrive, for the kernel to copy them, for the next chunk of work. How a program waits, and what it does with the data once it arrives, decides how fast it can go and how readable its code stays.

In this ACCU 2022 talk, Björn Fahller, who works on networking and video-streaming equipment at Net Insight, sets out to combine two newer tools. **io_uring** is the Linux kernel's completion-based asynchronous I/O interface, which avoids most system calls and copies. **C++20 coroutines** let you write asynchronous processing as plain-looking loops. He builds a UDP packet-processing program three times: first with classic `poll`/`read`, then with io_uring callbacks, and finally as a pipeline of coroutines fed directly from the ring and composed with a pipe operator.

He is candid about the result. Coroutines are powerful, but the standard library gives you almost nothing to build them with, the types you must write are "mindbogglingly hard to understand", and debugging lifetime bugs is "laughably bad". This chapter follows his journey step by step and adds the background a student needs: file descriptors and readiness versus completion I/O, how io_uring's queues work, what the compiler does with a coroutine, and what each piece of the promise/awaitable protocol is for. The code was checked by compiling it; where the slide code needs a change to compile today, the chapter says so.

## What you will learn

- Explain readiness-based I/O (select/poll/epoll + read) and write a small poll-based dispatcher
- Describe io_uring's submission and completion queues and the liburing calls used to queue reads and reap completions
- Explain why io_uring reduces system calls and copies, and how to measure system calls with strace -c
- Describe the mental model of a coroutine as a resumable loop whose locals live in a heap-allocated frame
- Write the three types a C++20 coroutine needs here: a return object (task), a promise type and an awaitable
- Trace the exact sequence of calls the compiler makes when a coroutine is created, suspended and resumed
- Build a push-driven pipeline of coroutines using co_await to read upstream and co_yield to write downstream
- Recognise coroutine lifetime pitfalls (dangling references, lambda coroutines) and design stages as values
- Compose pipeline stages with an operator| built from a small inheritance-based wrapper
- Connect an io_uring completion callback to a coroutine pipeline, and judge the remaining rough edges (cancellation, debugging, missing library support)

## Before you start

- Comfortable with modern C++ (templates, lambdas, move semantics, std::unique_ptr)
- Basic idea of what a system call and a file descriptor are on Linux
- No prior experience with io_uring or coroutines is needed
- To build the io_uring examples you need Linux 5.1+ and the liburing development package

## Contents

1. [Why asynchronous I/O and coroutines?](#1-why-asynchronous-io-and-coroutines) — [0:00](https://www.youtube.com/watch?v=54oGgmMAUNk&t=0s)
2. [Readiness-based I/O: poll and read](#2-readiness-based-io-poll-and-read) — [0:54](https://www.youtube.com/watch?v=54oGgmMAUNk&t=54s)
3. [Demo: counting packets the classic way](#3-demo-counting-packets-the-classic-way) — [3:35](https://www.youtube.com/watch?v=54oGgmMAUNk&t=215s)
4. [io_uring: completion-based I/O through shared rings](#4-iouring-completion-based-io-through-shared-rings) — [6:20](https://www.youtube.com/watch?v=54oGgmMAUNk&t=380s)
5. [A ring class, and why it is faster](#5-a-ring-class-and-why-it-is-faster) — [9:10](https://www.youtube.com/watch?v=54oGgmMAUNk&t=550s)
6. [Demo: ring::wait and counting system calls](#6-demo-ringwait-and-counting-system-calls) — [12:20](https://www.youtube.com/watch?v=54oGgmMAUNk&t=740s)
7. [Coroutines: loops that can pause](#7-coroutines-loops-that-can-pause) — [17:10](https://www.youtube.com/watch?v=54oGgmMAUNk&t=1030s)
8. [What the compiler makes of a coroutine](#8-what-the-compiler-makes-of-a-coroutine) — [19:15](https://www.youtube.com/watch?v=54oGgmMAUNk&t=1155s)
9. [The return object: task and owning the promise](#9-the-return-object-task-and-owning-the-promise) — [21:15](https://www.youtube.com/watch?v=54oGgmMAUNk&t=1275s)
10. [The promise type: the coroutine's control panel](#10-the-promise-type-the-coroutines-control-panel) — [23:05](https://www.youtube.com/watch?v=54oGgmMAUNk&t=1385s)
11. [Making a task awaitable](#11-making-a-task-awaitable) — [26:00](https://www.youtube.com/watch?v=54oGgmMAUNk&t=1560s)
12. [Demo: tracing the conversation between task, promise and awaitable](#12-demo-tracing-the-conversation-between-task-promise-and-awaitable) — [26:40](https://www.youtube.com/watch?v=54oGgmMAUNk&t=1600s)
13. [Pipelines: co_await upstream, co_yield downstream](#13-pipelines-coawait-upstream-coyield-downstream) — [30:30](https://www.youtube.com/watch?v=54oGgmMAUNk&t=1830s)
14. [Putting io_uring and coroutines together](#14-putting-iouring-and-coroutines-together) — [32:50](https://www.youtube.com/watch?v=54oGgmMAUNk&t=1970s)
15. [Lifetimes, and pipelines as values](#15-lifetimes-and-pipelines-as-values) — [38:40](https://www.youtube.com/watch?v=54oGgmMAUNk&t=2320s)
16. [Higher-order stages and a pipe operator](#16-higher-order-stages-and-a-pipe-operator) — [42:00](https://www.youtube.com/watch?v=54oGgmMAUNk&t=2520s)
17. [Feeding the pipeline from io_uring](#17-feeding-the-pipeline-from-iouring) — [46:20](https://www.youtube.com/watch?v=54oGgmMAUNk&t=2780s)
18. [Takeaways, debugging, and questions](#18-takeaways-debugging-and-questions) — [55:30](https://www.youtube.com/watch?v=54oGgmMAUNk&t=3330s)

## 1. Why asynchronous I/O and coroutines?

*Talk segment: [0:00](https://www.youtube.com/watch?v=54oGgmMAUNk&t=0s)*

Björn Fahller opens by explaining where he is coming from. His day job is at Net Insight in Stockholm, where they "make networking stuff": rack-mounted equipment full of plug-in boards, each with "at least two CPUs and at least one FPGA", plus network management systems and cloud-based video-streaming software. Moving data through networks quickly is his everyday work.

He wrote this talk "primarily out of curiosity". He has worked with networking for a long time but had not used **io_uring**, Linux's newer asynchronous I/O interface, and he was curious about **coroutines**, added to C++ in C++20. The talk is the record of diving into both and asking what they can do together. He also warns the audience that he finished it "slightly less enthusiastic" than when he wrote the abstract. That honesty is worth keeping in mind, because it is one of the most useful things the talk offers.

### The promise on the title slide

The title slide pairs an io_uring logo with a few lines of code:

**Listing 1** — code from the talk (Asynchronous I/O and coroutines for smooth data streaming), shown at [1:15](https://www.youtube.com/watch?v=54oGgmMAUNk&t=75s)

```cpp
#include <coroutine>
...
x = co_await source;
co_yield computation(x);
```

Those lines are the talk's thesis in miniature. Somewhere data arrives asynchronously (`source`). A piece of code waits for the next value with `co_await`, computes something, and passes the result on with `co_yield`. It reads like a loop over a container, but the "container" is the network, and the waiting happens without blocking a thread.

### Why not just use callbacks?

The talk's abstract names the problem it wants to avoid: **callback hell**. Asynchronous I/O makes a program responsive, but the usual way to consume asynchronous data is to register a callback, which processes the data and feeds it to another callback, and so on. Each step is a separate function. State that a loop would keep in local variables has to live in objects that the callbacks share, and the flow of control becomes hard to see.

Here is a small illustration of the contrast. Both versions turn incoming text fragments into lines of at most 20 characters, a stage you will meet again later in the talk. The callback version must keep its state in an object and spread the logic across a callback. The coroutine version keeps it in local variables of what looks like a single loop.

**Listing 2** — Callback state versus coroutine locals *(illustrative example, not from the talk)*

```cpp
#include <generator>
#include <iostream>
#include <string>
#include <vector>

// Callback style: state lives in an object, logic lives in a callback.
struct line_builder {
    std::size_t width;
    std::string current;
    void on_piece(const std::string& piece, auto&& emit)
    {
        if (current.size() + piece.size() + 1 > width) {
            emit(current);
            current = piece;
        } else if (current.empty()) {
            current = piece;
        } else {
            current += " " + piece;
        }
    }
};

// Coroutine style: state is just local variables in a loop.
std::generator<std::string> lines(std::vector<std::string> pieces, std::size_t width)
{
    std::string current;
    for (auto& piece : pieces) {
        if (current.size() + piece.size() + 1 > width) {
            co_yield current;
            current = piece;
        } else if (current.empty()) {
            current = piece;
        } else {
            current += " " + piece;
        }
    }
    if (!current.empty()) co_yield current;
}

int main()
{
    std::vector<std::string> words{"asynchronous", "io", "and", "coroutines", "for", "smooth", "streaming"};

    line_builder b{20, {}};
    for (auto& w : words) b.on_piece(w, [](const std::string& l) { std::cout << "cb: " << l << '\n'; });
    if (!b.current.empty()) std::cout << "cb: " << b.current << '\n';   // callbacks need an explicit end-of-stream step

    for (auto&& l : lines(words, 20)) std::cout << "co: " << l << '\n';
}
```

The coroutine version uses `std::generator` from C++23, the only coroutine return type the standard library provides. It is *pull*-driven: the consumer asks for the next value. The talk needs the opposite, a *push*-driven pipeline where the network pushes each packet through the stages as it arrives, and for that there is no standard type at all. Building one is a large part of the talk.

### The road map

The talk builds the same small program three times:

1. **Classic readiness-based I/O** with `poll` and `read`, as a baseline.
2. **io_uring** with callbacks, to see what the new kernel interface buys.
3. **Coroutines**: first the machinery (task, promise, awaitable), then pipelines of coroutine stages, and finally a pipeline fed directly from io_uring and written with a pipe operator.

Along the way the speaker measures system calls, runs live demos, and shows the bugs and rough edges he hit.

## 2. Readiness-based I/O: poll and read

*Talk segment: [0:54](https://www.youtube.com/watch?v=54oGgmMAUNk&t=54s)*

### Files, sockets and waiting

On Linux almost everything you read from or write to is a **file descriptor**: a small integer that identifies an open file, pipe, terminal or network socket inside your process. A UDP socket bound to port 4000 is one, and reading a packet from it is a `read` (or `recv`) on that descriptor.

A plain `read` on a socket with no data **blocks**: the thread sleeps until a packet arrives. That is fine for one socket. With many sockets you need to ask "which of these has something for me?" without blocking on any single one. That question is what readiness notification answers.

"Traditionally we use `select`/`poll`/`epoll` to register file descriptors we want to react to," the speaker summarises, "and `read`/`recv`/`recvmsg`/`recvmmsg` to read the data (and correspondingly to send)." `select` and `poll` take a list of descriptors on every call. `epoll` keeps the interest list inside the kernel, which scales better to thousands of descriptors. All three only tell you that a descriptor is **ready**. You still perform the read yourself, which is why this model is called **readiness-based I/O**.

### A poller in a dozen lines

The usual shape of such code is a small dispatcher that maps descriptors to callbacks:

**Listing 3** — code from the talk ((code slide)), shown at [3:01](https://www.youtube.com/watch?v=54oGgmMAUNk&t=181s)

```cpp
class poller {
public:
  using worker = std::function<void(std::span<char> data)>;
  void add(int fd, worker w) {
    fds_.push_back({fd, POLLIN, 0});
    cbs_.emplace(fd, std::move(w));
  }
  void wait() {
    auto r = poll(fds_.data(), fds_.size(), -1);
    for (auto& e : fds_) {
      if (e.revents & POLLIN) {
        char buffer[1500];
        auto len = ::read(e.fd, buffer, sizeof(buffer));
        cbs_[e.fd](std::span(buffer).first(len));
      }
    }
  }
private:
  std::vector<pollfd> fds_;
  std::map<int, worker> cbs_;
};
```

Walk through it:

- `using worker = std::function<void(std::span<char> data)>;` is the callback type. It receives the bytes of one packet as a `std::span<char>`, a non-owning view of a contiguous range.
- `add(fd, w)` records a `pollfd` entry `{fd, POLLIN, 0}` (descriptor, events we care about, returned events) and stores the callback in a map keyed by descriptor.
- `wait()` calls `poll` with a timeout of `-1`, meaning wait forever, until at least one descriptor is ready. It then loops over the entries. For each one whose `revents` has `POLLIN` set, it reads into a 1500-byte stack buffer and calls the callback with a span covering exactly the bytes received (`std::span(buffer).first(len)`).

About the magic number, the speaker admits: "that is because I'm a very lazy person." Real networking code would use proper message buffers, but that "just complicates matters so much" on a slide. 1500 bytes is the usual Ethernet MTU, so a UDP datagram on a local network fits. (A UDP datagram can in principle be up to 64 KiB; a larger one would be truncated by this read.)

Note also what the slide leaves out for clarity: the result of `poll` (`r`) and of `read` (`len`) are not checked for errors. Production code must handle `-1` and `errno`.

### What happens on the timeline

The speaker draws the life of a packet as a timeline:

1. The program calls `poll()` and sleeps.
2. A packet arrives. The kernel's IP stack **fills a buffer in kernel memory**.
3. `poll` returns; the program calls `read()`, which **copies** the data from the kernel buffer into the program's buffer.
4. The program **processes** the data. Meanwhile, more packets may arrive and fill kernel buffers in parallel.
5. The program calls `poll()` again.

So every packet costs at least two system calls (`poll` and `read`) and one copy from kernel to user memory. System calls are not free: each one crosses the user/kernel boundary. Copies cost memory bandwidth and pollute caches. For a few packets none of this matters. For high-rate streams it starts to dominate, and that is the motivation for io_uring later.

### The same idea with epoll

For comparison, here is the dispatcher written with `epoll`. The interest list now lives in the kernel (`epoll_ctl` once per descriptor), and `epoll_wait` returns only the ready descriptors instead of making us scan them all. The per-packet cost is unchanged, though: one wait and one read.

**Listing 4** — The poller rewritten with epoll *(illustrative example, not from the talk)*

```cpp
#include <sys/epoll.h>
#include <unistd.h>

#include <array>
#include <functional>
#include <map>
#include <span>
#include <stdexcept>

class epoller {
public:
    using worker = std::function<void(std::span<char>)>;

    epoller() : epfd_(epoll_create1(0))
    {
        if (epfd_ < 0) throw std::runtime_error("epoll_create1");
    }
    ~epoller() { ::close(epfd_); }
    epoller(const epoller&) = delete;
    epoller& operator=(const epoller&) = delete;

    void add(int fd, worker w)
    {
        epoll_event ev{};
        ev.events = EPOLLIN;
        ev.data.fd = fd;                       // returned to us in epoll_wait
        if (epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &ev) < 0) throw std::runtime_error("epoll_ctl");
        cbs_.emplace(fd, std::move(w));
    }

    void wait()
    {
        std::array<epoll_event, 16> events;
        int n = epoll_wait(epfd_, events.data(), static_cast<int>(events.size()), -1);
        for (int i = 0; i < n; ++i) {         // only ready descriptors are reported
            char buffer[1500];
            int fd = events[i].data.fd;
            auto len = ::read(fd, buffer, sizeof(buffer));
            if (len >= 0) cbs_[fd](std::span(buffer).first(static_cast<std::size_t>(len)));
        }
    }

private:
    int epfd_;
    std::map<int, worker> cbs_;
};

int main() { epoller e; (void)e; }
```

## 3. Demo: counting packets the classic way

*Talk segment: [3:35](https://www.youtube.com/watch?v=54oGgmMAUNk&t=215s)*

### The test program

To have something concrete to measure, the speaker writes a tiny program around the poller:

**Listing 5** — code from the talk (Live demo: poll_sum.cpp), shown at [4:05](https://www.youtube.com/watch?v=54oGgmMAUNk&t=245s)

```cpp
int main()
{
  auto port4000 = udp_socket("127.0.0.1", 4000);
  auto port4001 = udp_socket("127.0.0.1", 4001);

  uint64_t packets = 0;
  uint64_t sum = 0;
  bool done = false;

  auto calc_sum = [&](std::span<char> data) {
    ++packets;
    for (auto c : data) {
      sum += c;
    }
  };
  auto print_and_exit = [&](std::span<char>) {
    std::cout << "packets=" << packets << " sum=" << sum << '\n';
    done = true;
  };
  poller poll;
  poll.add(port4000.fd(), calc_sum);
  poll.add(port4001.fd(), print_and_exit);
  while (!done) {
    poll.wait();
  }
}
```

It opens two UDP sockets on the loopback address. `udp_socket` is a small helper from his demo code that the talk doesn't show; an equivalent is given below. Port 4000 carries data and port 4001 is a "stop" signal:

- `calc_sum` counts packets and adds up the byte values of their contents.
- `print_and_exit` prints the totals and sets `done`.

Both are lambdas capturing by reference (`[&]`), so they update the locals of `main`. The loop calls `poll.wait()` until a packet on port 4001 sets `done`.

Here is one way to write the missing helper: an RAII wrapper that creates a datagram socket, binds it to the address and port, and closes it in its destructor.

**Listing 6** — A minimal udp_socket helper (the talk uses one but doesn't show it) *(illustrative example, not from the talk)*

```cpp
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdint>
#include <stdexcept>
#include <utility>

class udp_socket {
public:
    udp_socket(const char* addr, std::uint16_t port)
        : fd_(::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))
    {
        if (fd_ < 0) throw std::runtime_error("socket");
        sockaddr_in sa{};
        sa.sin_family = AF_INET;
        sa.sin_port = htons(port);
        if (::inet_pton(AF_INET, addr, &sa.sin_addr) != 1) throw std::runtime_error("inet_pton");
        if (::bind(fd_, reinterpret_cast<sockaddr*>(&sa), sizeof sa) < 0) throw std::runtime_error("bind");
    }
    udp_socket(udp_socket&& o) noexcept : fd_(std::exchange(o.fd_, -1)) {}
    udp_socket& operator=(udp_socket&&) = delete;
    ~udp_socket() { if (fd_ >= 0) ::close(fd_); }

    int fd() const noexcept { return fd_; }

private:
    int fd_;
};

int main()
{
    auto port4000 = udp_socket("127.0.0.1", 4000);
    return port4000.fd() >= 0 ? 0 : 1;
}
```

### Watching the system calls with strace

"strace is an excellent debugging tool in Linux." It runs a program and prints every system call it makes, with arguments and return values. Running the demo under `strace` shows the start-up calls (`socket`, `bind` for each port) and then:

```
poll([{fd=3, events=POLLIN}, {fd=4, events=POLLIN}], 2, -1
```

The program is now asleep inside `poll`, waiting for either socket.

### Sending packets from the shell

To send test packets, the speaker uses "another cool Linux thing, or actually it's a bash shell thing": bash treats paths of the form `/dev/udp/HOST/PORT` specially in redirections. Writing to one sends the output as a UDP datagram:

```
echo aa > /dev/udp/127.0.0.1/4000
```

Each `echo` makes `poll` return, after which strace shows `read(3, "aa\n", 1500) = 3` and another `poll`. After a few packets to port 4000 and one to port 4001, the program prints:

```
packets=3 sum=612
```

(Three packets of `"aa\n"` are 3 × (97 + 97 + 10) = 612.)

"Okay, nothing remarkable in any way." The point of the demo is the pattern in the trace: **poll, read, poll, read**, two system calls for every packet. Keep that number in mind; the io_uring demo will be compared against it.

### A C++ test sender

The bash trick is handy interactively. For repeatable tests you may prefer a small sender program:

**Listing 7** — Sending N test datagrams from C++ *(illustrative example, not from the talk)*

```cpp
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <string_view>

int main()
{
    int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in to{};
    to.sin_family = AF_INET;
    to.sin_port = htons(4000);
    ::inet_pton(AF_INET, "127.0.0.1", &to.sin_addr);

    std::string_view msg = "aa\n";
    for (int i = 0; i < 3; ++i) {
        ::sendto(fd, msg.data(), msg.size(), 0, reinterpret_cast<sockaddr*>(&to), sizeof to);
    }
    // and the "stop" packet
    to.sin_port = htons(4001);
    ::sendto(fd, "x", 1, 0, reinterpret_cast<sockaddr*>(&to), sizeof to);
    ::close(fd);
}
```

## 4. io_uring: completion-based I/O through shared rings

*Talk segment: [6:20](https://www.youtube.com/watch?v=54oGgmMAUNk&t=380s)*

### A different model: completion instead of readiness

With `poll`, the kernel tells you *when you may read*, and then you read. **io_uring** reverses the order. You hand the kernel the whole operation up front: "read from this descriptor into this buffer". The kernel then tells you *when it is done*. This is **completion-based I/O**, the model Windows has long used with I/O completion ports, and the one io_uring brought to Linux in a general form.

"io_uring arrived in the Linux kernel 5.1," the speaker says, and he describes what it is: "a shared memory data structure, shared between the kernel and your user space process … lock-free, and it models two queues: a **submission queue** and a **completion queue**."

| Queue | Written by | Read by | Contains |
|---|---|---|---|
| Submission queue (SQ) | your program | the kernel | **SQEs**: requests such as "read fd 3 into this buffer" |
| Completion queue (CQ) | the kernel | your program | **CQEs**: results, i.e. the return value plus the request's user data |

Because both rings live in memory that both sides can see, queuing a request and collecting a result are just memory writes and reads. A system call (`io_uring_enter`) is only needed to tell the kernel "there is new work" or to sleep until completions arrive, and one call can cover many requests. You can use it for "IP I/O and file system I/O", the two cases on his diagram, labelled *IP stack* and *Filesystem*.

### Two APIs, neither very high level

"You have two APIs for io_uring." The raw interface is "basically just a description of how this lock-free shared memory data structure works, and then it's up to you to not wreak too much havoc on the system." Most people use **liburing**, a C library that wraps it in functions. The speaker calls it "the so-called high-level API, which … is really not high level. `void*`s, just saying." It is a C API, after all.

### The life of a request

The slide shows the essential calls in order:

**Listing 8** — code from the talk (io_uring), shown at [8:51](https://www.youtube.com/watch?v=54oGgmMAUNk&t=531s)

```cpp
#include <liburing.h>

io_uring uring;
io_uring_queue_init(8, &uring, 0);

...

auto entry = io_uring_get_sqe(&uring);
io_uring_prep_read(entry, fd, ptr, size, 0);
io_uring_sqe_set_data(entry, work);

...

io_uring_cqe* entry;
auto e = io_uring_wait_cqe(&uring, &entry);
```

**Setting up.** `io_uring_queue_init(8, &uring, 0)` creates the rings. "The size is the maximum number of pending requests that you can have": it sizes the submission and completion queues, and "you cannot grow it" afterwards. The last argument holds flags, unused here.

**Queuing a read.** `io_uring_get_sqe(&uring)` returns a pointer to a free submission queue entry, or null if the ring is full. `io_uring_prep_read(entry, fd, ptr, size, 0)` fills it in: read up to `size` bytes from `fd` into `ptr`. The final `0` is the file offset, which is ignored for sockets. The speaker calls it "just a flag that we can ignore right now".

**Remembering what the request was for.** `io_uring_sqe_set_data(entry, work)` attaches a 64-bit **user data** value to the request, and the kernel copies it into the matching completion. That is how you recognise which request finished, because completions may arrive in any order. The slide's callout: "Annoyingly, only one word, 64 bits, to express the operation and the data, so an indirection is almost always needed." In practice you store a pointer to a per-request state object.

**Collecting results.** `io_uring_wait_cqe(&uring, &entry)` blocks until a completion is available and gives you a pointer to it. The CQE's `res` field holds what the system call would have returned: the number of bytes read, or a negative error code. Not shown on the slide but essential: you must call `io_uring_submit` to hand queued SQEs to the kernel (`io_uring_submit_and_wait` combines submitting and waiting), and you must mark each CQE as consumed with `io_uring_cqe_seen` so its slot can be reused.

Here is a complete minimal program that reads one datagram this way. It needs the liburing development headers (`liburing-dev` on Debian and Ubuntu) and linking with `-luring`.

**Listing 9** — Reading one UDP datagram with liburing (requires liburing-dev, link with -luring) *(illustrative example, not from the talk)*

```cpp
#include <liburing.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <cstdio>

int main()
{
    // A UDP socket on 127.0.0.1:4000.
    int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in sa{};
    sa.sin_family = AF_INET;
    sa.sin_port = htons(4000);
    ::inet_pton(AF_INET, "127.0.0.1", &sa.sin_addr);
    ::bind(fd, reinterpret_cast<sockaddr*>(&sa), sizeof sa);

    io_uring ring;
    if (io_uring_queue_init(8, &ring, 0) < 0) return 1;      // SQ/CQ with 8 entries

    std::array<char, 1500> buffer;                         // must outlive the request
    io_uring_sqe* sqe = io_uring_get_sqe(&ring);
    io_uring_prep_read(sqe, fd, buffer.data(), buffer.size(), 0);
    io_uring_sqe_set_data(sqe, buffer.data());             // our 64 bits of context

    io_uring_submit(&ring);                                // one system call

    io_uring_cqe* cqe = nullptr;
    if (io_uring_wait_cqe(&ring, &cqe) == 0) {             // sleeps until completion
        if (cqe->res >= 0) {
            std::printf("read %d bytes: %.*s", cqe->res, cqe->res,
                        static_cast<char*>(io_uring_cqe_get_data(cqe)));
        }
        io_uring_cqe_seen(&ring, cqe);                     // free the CQ slot
    }

    io_uring_queue_exit(&ring);
    ::close(fd);
}
```

> **Readiness vs completion in one sentence.** With `poll` the kernel says "you can read now"; with io_uring the kernel says "I have read it, here it is".

## 5. A ring class, and why it is faster

*Talk segment: [9:10](https://www.youtube.com/watch?v=54oGgmMAUNk&t=550s)*

### Wrapping the ring in a class

To compare like with like, the speaker wraps io_uring in a class with the same shape as the poller: you `add` a descriptor with some work, and you `wait`.

**Listing 10** — code from the talk (uring), shown at [9:13](https://www.youtube.com/watch?v=54oGgmMAUNk&t=553s)

```cpp
#include <liburing.h>

class ring
{
public:
  using work = std::function<bool(std::span<char>)>;
  ring();
  ring& operator=(ring&&) = delete;
  ~ring();

  void add(int fd, work);
  void wait();
private:
  struct read_work;
  std::list<read_work> pending_;
  io_uring uring_;
};
```

There are a few differences from the poller:

- The callback type is `std::function<bool(std::span<char>)>`. It now returns a `bool`: `true` means "keep reading this descriptor", `false` means "stop". With io_uring every read is a separate request, so after each completion we decide whether to queue the next one.
- The class is not movable (`operator=(ring&&) = delete`, which also suppresses the implicit copy operations). That matters because the kernel holds pointers into it.
- Pending requests are kept in a `std::list<read_work>`, and the class owns an `io_uring` instance.

### The buffer must exist before the read

The most important change is where the buffer lives. With `poll`, the buffer could be a local array created only after we knew data was ready. With io_uring that is impossible. The kernel writes the data directly into memory *we* nominated when queuing the read, possibly long before any packet arrives. As the speaker puts it: "I cannot defer having this 1500 bytes buffer to read into until after things are done, because this is a shared memory thing. It needs to know where to store things." (One of the deck's callouts says it directly: the data area to read into needs to be available when preparing the read operation.)

**Listing 11** — code from the talk (uring), shown at [10:17](https://www.youtube.com/watch?v=54oGgmMAUNk&t=617s)

```cpp
struct ring::read_work {
  work cb_;
  int fd_;
  std::array<char, 1500> buffer_;
};

void ring::add(int fd, work w)
{
  auto& work = pending_.emplace_back();
  work.cb_ = std::move(w);
  work.fd_ = fd;
  auto entry = io_uring_get_sqe(&uring_);
  io_uring_prep_read(entry, fd,
                     work.buffer_.data(),
                     work.buffer_.size(),
                     0);
  io_uring_sqe_set_data(entry, &work);
}
```

Each `read_work` bundles everything one pending request needs: the callback, the descriptor and the 1500-byte buffer. `add` creates one at the end of the list, fills it in, takes a submission entry, prepares a read into `work.buffer_`, and stores `&work` as the request's user data. When the completion arrives, that pointer leads straight back to the right callback and buffer. This is exactly the "indirection" the previous slide said would almost always be needed.

Why a `std::list`? Because the kernel and the completion's user data both hold **raw addresses** of `read_work` objects. A `std::list` never moves its elements when others are added or removed. A `std::vector` reallocates when it grows and moves every element to a new address, leaving the kernel writing into freed memory:

**Listing 12** — Why the pending requests cannot live in a std::vector *(illustrative example, not from the talk)*

```cpp
#include <array>
#include <cassert>
#include <list>
#include <vector>

struct read_work { int fd; std::array<char, 1500> buffer; };

int main()
{
    // With a vector, growing may move every element:
    std::vector<read_work> v;
    v.push_back({3, {}});
    char* kernel_target = v[0].buffer.data();   // what we would hand to io_uring_prep_read
    v.push_back({4, {}});                       // may reallocate...
    bool moved = kernel_target != v[0].buffer.data();
    (void)moved;                                // ...and the kernel would write into freed memory

    // With a list, element addresses are stable for the element's lifetime:
    std::list<read_work> l;
    auto& w = l.emplace_back(read_work{3, {}});
    char* stable = w.buffer.data();
    for (int i = 0; i < 100; ++i) l.emplace_back(read_work{i, {}});
    assert(stable == l.front().buffer.data());
}
```

"When I say prepare read, this is an incredibly cheap operation. This is just direct access into an array. There are no system calls here."

### Why it is faster

The speaker's timeline diagram for io_uring has *several* buffers ready at once. While the program waits, the kernel fills one buffer. As soon as that one is complete it starts on the next while the program processes the first, and so on. "So we essentially get multithreading and zero copy." The kernel works in parallel with us, and the data lands where we want it with no extra copy.

For the toy demo, he admits, "this doesn't absolutely matter at all". But in his work on video-streaming software, "each video stream is maybe 10 megabytes per second, and you have 50, 60 of these at the same time. Saving a copy for each does lower the pressure on the caches quite substantially." And, as the last diagram says: **"Fewer system calls too!"**

## 6. Demo: ring::wait and counting system calls

*Talk segment: [12:20](https://www.youtube.com/watch?v=54oGgmMAUNk&t=740s)*

### Completing requests

The talk's slides skip `ring::wait`, but the live demo shows it:

**Listing 13** — code from the talk (Live demo: uring_sum.cpp), shown at [13:19](https://www.youtube.com/watch?v=54oGgmMAUNk&t=799s)

```cpp
void ring::wait()
{

  auto work_cqe = [this](io_uring_cqe* entry) {
    auto w = static_cast<read_work*>(io_uring_cqe_get_data(entry));
    if (w->cb_({w->data_.data(), size_t(entry->res)})) {
      io_uring_cqe_seen(&uring_, entry);
      auto entry = io_uring_get_sqe(&uring_);
      io_uring_prep_read(entry, w->fd_, w->data_.data(), w->data_.size(), 0);
      io_uring_sqe_set_data(entry, w);
    }
    else {
      io_uring_cqe_seen(&uring_, entry);
      pending_.remove_if([&](const auto& e) { return &e == w; });
    }
  };
  io_uring_cqe* entry;
  io_uring_submit(&uring_);
  auto e = io_uring_wait_cqe(&uring_, &entry);
  if (e == 0) {
    work_cqe(entry);
    while (io_uring_peek_cqe(&uring_, &entry) == 0) {
      work_cqe(entry);
    }
  }
}
```

Two details differ from the slides. The demo's `read_work` calls its buffer `data_` rather than `buffer_`. And `wait` defines a lambda, `work_cqe`, that handles one completion:

1. Recover the `read_work*` from the completion's user data.
2. Call the callback with a span over the bytes actually received. `entry->res` is the byte count, cast to `size_t`. (A negative `res` would mean an error. The demo doesn't check for it, and a production version must.)
3. If the callback returned `true`, mark the CQE as seen, queue a **new** read for the same descriptor into the same buffer, and attach the same `read_work` again. The buffer is reused for the next packet.
4. If it returned `false`, mark the CQE as seen and remove the `read_work` from the list.

The body of `wait` then calls `io_uring_submit` to hand the kernel any newly prepared reads, followed by `io_uring_wait_cqe` to sleep until at least one completion arrives. Then comes the interesting part: a loop of `io_uring_peek_cqe`, which returns a completion *without* a system call if one is already in the ring. One wakeup can drain many completions.

An audience member asks how the waiting works: is there a condition variable inside? "I actually don't know how it's implemented." (For the record: `io_uring_wait_cqe` first checks the shared completion ring in user space. Only if it is empty does it make an `io_uring_enter` system call asking the kernel to sleep until completions arrive.)

### The test program, io_uring edition

**Listing 14** — code from the talk (Live demo: uring_sum.cpp), shown at [13:45](https://www.youtube.com/watch?v=54oGgmMAUNk&t=825s)

```cpp
int main()
{
  auto port4000 = udp_socket("127.0.0.1", 4000);
  auto port4001 = udp_socket("127.0.0.1", 4001);

  uint64_t sum = 0;
  uint64_t packets = 0;
  bool done = false;

  auto calc_sum = [&](std::span<char> data) {
    for (auto c : data) {
      sum += c;
    }
    ++packets;
    return true;
  };
  auto print_and_exit = [&](auto&&) {
    std::cout << "packets=" << packets << " sum=" << sum << '\n';
    done = true;
    return false;
  };
  ring r;
  r.add(port4000.fd(), calc_sum);
  r.add(port4000.fd(), calc_sum);
  r.add(port4000.fd(), calc_sum);
  r.add(port4000.fd(), calc_sum);
  r.add(port4001.fd(), print_and_exit);
  while (!done) {
    r.wait();
  }
}
```

`main` is almost the same as before, with one important change: `calc_sum` is registered **four times** on port 4000. Each `add` queues its own read with its own buffer, so up to four packets can be received into user memory before the program has processed any of them. The callbacks now return `true` (keep going) or `false` (stop).

### Counting system calls

Under `strace`, the start-up now shows `io_uring_setup(8, …)` and some `mmap` calls that map the rings into the process, followed by `io_uring_enter` calls. Sending single packets by hand, though, is "quite unexciting, because we still get two system calls per packet": one `io_uring_enter` to submit the re-armed read and one to wait for the next completion.

The difference shows up under load. The speaker switches to `strace -c`, which counts calls by type instead of printing each one, and uses **nping**, a packet generator from the Nmap project:

```
nping -q --udp -p 4000 --rate 1000 -c 20000 --data-length=1400 127.0.0.1
```

Here `-q` means quiet, `-c 20000` sends 20,000 packets and `--data-length=1400` gives 1,400-byte payloads. Comparing the summaries:

| Version | Packets counted | Main system calls |
|---|---|---|
| poll + read | 7,418 of 20,000 | ~7,400 `poll` + ~7,400 `read` |
| io_uring | 20,000 of 20,000 | ~5,350 `io_uring_enter` |

The poll version kept up with only part of the stream, and it needed two calls per packet it did receive. The io_uring version received every packet with "a little more than five thousand" calls for twenty thousand packets, because each wakeup collected several completions. "That is a lot cheaper. System calls are not cheap." And there was less copying.

He adds a caveat: "Don't draw too many conclusions on this, because what this is is measuring basically the overhead of strace, which is substantial." Tracing makes every system call far more expensive, so the absolute numbers say little about real throughput. The *ratio* of calls to packets is the meaningful part.

### Draining completions in batches

liburing can also hand you several completions at once, which makes the batching explicit:

**Listing 15** — Processing completions in batches (requires liburing-dev) *(illustrative example, not from the talk)*

```cpp
#include <liburing.h>

#include <array>

// Process every completion currently in the ring with one wait at most.
template <typename Handler>
unsigned drain_completions(io_uring& ring, Handler&& handle)
{
    io_uring_submit_and_wait(&ring, 1);                 // submit pending SQEs, wait for >= 1 CQE

    std::array<io_uring_cqe*, 32> cqes;
    unsigned n = io_uring_peek_batch_cqe(&ring, cqes.data(), cqes.size());
    for (unsigned i = 0; i < n; ++i) {
        handle(cqes[i]);                                // may queue new SQEs
    }
    io_uring_cq_advance(&ring, n);                     // mark all n as seen at once
    return n;
}

int main()
{
    io_uring ring;
    io_uring_queue_init(8, &ring, 0);
    // ... queue reads with io_uring_prep_read ...
    io_uring_queue_exit(&ring);
}
```

## 7. Coroutines: loops that can pause

*Talk segment: [17:10](https://www.youtube.com/watch?v=54oGgmMAUNk&t=1030s)*

"That was a bit off topic, but we'll merge them together in a while," the speaker says. The second strand of the talk is coroutines.

### What coroutines promise

The slide states it plainly:

- Coroutines offer a way for you to **write asynchronous code as if it were continuous loops**.
- There is **language support from C++20**.
- **Compiler magic** converts it to something else, via **types that you must write**.
  - "And they're mindbogglingly hard to understand."
  - "And the standard library doesn't help." (The slide adds an angry emoji.)

C++20 added the *language* mechanism: three keywords, `co_await`, `co_yield` and `co_return`, plus a set of rules for how the compiler transforms a function that uses them. It added almost no *library* types to go with it. Apart from a few helpers in `<coroutine>`, such as `std::coroutine_handle`, `std::suspend_always` and `std::suspend_never`, you write the return types yourself. C++23 added `std::generator` for pull-style sequences. For anything else, including the push-driven pipelines in this talk, you build your own or use a third-party library. "Here comes the whining," as the speaker puts it.

### A function that can pause

An ordinary function runs from start to finish. Its local variables live on the thread's stack and vanish when it returns. A **coroutine** can **suspend** in the middle, return control to someone else, and later **resume** exactly where it stopped, with all its local variables intact.

C++20 coroutines are **stackless**. A coroutine does not get its own stack. Instead, everything it needs to survive a suspension (its parameters, its locals, and a marker for where it stopped) is kept in a **coroutine frame**, normally allocated on the heap. Suspending simply returns to the caller or resumer. Resuming jumps back into the function body using the saved state.

### Two loops passing a baton

The speaker's diagram shows two loops side by side, `for (;;) { … }` on the left and on the right:

1. The right-hand loop is **suspended**, waiting.
2. The left-hand loop **computes x**, then **suspends**, handing x over.
3. The right-hand loop **resumes with x**, **works with x**, and suspends again.
4. Control returns to the left, which **resumes**, computes the next x, suspends…

"Right, left, right… you see the idea." Neither loop knows about threads or callbacks. Each just looks like a loop that occasionally waits. That is the attraction: logic that would otherwise be scattered across callbacks reads as straight-line code.

### The one library type you do get

To get a feel for suspension before the talk's do-it-yourself machinery, here is `std::generator` (C++23). The function looks like a loop, but each `co_yield` suspends it and hands one value to the consumer. It resumes only when the consumer asks for the next value.

**Listing 16** — Two loops taking turns with std::generator *(illustrative example, not from the talk)*

```cpp
#include <generator>
#include <iostream>

std::generator<int> producer()
{
    for (int x = 1; x <= 3; ++x) {
        std::cout << "  producer: computed " << x << ", suspending\n";
        co_yield x;                                   // suspend, hand x over
        std::cout << "  producer: resumed\n";
    }
}

int main()
{
    for (int x : producer()) {                        // each step resumes the producer
        std::cout << "consumer: working with " << x << '\n';
    }
}
```

Notice the interleaving in the output: the producer and the consumer take turns, just like the two loops in the diagram. `std::generator` is *pull-driven*: the consumer drives. The talk's pipelines are *push-driven*: data arrives from the network and must be pushed through the stages. That direction is not covered by the standard library, so we need to look under the hood.

## 8. What the compiler makes of a coroutine

*Talk segment: [19:15](https://www.youtube.com/watch?v=54oGgmMAUNk&t=1155s)*

### A coroutine on a slide

Here is the speaker's first coroutine:

**Listing 17** — code from the talk (coroutines), shown at [19:19](https://www.youtube.com/watch?v=54oGgmMAUNk&t=1159s)

```cpp
coroutine_type
my_coro(int x, int y, coro_src& src)
{
   int result = 0;
   while (int a = co_await src)
   {
      result += work(a,x,y);
   }
   co_return result;
}

auto coro_obj = my_coro(3, 8, source);
```

`my_coro` takes two integers and a reference to some source of values. It loops: `co_await src` suspends until the source supplies a value `a` (the loop ends when that value is zero), and each value is folded into `result`. At the end, `co_return result;` delivers the final answer.

The function never says "coroutine". The compiler infers it: **any function whose body uses `co_await`, `co_yield` or `co_return` is a coroutine.** Its declared return type, `coroutine_type` on the slide, does not mean "returns an int when finished". It is the type of an object that *represents the running coroutine* and is handed back to the caller immediately. Calling `my_coro(3, 8, source)` does not run the loop to completion; it creates the coroutine and gives you a handle-like object, `coro_obj`.

### What the compiler does, roughly

"What the compiler does when it sees this … it transmogrifies it into something sort of kind of like this":

**Listing 18** — code from the talk (coroutines), shown at [20:15](https://www.youtube.com/watch?v=54oGgmMAUNk&t=1215s)

```cpp
class my_coro {
public:
   my_coro(int x_, int y_) : x(x_), y(y_) {}
   int get_result() const { return result; }
   void operator()(int a)
   {
      result += work(a, x, y);
   }
private:
   int x;
   int y;
   int result = 0;
};

auto coro_obj = impl(new my_coro(3, 8), source);
```

In this mental model:

- The coroutine becomes an **object**.
- Its parameters `x` and `y`, and its local `result`, become **member variables**: they must survive across suspensions, so they cannot live on the stack.
- The body of the `while` loop becomes something like a **function-call operator** that is invoked each time a new value `a` arrives.
- Calling `my_coro(3, 8, source)` becomes "allocate the object on the heap (`new my_coro(3, 8)`), plus some extra code and data that is used to administer all this, and hook it up to `source`".

The thought bubble on the next build of the slide is important: **"It's not this simple!"** The speaker calls it "a vast oversimplification", but "as a first step, as a mental model to get you to grasp roughly what actually happens, … this is probably okay enough to get you going."

### Closer to the truth

What the standard actually specifies is a little more structured. For a coroutine with return type `R`, the compiler:

1. Looks up `R::promise_type` (more precisely, `std::coroutine_traits<R, Args...>::promise_type`). This is the **promise type**, a class *you* write that customises the coroutine.
2. Allocates a **coroutine frame** holding copies of the parameters, the locals, a promise object, and the current suspension point.
3. Calls `promise.get_return_object()` to produce the `R` returned to the caller.
4. Executes `co_await promise.initial_suspend();` before the body runs, and `co_await promise.final_suspend();` after it ends.
5. Translates `co_yield e` into `co_await promise.yield_value(e)`, `co_return e` into `promise.return_value(e)` (or `return_void()`), and each `co_await x` into calls on an **awaiter** obtained from `x`.

A `std::coroutine_handle<promise_type>` is a pointer-like handle to the frame. Through it you can `resume()` or `destroy()` the coroutine, and reach its promise.

**Listing 19** — The pieces the compiler looks for, in the smallest working coroutine *(illustrative example, not from the talk)*

```cpp
#include <coroutine>
#include <iostream>

struct fire_and_forget {
    struct promise_type {
        fire_and_forget get_return_object() { return {}; }       // step 3
        std::suspend_never initial_suspend() noexcept { return {}; } // step 4: run immediately
        std::suspend_never final_suspend() noexcept { return {}; }   // frame frees itself
        void return_void() {}                                     // co_return;
        void unhandled_exception() { std::terminate(); }
    };
};

fire_and_forget hello(int n)       // a coroutine because it uses co_return
{
    for (int i = 0; i < n; ++i) std::cout << "hello " << i << '\n';
    co_return;
}

int main()
{
    hello(2);   // allocates a frame, runs to completion, frame destroyed at final_suspend
}
```

### Two types we must write

The final build of the slide circles two names. `coroutine_type` gets "We must write this type", and `coro_src&` gets "and this type". "Since we don't actually have much support from the standard library … this coroutine type is one that we must write, or if you're lucky enough to find an open source library that you can use, by all means. And also this coroutine source that we co_await values from, you have to write this one yourself." The next three sections do exactly that.

## 9. The return object: task and owning the promise

*Talk segment: [21:15](https://www.youtube.com/watch?v=54oGgmMAUNk&t=1275s)*

### The coroutine's return type: task

"Most blogs and articles and conference videos … that I've seen about coroutines refer to these as tasks, so I will not add to the confusion by using a different name. I'm sticking to task."

**Listing 20** — code from the talk (coroutine return object (task)), shown at [21:21](https://www.youtube.com/watch?v=54oGgmMAUNk&t=1281s)

```cpp
template <typename T>
struct task
{
  using promise_type = promise<T>;

  auto operator co_await() const noexcept;
private:
  task(promise<T>* p) : m_promise(p) { }
  friend class promise<T>;
  promise_ptr<T> m_promise;
};
```

Three things about this declaration:

1. **`using promise_type = promise<T>;`** is how the compiler finds the promise type. "A task must have a promise type, and no, it is not `std::promise`. It is absolutely not `std::promise`." `std::promise` belongs to the futures library and has nothing to do with coroutines. The name clash is unfortunate. "And we have to write the promise type also. We'll get there."
2. **`auto operator co_await() const noexcept;`**: "since I want this task to be co_awaitable, we must have an operator co_await". This is what lets one coroutine write `co_await some_task` to wait for values from another. Its definition comes two sections later.
3. **The private constructor takes a `promise<T>*`**, and `promise<T>` is a friend, so only the promise can create tasks. The task stores the pointer in a `promise_ptr<T>`.

### Who owns the coroutine frame?

The promise "is allocated by the compiler, compiler-generated code, so it's magic that happens there, and it is handed over to the task". The task must "keep track of its lifetime, and we must destroy it the right way". The promise lives *inside* the coroutine frame, so destroying it means destroying the whole frame. And "calling delete on this is a really, really, really bad idea. Ask me how I know."

`delete` would run only the promise's destructor and free memory the promise did not allocate. The frame also holds the parameters, the locals and bookkeeping, and it was allocated by compiler-generated code. The correct way is to get a **coroutine handle** for the frame and call `destroy()`:

**Listing 21** — code from the talk (coroutine return object (task)), shown at [22:43](https://www.youtube.com/watch?v=54oGgmMAUNk&t=1363s)

```cpp
struct coro_deleter
{
  template <typename promise>
  void operator()(promise* p) const noexcept {
    using handle = std::coroutine_handle<promise>;
    handle::from_promise(*p).destroy();
  }
};

template <typename T>
using promise_ptr = std::unique_ptr<promise<T>,
                                    coro_deleter>;
```

`std::coroutine_handle<P>::from_promise(p)` goes from a reference to the promise back to a handle for the frame containing it. `destroy()` runs the destructors of everything in the frame and deallocates it. Wrapping this in a deleter and using `std::unique_ptr<promise<T>, coro_deleter>` gives the task **unique ownership** with correct cleanup, and makes it **move-only**. That last point returns later: move-only tasks will not fit into `std::function`.

### Why this design?

There are two common ways to write an owning task:

- Store a `std::coroutine_handle<promise_type>` and call `destroy()` in the destructor (with the move operations written by hand).
- Store a `unique_ptr` to the promise with a deleter, as the speaker does, and let `unique_ptr` provide the move semantics.

Both are equivalent in effect. The handle-based version is more common in libraries, and it is a good exercise in writing a correct move-only RAII type:

**Listing 22** — An owning coroutine handle written by hand *(illustrative example, not from the talk)*

```cpp
#include <coroutine>
#include <utility>

template <typename Promise>
class unique_coroutine {
public:
    unique_coroutine() = default;
    explicit unique_coroutine(std::coroutine_handle<Promise> h) noexcept : h_(h) {}

    unique_coroutine(unique_coroutine&& o) noexcept : h_(std::exchange(o.h_, {})) {}
    unique_coroutine& operator=(unique_coroutine o) noexcept { std::swap(h_, o.h_); return *this; }
    ~unique_coroutine() { if (h_) h_.destroy(); }      // destroy the frame, never delete

    Promise& promise() const { return h_.promise(); }
    void resume() const { h_.resume(); }
    explicit operator bool() const noexcept { return static_cast<bool>(h_); }

private:
    std::coroutine_handle<Promise> h_{};
};

struct tiny_task {
    struct promise_type {
        tiny_task get_return_object()
        { return tiny_task{unique_coroutine<promise_type>{std::coroutine_handle<promise_type>::from_promise(*this)}}; }
        std::suspend_always initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() {}
    };
    unique_coroutine<promise_type> coro;
};

tiny_task work() { co_return; }

int main()
{
    tiny_task t = work();   // suspended at initial_suspend
    t.coro.resume();        // runs to final_suspend
}                           // frame destroyed by unique_coroutine
```

## 10. The promise type: the coroutine's control panel

*Talk segment: [23:05](https://www.youtube.com/watch?v=54oGgmMAUNk&t=1385s)*

### A ritual of member functions

"So then we get to the promise, which is not just a promise. There is … a lot of ritualistic code here. We must just follow this ritual."

**Listing 23** — code from the talk (coroutines promise), shown at [23:07](https://www.youtube.com/watch?v=54oGgmMAUNk&t=1387s)

```cpp
template <typename T>
struct promise
{
  task<T> get_return_object() noexcept;
  std::suspend_never initial_suspend() noexcept;
  std::suspend_always final_suspend() noexcept;

  bool is_ready() const noexcept;
  T get();

  void unhandled_exception();
  template <typename U>
  std::suspend_always yield_value(U&& u);
  void return_void();

  std::coroutine_handle<> m_continuation;
  std::optional<T> m_value;
};
```

The promise is the coroutine's control panel. The compiler calls its member functions at fixed points in the coroutine's life, and whatever they return decides what happens. Here is each group, as the speaker's callouts describe them.

**"The function that creates the `task<T>` object":** `get_return_object()` is called once, right after the frame and promise are created, to build the value the caller receives. In this design it returns `{this}`: a task constructed from a pointer to the promise, which is why the task's constructor is private and `promise<T>` is its friend.

**"Behaviour at the beginning and end of the life of the coroutine":** `initial_suspend()` and `final_suspend()` "typically don't have any function bodies at all, other than to return a default-constructed value of their return type". Their return types carry the meaning:

| Function | Returns | Effect here |
|---|---|---|
| `initial_suspend()` | `std::suspend_never` | The body starts running immediately, up to its first real suspension point. |
| `final_suspend()` | `std::suspend_always` | When the body finishes, the coroutine stays suspended instead of destroying its frame, so the owning task can safely destroy it later. |

"If it says suspend never, then it continues executing. If it says suspend always, then we're done there and continue with something else."

**"Utility functions for our own implementation":** `is_ready()` and `get()` are not part of the coroutine protocol. They are helpers for this task's own `co_await` support: "do you have a value?" and "give it to me".

**"Handle to suspended coroutine":** `m_continuation` stores a `std::coroutine_handle<>` for the coroutine that is waiting on this one. When a value arrives, the promise knows whom to wake up. The speaker notes this need not live in the promise: "you can have these continuations elsewhere in some cooperative scheduler. I'm sure people will do that. I'm also sure people will create messes of that, but it can probably be really cool if you know what you're doing."

**Values and errors:** `m_value` is a `std::optional<T>` holding at most one value in transit. "Strictly speaking not good enough, but for now." `unhandled_exception()` is called if the body throws, and here it is empty: "I have completely ignored exceptions in this, so if bad things happen, then bad things really happen." `return_void()` is required because the coroutine bodies in this talk end without returning a value.

### yield_value: pushing a value in

**Listing 24** — code from the talk (coroutines promise), shown at [25:55](https://www.youtube.com/watch?v=54oGgmMAUNk&t=1555s)

```cpp
template <typename T>
struct promise
{
  task<T> get_return_object() noexcept;
  std::suspend_never initial_suspend() noexcept;
  std::suspend_always final_suspend() noexcept;

  bool is_ready() const noexcept;
  T get();

  void unhandled_exception();
  template <typename U>
  std::suspend_always yield_value(U&& u){
    m_value.emplace(std::forward<U>(u));
    m_continuation.resume();
    return {};
  }
};
```

"We need to be able to put values into our promise, because that's the thing it's meant to communicate." `yield_value`:

1. **Stores** the value: `m_value.emplace(std::forward<U>(u))`.
2. **Resumes** the coroutine waiting for it: `m_continuation.resume()`. The callout reads: "Resume execution of the coroutine that is suspended waiting for a value." That coroutine runs *right now, on this thread*, inside the `yield_value` call, until it suspends again.
3. Returns `std::suspend_always`.

Two different callers use this one function:

- **The outside world** calls `promise.yield_value(x)` directly to inject a value. That is how packets will enter the pipeline. The returned `suspend_always` object is simply ignored.
- **A coroutine** that writes `co_yield x;` makes the compiler call `co_await promise.yield_value(x)`. The value is passed downstream, the downstream coroutine runs, and then the yielding coroutine *suspends* because of `suspend_always`. It waits until something resumes it, which is exactly what happens when its own input produces the next value.

### A promise that handles exceptions

The talk ignores errors on purpose. Here is how the same promise could carry an exception downstream instead of dropping it: store `std::current_exception()` and rethrow it where the value would have been read.

**Listing 25** — Storing exceptions instead of ignoring them *(illustrative example, not from the talk)*

```cpp
#include <coroutine>
#include <exception>
#include <optional>
#include <stdexcept>
#include <utility>
#include <variant>

// The value slot of a promise that can hold a value OR an exception.
template <typename T>
struct value_slot {
    std::variant<std::monostate, T, std::exception_ptr> v;

    bool is_ready() const noexcept { return v.index() != 0; }

    T get()
    {
        if (auto* e = std::get_if<std::exception_ptr>(&v)) {
            auto ex = *e;
            v = std::monostate{};
            std::rethrow_exception(ex);          // surfaces at the consumer's co_await
        }
        T t = std::move(std::get<T>(v));
        v = std::monostate{};
        return t;
    }

    void set_value(T t) { v = std::move(t); }
    void set_exception(std::exception_ptr e) { v = e; }   // call from unhandled_exception()
};

int main()
{
    value_slot<int> s;
    s.set_exception(std::make_exception_ptr(std::runtime_error("bad packet")));
    try { (void)s.get(); } catch (const std::runtime_error&) { return 0; }
    return 1;
}
```

## 11. Making a task awaitable

*Talk segment: [26:00](https://www.youtube.com/watch?v=54oGgmMAUNk&t=1560s)*

### operator co_await

For one coroutine to write `co_await some_task`, the task must produce an **awaiter**. The speaker's `task::operator co_await` returns a small local struct that "communicates with the `promise<T>`":

**Listing 26** — code from the talk (coroutine return object (task) cont.), shown at [26:05](https://www.youtube.com/watch?v=54oGgmMAUNk&t=1565s)

```cpp
template <typename T>
struct task
{
   auto operator co_await() const noexcept {
      struct awaitable {
         bool await_ready() const noexcept {
            return m_promise.is_ready();
         }
         void await_suspend(std::coroutine_handle<> next) const noexcept {
            m_promise.m_continuation = next;
         }
         T await_resume() const {
            return m_promise.get();
         }
         promise<T>& m_promise;
      };
      return awaitable{ *m_promise };
   }
};
```

When a coroutine evaluates `co_await t`, the compiler calls `t.operator co_await()` to get the `awaitable`, then calls its three member functions in a fixed protocol:

| Step | Member | Question it answers | Here |
|---|---|---|---|
| 1 | `await_ready()` | "Is the result already available, so I needn't suspend?" | "Check if the `promise<T>` holds a value": `m_promise.is_ready()` |
| 2 | `await_suspend(handle)` | Called only if not ready, after the coroutine is suspended, with a handle to the coroutine. | "Store the calling coroutine as the one to continue when the `promise<T>` gets a value." |
| 3 | `await_resume()` | Called when the coroutine continues; its return value is the value of the `co_await` expression. | "And finally, on resume, get the value from the `promise<T>`, making it empty again." |

Put together with `yield_value` from the previous section, a full round trip looks like this:

1. Coroutine **B** executes `auto v = co_await a_task;`.
2. `await_ready()` returns `false`, since there is no value yet.
3. B is suspended. `await_suspend(handle_to_B)` stores `handle_to_B` in the promise's `m_continuation`. Control returns to whoever last resumed B.
4. Later, someone calls `promise.yield_value(42)`. The value is stored and `m_continuation.resume()` is called.
5. B continues. `await_resume()` calls `get()`, which moves the value out and resets the optional. The `co_await` expression evaluates to 42, and B carries on with its loop.

"Yeah, a lot of code that doesn't do much, it feels like." Most of the complexity is in *who calls whom*, not in what any single function does. The next section makes that visible with a trace.

### More about await_suspend

`await_suspend` may return `void` (always suspend, as here), `bool` (`false` means "don't suspend after all"), or another `std::coroutine_handle<>`. In the last case, that coroutine is resumed immediately; this is known as **symmetric transfer**. Symmetric transfer matters in deep chains. With `void` and a direct `resume()` call, as in this talk's `yield_value`, every hop down the pipeline adds a stack frame to the native call stack. Symmetric transfer lets the compiler hand control from coroutine to coroutine without growing it.

To see the protocol on its own, here is an awaiter that logs each step. Making `await_ready` return `true` would skip suspension entirely.

**Listing 27** — Watching the awaiter protocol step by step *(illustrative example, not from the talk)*

```cpp
#include <coroutine>
#include <iostream>

struct logging_awaiter {
    std::coroutine_handle<>* stash;

    bool await_ready() const noexcept { std::cout << "await_ready -> false\n"; return false; }
    void await_suspend(std::coroutine_handle<> h) const noexcept
    {
        std::cout << "await_suspend: stashing handle\n";
        *stash = h;                                  // someone else will resume us
    }
    int await_resume() const noexcept { std::cout << "await_resume -> 42\n"; return 42; }
};

struct detached {
    struct promise_type {
        detached get_return_object() { return {}; }
        std::suspend_never initial_suspend() noexcept { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() {}
    };
};

std::coroutine_handle<> waiting;

detached consumer()
{
    std::cout << "consumer: before co_await\n";
    int v = co_await logging_awaiter{&waiting};
    std::cout << "consumer: got " << v << '\n';
}

int main()
{
    consumer();                                      // runs until it suspends
    std::cout << "main: resuming consumer\n";
    waiting.resume();                                // like m_continuation.resume()
}
```

## 12. Demo: tracing the conversation between task, promise and awaitable

*Talk segment: [26:40](https://www.youtube.com/watch?v=54oGgmMAUNk&t=1600s)*

### Instrumenting everything

To show what really happens at run time, the speaker's live demo adds a `trace(...)` call to every member of the promise and the awaitable. He also gives each task a compile-time name, so that the output reads `promise<incoming>` instead of just `promise<int>`. The final section explains how that name works.

The traced promise:

**Listing 28** — code from the talk (Live demo: traced promise), shown at [27:11](https://www.youtube.com/watch?v=54oGgmMAUNk&t=1631s)

```cpp
struct promise
{
  promise() { trace("promise<",name,">()"); }
  ~promise() { trace("~promise<", name, ">()");}

  task<T,name> get_return_object() noexcept {
    trace("promise<",name,">::get_return_object()");
    return {this};
  }
  std::suspend_never initial_suspend() noexcept {
    trace("promise<",name,">::initial_suspend()");
    return {};
  }
  std::suspend_always final_suspend() noexcept {
    trace("promise<",name,">::final_suspend()");
    return {};
  }

  bool is_ready() const noexcept {
    bool has_value = m_value.has_value();
    trace("promise<",name,">::is_ready() -> ", has_value);
    return has_value;
  }

  T get() noexcept {
    auto t = std::move(m_value).value();
    trace("promise<",name,">::get() ->", t);
    m_value.reset();
    return t;
  }

  void unhandled_exception() {}
  template <typename U>
  std::suspend_always yield_value(U&& u)
  {
  trace("promise<",name,">::yield_value(",u,')');
  m_value.emplace(std::forward<U>(u));
  m_continuation.resume();
```

and the traced awaitable inside `task::operator co_await`, plus two members not shown on the slides:

**Listing 29** — code from the talk (Live demo: traced task), shown at [27:43](https://www.youtube.com/watch?v=54oGgmMAUNk&t=1663s)

```cpp
      using handle = std::coroutine_handle<>;
      bool await_ready() const noexcept
      {
    trace("awaitable<",name,">::await_ready()");
    return m_promise.is_ready();
      }
      void await_suspend(handle next) const noexcept
      {
    trace("awaitable<",name,">::await_suspend()");
    m_promise.m_continuation = next;
      }
      T await_resume() const
      {
    trace("awaitable<",name,">::await_resume()");
    return m_promise.get();
      }
      promise<T,name>& m_promise;
    };
    trace("task<", name, ">::co_await()");
    return awaitable{ *m_promise };
  }
  promise<T,name>& get_promise() const { return *m_promise;}
  ~task() {
    trace("~task<",name,">()");
  }
  static task<T,name> make() {
    co_await std::suspend_always{};
  }
private:
  task(promise<T,name>* p) : m_promise(p) {
    trace("task<",name,">()");
  }
  friend class promise<T,name>;
  promise_ptr<T,name> m_promise;
};
```

The new members matter:

- `get_promise()` gives outside code access to the promise, so it can call `yield_value`.
- `static task<T,name> make()` is "a static member make that just does `co_await std::suspend_always{}`". It is a coroutine that suspends forever. It never produces values itself; it exists to have a promise. Code outside pushes values into that promise with `yield_value`, which makes `make()` the **entry point** of a pipeline: the "source" that the earlier slide said we would have to write ourselves.

### The program

**Listing 30** — code from the talk (Live demo), shown at [28:49](https://www.youtube.com/watch?v=54oGgmMAUNk&t=1729s)

```cpp
template <typename T, str name>
task<void, "printer"> print_all(task<T,name>& values)
{
  trace("print_all");
  for (;;) {
    auto v = co_await values;
    std::cout << v << '\n' << std::flush;
  }
  trace("print_all done");
}


int main()
{
  auto incoming = task<int,"incoming">::make();
  auto h = print_all(incoming);
  auto& promise = incoming.get_promise();
  promise.yield_value(4);
  promise.yield_value(2);
}
```

`print_all` is a coroutine that loops forever, `co_await`ing the next value from the task it was given and printing it. `main`:

1. creates a source task named `"incoming"`,
2. starts `print_all` on it (the resulting task is named `"printer"`),
3. gets the source's promise, and
4. pushes two values, 4 and 2.

### The trace, line by line

**Listing 31** — code from the talk (Live demo: co_1), shown at [30:13](https://www.youtube.com/watch?v=54oGgmMAUNk&t=1813s)

```console
$ ./co_1
promise<incoming>()
promise<incoming>::get_return_object()
task<incoming>()
promise<incoming>::initial_suspend()
promise<printer>()
promise<printer>::get_return_object()
task<printer>()
promise<printer>::initial_suspend()
print_all
task<incoming>::co_await()
awaitable<incoming>::await_ready()
promise<incoming>::is_ready() -> 0
awaitable<incoming>::await_suspend()
promise<incoming>::yield_value(4)
awaitable<incoming>::await_resume()
promise<incoming>::get() ->4
4
task<incoming>::co_await()
awaitable<incoming>::await_ready()
promise<incoming>::is_ready() -> 0
awaitable<incoming>::await_suspend()
promise<incoming>::yield_value(2)
awaitable<incoming>::await_resume()
promise<incoming>::get() ->2
2
task<incoming>::co_await()
awaitable<incoming>::await_ready()
promise<incoming>::is_ready() -> 0
awaitable<incoming>::await_suspend()
~task<printer>()
~promise<printer>()
~task<incoming>()
~promise<incoming>()
```

Here is how the trace maps to the code:

- **`task<int,"incoming">::make()`.** A frame and promise are created (`promise<incoming>()`), then `get_return_object()` creates the task (`task<incoming>()`). `initial_suspend()` returns `suspend_never`, so the body runs at once and immediately suspends at `co_await std::suspend_always{}`. Control returns to `main`.
- **`print_all(incoming)`.** The same creation sequence happens for the printer. Its body runs (`print_all`), reaches `co_await values`, and calls `task<incoming>::co_await()`. `await_ready()` asks `is_ready()`, which says 0 (no value), so `await_suspend()` stores the printer as the continuation. Control returns to `main`.
- **`promise.yield_value(4)`.** The value is stored and the continuation is resumed. Inside the printer, `await_resume()` calls `get()`, which returns 4. The printer prints `4`, loops, and `co_await`s again, which gives the same ready/suspend sequence. Then control returns into `yield_value`, and from there to `main`.
- **`promise.yield_value(2)`** follows the same pattern.
- **End of `main`.** The tasks are destroyed in reverse order of creation: `~task<printer>`, `~promise<printer>`, `~task<incoming>`, `~promise<incoming>`. The printer is still suspended in its loop; destroying its frame is how it "ends".

"It's a lot of things happening," and "a lot of work, but that is the way this happens. You have this communication between our coroutine code, the task and the promise and the awaitable, and … they're all chatting via the promise to ensure the correct flow of information and execution."

### A self-contained tracer

The tracing technique is the single best tool for understanding coroutine machinery. Here is a tiny version you can paste into Compiler Explorer and modify. Try changing `initial_suspend` to `suspend_always` and watch the order change.

**Listing 32** — A minimal traced coroutine to experiment with *(illustrative example, not from the talk)*

```cpp
#include <coroutine>
#include <iostream>

struct traced {
    struct promise_type {
        promise_type() { std::cout << "promise()\n"; }
        ~promise_type() { std::cout << "~promise()\n"; }
        traced get_return_object()
        {
            std::cout << "get_return_object()\n";
            return traced{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        std::suspend_never initial_suspend() noexcept { std::cout << "initial_suspend()\n"; return {}; }
        std::suspend_always final_suspend() noexcept { std::cout << "final_suspend()\n"; return {}; }
        void return_void() { std::cout << "return_void()\n"; }
        void unhandled_exception() {}
    };
    std::coroutine_handle<promise_type> h;
    ~traced() { if (h) h.destroy(); }
};

traced body()
{
    std::cout << "body: start\n";
    co_await std::suspend_always{};
    std::cout << "body: resumed\n";
}

int main()
{
    traced t = body();
    std::cout << "main: resuming\n";
    t.h.resume();
    std::cout << "main: done\n";
}
```

## 13. Pipelines: co_await upstream, co_yield downstream

*Talk segment: [30:30](https://www.youtube.com/watch?v=54oGgmMAUNk&t=1830s)*

### A filter stage

With a task that can be both awaited and yielded into, building a processing stage takes only a few lines:

**Listing 33** — code from the talk (Making a computation pipeline), shown at [30:33](https://www.youtube.com/watch?v=54oGgmMAUNk&t=1833s)

```cpp
template <typename T, typename P>
task<T> filter_in(P predicate, task<T>& in)
{
  for (;;) {
    auto v = co_await in;
    if (predicate(v)) {
      co_yield v;
    }
  }
}
```

`filter_in` loops forever. It awaits the next value from its input task and, if the predicate likes it, `co_yield`s it. Its own return type is `task<T>`, so whatever is downstream can `co_await` the filter exactly as the filter awaits its input.

As the callout points out, **`co_yield` "calls the `yield_value()` member function on the promise of the return type for the coroutine."** That is the language rule mentioned earlier (`co_yield e` means `co_await promise.yield_value(e)`), and "it wasn't a completely arbitrary choice that I called this function yield_value." Outside code and coroutines push values downstream through the very same function.

### No fixed ratio between in and out

"One thing to note here is this asymmetry." A stage may `co_await` many times between `co_yield`s, as a filter does when it drops values. It may also `co_yield` several times for one input: "we could have one that duplicates values and co_yields every value twice." A stage is not a one-in-one-out transform; it is a little program with its own loop.

That is the intent. When the examples for this chapter were run against the talk's types, though, a duplicating stage did **not** work. Feeding 1, 2, 3 through "square, then duplicate" printed `1 1 9` instead of `1 1 4 4 9 9`:

**Listing 34** — A transform stage and a duplicating stage *(illustrative example, not from the talk)*

```cpp
// Uses task<T> and promise<T> from Listing 20, Listing 21, Listing 23, Listing 24 and Listing 26 (with the promise<void> specialisation below).
// With the talk's yield_value (returning std::suspend_always) this prints 1 1 9.
template <typename T, typename F>
task<T> transform(F f, task<T>& in)
{
  for (;;) {
    auto v = co_await in;
    co_yield f(v);                 // exactly one out per in
  }
}

template <typename T>
task<T> duplicate(task<T>& in)
{
  for (;;) {
    auto v = co_await in;
    co_yield v;                    // two outs per in
    co_yield v;
  }
}

template <typename T>
task<void> print_each(task<T>& in)
{
  for (;;) {
    std::cout << co_await in << ' ';
  }
}

int main()
{
  auto incoming = task<int>::make();
  auto squared  = transform([](int v) { return v * v; }, incoming);
  auto twice    = duplicate(squared);
  auto printer  = print_each(twice);
  for (int i = 1; i <= 3; ++i) {
    incoming.get_promise().yield_value(i);   // intended: 1 1 4 4 9 9
  }
}
```

The cause is the return type of `yield_value`. It returns `std::suspend_always`, so a coroutine that executes `co_yield` stays suspended *after* the downstream stage has consumed the value. Who resumes it? The only stored continuation is in the **upstream** promise, and the upstream resumes it only when it pushes its **next** value. A stage that alternates strictly (await, maybe yield, await, …), like `filter_in`, never notices: by the time it is resumed, the value it awaits next is already there. A stage that yields twice in a row is resumed for its second `co_yield` only when the next input arrives. It then yields a stale value and is out of step from then on, which is how the 4 was lost.

In this push-driven design the fix is a single line. `m_continuation.resume()` inside `yield_value` already runs the downstream stage until it is waiting again, so by the time `yield_value` returns the value has been consumed and the yielder can simply continue. Return `std::suspend_never` instead:

**Listing 35** — The one-line fix: let a yielding coroutine continue *(illustrative example, not from the talk)*

```cpp
// In promise<T> from Listing 23 and Listing 24: return suspend_never from yield_value.
// The downstream stage has already consumed the value when resume() returns.
template <typename U>
std::suspend_never yield_value(U&& u) {
  m_value.emplace(std::forward<U>(u));
  m_continuation.resume();
  return {};
}
```

With that change the duplicating stage prints `1 1 4 4 9 9`, and the talk's own pipelines still print `1 3 5 7 9`. This is not a criticism of the talk: the speaker calls his types "simplistic" and warns that there are many ways to tune coroutine behaviour. It is a good example of why the protocol needs careful testing with more than one shape of stage.

### Wiring the pipeline

**Listing 36** — code from the talk (Making a computation pipeline), shown at [31:39](https://www.youtube.com/watch?v=54oGgmMAUNk&t=1899s)

```cpp
int main()
{
  auto is_odd = [](auto v) { return v & 1;};
  auto incoming = task<int>::make();
  auto odd_values = filter_in(is_odd, incoming);
  auto printer = print_all(odd_values);
  for (int i = 0; i < 10; ++i) {
    incoming.get_promise().yield_value(i);
  }
}
```

Read it from the top:

1. `incoming` is a source task, created by `task<int>::make()`, that suspends forever and whose promise we push into.
2. `odd_values` is a running `filter_in` coroutine that awaits `incoming`.
3. `printer` is a running `print_all` coroutine that awaits `odd_values`.
4. The loop pushes 0 to 9 into the source's promise.

Each `yield_value(i)` resumes the filter. If `i` is odd, the filter's `co_yield` resumes the printer, which prints and then suspends waiting again. Control unwinds back to the filter, which suspends at its `co_yield`, and back to `main`. Every value travels the whole pipeline before `main` pushes the next one. The pipeline is **push-driven** and **synchronous**: no threads, no queues, just coroutines resuming each other.

### The demo

The demo puts the task machinery in a header and runs the same pipeline with named tasks:

**Listing 37** — code from the talk (Live demo: pipeline.cpp), shown at [32:35](https://www.youtube.com/watch?v=54oGgmMAUNk&t=1955s)

```cpp
#include "task.hpp"

template <typename T, str name>
task<void, "print_all"> print_all(task<T, name>& values)
{
  for (;;) {
    auto v = co_await values;
    std::cout << v << '\n' << std::flush;
  }
}

template <typename T, str name, typename P>
task<T, "filter_in"> filter_in(P predicate, task<T, name>& in)
{
  for (;;) {
    auto v = co_await in;
    if (predicate(v)) {
      co_yield v;
    }
  }
}


int main()
{
  auto incoming = task<int, "incoming">::make();
  auto is_odd = [](auto v) { return v & 1;};
  auto odd_values = filter_in(is_odd, incoming);
  auto printer = print_all(odd_values);
  for (int i = 0; i < 10; ++i) {
    incoming.get_promise().yield_value(i);
  }
}
```

"And when we run this, we see that we got indeed the odd values": 1, 3, 5, 7, 9.

### One piece the slides leave out

`print_all` returns `task<void>`, but the slides' `promise<T>` stores its value in a `std::optional<T>`, and `std::optional<void>` is ill-formed. When the slide code was compiled for this chapter, that is exactly the error it produced. The speaker's real code evidently had a specialisation for `void`, since his demo prints. Here is a minimal one that makes the slide code compile and print 1 3 5 7 9:

**Listing 38** — The promise<void> specialisation that task<void> needs *(illustrative example, not from the talk)*

```cpp
// Add after the primary promise<T> template from Listing 23 and Listing 24.
template <>
struct promise<void>
{
  task<void> get_return_object() noexcept { return {this}; }
  std::suspend_never initial_suspend() noexcept { return {}; }
  std::suspend_always final_suspend() noexcept { return {}; }
  void unhandled_exception() {}
  void return_void() {}
  std::coroutine_handle<> m_continuation;
};
```

A `task<void>` never produces values, so it needs no value slot, no `yield_value`, and no `co_await` support. It exists only to own the frame of a coroutine that consumes values.

## 14. Putting io_uring and coroutines together

*Talk segment: [32:50](https://www.youtube.com/watch?v=54oGgmMAUNk&t=1970s)*

### The pieces are ready

"So let's put these together: io_uring and coroutines." The recap slide lists what we have seen:

- `io_uring` offers asynchronous data, without copying.
- Calling `yield_value()` on a coroutine promise pushes data through the coroutine pipeline.
- How to read values from an upstream coroutine with `co_await`.
- How to forward values downstream with `co_yield`.

"It feels like it should be possible to merge these ideas, right?"

### The glue: a callback that yields

The ring wants callbacks of the form `bool(std::span<char>)`. The pipeline wants values pushed into a promise. A small helper bridges the two:

**Listing 39** — code from the talk (uring + coroutines), shown at [33:49](https://www.youtube.com/watch?v=54oGgmMAUNk&t=2029s)

```cpp
auto to_promise = [](auto& promise) {
  return [&](auto packet) {
    promise.yield_value(packet);
    return true;
  };
};
```

`to_promise(p)` returns a lambda that, when the ring calls it with a packet, pushes the packet into `p` and returns `true` ("keep reading"). The callout calls it a "convenient tool to generate a callback that writes to a promise, and thus drives a coroutine pipeline". Note that the inner lambda captures `promise` **by reference**, so the promise must outlive the ring registration.

### The pipeline

**Listing 40** — code from the talk (uring + coroutines), shown at [33:55](https://www.youtube.com/watch?v=54oGgmMAUNk&t=2035s)

```cpp
int main()
{
  uint64_t num_bytes = 0;
  uint64_t num_packets = 0;
  bool done = false;

  auto in_4000 = task<std::span<char>>::make();
  auto counted_packets = count_packet_data(num_packets, num_bytes, in_4000);
  auto strings = to_string(counted_packets);
  auto stripped_strings = strip_trailing_newline(strings);
  auto lines = concatenate_to<40>(stripped_strings);
  auto print = print_lines(std::cout, lines);
…
```

The program reads like a description of the data flow: packets from port 4000 are counted, converted to strings, stripped of trailing newlines, concatenated into lines of up to 40 characters, and printed. Each stage is a coroutine.

**Counting.** References to the counters are passed in, and the packet is passed through unchanged:

**Listing 41** — code from the talk (uring + coroutines), shown at [34:23](https://www.youtube.com/watch?v=54oGgmMAUNk&t=2063s)

```cpp
task<std::span<char>>
count_packet_data(uint64_t& packets,
                  uint64_t& bytes,
                  task<std::span<char>>& in)
{
  for (;;) {
    auto packet = co_await in;
    ++packets;
    bytes += packet.size();
    co_yield packet;
  }
}
```

**Converting** each `std::span<char>` packet into a `std::string`. This is where the bytes are finally copied out of the ring's buffer:

**Listing 42** — code from the talk (uring + coroutines), shown at [34:49](https://www.youtube.com/watch?v=54oGgmMAUNk&t=2089s)

```cpp
task<std::string> to_string(task<std::span<char>>& in)
{
  for (;;) {
    auto packet = co_await in;
    co_yield std::string{packet.begin(), packet.end()};
  }
}
```

**Cleaning up** trailing newlines (the `echo` command adds one):

**Listing 43** — code from the talk (uring + coroutines), shown at [34:55](https://www.youtube.com/watch?v=54oGgmMAUNk&t=2095s)

```cpp
task<std::string>
strip_trailing_newline(task<std::string>& in)
{
  for (;;) {
    auto s = co_await in;
    while (s.ends_with("\n")) {
      s.resize(s.length() - 1);
    }
    co_yield s;
  }
}
```

**Concatenating** pieces into lines no longer than `line_length`:

**Listing 44** — code from the talk (uring + coroutines), shown at [35:49](https://www.youtube.com/watch?v=54oGgmMAUNk&t=2149s)

```cpp
template <size_t line_length>
task<std::string> concatenate_to(task<std::string>& in)
{
  std::string current_line;
  for (;;) {
    auto next_piece = co_await in;
    if (current_line.length() + next_piece.length() + 1 > line_length) {
      co_yield std::exchange(current_line, next_piece);
    } else if (current_line.empty()) {
      current_line = next_piece;
    } else {
      current_line += " " + next_piece;
    }
  }
}
```

This stage deserves a closer look, because it shows something callbacks make awkward. `current_line` is a **local variable that persists across suspensions**. "If you remember, a local variable in a coroutine becomes a member variable of the coroutine object, so this is a coroutine that has and changes its state every time it gets some new data." If adding the next piece would make the line too long, the stage yields the current line and starts a new one (`std::exchange` returns the old value while storing the new). Otherwise it appends. The stage yields less often than it awaits.

**Printing**, with `:` delimiters "just to make it a little bit easier to see where things are":

**Listing 45** — code from the talk (uring + coroutines), shown at [36:03](https://www.youtube.com/watch?v=54oGgmMAUNk&t=2163s)

```cpp
task<void> print_lines(std::ostream& os, task<std::string>& in)
{
  for (;;) {
    auto line = co_await in;
    os << ':' << line << ":\n";
  }
}
```

### Connecting the ring

**Listing 46** — code from the talk (uring + coroutines), shown at [36:23](https://www.youtube.com/watch?v=54oGgmMAUNk&t=2183s)

```cpp
...
  auto print_and_exit = [&](auto&&) {
    std::cout << "packets=" << num_packets << " bytes=" << num_bytes << '\n';
    done = true;
    return false;
  };

  ring r;
  auto port4000 = udp_socket("127.0.0.1", 4000);
  auto port4001 = udp_socket("127.0.0.1", 4001);
  r.add(port4000.fd(), to_promise(in_4000.get_promise()));
  r.add(port4001.fd(), print_and_exit);
  while (!done) {
    r.wait();
  }
}
```

Port 4000's reads now go straight into the source task's promise, and port 4001 is still the "stop" signal. The demo sends something more interesting than `aa`. A shell loop sends the name of every device in `/dev` as a separate packet:

```
ls /dev | while read n; do echo $n > /dev/udp/127.0.0.1/4000; done
```

The output shows the device names gathered into 40-column lines between colons, and the final message after the stop packet: `packets=248 bytes=1532`.

### Where does the span point?

An audience question at the end of the talk is worth bringing forward. The first stage receives a `std::span<char>` that points into the ring's 1500-byte buffer. Isn't that buffer gone by the time the data reaches later stages? No, "because the original caller is still in scope". The ring's `wait` calls the callback, the callback calls `yield_value`, which *synchronously* resumes the first stage, whose `co_yield` resumes the second, and so on. The whole chain runs on top of the call that owns the buffer, and only then do things "trickle back". The buffer is re-armed for the next read only after the callback returns. Any stage that wants to *keep* the data must copy it, as `to_string` does.

Here is one more stage in the same style, a word splitter. It yields any number of values for each value it awaits, so it needs the `yield_value` fix from the pipelines section; with the talk's original `suspend_always` it prints only `[hello]`.

**Listing 47** — A word-splitting stage *(illustrative example, not from the talk)*

```cpp
// Uses task<T> and promise<T> from Listing 20, Listing 21, Listing 23, Listing 24 and Listing 26,
// with yield_value returning std::suspend_never (see the pipelines section).
#include <sstream>

task<std::string> split_words(task<std::string>& in)
{
  for (;;) {
    auto line = co_await in;
    std::istringstream words(line);
    for (std::string w; words >> w; ) {
      co_yield w;                 // zero or more outputs per input
    }
  }
}

template <typename T>
task<void> print_each(task<T>& in)
{
  for (;;) std::cout << '[' << co_await in << ']';
}

int main()
{
  auto incoming = task<std::string>::make();
  auto words    = split_words(incoming);
  auto printer  = print_each(words);
  incoming.get_promise().yield_value(std::string("hello coroutine world"));
  std::cout << '\n';             // [hello][coroutine][world]
}
```

## 15. Lifetimes, and pipelines as values

*Talk segment: [38:40](https://www.youtube.com/watch?v=54oGgmMAUNk&t=2320s)*

### "This is horrible code"

The io_uring pipeline works, but look at `main` again: five local variables (`counted_packets`, `strings`, `stripped_strings`, `lines`, `print`) exist only so that their coroutines stay alive. The slide highlights them all: "Keeping these coroutine objects alive as explicit objects is ugly, annoying and error prone."

The variables matter. Each stage holds a **reference** to its upstream task (`task<T>& in`). If a task is destroyed while a downstream stage still refers to it, the next value pushed through calls into a destroyed coroutine frame. "Trust me, you need to keep them alive. You will get extremely confusing and frustrating bug hunting if you don't, because then you're calling functions on dead [objects]."

The trap is easy to fall into, because writing the pipeline as one expression looks perfectly natural:

**Listing 48** — A dangling pipeline: the temporary dies before the values arrive *(illustrative example, not from the talk)*

```cpp
// Uses task<T> and promise<T> from Listing 20, Listing 21, Listing 23, Listing 24 and Listing 26, and filter_in from Listing 33.
template <typename T>
task<void> print_all(task<T>& values)
{
  for (;;) std::cout << co_await values << '\n';
}

int main()
{
  auto is_odd = [](auto v) { return v & 1; };
  auto incoming = task<int>::make();

  // BUG: filter_in(...) returns a temporary task. print_all keeps a *reference*
  // to it, but the temporary is destroyed at the end of this full-expression,
  // destroying the filter's coroutine frame while print_all still awaits it.
  // auto printer = print_all(filter_in(is_odd, incoming));   // does not even compile:
  //                                  ^ a temporary cannot bind to task<T>&
  // ...but the same bug compiles if print_all takes `const task<T>&`, or if the
  // named variable holding the filter goes out of scope early:
  auto printer = [&] {
    auto odd = filter_in(is_odd, incoming);
    return print_all(odd);         // returns a task that refers to `odd`...
  }();                             // ...which is destroyed here
  // incoming.get_promise().yield_value(1);   // would resume into a dead frame
}
```

### What a coroutine keeps, and what it doesn't

Here is the rule that explains these bugs. When a coroutine is called, its **parameters are copied (or moved) into the coroutine frame**. For a by-value parameter, the frame then owns its own copy, which lives as long as the frame does. For a **reference** parameter, only the reference is stored, and the frame has no say over the referenced object's lifetime. Since the coroutine may run long after the call returned, reference parameters to coroutines are a classic source of dangling references. (The C++ Core Guidelines advise against reference parameters for coroutines for exactly this reason.)

### Pipelines as values

So: "Can we do better?" The first step is to "reduce the number of references that we need to take care of": take the upstream task **by value**.

**Listing 49** — code from the talk (pipeline as a value type), shown at [39:31](https://www.youtube.com/watch?v=54oGgmMAUNk&t=2371s)

```cpp
template <typename T, typename P>
task<T> filter_in(P predicate, task<T> in)
{
  for (;;) {
    auto v = co_await in;
    if (predicate(v)) {
      co_yield v;
    }
  }
}
```

**Listing 50** — code from the talk (pipeline as a value type), shown at [39:31](https://www.youtube.com/watch?v=54oGgmMAUNk&t=2371s)

```cpp
template <typename T>
task<void> print_all(task<T> in)
{
  for (;;) {
    auto v = co_await in;
    std::cout << v << '\n';
  }
}
```

Now each stage's frame *owns* its upstream task, and with it the upstream coroutine's frame, through the task's `unique_ptr`. Destroying the last stage destroys the whole chain from the end back to the source. The tasks are move-only, so they have to be moved in:

**Listing 51** — code from the talk (pipeline as a value type), shown at [39:51](https://www.youtube.com/watch?v=54oGgmMAUNk&t=2391s)

```cpp
int main()
{
  auto is_odd = [](auto v) { return v & 1;};
  auto incoming = task<int>::make();
  auto &promise = incoming.get_promise();
  auto pipeline = print_all(filter_in(is_odd,
                                      std::move(incoming));
  for (int i = 0; i < 10; ++i) {
    promise.yield_value(i);
  }
}
```

(The slide is one closing parenthesis short at the end of that call; the version compiled for this chapter adds it.) `std::move(incoming)` hands the source task to `filter_in`, the resulting temporary task is moved straight into `print_all`, and only one variable, `pipeline`, has to be kept alive. The promise reference has to be taken *before* the move: `incoming` is empty afterwards, but the promise lives on in the heap frame that `filter_in` now owns.

"It's still not great, but there are fewer objects to keep alive, so it's progress." The thought bubble on the slide puts it more bluntly: *But this is not very nice either, is it?* Reading inside-out, `print_all(filter_in(is_odd, std::move(incoming)))`, is the opposite of the data flow, and the source still has to be named and moved. The next section adds "another level of indirection" to fix the syntax.

## 16. Higher-order stages and a pipe operator

*Talk segment: [42:00](https://www.youtube.com/watch?v=54oGgmMAUNk&t=2520s)*

### Stages that are configured first and connected later

"So what can we do? We can add another level of indirection." Instead of a coroutine that takes both its configuration (the predicate) and its input (the upstream task), make `filter_in` a **higher-order function**: given a predicate, it returns a coroutine that is waiting to be given an input.

**Listing 52** — code from the talk (Another level of indirection), shown at [42:05](https://www.youtube.com/watch?v=54oGgmMAUNk&t=2525s)

```cpp
auto filter_in = [](auto predicate)
{
   return [predicate]<typename T>(task<T> in) -> task<T>
   {
      for (;;) {
         auto v = co_await in;
         if (predicate(v)) {
            co_yield v;
         }
      }
   };
};
```

The outer lambda takes the predicate. It returns an inner lambda that captures the predicate and is itself a coroutine. Being a generic lambda, it works with any `task<T>`. Two remarks from the talk:

- **Lambdas can be coroutines.** "I've been told at one point by another conference presenter that lambdas cannot be coroutines. That is incorrect." But they are tricky, because there are now **two objects**: the lambda object, which holds the captures, and the coroutine frame created when you call it. The frame refers to the captures *through the lambda object*. "If you have moved one of these, bad things will happen … you have to be really, really, really careful about object lifetime."
- **The return type must be explicit** (`-> task<T>`). A coroutine's return type cannot be deduced from its body, because `co_yield` and `co_return` don't say which coroutine type you want. "It knows it must be some coroutine type, but which?"

**Listing 53** — A lambda coroutine needs an explicit return type and a living lambda *(illustrative example, not from the talk)*

```cpp
// Uses task<T> and promise<T> from Listing 20, Listing 21, Listing 23, Listing 24 and Listing 26, with promise<void> added.
int main()
{
  int threshold = 5;

  // The lambda object holds `threshold`; the coroutine frame refers to it via the lambda.
  auto above = [threshold]<typename T>(task<T> in) -> task<T> {   // explicit return type
    for (;;) {
      auto v = co_await in;
      if (v > threshold) co_yield v;
    }
  };

  auto source = task<int>::make();
  auto& p = source.get_promise();
  auto stage = above(std::move(source));    // `above` must outlive `stage`'s coroutine
  auto print = [](task<int> in) -> task<void> {
    for (;;) std::cout << co_await in << ' ';
  }(std::move(stage));                      // captureless: nothing to dangle

  for (int i = 0; i < 10; ++i) p.yield_value(i);   // 6 7 8 9
  std::cout << '\n';
}
```

`print_all` gets the same treatment, capturing the output stream ("Capture the stream"):

**Listing 54** — code from the talk (Another level of indirection), shown at [42:11](https://www.youtube.com/watch?v=54oGgmMAUNk&t=2531s)

```cpp
auto print_all = [](std::ostream& dest)
{
   return [&dest]<typename T>(task<T> in) -> task<void>
   {
      for (;;)
      {
         auto v = co_await in;
         dest << v << '\n' << std::flush;
      }
   };
};
```

The slide's `main` then tries to connect the two:

**Listing 55** — code from the talk (Another level of indirection), shown at [42:19](https://www.youtube.com/watch?v=54oGgmMAUNk&t=2539s)

```cpp
int main()
{
  auto is_odd = [](auto v) { return v & 1;};
  auto incoming = task<int>::make();
  auto &promise = incoming.get_promise();
  auto to_cout = print_all(std::cout);
  auto pipeline = to_cout(filter_in(is_odd));
  auto coro = pipeline(std::move(incoming));
  for (int i = 0; i < 10; ++i) {
    promise.yield_value(i);
  }
}
```

The speaker presents this as an intermediate step that is "still not great". As written, `to_cout(filter_in(is_odd))` does not actually compile: `to_cout` expects a `task<T>`, but `filter_in(is_odd)` is a *stage*, a lambda still waiting for its input. What the slide is reaching for is **composition**, "connect this stage to that stage", and C++ has no built-in operator for composing functions. So the speaker writes one.

### A little helper: coro_stage and operator|

**Listing 56** — code from the talk (A little helper), shown at [43:41](https://www.youtube.com/watch?v=54oGgmMAUNk&t=2621s)

```cpp
template <typename T>
struct coro_stage : T {
     coro_stage(T t) : T(std::move(t)) {}
};

template <typename T>
coro_stage(T) -> coro_stage<T>;

template <typename T, typename U>
auto operator|(coro_stage<T> lh, coro_stage<U> rh) {
   return coro_stage{
      [lh = std::move(lh), rh=std::move(rh)]
      <typename T>(task<T> in)
      {
         return rh(lh(std::move(in)));
      }
   };
}
```

The callouts explain each part:

- **"An indirection wrapper that can be created from any class type."** `coro_stage<T>` inherits from `T`. Lambdas are class types, so a `coro_stage` wrapping a lambda *is* that lambda, with its call operator inherited, but it has a distinct type we can overload operators for.
- **The deduction guide** `coro_stage(T) -> coro_stage<T>;` lets you write `coro_stage{some_lambda}` and get `coro_stage<decltype(some_lambda)>`.
- **"Pipe operators are cool."** `operator|` takes two stages **by value** and returns a new stage. Called with a task, the new stage feeds it through the left stage and then the right: "Call the right side with the result from calling the left side": `rh(lh(std::move(in)))`.
- **"Move these, in case they hold move-only data."** The captures are initialised with `std::move`, because stages can hold move-only things such as tasks.
- **"And wrap this new stage for more pipes."** Because the result is itself a `coro_stage`, it can be piped again: `a | b | c`.

**A compile problem in the slide code.** The inner lambda declares its own template parameter `<typename T>`, but `T` is already a template parameter of `operator|`. Redeclaring a template parameter name in a nested scope is ill-formed. Clang 20 and GCC 16 both reject it ("declaration of template parameter 'T' shadows template parameter"); GCC 16 accepts it only with `-Wno-template-body`, so the GCC used in 2022 probably took it silently. Renaming the inner parameter fixes it, and the pipeline then runs correctly:

**Listing 57** — operator| with the shadowing fixed *(illustrative example, not from the talk)*

```cpp
// The slide's operator| with the inner template parameter renamed from T to V.
template <typename T>
struct coro_stage : T {
     coro_stage(T t) : T(std::move(t)) {}
};

template <typename T>
coro_stage(T) -> coro_stage<T>;

template <typename T, typename U>
auto operator|(coro_stage<T> lh, coro_stage<U> rh) {
   return coro_stage{
      [lh = std::move(lh), rh = std::move(rh)]
      <typename V>(task<V> in)              // was <typename T>: shadows operator|'s T
      {
         return rh(lh(std::move(in)));
      }
   };
}
```

### Using the wrapper

The stage factories return their coroutine lambdas wrapped in `coro_stage` ("Use the wrapper"):

**Listing 58** — code from the talk (Another level of indirection), shown at [44:39](https://www.youtube.com/watch?v=54oGgmMAUNk&t=2679s)

```cpp
auto filter_in = [](auto predicate)
{
   return coro_stage{
      [predicate]<typename T>(task<T> in) -> task<T>
      {
        for (;;) {
          auto v = co_await in;
          if (predicate(v)) {
            co_yield v;
          }
        }
      }
   };
};
```

**Listing 59** — code from the talk (Another level of indirection), shown at [44:43](https://www.youtube.com/watch?v=54oGgmMAUNk&t=2683s)

```cpp
auto print_all = [](std::ostream& dest)
{
   return coro_stage{
    [&dest]<typename T>(task<T> in) -> task<void>
    {
      for (;;)
      {
        auto v = co_await in;
        dest << v << '\n' << std::flush;
      }
    }
  };
};
```

and `main` becomes:

**Listing 60** — code from the talk (Another level of indirection), shown at [45:01](https://www.youtube.com/watch?v=54oGgmMAUNk&t=2701s)

```cpp
int main()
{
  auto is_odd = [](auto v) { return v & 1;};
  auto incoming = task<int>::make();
  auto &promise = incoming.get_promise();
  auto pipeline = filter_in(is_odd) | print_all(std::cout);
  auto coro = pipeline(std::move(incoming));
  for (int i = 0; i < 10; ++i) {
    promise.yield_value(i);
  }
}
```

"Now we're getting somewhere!" `filter_in(is_odd) | print_all(std::cout)` reads left to right in the direction the data flows, and only the composed `coro` needs to be kept alive. "This is actually good, I think. This is quite readable." The live demo of the same code prints the odd numbers again.

There is a subtle lifetime point here too. The composed stage holds the stage lambdas *by value inside its own lambda's captures*, and calling `pipeline(std::move(incoming))` starts coroutines that refer back to those captures. `pipeline` must therefore stay alive and **must not be moved** while `coro` is in use. Keep that in mind; it comes back in the next section.

## 17. Feeding the pipeline from io_uring

*Talk segment: [46:20](https://www.youtube.com/watch?v=54oGgmMAUNk&t=2780s)*

### Problem 1: std::function wants copies

"So how do I get data from the ring into our pipeline, though?" The ring stores its callbacks as `std::function`. The slide shows the old `poller` class here, and the speaker corrects himself on stage: "that's a bug on the slide. It's poller, it shouldn't be, it's ring."

**Listing 61** — code from the talk (Feeding the pipeline from io_uring), shown at [47:13](https://www.youtube.com/watch?v=54oGgmMAUNk&t=2833s)

```cpp
class poller {
public:
  using worker = std::function<void(std::span<char> data)>;
  void add(int fd, worker w) {
    fds_.push_back({fd, POLLIN, 0});
    cbs_.emplace(fd, std::move(w));
  }
  ...
};
```

The callout states the problem: **`std::function<F>` requires `F` to be copyable.** `std::function` is copyable itself, so everything it wraps must be copyable too. But our pipeline stages hold tasks, which hold `std::unique_ptr`s: they are **move-only**. "So, bummer, right?"

**Ouch.** C++23 has the answer, `std::move_only_function`: a type-erased callable wrapper like `std::function` that only requires the target to be movable. The slide shows its cppreference page, marked *(since C++23)*. In 2022 compilers didn't ship it yet, so the speaker "rolled my own naive version":

**Listing 62** — code from the talk (Feeding the pipeline from io_uring), shown at [47:37](https://www.youtube.com/watch?v=54oGgmMAUNk&t=2857s)

```cpp
template <typename>
class move_only_function;

template <typename R, typename ... Args>
class move_only_function<R(Args...)>
{
public:
     move_only_function() = default;
     move_only_function(move_only_function&& r)
     ...
};
```

and swapped it in:

**Listing 63** — code from the talk (Feeding the pipeline from io_uring), shown at [47:45](https://www.youtube.com/watch?v=54oGgmMAUNk&t=2865s)

```cpp
class poller {
public:
  using worker = move_only_function<void(std::span<char> data)>;
  void add(int fd, worker w) {
    fds_.push_back({fd, POLLIN, 0});
    cbs_.emplace(fd, std::move(w));
  }
  ...
};
```

Today you can use the standard one directly. Here is a minimal demonstration of the difference:

**Listing 64** — std::function rejects move-only callables; std::move_only_function accepts them *(illustrative example, not from the talk)*

```cpp
#include <functional>
#include <memory>
#include <span>

int main()
{
    auto state = std::make_unique<int>(0);
    auto callback = [s = std::move(state)](std::span<char> data) mutable {
        *s += static_cast<int>(data.size());
        return true;
    };

    // std::function<bool(std::span<char>)> f = std::move(callback);   // error: lambda is not copyable
    std::move_only_function<bool(std::span<char>)> g = std::move(callback);   // OK (C++23)

    char buf[4]{};
    return g(buf) ? 0 : 1;
}
```

### Problem 2: turning a pipeline into a callback

"Then we have the next problem: how do you create something that is callable with a `span<char>` that feeds into a pipeline via a `task<>`?" The answer on the slide is `init_task`:

**Listing 65** — code from the talk (Feeding the pipeline from io_uring), shown at [48:29](https://www.youtube.com/watch?v=54oGgmMAUNk&t=2909s)

```cpp
template <typename task_type, typename pipeline>
auto init_task(pipeline pipe)
{
   auto input = task<task_type>::make();
   auto& promise = input.get_promise();
   using coro_type = decltype(pipe(std::move(input)));
   return [&promise,
           pipe = std::move(pipe),
           input = std::move(input),
           coro = std::unique_ptr<coro_type>{}](task_type value) mutable
   {
      if (!coro) coro = std::make_unique<coro_type>(pipe(std::move(input)));
      promise.yield_value(value);
      return true;
   };
}
```

Step by step, following the callouts:

1. **"Create the task"**: `input` is a fresh source task of the right element type, and `promise` is a reference to its promise.
2. `coro_type` is the type of the task we would get by feeding `input` through the pipeline. It is computed with `decltype`, *without* calling the pipeline yet.
3. **"Return something callable that initialises the coroutine return object on the first call."** The returned `mutable` lambda captures, *by move*, the pipeline, the input task and an empty `unique_ptr<coro_type>`. On its first call it runs `pipe(std::move(input))` and stores the resulting task on the heap.
4. **"And yields the value to the promise on each call"**: `promise.yield_value(value)`, then `return true` so the ring keeps reading.

The thought bubble says "Not proud of this code", and in the talk he says more. "I had a solution that was very simple, very elegant, very easy to understand, and dead wrong, as I discovered on Wednesday evening." "So I panicked a little bit and I wrote this horrible thing, which I'm not very proud of, but it works."

**Why the lazy initialisation?** The talk doesn't say what was wrong with the simple version, but the previous section suggests why the coroutine is created on the *first call* rather than inside `init_task`. The callback is moved several times on its way into the ring: returned from `init_task`, moved into a `move_only_function`, moved into a `read_work`. Calling `pipe(...)` starts coroutines whose frames refer back to the lambda captures inside `pipe`. If those coroutines were started before the moves, they would refer to a moved-from, destroyed copy. By the time the ring calls the callback for the first time, it sits at its final address inside the `std::list` element and will not move again. Starting the pipeline then is safe. The `unique_ptr` keeps the started pipeline's task on the heap, so its address is stable as well. It's a reasonable guess at what "dead wrong" meant, and it shows how much care lifetimes need.

### The payoff

**Listing 66** — code from the talk (Feeding the pipeline from the uring), shown at [50:09](https://www.youtube.com/watch?v=54oGgmMAUNk&t=3009s)

```cpp
int main()
{
  uint64_t num_bytes = 0;
  uint64_t num_packets = 0;
  bool done = false;

  ring r;
  r.add(port4000.fd(),
        init_task<std::span<char>>(count_packet_data(num_packets, num_bytes)
                                   | to_string
                                   | strip_trailing_newline
                                   | concatenate_to(40)
                                   | print_lines(std::cout)
                                   ));
…
```

"This works." And: "As horrible as that implementation is, we can now write this." One `r.add` call registers port 4000 with a pipeline that counts, converts to strings, strips newlines, concatenates to 40 characters and prints, all on one readable line per stage. "I like this code. This is easy to understand what is happening. This is easy to read." The callout adds: *But this is quite nice, isn't it?*

To get there, every stage was rewritten as a higher-order function returning a `coro_stage`, like `filter_in` in the previous section. `concatenate_to` now takes the line length as a function argument, `concatenate_to(40)`, instead of a template parameter, and `print_lines` takes the output stream. The final live demo uses exactly this code, receiving the device names again and printing them as 40-column lines. "So, implementation gory guts aside, the end result is pretty."

### A tidier init_task

With `std::move_only_function` available, you can at least drop the hand-rolled wrapper, and naming the steps makes the lazy start explicit:

**Listing 67** — init_task with the lazy start spelled out *(illustrative example, not from the talk)*

```cpp
// Uses task<T> and promise<T> from Listing 20, Listing 21, Listing 23, Listing 24 and Listing 26.
#include <functional>

template <typename T, typename Pipeline>
std::move_only_function<bool(T)> make_pipeline_callback(Pipeline pipe)
{
  auto input = task<T>::make();
  auto& promise = input.get_promise();
  using coro_type = decltype(pipe(std::move(input)));

  return [&promise, pipe = std::move(pipe), input = std::move(input),
          coro = std::unique_ptr<coro_type>{}](T value) mutable {
    if (!coro) {
      // First call: we are now at our final address, so the pipeline's coroutines
      // may safely refer back to `pipe`'s captures.
      coro = std::make_unique<coro_type>(pipe(std::move(input)));
    }
    promise.yield_value(value);
    return true;
  };
}

int main()
{
  auto pipe = [](task<int> in) -> task<void> {
    for (;;) std::cout << co_await in << ' ';
  };
  auto cb = make_pipeline_callback<int>(pipe);
  for (int i = 1; i <= 3; ++i) cb(i);   // 1 2 3
  std::cout << '\n';
}
```

## 18. Takeaways, debugging, and questions

*Talk segment: [55:30](https://www.youtube.com/watch?v=54oGgmMAUNk&t=3330s)*

### Takeaways

The speaker's verdict, from the takeaways slide:

- **Coroutines can be cool and powerful.** "There's absolutely no doubt about that."
- **Lack of a good library is a major pain.**
- **Cancelling both coroutines and operations from io_uring can be tricky**, "for completely different reasons":
  - With io_uring, pending operations sit in a shared data structure "mutated by the kernel outside of your control. You cannot just remove the objects … because then the kernel will write to wherever." You must cancel the requests and wait for their completions before freeing the buffers.
  - With coroutines, "the easy way to cancel them is … with exceptions, because then the coroutine chain sort of just dies, but normally you don't want to implement normal control flow with exceptions." It gets "extra tricky if you want to propagate backwards": a stage far down the chain that decides "stop, I don't want more data" has no easy channel back upstream.
- **There are so many ways you can tweak coroutine behaviour; this was but one simplistic example.** The compiler checks which member functions your types have and with which signatures, and "it can generate completely different code depending on these". That is why blog posts about coroutines all look different, which "in the beginning is very confusing". He chose not to touch, for example, how the coroutine frame is allocated.
- **We need a good library and we don't have one, and writing one is *really* hard.**

He also explains why there is no GitHub repository this time: "I fear that someone will not listen to what I say now, that is crap code, and they will download it and use it, and I will feel really bad about it."

### Debugging

"Testing and debugging is terrible, especially if you get the `future<>`/`task<>`/`awaitable<>` types wrong." When writing the coroutine types, "the debugging experience is so bad, it's laughably bad, especially when you get lifetime issues, which I got several times."

His workaround is the named tasks seen in the trace demo. He wrote "a constexpr string class, a very, very simple one, that can be used as a non-type template parameter":

**Listing 68** — code from the talk (Takeaways), shown at [56:43](https://www.youtube.com/watch?v=54oGgmMAUNk&t=3403s)

```cpp
template <size_t N>
struct str {
  constexpr str(const char* p) noexcept {
    std::copy_n(p, N, cstr);
  }
  friend std::ostream& operator<<(std::ostream& os, const str& s) {
    return os << s.cstr;
  }
  char cstr[N];
};

template <size_t N>
str(const char (&)[N]) -> str<N>;
```

and threaded a name through every task and promise type, so that each trace line says which stage it came from:

**Listing 69** — code from the talk (Takeaways), shown at [56:47](https://www.youtube.com/watch?v=54oGgmMAUNk&t=3407s)

```cpp
template <typename T, str name>
struct promise
{
  task<T,name> get_return_object() noexcept {
    std::cerr << "task<T, " << name << ">::get_return_object()\n";
    return {this};
  }
…
};

template <str name>
task<void, "print_lines"> print_lines(std::ostream& os, task<std::string, name>& in)
{
…
```

This uses a C++20 feature: **class types as non-type template parameters**. A literal class type whose members are all public and structural can be a template argument, and the deduction guide `str(const char (&)[N]) -> str<N>` lets you write `task<void, "print_lines">` with a string literal. "It feels 1980s, I think this is just horrible, but it works. It helps. This is how I solved the majority of the bugs I had."

**Listing 70** — A string literal as a template argument (C++20) *(illustrative example, not from the talk)*

```cpp
#include <algorithm>
#include <cstddef>
#include <iostream>

template <std::size_t N>
struct str {
    constexpr str(const char (&p)[N]) noexcept { std::copy_n(p, N, cstr); }
    friend std::ostream& operator<<(std::ostream& os, const str& s) { return os << s.cstr; }
    char cstr[N];
};

template <typename T, str name>
struct tagged {
    T value;
    void trace(const char* what) const { std::cerr << name << ": " << what << '\n'; }
};

int main()
{
    tagged<int, "incoming"> a{4};
    tagged<int, "printer"> b{2};       // a different type from tagged<int, "incoming">
    a.trace("created");
    b.trace("created");
}
```

In the Q&A someone asks about sanitizers. AddressSanitizer and UBSan "will find these for you. The message you get from them may not be the most friendly ever, but you will get the information." Building coroutine code with `-fsanitize=address,undefined` while developing it is cheap insurance against exactly the use-after-destroy bugs this talk warns about.

### The good news

The final takeaways come back to why this is worth the trouble:

- **Writing asynchronous code as if it were local loops is very convenient.** "It's so powerful, it makes life so much easier."
- **Connecting "pipelines" offers great support for generic utilities.** Filters, counters and similar stages "are completely generic. They will work with any type. You just use them."

### Resources

From the resources slide and the Q&A:

- **Shuveb Hussain, "Lord of the io_uring"** (unixism.net/loti), "a really good source, very accessible, easy to understand".
- **Pavel Novikov, "Understanding coroutines by example"**, C++London, February 2021. "For me the key to grok what's happening with coroutines."
- **Lewis Baker, CppCoro**, crossed out on the slide. It was written for the Coroutines TS before standardisation and is no longer maintained. Baker's blog posts on coroutine mechanics "are really worth reading".
- **libunifex** (Facebook experimental), which "uses coroutines and sender/receiver together with io_uring". The speaker hadn't studied it himself, but people he trusts like it.

### Questions

- *Do long pipelines hurt performance, with coroutine frames spread around memory?* "It could be, but I think it's comparable to what you would get if you just chained callbacks." He hasn't measured it. For memory-intensive work, it is "definitely faster than spreading it into threads, because communicating data across core caches is really expensive."
- *Where does the `span<char>` live?* In the ring's per-request buffer, and it stays valid because the original caller is still on the stack while the value trickles through the chain. This was discussed in the section on combining io_uring and coroutines.

The talk ends where it began. The combination of io_uring and coroutines works and the resulting code can be beautiful, but today you have to build, debug and maintain the machinery yourself.

## Key takeaways

- Readiness-based I/O (poll + read) costs at least two system calls and one copy per message; io_uring's completion model lets the kernel write straight into buffers you queued and lets you batch submissions and completions.
- With io_uring the buffer and any per-request state must stay alive and at a stable address until the completion arrives; the 64-bit user_data word usually points to such a state object.
- A C++20 coroutine is turned into a heap-allocated frame plus calls to a promise object; you must supply the return type, the promise type and awaitables yourself.
- co_await asks an awaitable whether to suspend and what value to return on resume; co_yield calls promise.yield_value.
- Push-driven pipelines connect stages that co_await from upstream and co_yield downstream; local variables become persistent per-stage state.
- Lifetimes are the main hazard: coroutine frames, the objects they reference, and lambda objects whose call operator is a coroutine must all outlive their use.
- Small composition helpers (a coro_stage wrapper and operator|) turn a tangle of variables into a readable pipeline expression.
- The approach works, but cancellation, error handling, debugging and the lack of a standard coroutine library remain serious practical obstacles.

## Glossary

| Term | Meaning |
|---|---|
| File descriptor | An integer handle the Linux kernel gives a process for an open file, socket, pipe, etc. |
| Readiness-based I/O | The kernel tells you a descriptor is ready (select/poll/epoll); you then perform the read or write yourself. |
| Completion-based I/O | You hand the kernel the whole operation and buffer up front; it tells you when the operation has completed. |
| io_uring | Linux (5.1+) interface for asynchronous I/O built on two ring buffers shared between kernel and user space. |
| SQE / CQE | Submission Queue Entry (a request you queue) and Completion Queue Entry (the result the kernel posts). |
| user_data | A 64-bit value attached to an SQE and returned in its CQE, typically a pointer to per-request state. |
| Coroutine | A function that can suspend and later resume; in C++20 any function using co_await, co_yield or co_return. |
| Coroutine frame | The compiler-generated, usually heap-allocated object that holds a coroutine's parameters, locals, promise and resume point. |
| Promise type | The class (found via the return type's promise_type) whose member functions customise a coroutine's behaviour. |
| Awaitable / awaiter | An object usable with co_await; its await_ready, await_suspend and await_resume decide whether and how to suspend and what value to produce. |
| coroutine_handle | A non-owning handle to a coroutine frame used to resume or destroy it. |
| Continuation | The coroutine to resume when some awaited event happens. |
| Push-driven pipeline | A chain of stages where the producer pushes each value through the stages, rather than consumers pulling. |
| Higher-order function | A function that takes or returns functions; here, stage factories that return coroutine lambdas. |
| CTAD / deduction guide | Class Template Argument Deduction; a deduction guide tells the compiler which specialisation to create from constructor arguments. |
| std::move_only_function | C++23 type-erased callable wrapper like std::function that accepts move-only callables. |

## Exercises

1. Rewrite the poller from the talk with epoll instead of poll. Which part of the design changes, and does it reduce the number of system calls per packet?

   <details><summary>Hint</summary>epoll_wait still reports readiness; you still need a read per packet.</details>

2. Why does the ring store read_work objects in a std::list rather than a std::vector? What would happen with a vector?

   <details><summary>Hint</summary>The kernel holds a pointer to the buffer and you store &work as user data.</details>

3. In the promise type from the talk, what would change if initial_suspend returned std::suspend_always? Trace the co_1 demo with that change.

   <details><summary>Hint</summary>print_all would not run to its first co_await until someone resumes it.</details>

4. The talk's promise stores values in a std::optional and ignores exceptions. Extend it so that an exception thrown inside a stage is rethrown at the co_await in the next stage.

   <details><summary>Hint</summary>Store std::exception_ptr in unhandled_exception and rethrow in await_resume.</details>

5. Write a coroutine stage that splits each incoming string on spaces and co_yields each word. How many times is yield_value called per co_await, and which change to the talk's promise does it need?

   <details><summary>Hint</summary>Zero or more; see the yield_value return type discussed in the pipelines section.</details>

6. Explain why a lambda whose body is a coroutine is dangerous to move after it has been called, using filter_in from the 'Another level of indirection' slides.

   <details><summary>Hint</summary>The coroutine frame refers to the lambda's captures through the lambda object's this pointer.</details>

7. The operator| on the 'A little helper' slide declares <typename T> twice. Fix it and explain the error your compiler gives for the original.

   <details><summary>Hint</summary>A template parameter name cannot be redeclared in a nested scope.</details>

8. Sketch how you would cancel the pipeline and the outstanding io_uring reads cleanly when port 4001 receives a message.

   <details><summary>Hint</summary>io_uring_prep_cancel; wait for the cancellations' CQEs before freeing buffers; then destroy coroutines from the consumer end.</details>

## Further reading

- Shuveb Hussain, "Lord of the io_uring", https://unixism.net/loti/
- Pavel Novikov, "Understanding coroutines by example", C++London, February 2021
- Lewis Baker, CppCoro (no longer maintained) and his blog posts on coroutine mechanics
- Facebook experimental, libunifex (coroutines and sender/receiver with io_uring support)
- liburing (the library behind the io_uring 'high-level' API used in the talk)
- std::move_only_function (C++23), cppreference
- strace and nping (the measurement tools used in the demos)
- Björn Fahller's slides: https://speakerdeck.com/rollbear/o-and-coroutines-for-smooth-data-streaming-2067ad42-9d0b-40ee-964e-bcca3f9e1e04
