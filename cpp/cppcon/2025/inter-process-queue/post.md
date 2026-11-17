---
title: "Building Robust Inter-Process Queues in C++"
date: 2026-11-15
tags: ["C++", "interprocess communication", "shared memory", "lock-free", "ring buffer", "mmap", "fork", "implicit lifetime", "std::atomic", "API design", "low latency", "CppCon"]
source: "https://www.youtube.com/watch?v=AmPLoOfRFDs"
video_title: "How To Build Robust C++ Inter-Process Queues - Jody Hagins - CppCon 2025"
channel: "CppCon"
---

> This chapter is based on the talk [How To Build Robust C++ Inter-Process Queues - Jody Hagins - CppCon 2025](https://www.youtube.com/watch?v=AmPLoOfRFDs) (CppCon). Code listings marked *from the talk* are transcribed from the slides; listings marked *illustrative* were written for this chapter to explain the ideas.

Moving a queue from threads to processes looks like a small step. You take a single-producer/single-consumer ring buffer that works well between threads, put it in a block of shared memory, and let two programs use it. In practice that step crosses a border the C++ standard hardly mentions. The standard is written for one program running in one address space. Processes appear in it essentially once, in a note about lock-free atomics. Everything else, such as how two processes agree on what a block of bytes *is*, what happens when one of them crashes while holding a lock, and what `fork` does to your carefully cached state, is left to the operating system and to hard-won experience.

In this CppCon 2025 talk, Jody Hagins (Hudson River Trading), who has built low-latency C++ systems for high-frequency trading for decades, walks through what it takes to build message queues that are *robust* across processes. The talk is less about the lock-free algorithm than about everything around it. The topics are an API that enforces who may produce and who may consume, metadata that lets processes check they are talking to the right queue, a lock that can be taken back from a dead process, the object-lifetime rules that make shared-memory code defined behaviour, why `std::atomic` is a trap there, the cache-line size, and the "magic buffer" trick that makes a ring buffer wrap around for free. It ends with the producer and consumer of a multicast queue.

Projector trouble took up much of the talk's first quarter of an hour, so the speaker presented the opening ideas without slides, and he ran out of time at the very end. This chapter follows his argument in order and fills in the background a student needs: processes and virtual memory, `mmap`, SPSC ring buffers, `fork`, implicit lifetime and the rule of special member functions. The talk's code comes from his published slides, and it has been compiled with GCC and Clang to check the claims the talk makes.

## What you will learn

- Distinguish message queues from task queues, and explain why queues between processes are harder than queues between threads
- Explain why pointers stored in shared memory are meaningless to another process, and how to work around it
- Design a queue API that separates the queue, the producer and the consumer and enforces single ownership (the Highlander rule) in code
- Describe what queue metadata (signature, UUID, sizes, flags) protects against
- Build a crash-tolerant lock from a process identity that survives PID reuse, and explain its compare-exchange logic
- Explain what fork does to a multithreaded process and what is allowed in pthread_atfork handlers
- State the C++20 implicit-lifetime rules and decide whether a type is an implicit-lifetime type
- Explain why std::atomic is not an implicit-lifetime type since C++20, and the alternatives (atomic_ref, a custom atomic)
- Explain why std::hardware_destructive_interference_size is unreliable in library headers
- Implement the 'magic buffer' double mapping with mmap and explain why it removes wrap-around logic
- Read the producer and consumer of a multicast queue: callback-based writes, commit with release, lapped detection

## Before you start

- Comfortable with modern C++ (templates, lambdas, RAII, std::span, concepts at a reading level)
- Basic multithreading: std::atomic, memory_order acquire/release, mutexes
- Some familiarity with POSIX (processes, file descriptors) helps; the chapter introduces mmap, fork and pthread_atfork
- Examples use C++20/23 and POSIX APIs; they were tested on Linux

## Contents

1. [Queues across processes: what C++ does not say](#1-queues-across-processes-what-c-does-not-say) — [0:00](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=0s)
2. [Message queues, not task queues](#2-message-queues-not-task-queues) — [3:26](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=206s)
3. [The SPSC ring buffer, and the pointer problem](#3-the-spsc-ring-buffer-and-the-pointer-problem) — [8:20](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=500s)
4. [std::queue, coupling and cohesion](#4-stdqueue-coupling-and-cohesion) — [11:00](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=660s)
5. [Separation of concerns: queue, producer, consumer](#5-separation-of-concerns-queue-producer-consumer) — [13:38](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=818s)
6. [Advisory file locks](#6-advisory-file-locks) — [19:29](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=1169s)
7. [Characteristics and metadata: is this the right queue?](#7-characteristics-and-metadata-is-this-the-right-queue) — [20:35](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=1235s)
8. [A process identity that survives PID reuse](#8-a-process-identity-that-survives-pid-reuse) — [25:05](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=1505s)
9. [Caching the identity, and what fork does to it](#9-caching-the-identity-and-what-fork-does-to-it) — [26:30](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=1590s)
10. [A lock you can take back from a dead process](#10-a-lock-you-can-take-back-from-a-dead-process) — [31:30](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=1890s)
11. [The producer API: write through a callback](#11-the-producer-api-write-through-a-callback) — [34:10](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=2050s)
12. [The consumer API: try_read, no_data and lapped](#12-the-consumer-api-tryread-nodata-and-lapped) — [36:50](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=2210s)
13. [The Daugaard SPSC ring buffer](#13-the-daugaard-spsc-ring-buffer) — [38:20](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=2300s)
14. [Two problems with putting it in shared memory](#14-two-problems-with-putting-it-in-shared-memory) — [41:20](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=2480s)
15. [Implicit lifetime](#15-implicit-lifetime) — [43:40](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=2620s)
16. [std::atomic in shared memory, and the Hinnant rule](#16-stdatomic-in-shared-memory-and-the-hinnant-rule) — [46:30](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=2790s)
17. [Messages must be implicit-lifetime; the cache line size](#17-messages-must-be-implicit-lifetime-the-cache-line-size) — [50:05](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=3005s)
18. [The magic buffer](#18-the-magic-buffer) — [51:50](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=3110s)
19. [Multicast queues, and a question about hosts](#19-multicast-queues-and-a-question-about-hosts) — [55:50](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=3350s)
20. [The multicast producer and consumer](#20-the-multicast-producer-and-consumer) — [59:05](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=3545s)

## 1. Queues across processes: what C++ does not say

*Talk segment: [0:00](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=0s)*

### A border the standard barely mentions

Most C++ developers have written, or at least used, a queue that moves data between two threads. The pattern is familiar: one thread pushes, another pops, and a mutex or a pair of atomic counters keeps them from tripping over each other. Jody Hagins opens his talk by pointing out that this talk is *not* mainly about such queues. It is about what happens when you try to use them **between processes**, and about the issues that come up there.

The reason this deserves a whole talk is that C++ "says almost nothing about processes". The C++ standard describes an *abstract machine* that runs a single program. Threads are part of that model: they share one address space, and the memory model tells you exactly when a write in one thread becomes visible in another. A *process*, by contrast, is an operating-system idea. Each process has its own virtual address space, its own copy of every global variable, and its own heap. When two processes share memory, they do so through operating-system calls such as `mmap`, outside anything the standard defines.

So, the speaker says, what you know about inter-process code is mostly "wisdom that you acquire over time", and it sorts into four kinds:

| Category | What it means |
|---|---|
| Defined in the language | The standard guarantees it. |
| Works | Not promised by the standard, but guaranteed by the platform (POSIX, the compiler, the CPU). |
| Works most of the time | Fine until an unusual timing, a crash, or a new compiler version. |
| Works some of the time | A bug that has not happened yet. |

Robust inter-process code tries to stay in the first two rows. A large part of this chapter is about telling them apart.

### Threads versus processes in one picture

The difference is easy to see in a small experiment. After `fork()`, the child is a copy of the parent, but a *copy*: changes the child makes to an ordinary global variable are invisible to the parent. Threads would see each other's writes; processes do not.

**Listing 1** — After fork, parent and child have separate copies of every variable (POSIX) *(illustrative example, not from the talk)*

```cpp
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
```

Run it and the child prints `child sees 2` while the parent still prints `parent sees 1`. Both processes printed the same *address* for `counter`, yet they were looking at different memory. Each process has its own page tables, so the same virtual address maps to different physical pages. Sharing memory between processes is therefore something you must arrange on purpose, and once you do, many of the comfortable assumptions of single-process C++ (that a pointer means the same thing everywhere, that every object was constructed by someone, that a lock is always released) stop holding.

### What this talk covers

Earlier in 2025 the speaker gave a related talk at C++ on Sea, which went into detail about how the queues themselves work and how their atomic operations are done. This one deliberately does not repeat that. Instead, prompted by questions he received afterwards, he concentrates on:

- the issues that come up around inter-process queues, including "our favorite implicit lifetime stuff";
- details of the implementation, including how to build the **magic buffer**, about which he had had many questions;
- API design that makes the queue hard to misuse across processes.

"There'll be a lot of code", he warned. In the recording, the projector failed for roughly the first quarter of an hour, so the opening ideas (message queues, the classic single-producer/single-consumer design and its problems) were presented without slides. The next sections reconstruct them, adding the background a student needs.

> **Key idea.** Threads share one address space and the C++ memory model describes them. Processes do not, and the standard is almost silent about them. Code that crosses that border must be designed for it.

## 2. Message queues, not task queues

*Talk segment: [3:26](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=206s)*

### Two kinds of queue

The word "queue" covers two different ideas, and the speaker is careful to separate them.

A **task queue** (or job queue) is what most of us picture first. You put tasks into the queue, and a pool of workers takes them out and runs them. If there are ten workers and twenty tasks, the workers "gobble" the first ten, and each task is executed by exactly one worker. A thread pool is built this way.

A **message queue** is different. You put a *message* on the queue to tell someone else something: a price changed, an order was filled, a file is ready. The consumer is not executing your work; it is reacting to information. And when there are several consumers, the speaker's queues give **every consumer every message**, rather than splitting the messages among them.

| | Task queue | Message queue (in this talk) |
|---|---|---|
| What goes in | Work to be done | Information to be delivered |
| With several consumers | Each item goes to one consumer | Each consumer sees every item |
| Typical use | Thread pools, job schedulers | Pipelines, market data, event distribution |

### Messages as a design idea

The speaker points to Alan Kay's original idea of object-oriented programming: in Kay's view, object orientation is "all about the messages" passed between encapsulated components, not about classes and inheritance. He also mentions a 2022 talk of his own on reviving an old design built on *streams* of message-passing modules, which he could now implement at compile time.

The point is to think of message queues *conceptually*. At the most abstract level, "a message queue could be a pointer in a register that gets passed to a function call." If every component just sends messages to "the next thing", you gain a lot of composability, because the sender does not care what the next thing is:

- a function in the same program;
- a queue to a thread in the same process;
- a shared-memory queue read by another process;
- a queue that is forwarded across the network "to something in Europe".

Apart from latency, the sending code is the same in every case. This chapter is about the third case, which the speaker cares about most because of his background in low-latency trading.

### The queues in this talk

Concretely, the queues the talk discusses are:

- **mapped into each process's address space**, so producer and consumer read and write the same physical memory with ordinary loads and stores, without system calls on the fast path;
- **FIFO**: messages come out in the order they went in;
- **single or multiple consumer**: with one consumer it gets every message; with several, *every* consumer sees *every* message;
- **very low latency**.

### Background: how two processes share memory

Since each process has its own address space (see the previous section), sharing must be set up explicitly. On Linux and macOS the main tool is `mmap` with the `MAP_SHARED` flag. It maps some *backing object* into the process's address space, and with `MAP_SHARED`, writes are visible to every other process that maps the same object.

There are two common backing objects:

1. **Anonymous shared memory** (`MAP_SHARED | MAP_ANONYMOUS`). There is no file; the memory is inherited by children across `fork()`. This suits a parent that creates a queue and then forks its workers.
2. **A named object**: a POSIX shared-memory object from `shm_open("/name", ...)` (on Linux it appears under `/dev/shm`), or a file on a RAM-backed or huge-page file system. Unrelated processes that know the name can open and map it. This is what an inter-process queue between *unrelated* programs uses.

The first example shows the mechanism with anonymous memory and `fork`. Unlike the previous section's global variable, the counter now lives in shared pages, so the parent sees the child's updates.

**Listing 2** — Parent and child share a counter through MAP_SHARED anonymous memory (POSIX, C++20) *(illustrative example, not from the talk)*

```cpp
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
```

Notice two details that will matter later. First, the counter is updated through `std::atomic_ref`, because two processes access it concurrently. Second, the code never *constructs* a `long` in the mapped memory; it just starts using the bytes as one. Whether that is allowed is the subject of the implicit-lifetime section later in the chapter.

For unrelated processes, the creating side uses a name instead of `fork`:

**Listing 3** — Creating a named shared-memory region that unrelated processes can open (POSIX) *(illustrative example, not from the talk)*

```cpp
#include <cstdio>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

int main() {
    constexpr std::size_t size = 1 << 20;                  // 1 MiB
    int fd = ::shm_open("/demo_queue", O_CREAT | O_EXCL | O_RDWR, 0600);
    if (fd < 0) { std::perror("shm_open"); return 1; }     // EEXIST: someone created it
    if (::ftruncate(fd, size) != 0) { std::perror("ftruncate"); return 1; }

    void* base = ::mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    ::close(fd);                                           // the mapping stays valid
    if (base == MAP_FAILED) { std::perror("mmap"); return 1; }
    std::printf("mapped /demo_queue at %p\n", base);       // other processes get other addresses

    ::munmap(base, size);
    ::shm_unlink("/demo_queue");                           // remove the name when done
}
```

A second program would call `shm_open` with the same name but without `O_CREAT`, then `mmap` it. The data it sees is the same, but **the address it gets back from `mmap` will usually be different**. That one fact causes the first problem in the next section.

## 3. The SPSC ring buffer, and the pointer problem

*Talk segment: [8:20](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=500s)*

### The classic SPSC ring buffer

Without slides, the speaker described the kind of queue most people find when they look for one: a **single-producer, single-consumer (SPSC)** queue built on **atomic counters**.

The structure has three parts:

- a **buffer** of bytes or slots, used *circularly*: when the writer reaches the end it continues at the beginning (a *ring buffer*);
- a **write counter**, saying where the writer will put the next bytes;
- a **read counter**, saying where the reader will take the next bytes from.

The writer writes some bytes after its counter and then moves the counter forward. The reader sees the new write position, reads everything between its own counter and the writer's, and moves its own counter forward. Because there is exactly one writer and one reader, each counter has a single owner who writes it, and the other side only reads it. No locks are needed; the counters only need to be atomic, with the right memory ordering:

- the writer must store its data **before** publishing the new write counter, so it uses a *release* store;
- the reader must load the write counter with an *acquire* load before reading the data, so it is guaranteed to see the data that was published.

The first example is a minimal version for threads, with fixed-size `int` slots so the essentials stand out.

**Listing 4** — A minimal SPSC ring buffer between two threads (C++20) *(illustrative example, not from the talk)*

```cpp
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdio>
#include <optional>
#include <thread>

template <std::size_t N>              // N must be a power of two
class SpscRing {
    static_assert((N & (N - 1)) == 0);
    std::array<int, N> slots_{};
    std::atomic<std::size_t> head_{0};   // written only by the producer
    std::atomic<std::size_t> tail_{0};   // written only by the consumer
public:
    bool try_push(int v) {
        auto head = head_.load(std::memory_order_relaxed);
        if (head - tail_.load(std::memory_order_acquire) == N) return false;  // full
        slots_[head & (N - 1)] = v;
        head_.store(head + 1, std::memory_order_release);   // publish the slot
        return true;
    }
    std::optional<int> try_pop() {
        auto tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire)) return std::nullopt; // empty
        int v = slots_[tail & (N - 1)];
        tail_.store(tail + 1, std::memory_order_release);   // free the slot
        return v;
    }
};

int main() {
    SpscRing<1024> q;
    std::jthread producer([&] {
        for (int i = 1; i <= 1'000'000; ++i)
            while (!q.try_push(i)) std::this_thread::yield();
    });
    long long sum = 0;
    for (int received = 0; received < 1'000'000;)
        if (auto v = q.try_pop()) { sum += *v; ++received; }
    std::printf("sum = %lld (expected 500000500000)\n", sum);
}
```

The counters only ever grow. The slot index is `counter % N`, and `head - tail` is the number of items currently stored. With `N` a power of two, the modulo compiles to a mask. Later in the chapter the speaker's multicast queue uses exactly this "monotonic counter plus mask" scheme for bytes rather than slots.

### Good implementations exist; can we reuse them?

These queues are "okay to use within a process across threads", and there are excellent ones. The speaker mentions a CppCon talk dedicated to building one with all its intricacies (Charles Frasch's *Single Producer Single Consumer Lock-free FIFO From the Ground Up*), Erik Rigtorp's implementation, and Kaspar Daugaard's, which the talk examines later.

So the natural idea is to take one of them, put the data structure into shared memory, `mmap` it in both processes ("or call `VirtualAlloc` or whatever"), and use it. The first problem appears immediately.

### The pointer problem

A typical ring buffer object contains a **pointer to its buffer**. In one process that is fine. But as the previous section showed, two processes that map the same memory usually get it at **different virtual addresses**. A pointer stored inside the shared structure holds the address *in the process that wrote it*. In any other process that address points somewhere else, perhaps to unmapped memory or, worse, to unrelated data.

You can see the effect without two processes, by mapping the same memory twice in one process:

**Listing 5** — The same memory at two addresses: values travel, pointers do not (Linux) *(illustrative example, not from the talk)*

```cpp
#include <cstddef>
#include <cstdio>
#include <sys/mman.h>
#include <unistd.h>

struct Shared {
    int  value;
    int* pointer;            // an absolute address: only valid in one view
    std::size_t offset;      // relative to the start of the region: valid everywhere
};

int main() {
    int fd = ::memfd_create("demo", 0);                 // an anonymous file (Linux)
    if (fd < 0 || ::ftruncate(fd, 4096) != 0) { std::perror("memfd"); return 1; }
    auto* a = static_cast<Shared*>(::mmap(nullptr, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
    auto* b = static_cast<Shared*>(::mmap(nullptr, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
    std::printf("view A at %p, view B at %p\n", static_cast<void*>(a), static_cast<void*>(b));

    a->value = 42;
    a->pointer = &a->value;                             // points into view A
    a->offset = offsetof(Shared, value);
    std::printf("B reads value %d\n", b->value);        // 42: the bytes are shared
    std::printf("B's pointer still points into A: %s\n",
                b->pointer == &a->value ? "yes" : "no");
    auto* via_offset = reinterpret_cast<int*>(reinterpret_cast<char*>(b) + b->offset);
    std::printf("B via offset: %p (inside view B), value %d\n",
                static_cast<void*>(via_offset), *via_offset);
}
```

The two views share bytes, so a value written through one is read through the other. But a pointer stored through view A still points into view A when read through view B. The general rule is: **store offsets, not pointers, in shared memory**, and let each process compute its own pointers from its own base address.

The speaker frames the fix in terms of roles. The data structure needs a pointer for the reader and a pointer for the writer, each valid only in the process that uses it. That is acceptable, but then you must make sure that "when the writer and the reader come along, they get the pointer so that they can put data in and that *only they* get it." In other words, the question of *who is the producer* and *who is the consumer* becomes part of correctness. That leads to the API discussion in the next two sections.

> **Key idea.** An object that is correct between threads is not automatically correct between processes. Anything that depends on the address space (pointers, vtable pointers, references, many standard containers) is meaningless in a different process.

## 4. std::queue, coupling and cohesion

*Talk segment: [11:00](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=660s)*

### What std::queue lets you do

Consider the API of `std::queue`. The speaker's slide summarised it as "just push and pop, and a constructor or two", and then he corrected himself: it actually has many constructors (around fourteen in recent standards, counting the allocator-aware and iterator-range ones). It also lets you look at the first *and the last* element ("kind of interesting" for a queue), ask for the size, and swap.

The important property is not the number of member functions but who can call them. **Anyone holding a `std::queue` can do everything**: create it, push, pop, inspect. Inside one program, teams cope with this by convention: "this code should only use the push part of the API, and this code should only use the pop part." The speaker argues that across processes "we don't have the luxury" of such a policy. One process creates the queue in shared memory, another is the producer, another the consumer. Why should the producer be *able* to consume? And if one process is the producer, "what's to prevent some other process from just being started up and saying, 'Hey, I want to be the producer, too'?" With normal APIs there are no checks at all. He has seen production systems that do check, but has "yet to find one on the internet that actually does anything with this."

He adds that this care with separation of concerns is worth taking even within a single process, but across processes it is essential.

### Coupling and cohesion

Once the slides returned, the first one was a quotation from James Coplien's *Multi-Paradigm Design for C++*:

> "One goal of design is to minimize coupling between parts and to maximize cohesion within them."

**Coupling** is the strength of the connections *between* program units, how much one depends on another. **Cohesion** is how strongly the things *inside* one unit belong together. A queue object that every component can push to, pop from and resize couples all those components through one wide interface. A producer object that can only write, and a consumer object that can only read, each have high cohesion (they do one thing) and couple to each other only through the queue.

### Requirements of an inter-process queue

The talk then lists what the speaker's queues must do:

- **FIFO**: messages come out in order.
- **Multiple message types**: a queue carries messages of different types and sizes, not one `T`. That is why, later, the producer writes *bytes* and the consumer reads *bytes*.
- **Between unrelated processes**: not only parent and child after `fork`, but programs started independently that find the queue by name.
- **Low latency**.

The opening part of the talk, without slides, adds two more that come back repeatedly: the queue must be **recoverable** (if a producer dies, another can take over), and its API must make wrong usage hard.

### Making roles part of the type

In C++ the natural way to express "this code may only push" is with types: give the code an object whose type only *has* a push operation. The example below wraps a thread-safe in-process queue so that the roles are enforced by the compiler. It is not inter-process yet; the next sections add the cross-process part (making sure there is only one producer). But it shows the shape of the API the speaker argues for.

**Listing 6** — Role-typed handles: the type system decides who may push and who may pop (C++20) *(illustrative example, not from the talk)*

```cpp
#include <condition_variable>
#include <cstdio>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <utility>

class MessageQueue {
    std::mutex m_;
    std::condition_variable cv_;
    std::deque<std::string> items_;
    void push(std::string s) {
        { std::lock_guard lk(m_); items_.push_back(std::move(s)); }
        cv_.notify_one();
    }
    std::string pop() {
        std::unique_lock lk(m_);
        cv_.wait(lk, [&] { return !items_.empty(); });
        auto s = std::move(items_.front());
        items_.pop_front();
        return s;
    }
public:
    class Producer {                       // can only write
        MessageQueue* q_;
        friend MessageQueue;
        explicit Producer(MessageQueue& q) : q_(&q) {}
    public:
        Producer(Producer&&) = default;
        Producer(Producer const&) = delete;
        void push(std::string s) { q_->push(std::move(s)); }
    };
    class Consumer {                       // can only read
        MessageQueue* q_;
        friend MessageQueue;
        explicit Consumer(MessageQueue& q) : q_(&q) {}
    public:
        Consumer(Consumer&&) = default;
        Consumer(Consumer const&) = delete;
        std::string pop() { return q_->pop(); }
    };
    Producer producer() { return Producer(*this); }
    Consumer consumer() { return Consumer(*this); }
};

int main() {
    MessageQueue q;
    auto prod = q.producer();
    auto cons = q.consumer();
    std::jthread t([p = std::move(prod)]() mutable {
        for (int i = 0; i < 3; ++i) p.push("message " + std::to_string(i));
    });
    for (int i = 0; i < 3; ++i) std::printf("%s\n", cons.pop().c_str());
    // cons.push("x");   // error: Consumer has no member named 'push'
}
```

A `Producer` has no `pop`, and a `Consumer` has no `push`: misuse is a compile error rather than a code-review comment. The roles are also move-only, which is a first step towards "there can be only one".

## 5. Separation of concerns: queue, producer, consumer

*Talk segment: [13:38](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=818s)*

### Three objects, three responsibilities

The speaker recommends Klaus Iglberger's *C++ Software Design* here: its first two chapters are largely about separation of concerns and related ideas ("grab Klaus at the conference... he'll be very happy to talk to you all night long"). His speaker notes add why we get this wrong so often: we like what we are comfortable with, and we are shaped by our experiences, like a driver who learned to drive on the left.

Applied to an inter-process queue, separation of concerns gives three objects, each with a short list of responsibilities. This was the central slide of the first part of the talk:

| Object | Responsibilities |
|---|---|
| **The Queue** | Construction (the metadata and the buffer); synchronisation of creation (races with processes starting) |
| **The Producer** | Must have an existing Queue; the Highlander rule; can only write into the queue |
| **The Consumer** | Must have an existing Queue; the Highlander rule; can only read from the queue |

Let us take them one at a time.

### The Queue

The queue object represents the queue and nothing else. It owns the shared memory, which has two parts: **metadata** (what kind of queue this is, its sizes, its locks; see the coming sections) and the **buffer** that holds messages.

Even the queue, which neither produces nor consumes, needs synchronisation. Suppose two programs start at the same moment and both try to create the queue. Exactly one should succeed in creating and initialising it, and the other should either fail or wait until it is ready and then open it. Without care, both might initialise it, or one might open a half-initialised queue. On POSIX, `O_CREAT | O_EXCL` gives you an atomic "create only if it does not exist", and an "initialised" flag written last (with release semantics) tells openers when the metadata is complete.

### The Producer and the Consumer

A producer **must have an existing queue**. The speaker sees two ways to arrange that:

1. the queue has factory functions, for example `make_producer()` and `make_consumer()`;
2. the producer's constructor takes the queue as a parameter.

Both have pros and cons, and usually one fits the surrounding code better. The speaker prefers the first: he does not like "passing a bunch of things into constructors".

Next comes the **Highlander rule**, after the film's motto: *there can be only one*. In a single-producer queue there must be at most one producer at any time, in any process. "Trying to create another producer should fail if there's already a producer in existence." The same holds for the consumer of a single-consumer queue; multi-consumer queues, discussed at the end of the chapter, relax it.

Finally, the producer **can only write** and the consumer **can only read**. That is all they can do.

The speaker stresses that this must be **enforced by code, not by convention**. Within one program, conventions like "you're the producer, you're the consumer" are already fragile, and having two producers is undefined behaviour for an SPSC queue. Across processes it is "almost impossible to enforce" by convention, because the processes may be built by different teams and started by scripts.

### The crash problem

Enforcing the Highlander rule immediately raises a question. The obvious mechanism is a lock in the shared metadata: a would-be producer takes the "producer lock" and holds it for as long as it lives. If the producer shuts down cleanly, its destructor releases the lock. But **what if it crashes?**

This is where the speaker's requirement of **recoverability** comes in. For him, an inter-process queue must allow another producer, perhaps one already running as a standby, to pick up where the dead one left off, on the same queue. That is impossible if the lock is still held by a process that no longer exists. Much of the next part of the talk (process identities, a lock that detects dead owners) solves exactly this.

### Not optional

The slide ended with the words **Not Optional** stamped across it. The speaker argues that if you write queues across processes and do not separate these concerns, you will regret it: "I've done it the other way and there is lots of pain there. It is pain that you experience for days, and the trauma lasts for weeks and months and years, decades."

### Sketching the API

The example below shows one way to express these rules, with the role claim done through a `try_lock` on a lock stored in the queue's metadata. It runs in one process, and the lock is a plain atomic flag; the following sections replace it with a lock that works across processes and survives crashes. What matters here is the shape: the queue hands out roles, a second claim fails with an error instead of silently succeeding, and releasing the role (by destroying the producer) lets someone else take it.

**Listing 7** — A queue that hands out at most one producer at a time (C++23 sketch) *(illustrative example, not from the talk)*

```cpp
#include <atomic>
#include <cstdio>
#include <expected>
#include <utility>

enum class RoleError { already_taken };

class Queue {
    struct Metadata { std::atomic<bool> producer_taken{false}; };   // lives in shared memory later
    Metadata meta_;
public:
    class Producer {
        Metadata* meta_;
        friend Queue;
        explicit Producer(Metadata& m) : meta_(&m) {}
    public:
        Producer(Producer&& o) noexcept : meta_(std::exchange(o.meta_, nullptr)) {}
        Producer& operator=(Producer&&) = delete;
        ~Producer() { if (meta_) meta_->producer_taken.store(false, std::memory_order_release); }
        void write(int /*message*/) { /* only writing is possible */ }
    };

    std::expected<Producer, RoleError> make_producer() {
        bool expected = false;
        if (!meta_.producer_taken.compare_exchange_strong(expected, true, std::memory_order_acquire))
            return std::unexpected(RoleError::already_taken);   // the Highlander rule
        return Producer(meta_);
    }
};

int main() {
    Queue q;
    {
        auto p1 = q.make_producer();
        std::printf("first producer: %s\n", p1 ? "ok" : "refused");
        auto p2 = q.make_producer();
        std::printf("second producer: %s\n", p2 ? "ok" : "refused (role already taken)");
    }
    auto p3 = q.make_producer();
    std::printf("after the first is gone: %s\n", p3 ? "ok" : "refused");
}
```

Output:

    first producer: ok
    second producer: refused (role already taken)
    after the first is gone: ok

`std::expected` (C++23) carries either the role object or an error. The speaker's code uses `tl::expected`, a widely used library implementation of the same idea that predates the standard version.

## 6. Advisory file locks

*Talk segment: [19:29](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=1169s)*

### Shared memory as files

From here on, the speaker limits the discussion to Linux and macOS ("I'll admit I don't know anything about Windows"). On both systems, shared memory can be represented by a **file**:

- on Linux, in `/dev/shm` (a `tmpfs`, RAM-backed file system; this is where `shm_open` objects live) or on a `hugetlbfs` mount such as `/dev/hugepages`, which backs the mapping with large pages;
- on macOS, you can mount a temporary RAM-resident file system and do the same.

Representing a queue as a file has practical advantages. It has a path, which is a natural rendezvous point for unrelated processes; it has permissions; and tools like `ls` show you what queues exist.

### Advisory file locks

Files also come with locks. An **advisory lock** is a lock the kernel records but does not enforce: a process that ignores it can still read and write the file. It works only if every cooperating process checks it, which is fine for a queue library that controls all its users.

Advisory locks "sound awesome", the speaker says, and both Linux and macOS offer several kinds. They even have the property we wanted in the previous section: **when a process dies, the kernel releases its file locks**, so a crashed producer cannot hold a lock forever. The problem, in his words, is that "none of them really work the way that you and I would want them to."

He did not list the problems in the talk, but the well-known ones are worth knowing:

| Mechanism | Owned by | Pitfalls |
|---|---|---|
| `fcntl(F_SETLK)` (POSIX record locks) | the (process, file) pair | Closing **any** descriptor for the file releases **all** the process's locks on it, even one opened by a library you called. Not inherited by `fork` children. Two threads of one process both "own" it. |
| `flock()` (BSD locks) | the *open file description* | Shared with children after `fork` and with `dup`'ed descriptors, so ownership is murky. Whole-file only. Historically unreliable on NFS. |
| `fcntl(F_OFD_SETLK)` (open file description locks) | the open file description | Fixes the worst `fcntl` problems, but is Linux-specific. |

None of them can express "this lock belongs to *the producer role* of *this queue*" across fork, threads and crashes the way a queue needs. Each locking operation is also a system call, which is fine for a role claimed once at start-up but not for anything on the hot path.

### An RAII wrapper

Whatever mechanism you choose, the speaker says you want a class that manages the file lock and **automatically releases it when it goes away**. His slide shows the interface:

**Listing 8** — code from the talk (Advisory File Lock), shown at [20:29](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=1229s)

```cpp
struct AdvisoryFileLock
{
    struct Create { mode_t mode; };

    explicit AdvisoryFileLock(std::filesystem::path path);
    explicit AdvisoryFileLock(
        std::filesystem::path path, Create create);

    void swap(AdvisoryFileLock & that) noexcept;

    bool try_lock();
    void lock();
    void unlock() noexcept;

    std::filesystem::path const & path() const;
};
```

Reading it line by line:

- `struct Create { mode_t mode; };` is a **tag type with data**. Passing `Create{0600}` selects the constructor that creates the file, with those permissions. A separate type avoids an easily-misread `bool create` parameter.
- The first constructor opens an **existing** file; the second **creates** it. Both are `explicit`, so a path does not silently convert into a lock.
- `swap` is `noexcept`, the usual building block for move operations.
- `try_lock`, `lock` and `unlock` are exactly the member functions of the standard **Lockable** requirements. That means `std::unique_lock<AdvisoryFileLock>`, `std::scoped_lock` and `std::try_lock` all work with it. `unlock` is `noexcept` because it is called from destructors.
- `path()` returns the file's path for diagnostics.

Here is one possible implementation of that interface with `flock`, which exists on both Linux and macOS:

**Listing 9** — An AdvisoryFileLock with the slide's interface, implemented with flock (POSIX) *(illustrative example, not from the talk)*

```cpp
#include <cerrno>
#include <cstdio>
#include <filesystem>
#include <mutex>
#include <system_error>
#include <utility>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

struct AdvisoryFileLock {
    struct Create { mode_t mode; };

    explicit AdvisoryFileLock(std::filesystem::path path)
        : path_(std::move(path)), fd_(::open(path_.c_str(), O_RDWR | O_CLOEXEC)) { check(); }
    explicit AdvisoryFileLock(std::filesystem::path path, Create create)
        : path_(std::move(path)),
          fd_(::open(path_.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, create.mode)) { check(); }
    AdvisoryFileLock(AdvisoryFileLock&& that) noexcept : fd_(-1) { swap(that); }
    AdvisoryFileLock& operator=(AdvisoryFileLock that) noexcept { swap(that); return *this; }
    ~AdvisoryFileLock() { if (fd_ >= 0) ::close(fd_); }   // closing releases the lock

    void swap(AdvisoryFileLock& that) noexcept { std::swap(path_, that.path_); std::swap(fd_, that.fd_); }

    bool try_lock() {
        if (::flock(fd_, LOCK_EX | LOCK_NB) == 0) return true;
        if (errno == EWOULDBLOCK) return false;
        throw std::system_error(errno, std::generic_category(), "flock");
    }
    void lock() {
        while (::flock(fd_, LOCK_EX) != 0)
            if (errno != EINTR) throw std::system_error(errno, std::generic_category(), "flock");
    }
    void unlock() noexcept { ::flock(fd_, LOCK_UN); }

    std::filesystem::path const& path() const { return path_; }

private:
    void check() const { if (fd_ < 0) throw std::system_error(errno, std::generic_category(), path_.string()); }
    std::filesystem::path path_;
    int fd_;
};

int main() {
    auto file = std::filesystem::temp_directory_path() / "demo_queue.lock";
    AdvisoryFileLock creator(file, AdvisoryFileLock::Create{0600});
    {
        std::unique_lock first(creator, std::try_to_lock);           // works with std::unique_lock
        AdvisoryFileLock other(file);                                  // a second open file description
        std::printf("first: %d, second while first holds it: %d\n",
                    first.owns_lock(), other.try_lock());
        first.unlock();
        std::printf("second after first unlocked: %d\n", other.try_lock());
    }
    std::filesystem::remove(file);
}
```

The demonstration opens the same file twice. Because `flock` locks belong to the *open file description*, the second object cannot take the lock even inside the same process, which is the behaviour you want from a role lock. When the first object is destroyed, the kernel releases the lock, and the second succeeds. It would also be released if the process crashed. If the program instead `fork`ed while holding the lock, the child would share the lock with its parent, one of the subtleties in the table above.

## 7. Characteristics and metadata: is this the right queue?

*Talk segment: [20:35](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=1235s)*

### A queue should describe itself

When a producer and a consumer meet at a queue, they should be able to check that it is the queue they expect. The speaker puts a set of **characteristics** into the queue's metadata, making each queue "differentiable from other queues". None of this is about speed or atomics. As he puts it, it is about representing "enough data in the queue so that you can connect and make sure that you're not going to have problems later on."

### The signature

The first characteristic is a **signature**:

**Listing 10** — code from the talk (Characteristics), shown at [21:25](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=1285s)

```cpp
struct Characteristics
{
    struct Signature
    {
        std::array<char, 32> value_;

        std::string str() const
        {
            auto v = std::string_view(value_.data(), value_.size());
            return std::string(
                v.data(), std::min(v.size(), v.find('\0')));
        }

        constexpr auto operator <=> (Signature const &) const = default;
    };
```

A real system has several queue implementations, each designed for a particular job: single-producer/single-consumer, multicast, and so on. "There's not one of them that's like the best thing for everything." The signature is a short text description of *which kind* of queue this is. When a producer or consumer opens a queue, it compares the signature with what its code was built for: "my code is built for this type of queue; let me make sure that that's what I'm looking for."

Walking through the code:

- `std::array<char, 32> value_` is a fixed-size character array, not a `std::string`. Everything in shared memory must be plain bytes; a `std::string` contains a pointer to heap memory in one process, which is meaningless in another (recall the pointer problem).
- `str()` converts it to a `std::string` for printing. The text may be shorter than 32 characters and padded with `'\0'`, or exactly 32 characters with no terminator. `v.find('\0')` returns the position of the first zero, or `npos` (the largest `size_t`) if there is none, so `std::min(v.size(), v.find('\0'))` gives the right length in both cases.
- `operator<=>` is defaulted, so signatures can be compared with `==`, `<` and so on, member by member (C++20). It is `constexpr`, so signatures can also be compared at compile time.

The slides go on to define a few more characteristics:

- `struct UUID` holds 16 bytes (`std::array<std::byte, 16>`), with `str()`, a static `make()` that generates a new one, and a defaulted `operator<=>`.
- `Capacity`, `MsgSize`, `MsgAlignment` and `CacheLineSize` are each declared as an **enumeration with no enumerators**, for example `enum struct MsgSize : std::uint32_t {};`. This is a cheap *strong type*: it has the size and representation of its underlying integer, but a `MsgSize` cannot be passed where a `Capacity` is expected. Since C++17 you can create one with `MsgSize{64}` and read it with `std::to_underlying` (C++23).

### Why a UUID?

"Why, Jody? Why are you gonna put a UUID in there? That sounds kind of stupid." The answer comes from experience. Producers and consumers usually rendezvous on a **file path**, which is supposed to be unique. But suppose a clean-up script failed to remove an old queue file. A producer creates a new queue at the path while a consumer is still attached to the old one. Both are "on" the same path, yet they are connected to different queues, and nothing is flowing. That could get you fired, he jokes.

With a UUID, whoever creates a queue publishes not only where to find it but also its unique identifier. Anyone connecting later checks it and knows they are "talking to the right thing".

"How many times are you going to connect to the wrong queue? Never. Except that one day." He recalls an incident in which a problem was isolated and a person shut the system down, all in less than 60 seconds, and millions of dollars were still lost. He has also spoken about the 2012 Knight Capital failure, which he describes as not one error but a dozen, any one of which, had it not happened, would have meant "most of us never even know that incident occurred". His advice: **pay attention to the little things**.

### Sizes and flags

The remaining characteristics are the message size, alignment and cache-line size, and a set of **flags**:

**Listing 11** — code from the talk (Characteristics), shown at [24:15](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=1455s)

```cpp
enum Flags : std::uint16_t
{
    SingleProducer = 0x0001,
    MultiProducer = 0x0002,
    SingleProducerBlocks = 0x0004,
    MultiProducerBlocks = 0x0008,
    SingleConsumer = 0x0010,
    MultiConsumer = 0x0020,
    SingleConsumerBlocks = 0x0040,
    MultiConsumerBlocks = 0x0080,
    Interprocess = 0x0100,
    Multicast = 0x0200,
    Broadcast = Multicast,
    None = 0x0000,
    All = 0xffff,
};
```

Each flag is one bit, so they combine with `|`. They describe the queue's concurrency contract: single or multiple producers and consumers, and whether producers or consumers **block** when they cannot proceed. `Interprocess` marks a queue meant to be shared between processes, and `Multicast` (with `Broadcast` as an alias) marks a queue where every consumer sees every message. `None` and `All` are convenience values.

Why record `Interprocess`? "I know if it's interprocess because I'm going to find it on the system." But there are also in-process queues, and a thread might connect to one by mistake. "Bugs happen." These checks are what make the system **robust**, a word in the talk's title.

`Flags` is a plain (unscoped) `enum` with a fixed underlying type. An unscoped enumeration converts implicitly to an integer, so `SingleProducer | Interprocess` compiles directly, but the result is an `int`, and storing it back needs a cast: `Flags(SingleProducer | Interprocess)`.

### The metadata

All of this is collected in the queue's metadata, which also locates the buffer and holds the **role locks**:

**Listing 12** — code from the talk (MetaData), shown at [24:27](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=1467s)

```cpp
template <typename PidLockT>
requires ProcessIdLockC<PidLockT>
struct MetaData
{
    using PidLock = PidLockT;

    enum struct BufferOffset : std::size_t
    {
    };

    constexpr MetaData() = default;
    explicit MetaData(Characteristics const & c);
    explicit MetaData(
        Characteristics const & c,
        BufferOffset buffer_offset);
```

- `MetaData` is a template on the lock type and requires it to satisfy a concept, `ProcessIdLockC`, so you cannot instantiate it with a lock that is unsafe across processes.
- `BufferOffset` is another empty-enum strong type. The metadata stores where the buffer starts **as an offset**, never as a pointer, which is the rule from the pointer problem.
- The constructors take the characteristics and, optionally, the buffer offset.

The locks are exposed through small factory functions:

**Listing 13** — code from the talk (MetaData), shown at [24:31](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=1471s)

```cpp
std::unique_lock<PidLock> producer_lock()
{
    return std::unique_lock<PidLock>(producer_lock_);
}

std::unique_lock<PidLock> producer_lock(std::try_to_lock_t arg)
{
    return std::unique_lock<PidLock>(producer_lock_, arg);
}

std::unique_lock<PidLock> try_producer_lock()
{
    return producer_lock(std::try_to_lock);
}
```

Each returns a `std::unique_lock` over the producer lock in the metadata. `producer_lock()` blocks until the lock is acquired; `producer_lock(std::try_to_lock)` and its shorthand `try_producer_lock()` try once and return a `unique_lock` whose `owns_lock()` says whether it worked. There is a matching set of `consumer_lock` functions.

Returning a `std::unique_lock` is a neat design. It is movable, so a `Producer` object can keep it as a member. While the producer exists it holds the role, and when the producer is destroyed the role is released automatically. That is the Highlander rule expressed with standard RAII.

The first example puts these ideas together: a fixed-layout header with a signature, a UUID and sizes, checked when the queue is opened.

**Listing 14** — Validating a queue header on open: signature, UUID and sizes (C++23) *(illustrative example, not from the talk)*

```cpp
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <expected>
#include <type_traits>
#include <utility>

enum struct MsgSize : std::uint32_t {};               // strong types: same bits, different types
enum struct Capacity : std::uint64_t {};

struct QueueHeader {                                   // lives at offset 0 of the shared region
    std::array<char, 32> signature;
    std::array<std::byte, 16> uuid;
    Capacity capacity;
    MsgSize max_msg_size;
};
static_assert(std::is_trivially_copyable_v<QueueHeader> && std::is_standard_layout_v<QueueHeader>);

enum class OpenError { wrong_kind, wrong_queue, too_small };

std::expected<QueueHeader const*, OpenError>
open_queue(void const* region, char const* kind, std::array<std::byte, 16> const& uuid, MsgSize need) {
    auto const* h = static_cast<QueueHeader const*>(region);
    if (std::strncmp(h->signature.data(), kind, h->signature.size()) != 0)
        return std::unexpected(OpenError::wrong_kind);
    if (h->uuid != uuid) return std::unexpected(OpenError::wrong_queue);    // stale file at the same path?
    if (std::to_underlying(h->max_msg_size) < std::to_underlying(need))
        return std::unexpected(OpenError::too_small);
    return h;
}

int main() {
    alignas(QueueHeader) std::byte region[sizeof(QueueHeader)]{};
    auto* h = new (region) QueueHeader{};                 // the creator initialises the header
    std::strncpy(h->signature.data(), "spsc-bytes-v1", h->signature.size());
    h->uuid = {std::byte{0x12}, std::byte{0x34}};
    h->capacity = Capacity{1 << 20};
    h->max_msg_size = MsgSize{256};

    std::array<std::byte, 16> other_uuid{std::byte{0x99}};
    auto ok  = open_queue(region, "spsc-bytes-v1", h->uuid, MsgSize{128});
    auto bad = open_queue(region, "spsc-bytes-v1", other_uuid, MsgSize{128});
    std::printf("same uuid: %s, other uuid: %s\n", ok ? "ok" : "rejected",
                bad ? "ok" : (bad.error() == OpenError::wrong_queue ? "rejected (wrong queue)" : "rejected"));
}
```

The second shows the `unique_lock` pattern with a toy lockable type, the shape that the next sections fill in with a real cross-process lock.

**Listing 15** — Holding a role through a std::unique_lock member (C++20) *(illustrative example, not from the talk)*

```cpp
#include <atomic>
#include <cstdio>
#include <mutex>
#include <optional>
#include <utility>

struct SpinLock {                                   // any type with lock/try_lock/unlock works
    std::atomic<bool> held{false};
    bool try_lock() { return !held.exchange(true, std::memory_order_acquire); }
    void lock() { while (!try_lock()) {} }
    void unlock() { held.store(false, std::memory_order_release); }
};

struct MetaData {
    SpinLock producer_lock_;
    std::unique_lock<SpinLock> try_producer_lock() { return {producer_lock_, std::try_to_lock}; }
};

class Producer {
    std::unique_lock<SpinLock> role_;               // owning the lock == being the producer
public:
    explicit Producer(std::unique_lock<SpinLock> role) : role_(std::move(role)) {}
};

std::optional<Producer> make_producer(MetaData& md) {
    auto lk = md.try_producer_lock();
    if (!lk.owns_lock()) return std::nullopt;
    return Producer(std::move(lk));
}

int main() {
    MetaData md;
    auto a = make_producer(md);
    auto b = make_producer(md);
    std::printf("a: %d, b: %d\n", a.has_value(), b.has_value());     // a: 1, b: 0
    a.reset();                                                        // role released by RAII
    std::printf("after a is gone: %d\n", make_producer(md).has_value());
}
```

## 8. A process identity that survives PID reuse

*Talk segment: [25:05](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=1505s)*

### Locks that outlive their owners

The previous section ended with role locks in the queue's metadata. In shared memory they raise the crash problem again. If a producer takes the producer lock and then dies, "it will not release that mutex". The mutex, counter or atomic is sitting in shared memory, still saying "taken", long after the owner is gone. A normal mutex has no idea who holds it, so nobody can tell whether the holder is alive.

The speaker's solution starts with recording **who** holds the lock, in a form that can later be checked against the processes that are actually running.

### Why the PID is not enough

The obvious "who" is the process id, the PID. But, the speaker notes, "the PID is not unique. The PID is small and it gets reused." On Linux, PIDs are allocated from a range limited by `/proc/sys/kernel/pid_max` (often 4,194,304 on modern systems, 32,768 on older ones), and when the counter reaches the limit it wraps around and reuses the numbers of processes that have exited. On a busy machine this can happen within hours. So "the lock is held by PID 4711" does not prove anything: process 4711 may have died long ago, and a completely different program may now have that number.

The chance of accidental reuse at exactly the wrong moment is small, "but it does happen", and robust systems are built around the things that do happen.

### PID plus start time

The speaker's `ProcessId` stores two things:

1. the PID (`pid_t`) the system gave the process;
2. the **time the process started**, "in as much granularity as possible, hopefully in nanoseconds".

A PID can be recycled, but a recycled PID belongs to a process that started *later*. The pair (PID, start time) therefore identifies one process over the machine's lifetime: "it's unlikely that you're going to recycle through all the PIDs in that amount of time." To check whether the owner of a lock is still alive, you look up the process that currently has that PID, if any, and compare its start time with the one you stored.

The value is stored in an **atomic**, because it *is* the lock word: acquiring the lock means atomically swapping "nobody" for "me". That puts a size limit on it. The slide shows how the speaker chooses the representation:

**Listing 16** — code from the talk (Process ID), shown at [26:23](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=1583s)

```cpp
using Value = std::conditional_t<
    std::atomic<__uint128_t>::is_always_lock_free,
    __uint128_t,
    std::uint64_t>;
Value value_;
```

- `std::conditional_t<Cond, A, B>` picks type `A` if `Cond` is true and `B` otherwise, at compile time.
- The condition is `std::atomic<__uint128_t>::is_always_lock_free`: does this platform guarantee lock-free atomic operations on a 128-bit integer? (`__uint128_t` is a GCC/Clang extension, not standard C++.)
- If it does, the id uses 128 bits, with plenty of room for a PID and a nanosecond timestamp. If not, it falls back to 64 bits, and the PID and a coarser start time have to be packed together.

Lock-freedom is what matters here. A non-lock-free `std::atomic` is typically implemented with a hidden lock that lives in the process that uses it (libatomic uses a table of locks in each process's memory), so it would **not** synchronise two processes at all. The standard's note about atomics and processes, discussed later in the chapter, applies only to lock-free atomics.

### Which platforms qualify?

The speaker reports that on his Mac the 128-bit version is chosen, while on the Linux machines he uses, all Intel x86-64, only 64 bits are available. ARM64 has 128-bit atomic instructions, and x86-64 has `cmpxchg16b`, but whether the compiler *promises* lock-freedom depends on the compiler and its flags. On the machine used for this chapter (x86-64, Linux), the trait gave these values:

| Compiler | Flags | `atomic<__uint128_t>::is_always_lock_free` |
|---|---|---|
| GCC 16 | default | `false` |
| GCC 16 | `-mcx16` | `false` (GCC routes 16-byte atomics through libatomic) |
| Clang 20 | default | `false` |
| Clang 20 | `-mcx16` | `true` |

So the same source code can produce a 64-bit or a 128-bit `ProcessId` depending on how it is built. Since the id is in shared memory, every process that uses the queue must agree on the choice. That is another reason the queue's metadata records its layout and why both sides check it.

With 64 bits "theoretically it's possible that you could clash, but it's very unlikely", says the speaker: "the sun will explode tomorrow before that happens."

### Computing a process's identity on Linux

On Linux, the start time of any process can be read from `/proc/<pid>/stat`: field 22, `starttime`, is the time the process started after system boot, in clock ticks (usually 1/100 s). The example below builds a (PID, start time) identity this way and uses it to check whether a recorded owner is still alive. A finer clock would be better, as the speaker says, but ticks are enough to show the idea.

**Listing 17** — A (PID, start time) identity from /proc and a liveness check (Linux) *(illustrative example, not from the talk)*

```cpp
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
```

A run on the machine used for this chapter printed:

    me: pid 1753205 start 121361352 -> alive
    child: pid 1753206 start 121361352 -> alive while running
    child after exit: dead

Notice that parent and child have the **same** start time: the child was forked within the same 10 ms tick. Here the PIDs differ, so the identities are still distinct. But it shows why the speaker wants the start time "in as much granularity as possible": with coarse ticks, a PID that is recycled very quickly could produce the same (PID, start time) pair.

This is why computing a `ProcessId` is **expensive**: it involves opening and parsing a file in `/proc`. The next section shows how the speaker caches it, and what goes wrong with that cache when a process forks.

## 9. Caching the identity, and what fork does to it

*Talk segment: [26:30](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=1590s)*

### Caching an expensive value

Computing the current process's `ProcessId` is expensive: on Linux it reads `/proc`, and on macOS it calls other system APIs. Since a process's identity does not change, the natural thing is to compute it once and cache it. The speaker does this with a function-local `static` initialised by an **immediately invoked lambda**:

**Listing 18** — code from the talk (Process ID), shown at [28:27](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=1707s)

```cpp
ProcessId
ProcessId::
current()
{
    static ProcessId id = [] {
        ::pthread_atfork(
            nullptr, // prepare
            nullptr, // parent
            [] { id = ProcessId{::getpid()}; });
        return ProcessId{::getpid()};
    }();
    return id;
}
```

Start with the simple version, without the `pthread_atfork` call. `static ProcessId id = [] { return ProcessId{::getpid()}; }();` defines a lambda and calls it at once (the `()` at the end). The result initialises `id` the first time `current()` runs. C++11 guarantees that this initialisation happens exactly once even when several threads call `current()` at the same time (the so-called *magic statics*). Every later call just returns the cached value. The immediately invoked lambda is a common idiom for initialisation that needs several statements.

(As in the slide, the code constructs a `ProcessId` from just `::getpid()`. The real identity also contains the start time, as described in the previous section.)

### "But what happens if it forks?"

When the speaker showed this code at C++ on Sea, someone asked exactly that. He paused at the same slide this time "because that would be really cool" if it happened again.

`fork()` creates a child process that is a copy of the parent, *including the parent's memory*. If the parent has already called `current()`, the child inherits the initialised static, so in the child `current()` returns **the parent's identity**. A child that then takes a lock would record its parent as the owner. If the child dies holding that lock, the dead-owner check would see the parent alive and never recover it. Worse, the parent might believe it holds a lock that the child actually took.

This is not an exotic scenario. In a system of cooperating processes there is usually at least one **orchestrator** that starts the others: it forks and execs, and before exec it often does more sophisticated set-up of the environment for its children. Anything such a process caches is exposed to this problem.

### pthread_atfork

POSIX provides a hook for this: `pthread_atfork(prepare, parent, child)` registers three functions (any may be null):

- `prepare` runs in the parent **before** the fork;
- `parent` runs in the parent **after** the fork;
- `child` runs in the child **after** the fork.

In the slide, the `child` handler recomputes `id`. Note how it reaches `id`: the inner lambda has an empty capture list, yet it assigns to `id`. That works because `id` is a `static` variable with a fixed address, so a lambda can name it without capturing it. And it *must* be captureless, because `pthread_atfork` is a C function that takes plain **function pointers**, and only a captureless lambda converts to one. The lambda also refers to `id` inside `id`'s own initialiser, which is allowed: a variable's name is in scope from the end of its declarator, before the `=`.

The first example shows the stale cache and the fix side by side.

**Listing 19** — A cached process id goes stale in a forked child; pthread_atfork fixes it (POSIX) *(illustrative example, not from the talk)*

```cpp
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
```

Typical output:

    parent pid 61000
    child: real pid 61001, naive cache says 61000, atfork cache says 61001

### What you may do in the child

The speaker's next slide, **Fork Handling**, lists the rules that make this area dangerous.

After `fork()` in a *multithreaded* program, **only the thread that called fork exists in the child**. All the other threads have vanished, but the child still has the same memory and objects as the parent did. If another thread was holding a lock at the moment of the fork (the lock inside `malloc`, a logging library's mutex, a `std::mutex` of yours), that lock is copied into the child **in the locked state**, and there is no thread left to unlock it. The next attempt to take it in the child deadlocks forever.

POSIX therefore says that, until it calls `exec`, a child of a multithreaded process may only call **async-signal-safe** functions, the same restricted set that may be called from a signal handler. "If you can't write the code in a signal handler, then you can't write it in the child." That means:

- low-level system calls (`read`, `write`, `open`, `close`, `dup2`, `execve`, `_exit`...) are fine;
- **no** `malloc`, no `printf`, no locks, no `stdio`, and most library code, especially threaded libraries with internal locks, is off-limits.

The speaker points out an awkward detail: `mmap` is *not* on POSIX's list of async-signal-safe functions, and anonymous mappings are not POSIX at all. He has not been bitten by calling `mmap` in a child, "but technically you're not supposed to".

The child handler in the slide stays within these rules as long as computing the identity does (`getpid` is async-signal-safe, and the assignment is a plain store). A version that reads `/proc` with `open` and `read` would be allowed too; one that used `std::ifstream` would not.

### Managing at-fork handlers

`pthread_atfork` itself has limitations that led the speaker to write his own `AtFork` class:

- a registered handler can **never be removed**, and you cannot change it;
- **ordering is fixed by registration order**: `prepare` handlers run in the *reverse* order of registration, `parent` and `child` handlers in registration order;
- registration often happens during static initialisation or when libraries are loaded, so "you have no idea which one's going to get called first."

His class installs a single set of handlers and keeps its own list that can be added to, removed from and prioritised. The second example shows the built-in ordering.

**Listing 20** — The order in which pthread_atfork handlers run (POSIX) *(illustrative example, not from the talk)*

```cpp
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
```

Output:

    prepare 3
    prepare 2
    prepare 1
    child 1
    child 2
    child 3

The reverse order for `prepare` is deliberate. Libraries typically use `prepare` to take their locks before the fork, so that no lock is mid-operation when memory is copied, and `parent`/`child` to release them. Reverse-then-forward gives the same nesting as constructors and destructors.

The slide's last line is "Here be Dragons; Real and Scary Dragons." "If you ever find yourself thinking, hey, I need to write an atfork handler for this, you need to think about it again." Yet, the speaker says, if you have any kind of lock (even inside your own allocator) in a program that forks, you probably will need one. What does this have to do with message queues? "Nothing. But it has everything to do with processes."

## 10. A lock you can take back from a dead process

*Talk segment: [31:30](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=1890s)*

### The lock

With a process identity that can be checked for liveness, the speaker builds the lock that guards the producer and consumer roles. Its state is a single atomic `ProcessId`: the null id means "unlocked", any other value names the owner.

**Listing 21** — code from the talk (Process ID Lock), shown at [31:45](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=1905s)

```cpp
template <typename BlockPolicyT>
struct TProcessIdLock
{
    using BlockPolicy = BlockPolicyT;

    TProcessIdLock() = default;

    explicit TProcessIdLock(std::in_place_t inplace);

    bool try_lock()
    {
        return pidlock_detail::try_lock_impl(
            data_.pid,
            ProcessId::current());
    }
```

- `TProcessIdLock` is a template on a **block policy**, which decides what a thread does when it has to wait (spin, yield, sleep, or a futex-style wait on the lock word). The policy is part of the lock's type so different queues can choose differently.
- The default constructor is defaulted. The `std::in_place_t` constructor is how the lock is constructed in place in shared memory when a queue is created.
- `try_lock()` delegates to a free function, `pidlock_detail::try_lock_impl`, passing the lock word (`data_.pid`) and the caller's identity, `ProcessId::current()` from the previous section.

Because the class has `try_lock`, `lock` and `unlock`, it satisfies the standard Lockable requirements, which is why the metadata can hand out `std::unique_lock<PidLock>` objects for it.

### lock(): spin briefly, then wait

**Listing 22** — code from the talk (Process ID Lock), shown at [31:49](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=1909s)

```cpp
void lock()
{
    auto const me = ProcessId::current();
    for (int i = 0; i < 10; ++i) {
        if (pidlock_detail::try_lock_impl(data_.pid, me)) {
            return;
        }
        std::this_thread::yield();
    }
    data_.block_policy().wait([&] {
        return pidlock_detail::try_lock_impl(data_.pid, me);
    });
}
```

`lock()` first computes `me` once, because computing it is not free. Then it tries `try_lock_impl` up to ten times, calling `std::this_thread::yield()` between attempts so other threads can run. Most of the time the lock is free or about to be, and this fast path succeeds. If ten attempts fail, it hands over to the block policy: `wait` keeps calling the predicate (here, another `try_lock_impl` attempt) until it returns true, blocking or sleeping in between. In the speaker's words, it "tries a few times real quick" and then "goes into a more waiting policy where you might actually go to sleep".

### unlock(): only the owner can unlock

**Listing 23** — code from the talk (Process ID Lock), shown at [32:23](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=1943s)

```cpp
void unlock()
{
   auto me = ProcessId::current();
    data_.pid.compare_exchange_strong(
        me,
        ProcessId::null());
    data_.block_policy().notify_one();
}

ProcessId pid() const
{
    return data_.pid.load(std::memory_order_acquire);
}
```

`unlock()` uses `compare_exchange_strong(me, ProcessId::null())`: if the lock word currently equals `me`, replace it with null; otherwise change nothing. A plain store of null would let *any* caller release the lock. The compare-exchange is "a little bit safer just because it will only unlock it if you actually hold the lock. You shouldn't be calling it if you don't hold the lock." Note that `me` is a non-`const` local: `compare_exchange_strong` takes its first argument by reference and overwrites it with the current value when the comparison fails. After unlocking, `notify_one()` wakes a waiter blocked in the block policy, if any. (The slide's first line inside `unlock` is indented by three spaces instead of four, a harmless typo kept as shown.)

`pid()` reports the current owner with an acquire load, for diagnostics.

### try_lock_impl: taking the lock from a dead owner

The heart of the design is `try_lock_impl`. In the talk it was shown with two different lines highlighted.

**Listing 24** — code from the talk (Process ID Lock), shown at [33:45](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=2025s)

```cpp
bool try_lock_impl(wjh::Atomic<ProcessId> & pid, ProcessId const & me)
{
    auto expected = PID::null();
    if (exchange(pid, expected, me)) return true;
    if (expected != me) {
        if (auto p = PID::maybe(expected.pid());
            not p || expected != *p)
        {
            exchange(pid, expected, PID::null());
            expected = PID::null();
            if (exchange(pid, expected, me)) return true;
        }
    }
    return false;
}
```

`exchange(pid, expected, desired)` is a helper around a compare-exchange: if `pid` equals `expected`, store `desired` and return true; otherwise copy the current value into `expected` and return false. `PID` is an alias for `ProcessId`. Step by step:

1. **Try the easy case.** `expected` starts as null. If the lock word is null, it becomes `me` and we own the lock: return true.
2. **Find out who holds it.** If that failed, `expected` now contains the current owner.
3. **Is it me?** If `expected == me`, this process already holds the lock, perhaps through another thread. The function returns false; it is not a recursive lock.
4. **Is the owner alive?** Otherwise, take the owner's PID and compute the identity of whatever process has that PID now: `PID::maybe(expected.pid())`. This is the expensive computation from the previous sections, done only on this slow path. Two outcomes mean the owner is dead:
   - `not p`: no process with that PID exists;
   - `expected != *p`: a process with that PID exists, but with a different start time, so the PID was recycled.

   As the speaker put it, "either case, it means that process is dead and it's gone. And we will rip the lock out of his cold dead hands."
5. **Clear the dead owner** (the first highlighted line): `exchange(pid, expected, PID::null())`. This only succeeds if the lock word still holds the dead owner.
6. **Compete again** (the second highlighted line): reset `expected` to null and try once more to swap null for `me`.

Why not swap the dead owner directly for `me` in step 5? Because, as the speaker says, "we can't say that we have it... there could be another process somewhere running on our system that's trying to get that lock too." Several processes may notice the same dead owner at the same time. Each tries to clear it, and at most one succeeds; the others' compare-exchange simply fails. Then they all compete fairly for the free lock in step 6, with the same compare-exchange that a normal acquisition uses. Exactly one wins. Clearing and acquiring as separate steps keeps the recovery path from becoming a special way of acquiring the lock.

The example below runs the same logic in one process, with a fake process table standing in for `/proc`. It packs the PID and start time into one 64-bit atomic, the fallback representation from two sections back.

**Listing 25** — try_lock_impl's recovery logic, simulated with a fake process table (C++20) *(illustrative example, not from the talk)*

```cpp
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <map>
#include <optional>

struct PID {                                     // 64-bit packed (pid, start time)
    std::uint64_t v = 0;
    static PID null() { return {}; }
    static PID make(std::uint32_t pid, std::uint32_t start) { return {std::uint64_t(pid) << 32 | start}; }
    std::uint32_t pid() const { return std::uint32_t(v >> 32); }
    bool operator==(PID const&) const = default;
    static std::map<std::uint32_t, std::uint32_t> running;   // fake process table: pid -> start
    static std::optional<PID> maybe(std::uint32_t pid) {     // identity of the process with this pid now
        if (auto it = running.find(pid); it != running.end()) return make(pid, it->second);
        return std::nullopt;
    }
};
std::map<std::uint32_t, std::uint32_t> PID::running;

bool exchange(std::atomic<PID>& pid, PID& expected, PID desired) {
    return pid.compare_exchange_strong(expected, desired, std::memory_order_acq_rel,
                                       std::memory_order_acquire);
}

bool try_lock_impl(std::atomic<PID>& pid, PID const& me) {     // same logic as the talk's slide
    auto expected = PID::null();
    if (exchange(pid, expected, me)) return true;
    if (expected != me) {
        if (auto p = PID::maybe(expected.pid()); not p || expected != *p) {
            exchange(pid, expected, PID::null());              // clear the dead owner
            expected = PID::null();
            if (exchange(pid, expected, me)) return true;      // compete like everyone else
        }
    }
    return false;
}

int main() {
    std::atomic<PID> lock{PID::null()};
    PID::running = {{200, 5}, {300, 7}, {400, 9}};
    auto const p200 = PID::make(200, 5), p300 = PID::make(300, 7), p400 = PID::make(400, 9);

    std::printf("free lock, 200 tries: %s\n", try_lock_impl(lock, p200) ? "acquired" : "refused");
    std::printf("live owner 200, 300 tries: %s\n", try_lock_impl(lock, p300) ? "acquired" : "refused");
    PID::running.erase(200);                                   // 200 crashes holding the lock
    std::printf("owner 200 died, 300 tries: %s\n",
                try_lock_impl(lock, p300) ? "acquired (recovered)" : "refused");
    PID::running[300] = 8;                                     // 300 dies, its pid is reused
    std::printf("owner 300 recycled (same pid, new start), 400 tries: %s\n",
                try_lock_impl(lock, p400) ? "acquired (recovered)" : "refused");
}
```

Output:

    free lock, 200 tries: acquired
    live owner 200, 300 tries: refused
    owner 200 died, 300 tries: acquired (recovered)
    owner 300 recycled (same pid, new start), 400 tries: acquired (recovered)

For this chapter, the slides' `TProcessIdLock` and `try_lock_impl` were compiled verbatim with GCC 16 and Clang 20, with a stub `ProcessId` (64-bit, since neither compiler reports 128-bit atomics as always lock-free here), `wjh::Atomic` as an alias for `std::atomic`, an `exchange` helper, and a spinning block policy. The lock satisfies a Lockable concept, instantiates `MetaData<PidLock>`, and locks and unlocks correctly.

### Doesn't the OS already do this?

You might ask why the operating system doesn't already provide such a lock. On Linux it does: a POSIX mutex with the **robust** attribute (`PTHREAD_MUTEX_ROBUST`), placed in shared memory with `PTHREAD_PROCESS_SHARED`. If its owner dies, the next `pthread_mutex_lock` returns `EOWNERDEAD` instead of blocking forever. The new owner can repair the protected data and call `pthread_mutex_consistent` to make the mutex usable again.

**Listing 26** — A robust process-shared mutex reports EOWNERDEAD when its owner dies (Linux) *(illustrative example, not from the talk)*

```cpp
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
```

The speaker's reasons for not relying on it: there are some issues with robust mutexes in shared memory, and, decisively for him, **they don't work on macOS**, where his code also has to run. His `TProcessIdLock` behaves the same on both.

One consequence of building the lock on a *process* identity is worth noting. The owner is a process, not a thread. Two threads in the same process have the same `me`, so the second one's `try_lock` returns false (step 3), but nothing stops it from calling `unlock()` successfully. That fits the lock's purpose: it guards a role that belongs to a process, and within the process the producer object itself should not be shared between threads.

## 11. The producer API: write through a callback

*Talk segment: [34:10](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=2050s)*

### The producer can only write

With the role lock in place, the talk turns to what a producer can actually do. Following the separation of concerns, the answer is one thing: **write**. The producer shown here is the API of the speaker's multicast queue, which appears in full at the end of the chapter.

Writing a message is a three-step protocol under the hood:

1. **prepare** a buffer: reserve enough contiguous, suitably aligned bytes in the ring;
2. **fill** it with the message;
3. **publish** it: update the shared counter so consumers can see the new bytes.

Steps 1 and 3 involve the queue's internals (counters, alignment, atomics) and must happen in exactly the right order. The speaker does not want users to repeat them "over and over again". So the producer has a single entry point, `write`, that takes a callback:

**Listing 27** — code from the talk (Producer), shown at [35:31](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=2131s)

```cpp
template <typename InvocableT, typename... ArgTs>
std::invoke_result_t<InvocableT, Writer &, ArgTs...>
write(InvocableT && invocable, ArgTs &&... args) noexcept(
    noexcept(std::invoke(
        std::declval<InvocableT>(),
        std::declval<Writer &>(),
        std::declval<ArgTs>()...)));
```

Reading the declaration from the inside out:

- `write` takes an **invocable** (a lambda, a function, any callable) and any extra arguments.
- The producer constructs a `Writer` object and calls the invocable with **the writer first**, followed by the extra arguments. The callback uses the writer to put bytes in the buffer; when it returns, `write` publishes everything.
- The return type, `std::invoke_result_t<InvocableT, Writer &, ArgTs...>`, is whatever the callback returns, passed back to the caller. It may be `void`.
- The `noexcept` specification is *conditional*. The inner `noexcept(expr)` is an operator that asks at compile time whether `expr` can throw, without evaluating it. The expression is "calling the invocable with a `Writer &` and the arguments", built from `std::declval`, which produces a value of a type in an unevaluated context. The outer `noexcept(...)` turns that answer into the function's specification. So `write` is `noexcept` exactly when the user's callback is. (The talk highlighted `std::declval<Writer &>()`, the writer being passed as the callback's first argument.)

Why a callback rather than separate `prepare`, `fill` and `commit` calls? Because the API then makes the protocol impossible to get wrong. A user cannot forget to commit, cannot commit twice, and cannot write after committing. "All of the locking stuff is taken care of."

### The Writer

The writer is the object the callback receives. For the multicast queue it looks like this (the talk stepped through the three functions in turn):

**Listing 28** — code from the talk (Producer), shown at [35:57](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=2157s)

```cpp
class Writer
{
    template <std::size_t N>
    void write(std::span<std::byte const, N> data) noexcept;

    template <TriviallyCopyableC T>
    T & emplace(auto &&... args) noexcept(
        std::is_nothrow_constructible_v<T, decltype(args)...>)
    requires std::constructible_from<T, decltype(args)...>;

    template <std::size_t Alignment = 1>
    void write(void const * data, std::size_t size) noexcept;
};
```

There are three ways to write:

1. **`write(std::span<std::byte const, N>)`** writes a span of bytes. `N` is the span's *extent*: a template parameter so the function accepts spans of static extent (size known at compile time) and of dynamic extent (`std::dynamic_extent`) alike.
2. **`emplace<T>(args...)`** constructs a `T` **in place** in the buffer from the arguments, instead of building it elsewhere and copying. It returns a reference to the new object so the callback can fill in more fields. Two constraints keep it safe:
   - `TriviallyCopyableC` (the speaker's own concept) requires `T` to be trivially copyable. A consumer in another process will get the message as bytes and copy it out, which is only meaningful for trivially copyable types. (Two sections from now, the requirement becomes even more specific: messages must be implicit-lifetime types.)
   - `requires std::constructible_from<T, decltype(args)...>` rejects argument lists that cannot construct a `T`, with a clear error at the call site. Its `noexcept` is conditional on the constructor in the same way as `write`'s.
3. **`write(void const *, std::size_t)`** writes arbitrary raw bytes, with an `Alignment` template parameter (default 1) saying how the start must be aligned.

A single callback may call these any number of times, for example a header with `emplace` followed by a payload with `write`. Everything written in one callback is published together.

In the speaker's words: "The writer can do one thing. It can write and it can write safely." It has a rich API, but all the work of preparing buffers and dealing with atomics is in the implementation: "the user has to do nothing about that."

### A toy version

The example below models the same shape inside one process: a producer with a callback-based `write`, a `Writer` with `emplace` and `write`, a commit when the callback returns, and a **rollback** when it throws. The real queue's rollback works the same way, as the last section of the chapter shows.

**Listing 29** — A callback-based producer with commit on return and rollback on exception (C++20) *(illustrative example, not from the talk)*

```cpp
#include <concepts>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <functional>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <utility>

template <class T> concept TriviallyCopyableC = std::is_trivially_copyable_v<T>;

class Producer {
    alignas(8) std::byte buffer_[1024];
    std::size_t committed_ = 0;        // what readers may see (an atomic in a real queue)
    std::size_t local_ = 0;            // where the current write has got to
public:
    class Writer {
        Producer& p_;
        std::size_t saved_;
    public:
        explicit Writer(Producer& p) : p_(p), saved_(p.local_) {}
        ~Writer() { p_.local_ = saved_; }                         // roll back unless committed
        void commit() { saved_ = p_.local_; }
        template <TriviallyCopyableC T>
        T& emplace(auto&&... args) requires std::constructible_from<T, decltype(args)...> {
            p_.local_ = (p_.local_ + alignof(T) - 1) / alignof(T) * alignof(T);   // align
            auto* t = new (static_cast<void*>(p_.buffer_ + p_.local_)) T(std::forward<decltype(args)>(args)...);
            p_.local_ += sizeof(T);
            return *t;
        }
        void write(std::span<std::byte const> data) {
            std::memcpy(p_.buffer_ + p_.local_, data.data(), data.size());
            p_.local_ += data.size();
        }
    };

    template <class F, class... Args>
    std::invoke_result_t<F, Writer&, Args...> write(F&& f, Args&&... args)
        noexcept(std::is_nothrow_invocable_v<F, Writer&, Args...>)
    {
        Writer w(*this);
        if constexpr (std::is_void_v<std::invoke_result_t<F, Writer&, Args...>>) {
            std::invoke(std::forward<F>(f), w, std::forward<Args>(args)...);
            w.commit(); committed_ = local_;
        } else {
            auto r = std::invoke(std::forward<F>(f), w, std::forward<Args>(args)...);
            w.commit(); committed_ = local_;
            return r;
        }
    }
    std::size_t committed() const { return committed_; }
};

struct Header { int type; int length; };

int main() {
    Producer p;
    char const text[] = "hello, world";
    int r = p.write([&](Producer::Writer& w, int type) {
        w.emplace<Header>(type, int(sizeof text));
        w.write(std::as_bytes(std::span(text)));
        return 42;
    }, 7);
    std::printf("committed %zu bytes, returned %d\n", p.committed(), r);

    try {
        p.write([](Producer::Writer& w) {
            w.emplace<Header>(1, 0);
            throw std::runtime_error("oops");          // half-written message is discarded
        });
    } catch (std::exception const& e) {
        std::printf("callback threw: %s; still %zu bytes committed\n", e.what(), p.committed());
    }
}
```

Output (an 8-byte header plus the 13 bytes of the string):

    committed 21 bytes, returned 42
    callback threw: oops; still 21 bytes committed

The writer only advances a *local* counter. The shared one (here `committed_`) is updated once, after the callback has returned normally. If the callback throws, the writer's destructor rolls the local counter back and nothing becomes visible.

## 12. The consumer API: try_read, no_data and lapped

*Talk segment: [36:50](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=2210s)*

### The consumer can only read

The consumer is the mirror image of the producer, and again this is the same consumer the multicast queues use. Its whole interface is one function and two constants:

**Listing 30** — code from the talk (Consumer), shown at [37:41](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=2261s)

```cpp
static constexpr std::uint64_t no_data = 0;
static constexpr std::uint64_t lapped = -1;

tl::expected<std::size_t, std::uint64_t> try_read(
    std::span<std::byte> data);
```

`try_read` copies the next message into a buffer **the caller provides**, as a `std::span<std::byte>`, and returns how many bytes it wrote. The consumer gets **opaque bytes**, not objects. That follows from the requirement that one queue carries many message types: the queue does not know what type the next message is, so it hands over bytes, and the application decodes them (for example, by reading a header that says what follows).

### Three ways to fail

The return type is an *expected*: either a `std::size_t` (success, the message size) or an error code. `tl::expected` is a popular open-source implementation of the idea that became `std::expected` in C++23.

There are three kinds of failure:

- **`no_data`** (0): there is nothing new to read right now. This is not really an error; a polling consumer sees it constantly.
- **`lapped`** (−1): the consumer has fallen too far behind and **lost messages** (explained below).
- **Any other value**: the caller's buffer was too small, and the value is the size that would be needed. The consumer can grow its buffer and try again.

The third case explains a detail of the types. On the slide the error type is `std::uint64_t`, so `lapped = -1` converts to the largest 64-bit value, 18446744073709551615. In the implementation shown at the end of the chapter the error type is a *signed* `std::int_fast64_t`, so `lapped` really is −1 and sizes are positive. As the speaker explains, "the reason it's an int64 is because the other type of error you can get is that's the maximum number of bytes that I could get."

The calls are **synchronous**: when `try_read` returns, the copy has happened. (An audience member asked this; the same holds for the producer, whose callback has run by the time `write` returns.)

### Lapped

The multicast queues at the end of the talk have a producer that **never blocks**. If a consumer is slow, the producer simply keeps writing around the ring, over data the slow consumer has not read yet. Picture a running track: the producer has run a whole lap more than the consumer and passed it. "I had my ring buffer. There were a thousand things in it that I could read. By the time I get back to read it, the system recognizes that I've missed all thousand of them. I was too slow and I've been lapped."

What should a lapped consumer do? "Crash, recover, be faster next time, make a bigger buffer." The speaker is frank that "the most common recovery strategy is just to crash": a component that cannot keep up is broken, and restarting it cleanly is often the safest option. But there are alternatives, which he returns to in the multicast section. For example, one special consumer can archive every message, and a lapped consumer can ask for the missed messages to be replayed.

### Using it

A consumer loop handles the three outcomes explicitly. The example uses `std::expected` and a mock source that returns each outcome in turn, so you can see the control flow.

**Listing 31** — A consumer loop that handles no_data, a short buffer and lapped (C++23) *(illustrative example, not from the talk)*

```cpp
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <expected>
#include <span>
#include <string_view>
#include <vector>

constexpr std::int64_t no_data = 0;
constexpr std::int64_t lapped = -1;

struct MockConsumer {                       // returns a scripted sequence of outcomes
    std::vector<std::string_view> script{"hello", "", "a much longer message that needs more room", "LAPPED"};
    std::size_t next = 0;
    std::expected<std::size_t, std::int64_t> try_read(std::span<std::byte> data) {
        if (next == script.size()) return std::unexpected(no_data);
        auto m = script[next];
        if (m.empty())      { ++next; return std::unexpected(no_data); }
        if (m == "LAPPED")  { ++next; return std::unexpected(lapped); }
        if (m.size() > data.size()) return std::unexpected(std::int64_t(m.size()));   // need more room
        std::memcpy(data.data(), m.data(), m.size());
        ++next;
        return m.size();
    }
};

int main() {
    MockConsumer consumer;
    std::vector<std::byte> buf(16);
    for (int i = 0; i < 5; ++i) {
        auto r = consumer.try_read(buf);
        if (r) {
            std::printf("got %zu bytes: %.*s\n", *r, int(*r), reinterpret_cast<char const*>(buf.data()));
        } else if (r.error() == no_data) {
            std::puts("nothing yet");
        } else if (r.error() == lapped) {
            std::puts("lapped: messages were lost; resynchronising");
        } else {
            std::printf("buffer too small, need %lld bytes; growing\n", static_cast<long long>(r.error()));
            buf.resize(std::size_t(r.error()));
        }
    }
}
```

Output:

    got 5 bytes: hello
    nothing yet
    buffer too small, need 42 bytes; growing
    got 42 bytes: a much longer message that needs more room
    lapped: messages were lost; resynchronising

Notice how the API pushes the caller to decide what each outcome means. A `bool` or an exception would hide the important distinction between "nothing yet" and "you lost data".

## 13. The Daugaard SPSC ring buffer

*Talk segment: [38:20](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=2300s)*

### A real SPSC queue to study

To make the problems of shared memory concrete, the speaker takes a well-known SPSC ring buffer and asks what breaks when you put it in shared memory. He chose Kaspar Daugaard's, published with a series of blog posts at daugaard.org in 2018. The source file carries the author's notice:

**Listing 32** — code from the talk (The Daugaard Queue), shown at [38:45](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=2325s)

```cpp
// Copyright 2018 Kaspar Daugaard. For educational purposes only.
// See http://daugaard.org/blog/writing-a-fast-and-versatile-spsc-ring-buffer
```

The blog posts are *Writing a Fast and Versatile SPSC Ring Buffer*, a follow-up with performance results, and *How To Make Your SPSC Ring Buffer Do Nothing, More Efficiently*. They are worth reading in full. (At C++ on Sea the speaker went through this queue in depth; here, short of time, he went quickly.)

### Shared state

The ring buffer has the "traditional" shape described earlier in the chapter: a buffer and two positions.

**Listing 33** — code from the talk (Daugaard: Shared State), shown at [38:59](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=2339s)

```cpp
class RingBuffer
{
    // Writer and reader's shared positions.
    struct alignas(CACHE_LINE_SIZE) SharedState
    {
        std::atomic<size_t> pos;
    };

    SharedState m_WriterShared;
    SharedState m_ReaderShared;
```

- `SharedState` holds a single atomic position. `m_WriterShared.pos` says how far the writer has published; `m_ReaderShared.pos` says how far the reader has consumed. "The writer says here's where you start writing. The reader says here's where you start reading."
- Each is declared `alignas(CACHE_LINE_SIZE)`, so the two positions live in **different cache lines**. The writer stores to its position often and the reader to its own. If both were in the same cache line, every store by one core would invalidate the line in the other core's cache, even though they never touch the same variable. This effect is called **false sharing**, and it is a classic way to lose much of a queue's performance. The talk returns to `CACHE_LINE_SIZE` later.

### Local state

Each side also keeps **local state**, data only it uses:

**Listing 34** — code from the talk (Daugaard: Local State), shown at [40:27](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=2427s)

```cpp
class RingBuffer { // ...
    // Writer and reader's local state.
    struct alignas(CACHE_LINE_SIZE) LocalState
    {
        LocalState()
            : buffer(nullptr), pos(0), end(0), base(0), size(0) {}

        char* buffer;
        size_t pos;
        size_t end;
        size_t base;
        size_t size;
    };

    LocalState m_Writer;
    LocalState m_Reader;
```

The speaker described each field:

- `buffer`: where the bytes are, a pointer into the ring's storage.
- `pos` and `end`: the **window** this side can currently use without looking at the other side. "If the writer has written, you know, 100 bytes and there's only 40 left, then this window will be 40." The writer can keep writing until `pos` reaches `end`. Only then does it read the reader's shared position to find out whether more space has been freed.
- `base` and `size` (highlighted on the slide): the size of the whole buffer and where the current window sits relative to the start of the stream. These are kept "because you have to make sure where am I".

The local window is the key performance idea. Reading the *other* side's atomic position is expensive, because that cache line is being written by another core. By caching how much space (or data) it already knows about, each side touches the shared line only occasionally. Like `SharedState`, `LocalState` is cache-line aligned, so the writer's local state and the reader's do not falsely share a line either.

### Contiguous messages

Daugaard's queue does **in-place construction**: it hands the writer a pointer to raw bytes, and the writer constructs an object there. An object must occupy contiguous memory with proper alignment. So if a message needs 64 bytes and only 50 remain before the end of the buffer, the queue cannot split it. "It won't give you the 50 bytes at the end. It will wait until it has 64 bytes at the beginning," and it has to do that computation on every write that approaches the end.

The **magic buffer**, later in the chapter, makes this problem disappear: with the ring's memory mapped twice in a row, the bytes after the end *are* the bytes at the beginning, so any message is contiguous.

The published slides continue with Daugaard's write and read paths: `PrepareWrite(size, alignment)` aligns the position, asks for more space if the request goes past `end`, and returns a pointer; `FinishWrite()` publishes the new position with a `memory_order_release` store. In the talk the speaker only summarised them: "they've got some write commands and some read commands."

### The same queue under another name

The speaker notes that Daugaard's design is "very, very similar" to the **FastQueue** that David Gross presented at CppCon 2024, which is also the basis of the multicast queue at the end of this chapter (with the speaker's API on top). The key difference is blocking:

| | When full (writer) | When empty (reader) |
|---|---|---|
| Daugaard's SPSC queue | waits for the reader to catch up | waits |
| FastQueue / multicast queue | never waits; overwrites | returns "no data"; detects being lapped |

"If you don't block, and you just keep going, and if the reader doesn't block reading, it just reads and has an ability to catch up, the Daugaard queue and the FastQueue that David had described are almost identical."

### Local and shared state in a small queue

The example applies the shared/local split to the slot-based queue from earlier in the chapter. Each side keeps a cached copy of the other side's position in its own cache line and refreshes it only when it seems to have run out of space or data.

**Listing 35** — An SPSC queue with cache-line-separated shared positions and cached local views (C++20) *(illustrative example, not from the talk)*

```cpp
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdio>
#include <thread>

constexpr std::size_t line = 64;

template <std::size_t N>
class Ring {
    static_assert((N & (N - 1)) == 0);
    struct alignas(line) Shared { std::atomic<std::size_t> pos{0}; };
    struct alignas(line) Local  { std::size_t pos = 0; std::size_t cached_other = 0; std::size_t refreshes = 0; };
    Shared writer_shared_, reader_shared_;      // each written by one side, read by the other
    Local writer_, reader_;                     // touched by one side only
    std::array<int, N> slots_{};
public:
    bool try_push(int v) {
        if (writer_.pos - writer_.cached_other == N) {               // looks full: refresh
            writer_.cached_other = reader_shared_.pos.load(std::memory_order_acquire);
            ++writer_.refreshes;
            if (writer_.pos - writer_.cached_other == N) return false;
        }
        slots_[writer_.pos++ & (N - 1)] = v;
        writer_shared_.pos.store(writer_.pos, std::memory_order_release);
        return true;
    }
    bool try_pop(int& out) {
        if (reader_.pos == reader_.cached_other) {                   // looks empty: refresh
            reader_.cached_other = writer_shared_.pos.load(std::memory_order_acquire);
            ++reader_.refreshes;
            if (reader_.pos == reader_.cached_other) return false;
        }
        out = slots_[reader_.pos++ & (N - 1)];
        reader_shared_.pos.store(reader_.pos, std::memory_order_release);
        return true;
    }
    std::size_t writer_refreshes() const { return writer_.refreshes; }
    std::size_t reader_refreshes() const { return reader_.refreshes; }
};

int main() {
    constexpr int count = 1'000'000;
    static Ring<4096> q;
    std::jthread producer([] {
        for (int i = 1; i <= count; ++i) while (!q.try_push(i)) std::this_thread::yield();
    });
    long long sum = 0;
    for (int got = 0, v; got < count;) if (q.try_pop(v)) { sum += v; ++got; }
    producer.join();
    std::printf("sum = %lld; writer refreshed %zu times, reader %zu times, for %d messages\n",
                sum, q.writer_refreshes(), q.reader_refreshes(), count);
}
```

The checksum is the same as before; what changes is how often each thread reads the other's cache line. One run on the machine used for this chapter printed:

    sum = 500000500000; writer refreshed 318 times, reader 746448 times, for 1000000 messages

The numbers show both sides of the idea. The **writer** looked at the reader's position only 318 times for a million messages: it knows the ring has 4096 free slots and only checks again when it believes they are used up. The **reader** here is faster than the writer, so it keeps catching up and finding the ring empty, and each of those empty polls costs a refresh. The cache helps whichever side is behind. When there is a backlog, a single refresh reveals many items at once. (Counting empty polls as refreshes also inflates the reader's number; a real queue would back off or block rather than spin.)

## 14. Two problems with putting it in shared memory

*Talk segment: [41:20](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=2480s)*

### Initialising the ring

Before use, Daugaard's ring buffer must be initialised with its storage:

**Listing 36** — code from the talk (Daugaard: Initialization), shown at [42:19](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=2539s)

```cpp
// Initialize. Buffer must have required alignment. Size must be a
// power of two.
void Initialize(void* buffer, size_t size)
{
    Reset();
    m_Reader.buffer = m_Writer.buffer = static_cast<char*>(buffer);
    m_Reader.size = m_Writer.size = m_Writer.end = size;
}

void Reset()
{
    m_Reader = m_Writer = LocalState();
    m_ReaderShared.pos = m_WriterShared.pos = 0;
}
```

`Initialize` resets everything, then points both the reader's and the writer's `buffer` at the storage and records its size in both local states. The writer's window initially covers the whole buffer (`m_Writer.end = size`); the reader's is empty. The comment states the preconditions: the buffer must be suitably aligned and its size a power of two, so positions can be wrapped with a mask. `Reset` sets both local states back to default and both shared positions to zero.

Now the speaker's question to the audience: suppose we take this structure, put it in shared memory, and use it from two processes. "There's two problems, both of them identified on the screen... One of them is very easy to find, very easy to fix. The other one's a little more subtle. Anybody see the first one?"

### Problem one: the pointers

Someone in the audience answered quickly: **the buffer pointers**. `Initialize` stores `static_cast<char*>(buffer)` into both `m_Reader.buffer` and `m_Writer.buffer` (the highlighted line). That is an address in the process that called `Initialize`. As we saw early in the chapter, the other process has the same storage at a different address, so its copy of the pointer is meaningless.

The fix is easy *because of the separation of concerns*. There is only ever one producer and one consumer, and they are established by role locks before they touch the buffer. So once a process has become the producer, "he knows where this is. He can go in and adjust the writer pointer to point to the memory in his location, and you can do it safely because there can only ever be one producer." The consumer does the same with the reader pointer.

A cleaner alternative is not to store pointers in shared memory at all. Keep **offsets** in the shared region and keep each side's pointers in its own process-local object. The example at the end of this section does that.

### Problem two: the atomic

"What's the second problem? It's recognizable on this slide, but maybe not so obvious. There's a hint." The hint was the line `m_ReaderShared.pos = m_WriterShared.pos = 0;`. "It's an atomic."

An audience member suggested the assignment was the problem. It isn't: assigning to a `std::atomic` is an atomic store, which is fine. "The problem is that there's an atomic there **at all**."

Think about what the *second* process does. It did not create the queue. "I just memory mapped this thing. And I'm trying to tell the compiler that, hey, there's a queue there." But in that process, no constructor ever ran for the `RingBuffer`, for its `SharedState`, or for the `std::atomic<size_t>` inside it. In C++, an object exists only once its **lifetime has begun**, normally by a definition or a `new`-expression. Using an object whose lifetime never began is **undefined behaviour**: "No lifetime has ever been started."

This is not a theoretical quibble. Compilers optimise based on the rule that objects are only accessed within their lifetime, and the standard gives no guarantee about what accessing "a `std::atomic` that was never created" does.

Is there any way to create objects in memory you merely mapped? Yes, under certain conditions, and that is the subject of the next section. As we will see, `std::atomic` fails those conditions.

### Offsets in shared memory, pointers in local state

The example lays out a queue the robust way, at least as far as pointers go. The shared region contains only offsets and positions. Each role keeps its own local state with a pointer computed from *its own* mapping. Two mappings of the same memory stand in for two processes. The positions are accessed through `std::atomic_ref` over plain integers rather than as `std::atomic` members; why that matters is explained over the next two sections.

**Listing 37** — Shared memory holds offsets; each process keeps its own pointers (Linux, C++20) *(illustrative example, not from the talk)*

```cpp
#include <atomic>
#include <cstddef>
#include <cstdio>
#include <sys/mman.h>
#include <unistd.h>

struct SharedHeader {                     // lives at the start of the shared region
    std::size_t buffer_offset;            // where the slots start, relative to the region
    std::size_t capacity;                 // number of int slots, a power of two
    alignas(64) std::size_t write_pos;    // accessed only through std::atomic_ref
    alignas(64) std::size_t read_pos;
};

struct LocalView {                        // per-process state, never in shared memory
    SharedHeader* header;
    int* slots;
    explicit LocalView(void* region)
        : header(static_cast<SharedHeader*>(region)),
          slots(reinterpret_cast<int*>(static_cast<char*>(region) + header->buffer_offset)) {}
};

int main() {
    constexpr std::size_t bytes = 4096;
    int fd = ::memfd_create("queue", 0);
    if (fd < 0 || ::ftruncate(fd, bytes) != 0) return 1;
    void* producer_map = ::mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    void* consumer_map = ::mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    std::printf("producer maps region at %p, consumer at %p\n", producer_map, consumer_map);

    auto* h = static_cast<SharedHeader*>(producer_map);  // the creator fills in the header
    h->buffer_offset = sizeof(SharedHeader);
    h->capacity = 256;

    LocalView prod(producer_map), cons(consumer_map);    // each computes its own pointers
    for (int v = 10; v < 17; ++v) {
        auto w = std::atomic_ref(prod.header->write_pos).load(std::memory_order_relaxed);
        prod.slots[w & (prod.header->capacity - 1)] = v;
        std::atomic_ref(prod.header->write_pos).store(w + 1, std::memory_order_release);
    }

    auto end = std::atomic_ref(cons.header->write_pos).load(std::memory_order_acquire);
    auto r = std::atomic_ref(cons.header->read_pos).load(std::memory_order_relaxed);
    std::printf("consumer read %zu values:", end - r);
    for (; r != end; ++r) std::printf(" %d", cons.slots[r & (cons.header->capacity - 1)]);
    std::atomic_ref(cons.header->read_pos).store(r, std::memory_order_release);
    std::printf("\n");
}
```

Output (addresses vary):

    producer maps region at 0x7f3a1c5fe000, consumer at 0x7f3a1c5fd000
    consumer read 7 values: 10 11 12 13 14 15 16

## 15. Implicit lifetime

*Talk segment: [43:40](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=2620s)*

### When does an object exist?

C++ distinguishes **storage** (bytes of memory) from **objects** (typed things with a lifetime). Getting storage does not create an object. Traditionally an object is created only by a definition (`X x;`), a `new`-expression, a temporary, or a few similar constructs. Here is the speaker's first example:

**Listing 38** — code from the talk (Implicit Lifetime), shown at [43:57](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=2637s)

```cpp
struct X
{
    int a, b;
};

X * p = static_cast<X *>(std::malloc(sizeof(X)));
p->a = 1;
p->b = 2;
```

`std::malloc` returns storage. No `X` was ever created in it, so before C++20, `p->a = 1` accessed a member of an object that does not exist: **undefined behaviour**, according to the letter of the standard. Yet C programmers, and C++ programmers using C idioms, have written exactly this for decades, and every compiler does the expected thing. The standard was out of step with practice.

### Implicit object creation (C++20)

C++20 fixed this with *implicit object creation* (proposal P0593, "Implicit creation of objects for low-level object manipulation"), and it was adopted as a defect report, so it applies to earlier standards too. The rule is in [intro.object]:

- Some operations are specified as **implicitly creating objects** in a region of storage. They include `std::malloc` and friends, `operator new`, `std::memcpy` and `std::memmove` (into their destination), starting the lifetime of an array of `unsigned char` or `std::byte`, and `std::bit_cast`.
- Such an operation creates and starts the lifetime of **zero or more objects of implicit-lifetime types** in the region, **if doing so gives the program defined behaviour**. If several sets of objects would work, it is unspecified which set is created; if none would, the behaviour is undefined.
- Objects created this way do not include subobjects that are not themselves of implicit-lifetime types.

The phrase "if doing so would result in the program having defined behaviour" is remarkable. Whether an `X` is created by the `malloc` call depends on what the program does *later*: if it later uses the storage as an `X`, then an `X` was created back at the `malloc`. The speaker calls this "time travel".

The standard illustrates this with the same example the speaker had just shown, and the speaker put that example on the next slide, highlighting its comments:

**Listing 39** — code from the talk (Implicit Lifetime), shown at [44:29](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=2669s)

```cpp
#include <cstdlib>
struct X { int a, b; };
X *make_x() {
  // The call to std::malloc implicitly creates an object of type X
  // and its subobjects a and b, and returns a pointer to that X object
  // (or an object that is pointer-interconvertible (6.8.2) with it),
  // in order to give the subsequent class member access operations
  // defined behavior.
  X *p = (X*)std::malloc(sizeof(struct X));
  p->a = 1;
  p->b = 2;
  return p;
}
```

"After 20, this is perfectly fine. Before 20, it's not. Well, it's not exactly true because we have time travel." Since it was a defect report, compilers apply it in all modes.

### Implicit-lifetime types

Which types qualify? The definition comes in two steps. From [basic.types]:

> Scalar types, implicit-lifetime class types, array types, and cv-qualified versions of these types are collectively called **implicit-lifetime types**.

Scalars are the arithmetic types, enumerations, pointers, pointers to members and `std::nullptr_t`. Arrays are implicit-lifetime whatever their element type. And from [class.prop]:

> A class S is an **implicit-lifetime class** if it is an aggregate or has at least one trivial eligible constructor and a trivial, non-deleted destructor.

Unpacking the second half:

- an **aggregate** is, roughly, a class with no user-declared constructors, no private or protected direct non-static data members, and no virtual functions or virtual, private or protected base classes. A plain `struct` of data is one;
- otherwise the class needs at least one constructor that is **trivial** (it does nothing: not user-provided, no virtual functions, every member and base trivially constructible) and **eligible** (not deleted, and its constraints are satisfied);
- and its destructor must be trivial and not deleted.

Then the speaker showed this:

**Listing 40** — code from the talk (Implicit Lifetime), shown at [45:43](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=2743s)

```cpp
class Foo
{
    // ...
    Foo(tag_t) = default;
};
```

The idea is tempting: a constructor taking a tag, declared `= default` so it would be trivial, as a way to make a class with invariants still implicit-lifetime. "This by the way is not a trivial constructor... and the reason is because it doesn't even compile." Only special member functions (and, since C++20, comparison operators) can be defaulted. Both compilers used for this chapter agree:

    GCC 16:   error: 'Foo::Foo(tag_t)' cannot be defaulted
    Clang 20: error: only special member functions and comparison operators may be defaulted

So the only constructors that can ever be trivial are the **default, copy and move constructors**. "It would be awesome if a user could specify a trivial constructor", the speaker says, but that would need a proposal that nobody has written yet.

### Checking types

C++23 added the trait `std::is_implicit_lifetime` (in `<type_traits>`), so you can check these rules instead of working them out by hand. The example runs through several cases; all of these assertions hold with both GCC 16 and Clang 20.

**Listing 41** — Which types are implicit-lifetime? (C++23) *(illustrative example, not from the talk)*

```cpp
#include <string>
#include <type_traits>

struct Point { int x, y; };                          // aggregate
struct WithCtor { WithCtor(int); int v; };           // user constructor, but trivial copy/move ctor
struct NoTrivialCtor {                               // user-provided copy and move: none is trivial
    NoTrivialCtor(int);
    NoTrivialCtor(NoTrivialCtor const&);
    NoTrivialCtor(NoTrivialCtor&&);
};
struct WithDtor { ~WithDtor(); int v; };             // non-trivial destructor
struct Holder { std::string s; };                    // aggregate (any aggregate qualifies)

static_assert(std::is_implicit_lifetime_v<int>);
static_assert(std::is_implicit_lifetime_v<double*>);
static_assert(std::is_implicit_lifetime_v<Point>);
static_assert(std::is_implicit_lifetime_v<WithCtor>);        // its implicit copy ctor is trivial
static_assert(!std::is_implicit_lifetime_v<NoTrivialCtor>);
static_assert(!std::is_implicit_lifetime_v<WithDtor>);
static_assert(std::is_implicit_lifetime_v<Holder>);          // but its std::string member is not created!
static_assert(std::is_implicit_lifetime_v<std::string[4]>);  // arrays always are
static_assert(!std::is_implicit_lifetime_v<std::string>);

int main() {}
```

### start_lifetime_as

Implicit object creation needs one of the listed operations to happen. In our inter-process situation, the consumer process calls `mmap`, which is not a C++ operation at all. The standard does not say whether `mmap` implicitly creates objects (in practice compilers treat memory from an opaque system call the same way they treat `malloc`).

C++23 added an explicit tool: `std::start_lifetime_as<T>(p)` (in `<memory>`) declares that the bytes at `p` are now an object of type `T`, keeping their current value. It is the explicit version of what `memcpy` does implicitly, and the speaker mentions it was once proposed as `std::bless`. A consumer that maps a queue can use it to start the lifetime of the message it is about to read, legitimately.

**Listing 42** — Reading a message from raw bytes with std::start_lifetime_as (C++23) *(illustrative example, not from the talk)*

```cpp
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>

struct Trade {                       // an implicit-lifetime message type
    std::uint64_t id;
    double price;
    std::int32_t quantity;
};

int main() {
    alignas(Trade) std::byte region[sizeof(Trade)];     // stands in for mapped shared memory
    Trade t{42, 101.25, 300};
    std::memcpy(region, &t, sizeof t);                    // the "producer" writes bytes

    // The "consumer" declares that these bytes are a Trade, keeping their values:
    Trade const* msg = std::start_lifetime_as<Trade>(region);
    std::printf("trade %llu: %d @ %.2f\n",
                static_cast<unsigned long long>(msg->id), msg->quantity, msg->price);
}
```

GCC 16's library ships `start_lifetime_as`; it compiled and ran with both compilers for this chapter. There is one important restriction: **`T` must be an implicit-lifetime type**. That brings us back to `std::atomic`.

## 16. std::atomic in shared memory, and the Hinnant rule

*Talk segment: [46:30](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=2790s)*

### The one sentence about processes

"There's one place in the standard that talks about processes, one that I know of, and this is it." It is a note in the section on lock-free atomics ([atomics.lockfree]), and the speaker put it under Daugaard's `SharedState`:

> Operations that are lock-free should also be address-free. That is, atomic operations on the same memory location via two different addresses will communicate atomically. The implementation should not depend on any per-process state. This restriction enables communication by memory that is mapped into a process more than once and by memory that is shared between two processes.

"Sounds exactly like what we're doing." We have memory mapped into two processes, and we have put a lock-free `std::atomic` in it. The note promises that the *operations* will work. (It is only a note, and it says "should", but every mainstream implementation honours it for lock-free types.) What the note does not address is whether there is a `std::atomic` *object* there at all, the lifetime question from the previous two sections.

### C++20 changed std::atomic's default constructor

The speaker then highlighted the constructors of `std::atomic<T>` as listed on cppreference:

| Constructor | Standard |
|---|---|
| `atomic() noexcept = default;` | C++11 until C++20 |
| `constexpr atomic() noexcept(std::is_nothrow_default_constructible_v<T>);` | since C++20 |
| `constexpr atomic(T desired) noexcept;` | since C++11 |
| `atomic(const atomic&) = delete;` | since C++11 |

Before C++20, the default constructor was defaulted, and for an `int` it was trivial: it left the value uninitialised. C++20 (P0883, "Fixing atomic initialization") changed it to **value-initialise** the contained value, so `std::atomic<int> a;` now holds 0. "Sounds like a great thing because now I've got values that are initialized, but it's no longer trivial."

This is easy to confirm. On both GCC 16 and Clang 20, `std::is_trivially_default_constructible_v<std::atomic<int>>` is `true` with `-std=c++17` and `false` with `-std=c++20` or later.

"But wait a minute, Jody, you said any constructor that is trivial will do. They only changed the default constructor." An implicit-lifetime class needs *one* trivial eligible constructor. What about the copy and move constructors? "This is a great time for a message from our sponsor."

### The Hinnant rule

The "sponsor" is what the speaker calls "the greatest single C++ slide in history": Howard Hinnant's table of which special member functions the compiler declares implicitly, depending on which ones you declare yourself.

| You declare ↓ / compiler gives → | default ctor | destructor | copy ctor | copy assign | move ctor | move assign |
|---|---|---|---|---|---|---|
| nothing | defaulted | defaulted | defaulted | defaulted | defaulted | defaulted |
| any constructor | not declared | defaulted | defaulted | defaulted | defaulted | defaulted |
| default constructor | user-declared | defaulted | defaulted | defaulted | defaulted | defaulted |
| destructor | defaulted | user-declared | defaulted* | defaulted* | not declared | not declared |
| copy constructor | not declared | defaulted | user-declared | defaulted* | not declared | not declared |
| copy assignment | defaulted | defaulted | defaulted* | user-declared | not declared | not declared |
| move constructor | not declared | defaulted | deleted | deleted | user-declared | not declared |
| move assignment | defaulted | defaulted | deleted | deleted | not declared | user-declared |

(*Defaulted, but deprecated.) Two rules produce most of the table: declaring any copy operation or the destructor suppresses the implicit *move* operations, and declaring any move operation makes the implicit *copy* operations deleted.

"Some people think it's archaic... I've had people tell me I need to stop showing that because nobody needs to know that anymore." So here is the speaker's pop quiz:

**Listing 43** — code from the talk (Pop Quiz), shown at [47:35](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=2855s)

```cpp
struct Foo
{
    void operator = (Foo &&) = delete;
};

static_assert(std::is_default_constructible_v<Foo>);
static_assert(std::is_copy_constructible_v<Foo>);
static_assert(std::is_move_constructible_v<Foo>);
static_assert(std::is_copy_assignable_v<Foo>);
static_assert(std::is_move_assignable_v<Foo>);
static_assert(std::is_destructible_v<Foo>);
```

What does this code do? Does it compile? Which, if any, of these `static_assert`s fail? "If you are a professional programmer and you're unwilling to bet your paycheck on your answer, then you need to go back and look at that slide again."

The answer is in the table's bottom row. `Foo` has a **user-declared move assignment** operator. It is declared as deleted, "but it's been user-declared". Therefore:

- the **default constructor** is still defaulted, so `is_default_constructible` holds;
- the **destructor** is defaulted, so `is_destructible` holds;
- the **copy constructor** and **copy assignment** are implicitly **deleted**, so `is_copy_constructible` and `is_copy_assignable` fail;
- the **move constructor** is **not declared**. Moving a `Foo` would fall back to the copy constructor, which is deleted, so `is_move_constructible` fails;
- **move assignment** is explicitly deleted, so `is_move_assignable` fails.

Four of the six assertions fail, and the code does not compile. Both compilers agree; the example below states the correct answers.

**Listing 44** — The pop quiz, with the correct answers (C++20) *(illustrative example, not from the talk)*

```cpp
#include <type_traits>

struct Foo
{
    void operator = (Foo &&) = delete;     // user-declared move assignment
};

static_assert(std::is_default_constructible_v<Foo>);    // still defaulted
static_assert(!std::is_copy_constructible_v<Foo>);      // implicitly deleted
static_assert(!std::is_move_constructible_v<Foo>);      // not declared; falls back to deleted copy
static_assert(!std::is_copy_assignable_v<Foo>);         // implicitly deleted
static_assert(!std::is_move_assignable_v<Foo>);         // explicitly deleted
static_assert(std::is_destructible_v<Foo>);             // still defaulted

int main() {}
```

### Why std::atomic is not an implicit-lifetime type

Now apply the table to `std::atomic<T>`. Its copy constructor is declared deleted (`atomic(const atomic&) = delete;`). A user-declared copy constructor means the move constructor is **not declared**. So:

- the default constructor is not trivial (since C++20);
- the copy constructor is deleted, and a deleted function is not *eligible*;
- there is no move constructor at all.

There is no trivial eligible constructor, and `std::atomic` is not an aggregate. **`std::atomic` is not an implicit-lifetime type.** Even a perfectly lock-free, address-free `std::atomic<std::size_t>` cannot come into existence in memory that a process merely mapped. "How is the compiler ever supposed to know its lifetime?"

What about `std::start_lifetime_as` (once called `std::bless`)? It can only be used with implicit-lifetime types, "which `std::atomic` is not".

Here the two compilers used for this chapter **disagree**, which is a nice illustration of how subtle this is:

| | GCC 16 | Clang 20 |
|---|---|---|
| `std::is_implicit_lifetime_v<std::atomic<int>>` | `false` | `true` |
| `std::start_lifetime_as<std::atomic<int>>(p)` | rejected (static assertion in the library) | accepted |

Both used the same standard library (libstdc++), and the trait is implemented by a compiler built-in. The standard's definition of an *eligible* special member function ([special]) excludes deleted functions, so the deleted copy constructor does not count and GCC's answer is the correct one. Clang 20's built-in apparently counts the deleted copy constructor, which is trivial in the technical sense because it is not user-provided. Do not let a compiler's `true` convince you that the speaker is wrong.

The second example checks the properties that hold on both compilers:

**Listing 45** — Why std::atomic has no trivial eligible constructor (C++20 and later) *(illustrative example, not from the talk)*

```cpp
#include <atomic>
#include <type_traits>

using A = std::atomic<int>;
static_assert(!std::is_trivially_default_constructible_v<A>);   // value-initialises since C++20
static_assert(!std::is_copy_constructible_v<A>);                // copy constructor deleted
static_assert(!std::is_move_constructible_v<A>);                // move constructor not declared
static_assert(!std::is_aggregate_v<A>);
static_assert(std::is_trivially_destructible_v<A>);             // the destructor is not the problem
static_assert(A::is_always_lock_free);                          // lock-free and address-free, yet...

int main() {}
```

### What to do instead

The speaker considered the options:

- **`std::atomic_ref<T>`** (C++20) performs atomic operations on a plain object of type `T`. In shared memory you store a plain `std::size_t`, which is an implicit-lifetime type, and access it through `std::atomic_ref`. The catch: if you *ever* access that object both through `atomic_ref` and directly while another thread or process might be accessing it, that is undefined behaviour, and the type of the field "really doesn't say what it does". The `size_t` no longer documents that it is an atomic.
- **Your own atomic type.** The speaker implemented his own (`wjh::Atomic`, seen in the lock's code earlier). His first version was a raw implementation, because Apple Clang did not yet support `std::atomic_ref`. As soon as it did, he rebuilt his type on top of `atomic_ref`, "because it's much easier than trying to support the whole thing."

If you want to write your own atomics, Ben Saks gave a talk at the same conference on exactly that, *Implementing Your Own Atomics*. "But when you do, make sure that you don't make the constructor non-trivial."

The third example shows the shape of such a type: an aggregate holding a suitably aligned `T`, with every operation going through `std::atomic_ref`. Because it is an aggregate, it is an implicit-lifetime type, and because all access goes through member functions, nobody can accidentally read it non-atomically. The demonstration shares one across `fork` in anonymous shared memory.

**Listing 46** — An implicit-lifetime atomic built on std::atomic_ref, shared across fork (C++23, POSIX) *(illustrative example, not from the talk)*

```cpp
#include <atomic>
#include <cstdio>
#include <memory>
#include <sys/mman.h>
#include <sys/wait.h>
#include <type_traits>
#include <unistd.h>

template <class T>
struct Atomic {                                       // an aggregate: no constructors at all
    static_assert(std::atomic_ref<T>::is_always_lock_free, "must be address-free across processes");
    alignas(std::atomic_ref<T>::required_alignment) T value_;

    T load(std::memory_order o = std::memory_order_seq_cst) const {
        return std::atomic_ref<T const>(value_).load(o);
    }
    void store(T v, std::memory_order o = std::memory_order_seq_cst) {
        std::atomic_ref<T>(value_).store(v, o);
    }
    T fetch_add(T v, std::memory_order o = std::memory_order_seq_cst) {
        return std::atomic_ref<T>(value_).fetch_add(v, o);
    }
};
static_assert(std::is_implicit_lifetime_v<Atomic<long>>);

int main() {
    void* mem = ::mmap(nullptr, sizeof(Atomic<long>), PROT_READ | PROT_WRITE,
                       MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED) return 1;
    auto* counter = std::start_lifetime_as<Atomic<long>>(mem);   // explicit, and allowed

    pid_t pid = ::fork();
    for (int i = 0; i < 100'000; ++i) counter->fetch_add(1, std::memory_order_relaxed);
    if (pid == 0) return 0;
    ::waitpid(pid, nullptr, 0);
    std::printf("shared counter = %ld (expected 200000)\n", counter->load());
}
```

Output:

    shared counter = 200000 (expected 200000)

## 17. Messages must be implicit-lifetime; the cache line size

*Talk segment: [50:05](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=3005s)*

### Messages must be implicit-lifetime too

The previous sections were about the queue's own control data, but the same reasoning applies to what travels through it. A consumer in another process receives bytes that some other process wrote. For the consumer to use them as an object of type `M`, an `M` must come into existence in its address space, and for that to happen without a constructor running, `M` must be an **implicit-lifetime type**. The speaker's slide was blunt:

- Messages must be implicit lifetime.
- You must become comfortable with this for sharing data across processes.

"Oh, well, that kind of makes sense. Well, it's more than makes sense. In order for your program to have defined behavior, you really have to do this." In practice this means message types are simple structs of scalars and arrays: no `std::string`, no `std::vector`, no virtual functions, no user-provided constructors unless a trivial one remains. The consumer then copies the bytes out (`memcpy` implicitly creates objects in its destination) or uses `std::start_lifetime_as`.

### Cache lines

Daugaard's ring buffer aligns its shared and local state to `CACHE_LINE_SIZE`, which his code expects as a macro:

**Listing 47** — code from the talk (Cache Lines), shown at [50:23](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=3023s)

```cpp
#define CACHE_LINE_SIZE 64
```

A **cache line** is the unit in which caches move memory between cores, 64 bytes on most x86-64 processors. As the Daugaard section explained, keeping data that different cores write in different cache lines avoids *false sharing*. To do that, the code needs to know the line size at compile time, because it goes into `alignas`.

C++17 added a standard answer: `std::hardware_destructive_interference_size` (in `<new>`), "the minimum offset between two objects to avoid false sharing". The speaker's version of the cache-line setting prefers an explicit macro but falls back to the standard constant:

**Listing 48** — code from the talk (Daugaard: Cache Lines), shown at [50:33](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=3033s)

```cpp
#if defined(CACHE_LINE_SIZE)
inline constexpr std::size_t cache_line_size = CACHE_LINE_SIZE;
#elif defined(__cpp_lib_hardware_interference_size) && \
    (__cpp_lib_hardware_interference_size >= 201703L)
inline constexpr std::size_t cache_line_size =
    std::hardware_destructive_interference_size;
#else
    #error "cache_line_size cannot be defined"
#endif
```

- If the build defines `CACHE_LINE_SIZE`, that value wins.
- Otherwise, if the library advertises the interference-size feature (`__cpp_lib_hardware_interference_size >= 201703L`), use `std::hardware_destructive_interference_size`.
- Otherwise, refuse to compile with `#error`. Guessing would be worse than stopping.

`inline constexpr` makes the constant a single entity across all translation units, the modern replacement for a macro.

### Why not just use the standard constant?

The speaker's experience is a cautionary tale. On his Apple machine the L1 cache line is **128 bytes**, but when Apple Clang began supporting `hardware_destructive_interference_size`, it reported **64**. His code started compiling with the new constant, and a test failed: "fortunately I had a test that failed." The slide showed a Stack Overflow question from someone who had hit the same thing: *Is std::hardware_destructive_interference_size right for Macs?* The `sysctl` value `hw.cachelinesize` said 128 while the constant said 64.

Digging further, he found discussions on both the Clang and GCC mailing lists saying, in effect, *don't use this in header files of libraries*. "Well, where else am I going to use it? But yeah, you really shouldn't, because it's hard to get right and it can change depending on your compiler flags."

The underlying problem is that the constant is a **compile-time property of how one translation unit was compiled**, not a property of the machine the program runs on. If a library header uses it in `alignas`, the layout of a type can differ between two translation units compiled with different `-mtune`/`-mcpu` flags, or with different compiler versions. That violates the One Definition Rule, and for a queue in shared memory it means two *processes* may disagree on the layout of the same bytes. GCC even warns about this when the constant is used in a header. Compiling the speaker's pattern through a header on the machine used for this chapter gave:

    hdr.hpp:3:21: warning: use of 'std::hardware_destructive_interference_size' [-Winterference-size]
    note: its value can vary between compiler versions or with different '-mtune' or '-mcpu' flags
    note: if this use is part of a public ABI, change it to instead use a constant variable you define
    note: the default value for the current CPU tuning is 64 bytes

The speaker's recommendation: "If you're writing code like this, don't use... hardware_destructive_interference_size. That's the standard C++ way to do it. I'm telling you now, don't do that." Define the size yourself, and, as the characteristics section showed, record the cache-line size in the queue's metadata so that every process can check it agrees.

The first example prints what this machine reports; on x86-64 Linux all three values were 64. On an Apple Silicon Mac, `sysctl hw.cachelinesize` reports 128.

**Listing 49** — What the compiler and the OS say the cache line is (Linux) *(illustrative example, not from the talk)*

```cpp
#include <cstdio>
#include <new>
#include <unistd.h>

int main() {
#ifdef __cpp_lib_hardware_interference_size
    std::printf("hardware_destructive_interference_size  = %zu\n", std::hardware_destructive_interference_size);
    std::printf("hardware_constructive_interference_size = %zu\n", std::hardware_constructive_interference_size);
#endif
    std::printf("sysconf(_SC_LEVEL1_DCACHE_LINESIZE)     = %ld\n", ::sysconf(_SC_LEVEL1_DCACHE_LINESIZE));
}
```

### Seeing false sharing

How much does a cache line matter? The second example runs two threads, each incrementing its own counter. In one layout the counters are adjacent and share a cache line; in the other each has its own line.

**Listing 50** — Measuring false sharing: two counters in one cache line versus two (C++20) *(illustrative example, not from the talk)*

```cpp
#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>

constexpr std::size_t cache_line = 64;            // defined explicitly, as the speaker recommends

struct Adjacent { std::atomic<long> a{0}; std::atomic<long> b{0}; };
struct Separate { alignas(cache_line) std::atomic<long> a{0}; alignas(cache_line) std::atomic<long> b{0}; };

template <class Counters>
long long run_ms() {
    Counters c;
    auto start = std::chrono::steady_clock::now();
    {
        std::jthread t1([&] { for (int i = 0; i < 50'000'000; ++i) c.a.fetch_add(1, std::memory_order_relaxed); });
        std::jthread t2([&] { for (int i = 0; i < 50'000'000; ++i) c.b.fetch_add(1, std::memory_order_relaxed); });
    }
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
    return ms.count();
}

int main() {
    std::printf("same cache line:     %lld ms\n", run_ms<Adjacent>());
    std::printf("separate cache lines: %lld ms\n", run_ms<Separate>());
}
```

On the 64-byte-line machine used for this chapter, one run (GCC 16, `-O2`) printed:

    same cache line:     2033 ms
    separate cache lines: 254 ms

and a Clang 20 build gave 1655 ms against 254 ms. The threads never touch each other's counter, yet sharing a line made the program six to eight times slower. Timings vary between machines and runs, but the gap is consistently large. This is why queue implementations care so much about the cache-line size, and why getting it wrong (64 on a 128-byte machine) quietly costs performance.

## 18. The magic buffer

*Talk segment: [51:50](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=3110s)*

### The problem it solves

Recall the awkward case from Daugaard's queue: a message needs 64 contiguous bytes, but only 50 remain before the end of the buffer. A ring buffer normally handles this with extra code and extra state: either split the message into two pieces (impossible if you want to construct an object in place) or skip the tail and wait for space at the beginning. That is "extra code and extra data that you don't need to keep track of. Let the operating system keep track of it."

The **magic buffer** (sometimes called a *mirrored* or *virtual ring buffer*) is the speaker's answer. "I don't know why it got called magic buffer. It's been around like forever." Only a handful of the audience knew it, so he walked through an implementation.

### The idea

Your program only ever sees **virtual** addresses. The operating system maps each virtual page to some physical page, and nothing stops it from mapping *two* virtual pages to the *same* physical page.

Suppose the ring has 1 MB of storage (the pink box on the slides). Map those same physical pages **twice, back to back** in virtual memory:

    virtual:  [ first view: 1 MB ][ second view: 1 MB ]
                      \                  /
    physical:          [ the same 1 MB ]

Now the byte at `start + 1 MB + k` *is* the byte at `start + k`. Take any position in the stream, mask it to the 1 MB range, and from that point you have a full megabyte of contiguous addresses, even if it runs past the end of the first view. "I don't have to do any tricks. I can just access one meg and it automatically will wrap for me." Messages never need to be split, objects can always be constructed in place, and the wrap-around logic disappears from the hot path. It can also be faster, "though you must, as everything that talks about performance, you must measure."

### The implementation

The speaker's mapping code has an arguments structure:

**Listing 51** — code from the talk (Magic Buffer), shown at [54:01](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=3241s)

```cpp
struct Args
{
    void * addr = nullptr;
    size_type len = 0;
    int fd = -1;
    size_type offset = 0;
    bool magic = false;
    size_type page_size = 0;
};
```

- `addr`: where to map, or null to let the kernel choose;
- `len` and `offset`: how much of the backing object to map, from where;
- `fd`: the file descriptor of the backing object, or −1 for anonymous memory;
- `magic`: whether to build a doubled mapping;
- `page_size`: mappings work in whole pages, so lengths and offsets must be multiples of it.

His mapping object knows how to do this on Linux and on Apple systems, for file-backed and for anonymous memory. On Apple, anonymous memory needs different, Mach-specific APIs. The slides show the `mmap` version, in three steps.

**Step 1: reserve twice the space.**

**Listing 52** — code from the talk (Magic Buffer), shown at [54:23](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=3263s)

```cpp
// Perform an anonymous mapping of twice the requested length.
auto const saved_fd = std::exchange(args.fd, -1);
auto const offset = std::exchange(args.offset, 0);
args.len *= 2;
args.magic = false;
auto mapped = do_mapping(args);
```

`std::exchange(x, v)` sets `x` to `v` and returns its old value. The code saves the real file descriptor and offset, sets `fd` to −1 and `offset` to 0, doubles `len`, and turns off `magic` (so the nested call does not try to build another magic buffer). Then it maps: an **anonymous mapping of 2 MB**. This reserves 2 MB of *virtual address space*. "It has not reserved any physical pages at all."

Why reserve first? Because we need two views that are guaranteed to be **adjacent**. If we asked the kernel for two separate mappings of the file, it could place them anywhere. Reserving a 2 MB block and then mapping *over* parts of it with exact addresses guarantees adjacency, and guarantees the addresses are ours to use.

**Step 2: map the file over the first half.**

**Listing 53** — code from the talk (Magic Buffer), shown at [54:45](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=3285s)

```cpp
// Map the file over the first half of the anonymous mapping.
args.fd = saved_fd;
args.len /= 2;
args.offset = offset;
args.addr = mapped->address;
auto tmp = do_mapping(args);
tmp->release();
```

Restore the file descriptor and offset, halve the length back to 1 MB, and set `addr` to the start of the reserved block. Mapping with an address uses `MAP_FIXED` (see below), which **replaces** whatever was mapped at that address, here the anonymous reservation, with the file. "I know that address is good, because I already reserved it." `tmp->release()` tells the temporary mapping object not to unmap in its destructor: the memory is still owned by `mapped`, the object for the whole 2 MB range.

**Step 3: map the same file over the second half.**

**Listing 54** — code from the talk (Magic Buffer), shown at [55:29](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=3329s)

```cpp
// Map the file over the second half of the anonymous mapping.
args.addr = static_cast<std::byte *>(mapped->address) + args.len;
tmp = do_mapping(args);
tmp->release();

addr_ = std::bit_cast<pointer>(mapped->address);
size_ = mapped->length / 2;
mapped_ = std::move(mapped);
```

The address is now the start plus `args.len` (1 MB). After this, both halves of the 2 MB block show the same physical pages. The final three lines store the results: `addr_` is the start, converted to the class's `pointer` type with `std::bit_cast` (C++20; it reinterprets the bits of one trivially copyable type as another of the same size); `size_` is half the mapped length, the usable ring size; and `mapped_` takes ownership of the whole 2 MB mapping, so destroying the buffer unmaps everything at once.

"And now what happens is that whole two megabytes of virtual memory: I go through the first meg, and as soon as I get to the second meg, I'm automatically referencing that first meg of memory."

**The mapping call itself.**

**Listing 55** — code from the talk (Magic Buffer), shown at [56:03](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=3363s)

```cpp
static constexpr int prot = PROT_READ | PROT_WRITE;
int const flags = [&] {
    int f = MAP_SHARED;
    if (addr) f |= MAP_FIXED;
    if (fd < 0) f |= MAP_ANONYMOUS;
    return f;
}();
auto const mapped = ::mmap(
    addr, len, prot, flags, fd, off_t(offset));
```

The protection is always read and write. The flags are computed by an immediately invoked lambda:

- `MAP_SHARED` always: writes go to the shared backing pages, so both views and other processes see them.
- `MAP_FIXED` when an address was given: map exactly there. `MAP_FIXED` silently replaces any existing mapping in the range, which is precisely what steps 2 and 3 need, and precisely why it is dangerous anywhere else. That is the reason for the reservation in step 1: `MAP_FIXED` is only ever used inside address space this object owns.
- `MAP_ANONYMOUS` when there is no file descriptor.

Then `::mmap` is called with those values. (The slide stops there; the real code checks for `MAP_FAILED`.)

For this chapter, the four slides were also compiled exactly as shown, with GCC 16 and Clang 20, inside a small harness that supplies what the slides leave out (a `Mapping` object owning `address` and `length` with a `release()` member, and `do_mapping` wrapping the last slide's code). With a 1 MiB `memfd_create` file, it produced the doubled mapping, and a 6-byte write starting 3 bytes before the end showed up at offset 0. One lesson from writing that harness: the ownership protocol matters. The first version of the harness copied a temporary `Mapping` whose destructor unmapped the memory, and the program crashed on its first write. That is exactly the mistake `release()` exists to prevent.

The same technique works without files: "you can do it with regular memory", for example for a queue between threads, as long as there is some shareable backing object to map twice. On Linux, `memfd_create` provides one.

### A complete magic buffer

The example builds a 64 KiB magic buffer on Linux with `memfd_create`, following the same three steps, then writes a string that runs off the end of the ring and reads it back at the beginning.

**Listing 56** — A magic (mirrored) ring buffer with memfd_create and MAP_FIXED (Linux, C++20) *(illustrative example, not from the talk)*

```cpp
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <sys/mman.h>
#include <unistd.h>

class MagicBuffer {
    std::byte* addr_ = nullptr;
    std::size_t size_ = 0;
public:
    explicit MagicBuffer(std::size_t size) : size_(size) {
        if (size % static_cast<std::size_t>(::sysconf(_SC_PAGESIZE)) != 0)
            throw std::invalid_argument("size must be a multiple of the page size");
        int fd = ::memfd_create("magic", 0);
        if (fd < 0 || ::ftruncate(fd, static_cast<off_t>(size)) != 0) throw std::runtime_error("memfd");
        // 1. reserve 2 * size of address space with an anonymous mapping
        void* base = ::mmap(nullptr, 2 * size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (base == MAP_FAILED) { ::close(fd); throw std::runtime_error("reserve"); }
        addr_ = static_cast<std::byte*>(base);
        // 2. and 3. map the same file over both halves, at exact addresses
        for (std::byte* half : {addr_, addr_ + size}) {
            if (::mmap(half, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, 0) == MAP_FAILED) {
                ::munmap(addr_, 2 * size); ::close(fd);
                throw std::runtime_error("map half");
            }
        }
        ::close(fd);                         // the mappings keep the memory alive
    }
    ~MagicBuffer() { ::munmap(addr_, 2 * size_); }
    MagicBuffer(MagicBuffer const&) = delete;
    MagicBuffer& operator=(MagicBuffer const&) = delete;

    std::size_t size() const { return size_; }
    std::byte* at(std::size_t pos) { return addr_ + (pos & (size_ - 1)); }   // size_ bytes always follow
};

int main() {
    MagicBuffer ring(64 * 1024);
    std::printf("ring of %zu bytes at %p\n", ring.size(), static_cast<void*>(ring.at(0)));

    char const msg[] = "magic wrapped!";                // 14 characters
    std::size_t pos = ring.size() - 6;                  // only 6 bytes before the end
    std::memcpy(ring.at(pos), msg, sizeof msg - 1);     // one plain copy, no wrap logic

    std::printf("wrote across the end; start of ring now reads: \"%.8s\"\n",
                reinterpret_cast<char const*>(ring.at(0)));
    std::printf("the same bytes at ring[%zu]: \"%.14s\"\n", pos,
                reinterpret_cast<char const*>(ring.at(pos)));
}
```

Output (the address varies):

    ring of 65536 bytes at 0x7f0e2a1f0000
    wrote across the end; start of ring now reads: "wrapped!"
    the same bytes at ring[65530]: "magic wrapped!"

The write at offset 65530 is an ordinary `memcpy` of 14 bytes with no wrap-around logic. The last 8 bytes landed in the second view, which *is* the beginning of the ring.

## 19. Multicast queues, and a question about hosts

*Talk segment: [55:50](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=3350s)*

### Multicast queues

With a minute and a half of his slot left, the speaker moved to the last topic: **MCAST** (multicast) queues. Their properties, from the slide:

- **Single producer, multiple consumers.**
- **Every consumer gets every message** (multicast, or broadcast). This is the message-queue model from the beginning of the chapter, not a task queue.
- **The producer never blocks.** It writes at full speed regardless of the consumers.
- **Slow consumers may miss messages.** A consumer that falls more than one buffer behind is *lapped*, and it is up to the consumer to recover "if they can".
- **Most really fast ones have some kind of UB** (seqlocks, etc.).

The design follows from the domain. A market-data feed cannot slow down because one of its readers is slow; it would delay every other reader. So the producer never waits, and each consumer is responsible for keeping up.

### Recovering without crashing

The previous sections said that the most common reaction to being lapped is to crash. The speaker described a better option that the queue's flags make possible. Recall the `Flags` enumeration: a queue can be single-producer, multicast, *and* have a special consumer that blocks. So you can create a queue with:

- one **special consumer** that the producer *will* wait for if necessary, typically an archiver that saves every message somewhere ("normally they won't [block], but..."), and
- any number of ordinary multicast consumers that never slow the producer down.

A consumer that gets lapped can then ask a **recovery process** to replay the messages it missed from the archive. "So there is a way to do it without crashing."

### Seqlocks, and "some kind of UB"

The speaker recommended two talks by David Gross:

- *Trading at Light Speed: Designing Low Latency Systems in C++* (Meeting C++ 2022), which describes **seqlock**-based multicast queues;
- *When Nanoseconds Matter: Ultrafast Trading Systems in C++* (CppCon 2024), which describes **FastQueue**, "almost identical to the Daugaard queue we looked at earlier". The speaker's own multicast queue, shown in the last section, is "very, very similar" to FastQueue, though not the same code.

A **seqlock** (sequence lock) is "a type of lock that's used in the kernel a lot". It protects data with a sequence counter:

1. The writer increments the counter (making it odd: "write in progress"), writes the data, and increments it again (even: "stable").
2. A reader reads the counter, copies the data, and reads the counter again. If the two readings are equal and even, the copy is consistent; otherwise a write overlapped and the reader retries.

Writers never wait for readers, and readers never write shared state, so it scales very well to many readers.

Here is where "some kind of UB" comes in. A reader may copy the data *while the writer is changing it*, then discard the copy. If the data is copied with ordinary loads (or `memcpy`), those concurrent accesses are a **data race**, and in C++ any data race is undefined behaviour, even if the result is thrown away. It works on real hardware, which is why it is everywhere, but it is outside the language's guarantees. The standard-conforming fix is to make every access to the protected data an atomic operation (relaxed is enough) and to order them with fences, as Hans Boehm described. That is what the example does, through `std::atomic_ref` on each word. There is also a proposal for a "byte-wise atomic memcpy" (P1478) to make this efficient for larger data.

**Listing 57** — A seqlock without data races: relaxed atomic accesses plus fences (C++20) *(illustrative example, not from the talk)*

```cpp
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <thread>

struct Payload { std::uint64_t x, y; };      // invariant: y == 2 * x

class SeqLocked {
    std::atomic<std::uint64_t> seq_{0};
    Payload data_{};                            // accessed only through atomic_ref
public:
    void write(Payload p) {                     // single writer
        auto s = seq_.load(std::memory_order_relaxed);
        seq_.store(s + 1, std::memory_order_relaxed);          // odd: write in progress
        std::atomic_thread_fence(std::memory_order_release);
        std::atomic_ref(data_.x).store(p.x, std::memory_order_relaxed);
        std::atomic_ref(data_.y).store(p.y, std::memory_order_relaxed);
        seq_.store(s + 2, std::memory_order_release);          // even: stable
    }
    bool try_read(Payload& out) {
        auto s1 = seq_.load(std::memory_order_acquire);
        if (s1 & 1) return false;
        out.x = std::atomic_ref(data_.x).load(std::memory_order_relaxed);
        out.y = std::atomic_ref(data_.y).load(std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_acquire);
        return seq_.load(std::memory_order_relaxed) == s1;     // unchanged: the copy is consistent
    }
};

int main() {
    SeqLocked s;
    std::atomic<bool> done{false};
    std::jthread writer([&] {
        for (std::uint64_t i = 1; !done.load(std::memory_order_relaxed); ++i) s.write({i, 2 * i});
    });
    long reads = 0, retries = 0, torn = 0;
    while (reads < 1'000'000) {
        Payload p;
        if (!s.try_read(p)) { ++retries; continue; }
        ++reads;
        if (p.y != 2 * p.x) ++torn;
    }
    done = true;
    std::printf("reads: %ld, retries: %ld, torn reads: %ld\n", reads, retries, torn);
}
```

One run printed:

    reads: 1000000, retries: 304058442, torn reads: 0

The reader never saw a pair `(x, y)` with `y != 2 * x`, because every inconsistent copy was detected and retried. The retry count is large and varies enormously between runs (a Clang build printed about 17 million). The writer here writes non-stop, so the reader very often finds a write in progress. That is the price of a seqlock: readers pay with retries when writes are frequent.

The speaker's own multicast queue, in the next section, has a similar shape. The producer may overwrite data that a slow consumer is in the middle of copying, and the consumer detects this afterwards by checking whether it was lapped.

### A question: what about other machines?

Before showing the last code, the speaker asked for questions. One was: what happens if the processes are on different computers?

In his system, configuration determines where each queue lives, so a process knows the endpoint it wants to talk to. If the queue is remote and the system is configured to allow automatic transfer, it notices that the queue lives elsewhere and starts a **network communicator**. The communicator is a *consumer* of the queue on the remote host and a *producer* of a local queue on your host, which you then read like any other. Automatic transfer can be disabled, because sometimes a configuration is wrong and you do not want data silently flowing across the network. This is the location transparency described at the start of the chapter: the code that reads the queue does not care where the messages came from.

## 20. The multicast producer and consumer

*Talk segment: [59:05](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=3545s)*

### The producer's write, implemented

The final part of the talk shows the implementation behind the producer API from earlier. It begins with `write`:

**Listing 58** — code from the talk (MCAST Producer), shown at [59:25](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=3565s)

```cpp
template <typename InvocableT, typename... ArgTs>
std::invoke_result_t<InvocableT, Producer::Writer &, ArgTs...>
Producer::
write(InvocableT && invocable, ArgTs &&... args)
noexcept(noexcept(std::invoke(
    std::declval<InvocableT>(),
    std::declval<Writer &>(),
    std::declval<ArgTs>()...)))
{
    using R = std::invoke_result_t<InvocableT, Writer &, ArgTs...>;
    auto writer = Writer(info_);
```

The declaration is the one we already saw, now defined out of line as `Producer::write` (so the return type names `Producer::Writer`). The body starts by naming the callback's result type `R` and constructing a `Writer` from the producer's `info_`, the producer's private bookkeeping (its view of the buffer, its counters, a pointer to the shared state). "We give it some special information so it knows how to do some things."

The rest of the function calls the user and commits:

**Listing 59** — code from the talk (MCAST Producer), shown at [59:55](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=3595s)

```cpp
    auto call = [&]() -> R {
        return std::invoke(
            std::forward<InvocableT>(invocable),
            writer,
            std::forward<ArgTs>(args)...);
    };
    if constexpr (std::is_same_v<R, void>) {
        call();
        info_.finish_write();
        return;
    } else {
        R result = call();
        info_.finish_write();
        return result;
    }
}
```

- `call` is a lambda that captures everything by reference and invokes the user's callable with the writer and the forwarded arguments. Wrapping the call in a lambda lets the two branches below share it.
- `finish_write()` is the **commit**: the only place where the shared atomic counter is updated. "All the stuff that it's doing writing does not touch any of the atomics... when it's done, this `finish_write` is where the atomics get updated so that the readers can see."
- The `if constexpr` handles the result type. If the callback returns `void`, call it, commit, and return nothing. Otherwise, store the result, commit, and return it. Two branches are needed because you cannot declare a variable of type `void`. "Unfortunately, we still don't have a good pattern for this in C++." (Proposals for a "regular void" type have been discussed for years without being adopted.)

Notice what happens if the callback **throws**: the exception propagates out of `call()`, `finish_write()` is never reached, and the write is not committed. How the half-written bytes are discarded comes below.

### Writing a span of bytes

**Listing 60** — code from the talk (MCAST Producer), shown at [1:00:47](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=3647s)

```cpp
template <std::size_t N>
void
Producer::Writer::
write(std::span<std::byte const, N> data) noexcept
{
    auto const size = std::uint32_t(data.size());
    assert(size == data.size());
    auto buffer = info_.prepare_buffer<8>(sizeof(std::uint64_t) + size);
    std::memcpy(buffer.data(), &size, sizeof(size));
    std::memcpy(
        buffer.data() + sizeof(std::uint64_t), data.data(), size);
    info_.local_counter += buffer.size();
}
```

1. The size is narrowed to 32 bits, and the `assert` checks that nothing was lost.
2. `prepare_buffer<8>(sizeof(std::uint64_t) + size)` reserves space for an **8-byte length header plus the payload**, aligned to 8 bytes. This is the multicast version of Daugaard's `PrepareWrite`, with one crucial difference: "instead of waiting for them, it just grabs however many it needs because it assumes that the readers are going to keep up. So it'll never ever block."
3. The first `memcpy` stores the 32-bit size at the start of the header; the header is 8 bytes so the payload stays 8-byte aligned. The second `memcpy` copies the payload after the header.
4. `info_.local_counter += buffer.size()` advances the **local** counter. Nothing is visible to consumers yet.

The local counter exists because a callback may write as many times as it wants: "I give you a writer and you can write as many times as you want into it, and it's up to this thing to keep track." The only limit is that one callback cannot write more than a full buffer.

### Raw bytes and in-place construction

**Listing 61** — code from the talk (MCAST Producer), shown at [1:01:25](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=3685s)

```cpp
template <std::size_t Alignment>
void
Producer::Writer::
write(void const * data, std::size_t size) noexcept
{
    auto buffer = info_.prepare_buffer<Alignment>(size);
    std::memcpy(buffer.data(), data, size);
    info_.local_counter += size;
}
```

The raw-bytes version is the same without the length header: reserve `size` bytes at the requested alignment, copy, advance the local counter.

**Listing 62** — code from the talk (MCAST Producer), shown at [1:01:53](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=3713s)

```cpp
template <TriviallyCopyableC T>
T &
Producer::Writer::
emplace(auto &&... args) noexcept(
    std::is_nothrow_constructible_v<T, decltype(args)...>)
requires std::constructible_from<T, decltype(args)...>
{
    auto buffer = info_.prepare_buffer<alignof(T)>(sizeof(T));
    auto t = new (static_cast<void *>(buffer.data()))
        T(std::forward<decltype(args)>(args)...);
    info_.local_counter += sizeof(T);
    return *t;
}
```

`emplace` reserves `sizeof(T)` bytes at `alignof(T)` and constructs the object there with **placement new**: `new (address) T(args...)` runs a constructor on existing storage instead of allocating. The `static_cast<void *>` makes sure the standard placement form is used. This only works because the bytes are contiguous, "no matter where we are, we know that we're always going to have contiguous bytes because we have this magic buffer", which the queue enables with its magic flag. It returns a reference to the new object so the callback can keep filling it in.

### Commit and rollback

The writer's constructor and destructor implement **transactional writes**:

**Listing 63** — code from the talk (MCAST Producer), shown at [1:02:03](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=3723s)

```cpp
inline Producer::Writer::
Writer(Info & x)
: info_(x)
{
    // Save the counter, so we can restore it later, if the write
    // operation was aborted for some reason including a user exception.
    info_.saved_counter = info_.local_counter;
}
```

**Listing 64** — code from the talk (MCAST Producer), shown at [1:02:07](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=3727s)

```cpp
inline Producer::Writer::
~Writer()
{
    // Restore the counter.  Safe to do always because the tmp_counter is
    // set to the local_counter when finish_write is called.
    info_.local_counter = info_.saved_counter;
}
```

The constructor saves the local counter. The destructor *always* restores it. That looks wrong until you see `finish_write`:

**Listing 65** — code from the talk (MCAST Producer), shown at [1:02:15](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=3735s)

```cpp
inline void
Producer::Info::
finish_write()
{
    shared->read_counter.store(
        local_counter, std::memory_order_release);

    // Update the saved_counter because the write operation has been
    // committed, and there is no longer a need to restore the original
    // value.
    saved_counter = local_counter;
}
```

`finish_write` first publishes the local counter to the shared counter with a **release** store. Every byte written before it (headers, payloads, emplaced objects) becomes visible to any consumer that loads the counter with acquire. Then it sets `saved_counter = local_counter`. So:

- **On success**, `finish_write` has already set `saved_counter` to the new position, and the destructor's "restore" sets the local counter to the value it already has: a no-op.
- **On an exception** (or any early exit), `finish_write` never ran, `saved_counter` still holds the position from before the callback, and the destructor rolls the local counter back. The half-written bytes are simply overwritten by the next write. Consumers never saw them, because the shared counter was never advanced.

The comment in the destructor mentions a `tmp_counter`; the code uses `saved_counter`. It appears to be a leftover name from an earlier version, and the explanation in `finish_write` makes the intent clear. The shared counter is named `read_counter` because it is the position up to which consumers may read.

### The consumer's try_read

The talk ended while the speaker was on the consumer:

**Listing 66** — code from the talk (MCAST Consumer), shown at [1:02:31](https://www.youtube.com/watch?v=AmPLoOfRFDs&t=3751s)

```cpp
inline tl::expected<std::size_t, std::int_fast64_t>
Consumer::
try_read(std::span<std::byte> data)
{
    auto read_counter = aligned<Alignment::Int64>(info_.local_counter);
    std::uint32_t size;
    if (read_counter + sizeof(size) >= info_.cached_counter) {
        info_.cached_counter = info_.shared->read_counter.load(
            std::memory_order_acquire);
        if (read_counter + sizeof(size) > info_.cached_counter) {
            return tl::unexpected(no_data);
        }
    }
    auto const buf = info_.buffer + (read_counter & info_.mask);
    std::memcpy(&size, buf, sizeof(size));
```

- `aligned<Alignment::Int64>(info_.local_counter)` rounds the consumer's position up to a multiple of 8, matching how the producer aligned each message (highlighted in the talk).
- Before reading the 32-bit size, the consumer checks whether its **cached** copy of the producer's counter already shows those bytes. Only if not (the highlighted condition) does it reload the shared counter with an **acquire** load, which pairs with the producer's release store. If the bytes still are not there, it returns `no_data`. As in Daugaard's design, a consumer that knows about many published messages reads them without touching the shared cache line.
- `info_.buffer + (read_counter & info_.mask)` turns the monotonic counter into a buffer address. Thanks to the magic buffer, the message is contiguous from there even if it crosses the end of the ring.
- The size is copied out with `memcpy`.

The published slides continue with the rest of the function, which the talk did not reach:

- if the message is larger than the caller's span, return `lapped` if the consumer has been lapped (the "size" is then garbage), otherwise the needed size;
- advance past the 8-byte header and the payload, and return `no_data` if that goes beyond the cached counter;
- copy the payload into the caller's span;
- **then check again whether the consumer has been lapped**, and if so return `lapped`;
- otherwise store the new position in `local_counter` and return the size.

The speaker's last words explain why it copies: "We can't give a view into the buffer, because the writer might come around and write over it." The producer never blocks, so the bytes could be overwritten at any moment. The consumer copies first and checks afterwards whether the producer has lapped it during the copy, the same copy-then-validate idea as a seqlock. If the producer got that far, the copy may be corrupt and is discarded. As the previous section explained, copying bytes that another process may be writing at that moment is, strictly, one of the "some kind of UB" patterns that fast multicast queues live with.

### Checking the slides' code

The producer and consumer slides were compiled verbatim with GCC 16 and Clang 20, inside a harness that supplies the parts the talk did not show: a minimal `Info` with `prepare_buffer`, an `aligned` function, and the rest of `try_read` from the published slides. Exercising it confirmed the commit and rollback logic. A `write(span)` of 6 bytes followed by an `emplace` advanced the shared counter to 32 (two 16-byte, 8-aligned pieces). A third callback that wrote 4 bytes and then threw left the counter at 32, and the next callback simply overwrote the rolled-back bytes.

It also exposed a detail worth knowing. Of the three `Writer` functions on the slides, only `write(span)` writes the 8-byte length header; `emplace` and the raw `write` do not. `try_read`, however, starts *every* message by reading a length. In the harness, reading back the message written with `emplace<Hdr>(7, 2.5)` alone treated the first field, 7, as a 7-byte length. So with the consumer as shown, a callback must frame each message so that it starts with a length, for example by writing it with `write(span)`. The full implementation may handle framing in a way the slides do not show.

### A working model of the protocol

The example implements the whole protocol in one process: one producer, a fast consumer and a slow consumer, over a 64-byte ring. Messages use the same 8-byte length header and 8-byte alignment. Without a magic buffer, it copies in two pieces at the end of the ring. To keep lapping deterministic (and free of data races), the operations run in sequence rather than on threads.

**Listing 67** — A single-process model of the multicast protocol: length headers, release/acquire, lapping (C++23) *(illustrative example, not from the talk)*

```cpp
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <expected>
#include <span>
#include <string>
#include <vector>

constexpr std::uint64_t align8(std::uint64_t x) { return (x + 7) & ~std::uint64_t{7}; }

class Ring {                                         // power-of-two ring; copies split at the end
    std::vector<std::byte> buf_;
    std::uint64_t mask_;
public:
    std::atomic<std::uint64_t> read_counter{0};      // how far consumers may read (published)
    explicit Ring(std::size_t capacity) : buf_(capacity), mask_(capacity - 1) {}
    std::uint64_t capacity() const { return buf_.size(); }
    void copy_in(std::uint64_t pos, void const* src, std::size_t n) {
        auto off = pos & mask_, first = std::min<std::uint64_t>(n, buf_.size() - off);
        std::memcpy(buf_.data() + off, src, first);
        std::memcpy(buf_.data(), static_cast<std::byte const*>(src) + first, n - first);
    }
    void copy_out(void* dst, std::uint64_t pos, std::size_t n) const {
        auto off = pos & mask_, first = std::min<std::uint64_t>(n, buf_.size() - off);
        std::memcpy(dst, buf_.data() + off, first);
        std::memcpy(static_cast<std::byte*>(dst) + first, buf_.data(), n - first);
    }
};

class Producer {
    Ring& ring_;
    std::uint64_t local_counter_ = 0;
public:
    explicit Producer(Ring& r) : ring_(r) {}
    void write(std::span<std::byte const> data) {    // never waits for consumers
        auto pos = align8(local_counter_);
        auto size = std::uint32_t(data.size());
        ring_.copy_in(pos, &size, sizeof size);                     // 8-byte header slot
        ring_.copy_in(pos + sizeof(std::uint64_t), data.data(), size);
        local_counter_ = pos + sizeof(std::uint64_t) + size;
        ring_.read_counter.store(local_counter_, std::memory_order_release);   // finish_write
    }
};

constexpr std::int64_t no_data = 0, lapped = -1;

class Consumer {
    Ring const& ring_;
    std::uint64_t local_counter_ = 0, cached_counter_ = 0;
public:
    explicit Consumer(Ring const& r) : ring_(r) {}
    std::uint64_t behind() const { return ring_.read_counter.load(std::memory_order_acquire) - local_counter_; }
    bool is_lapped() const { return behind() > ring_.capacity(); }

    std::expected<std::size_t, std::int64_t> try_read(std::span<std::byte> data) {
        auto read_counter = align8(local_counter_);
        std::uint32_t size;
        if (read_counter + sizeof size >= cached_counter_) {
            cached_counter_ = ring_.read_counter.load(std::memory_order_acquire);
            if (read_counter + sizeof size > cached_counter_) return std::unexpected(no_data);
        }
        ring_.copy_out(&size, read_counter, sizeof size);
        if (size > data.size()) return std::unexpected(is_lapped() ? lapped : std::int64_t(size));
        read_counter += sizeof(std::uint64_t) + size;
        if (read_counter > cached_counter_) return std::unexpected(no_data);
        ring_.copy_out(data.data(), read_counter - size, size);
        if (is_lapped()) return std::unexpected(lapped);            // copied data may be overwritten
        local_counter_ = read_counter;
        return size;
    }
};

int main() {
    Ring ring(64);
    Producer producer(ring);
    Consumer fast(ring), slow(ring);
    std::byte buf[32];

    std::printf("fast consumer:");
    for (int i = 0; i < 10; ++i) {
        std::string msg = "m" + std::to_string(i);
        producer.write(std::as_bytes(std::span(msg)));
        if (auto n = fast.try_read(buf)) std::printf(" %.*s", int(*n), reinterpret_cast<char const*>(buf));
    }
    std::printf("\n");

    auto unread_overwritten = slow.behind() - ring.capacity();
    if (auto r = slow.try_read(buf); !r && r.error() == lapped)
        std::printf("slow consumer: lapped (the producer overwrote %llu unread bytes)\n",
                    static_cast<unsigned long long>(unread_overwritten));
}
```

Output:

    fast consumer: m0 m1 m2 m3 m4 m5 m6 m7 m8 m9
    slow consumer: lapped (the producer overwrote 90 unread bytes)

Each message occupies 16 bytes (an 8-byte header plus a 2-character payload, padded to 8 before the next message), so the 64-byte ring holds four. The fast consumer reads after every write and keeps up. The slow consumer starts reading only after ten messages (154 bytes, since the last one is not yet padded) have been published, finds that the producer is more than a full ring ahead of it, and reports `lapped` instead of returning corrupted data.

## Key takeaways

- C++ barely mentions processes; shared-memory code must be designed so it is defined behaviour, not just 'works on my machine'
- Inter-process queues need an API that separates queue, producer and consumer and enforces role ownership in code
- Pointers do not survive the trip between address spaces; store offsets, and let each role holder fix up its own local pointers
- Queue metadata (signature, UUID, sizes, flags) turns silent misconnections into detectable errors
- A lock in shared memory must be recoverable: identify owners by pid plus start time and reclaim locks from dead processes with CAS
- fork duplicates only the calling thread; cached per-process state needs atfork handling and the child may only use async-signal-safe calls
- Since C++20, malloc/mmap-style storage can hold implicit-lifetime objects, but std::atomic is not an implicit-lifetime type
- Messages placed in shared memory must be implicit-lifetime types
- Don't trust std::hardware_destructive_interference_size in library headers
- The magic buffer maps the same pages twice so a ring buffer never has to wrap a message
- A multicast producer never blocks; consumers detect being lapped; writes are committed with a single release store

## Glossary

| Term | Meaning |
|---|---|
| Message queue | A FIFO that carries messages from producers to consumers; with multicast every consumer sees every message (contrast a task queue, where each item goes to one worker). |
| SPSC | Single producer, single consumer: a queue with exactly one writer and one reader, which allows simple lock-free designs. |
| Highlander rule | 'There can be only one': at most one producer (or consumer) may hold the role of a single-producer (single-consumer) queue at a time. |
| Shared memory | Physical pages mapped into the address spaces of several processes, usually with mmap and MAP_SHARED. |
| Advisory lock | A file lock the OS records but does not enforce; cooperating processes must check it. |
| PID reuse | The OS recycles process ids after a process exits, so a pid alone does not identify a process over time. |
| Async-signal-safe | A function that may be called from a signal handler; after fork in a multithreaded program the child may only call such functions until exec. |
| pthread_atfork | POSIX function registering prepare, parent and child handlers that run around fork(). |
| Robust mutex | A POSIX mutex (PTHREAD_MUTEX_ROBUST) that reports EOWNERDEAD to the next locker when its owner died holding it. |
| Implicit-lifetime type | Scalars, arrays, and classes that are aggregates or have a trivial eligible constructor and a trivial non-deleted destructor; objects of these types can be created implicitly by operations like malloc and memcpy (C++20). |
| std::start_lifetime_as | C++23 function that starts the lifetime of an implicit-lifetime object over existing storage, keeping its bytes. |
| Address-free | An atomic operation that works through different virtual addresses of the same memory; the standard says lock-free atomics should be address-free. |
| False sharing | Slowdown when independent variables written by different cores share a cache line. |
| Magic buffer | A ring buffer whose physical pages are mapped twice consecutively, so reads and writes past the end appear at the start without wrap-around code. |
| Lapped | State of a multicast consumer that the producer has overtaken by more than a buffer's length, so messages were lost. |
| Seqlock | A reader-writer scheme where readers retry if a sequence counter changed during the read; fast, but it reads racing data. |

## Exercises

1. Two processes map the same 4 KiB shared-memory region. Process A stores the address of a node inside the region into the region; process B dereferences it. Explain what can go wrong and redesign the structure so it works in both processes.

   <details><summary>Hint</summary>Think about virtual addresses; store an offset from the region's base.</details>

2. Sketch a Queue API in which only one Producer can exist at a time, even across processes, and a second attempt fails cleanly. Where does the lock live, and what happens when the producer process is killed with SIGKILL?

   <details><summary>Hint</summary>The role lock lives in the queue's metadata; combine it with a process identity that allows recovery.</details>

3. In try_lock_impl, after detecting a dead owner the code exchanges the pid with null and then tries again to exchange null with me. Why not exchange the dead owner's id with me directly in one step? What race does the two-step version tolerate?

   <details><summary>Hint</summary>Consider two live processes discovering the same dead owner at the same time.</details>

4. A program caches its process identity in a function-local static. It then forks. Describe what the child sees without a pthread_atfork handler, and why the handler must not call malloc or take a lock.

   <details><summary>Hint</summary>fork copies memory but only the calling thread.</details>

5. For each type decide whether it is an implicit-lifetime type: (a) struct P { int x; double y; }; (b) struct Q { Q(int); int v; }; (c) struct R { ~R(); int v; }; (d) std::atomic<int> in C++20; (e) int[16].

   <details><summary>Hint</summary>Aggregate, or trivial eligible constructor plus trivial non-deleted destructor.</details>

6. Work the pop quiz: for struct Foo { void operator=(Foo&&) = delete; }; which of the six static_asserts fail, and why?

   <details><summary>Hint</summary>Use Hinnant's table: user-declared move assignment.</details>

7. Why does the magic buffer need the first anonymous mapping of twice the length, rather than mapping the file twice at two addresses chosen by the kernel?

   <details><summary>Hint</summary>MAP_FIXED and guaranteeing adjacency.</details>

8. In the multicast consumer, why must try_read copy the message out of the buffer instead of returning a span that points into it? What check must happen after the copy?

   <details><summary>Hint</summary>The producer never blocks.</details>

## Further reading

- Jody Hagins, Building Robust Inter-Process Queues in C++ - slides: https://github.com/CppCon/CppCon2025/blob/main/Presentations/Building_Robust_Inter-Process_Queues_in_Cpp.pdf
- Kaspar Daugaard, Writing a Fast and Versatile SPSC Ring Buffer (and follow-up posts) - https://www.daugaard.org/blog/
- Klaus Iglberger, C++ Software Design (O'Reilly)
- James O. Coplien, Multi-Paradigm Design for C++
- David Gross, Trading at Light Speed: Designing Low Latency Systems in C++ (Meeting C++ 2022)
- David Gross, When Nanoseconds Matter: Ultrafast Trading Systems in C++ (CppCon 2024)
- Ben Saks, Implementing Your Own Atomics (CppCon 2025)
- Howard Hinnant's table of implicitly declared special member functions
- The C++ standard: [intro.object] (implicit object creation), [class.prop] (implicit-lifetime class), [atomics.lockfree] (address-free atomics)
- POSIX: pthread_atfork, async-signal-safe functions, robust mutexes (PTHREAD_MUTEX_ROBUST)
