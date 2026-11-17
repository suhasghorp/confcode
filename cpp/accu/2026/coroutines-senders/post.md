---
title: "Refactoring Towards Structured Concurrency: Coroutines, Senders and Migration Paths in C++"
date: 2026-11-14
tags: ["C++", "structured concurrency", "coroutines", "senders and receivers", "std::execution", "C++26", "asio", "asynchronous programming", "refactoring", "cancellation"]
source: "https://www.youtube.com/watch?v=ttoQfPP_Gkg"
video_title: "Why Structured Concurrency is the Future of C++ | Roi Barkan"
channel: "ACCU Conference"
---

> This chapter is based on the talk [Why Structured Concurrency is the Future of C++ | Roi Barkan](https://www.youtube.com/watch?v=ttoQfPP_Gkg) (ACCU Conference). Code listings marked *from the talk* are transcribed from the slides; listings marked *illustrative* were written for this chapter to explain the ideas.

Modern software rarely runs one instruction after another in a single, predictable order. Servers receive thousands of independent and interdependent requests, hardware offers many cores and accelerators, and latency-sensitive domains such as high-frequency trading need every microsecond. Concurrency is how we describe and exploit this. Correct concurrent code, however, is notoriously hard to write, test and review.

This chapter follows Roi Barkan's ACCU on Sea 2026 talk *Refactoring Towards Structured Concurrency*. Barkan defines concurrency as the art of **describing dependencies between instructions**. He surveys the classic building blocks and techniques: threads, reactors, state machines and callbacks. He then argues that **structured concurrency** is the right way forward. In structured concurrency, tasks are treated like RAII-managed resources that belong to scopes. C++20 coroutines and C++26 senders/receivers (`std::execution`) both provide it, and they interoperate.

The last part of the chapter is practical. If you already own a working concurrent system that was not designed this way, how do you migrate it gradually? Barkan's tips cover which libraries to adopt, how to keep control of your main loop, where to start converting, how the function-coloring problem affects you, and why you should not fear cancellation.

## What you will learn

- Define concurrency as the description of dependencies between instructions and name its building blocks: non-blocking APIs, callbacks, futures, fork/join, synchronization primitives and cancellation.
- Explain the motivations for concurrency: parallelism, separation of concerns, tail-latency hiding and batching.
- Explain why correctness is the central challenge, and why 'no raw synchronization primitives' and 'correct by construction' are the goals.
- Compare OS threads, virtual (green) threads and reactor/proactor designs, and describe how state machines and continuation-passing style split work into tasks.
- Identify the lifetime problems in callback-style code such as the asio `shared_from_this` pattern.
- State the structured concurrency philosophy (tasks owned by scopes, scopes outlive tasks) and relate it to RAII and std::jthread.
- Explain how coroutines and senders/receivers each implement structured concurrency, including the sender/receiver 'onion'.
- Read equivalent coroutine and sender-chain code and explain how each handles object lifetimes.
- Apply Barkan's migration tips: use existing libraries, keep the main loop, go root-first for non-blocking code and leaf-first for blocking code, and use cancellation.

## Before you start

- Comfortable C++11/14: lambdas, std::function, smart pointers (shared_ptr), move semantics
- RAII and value semantics
- Basic familiarity with std::thread and mutexes
- Helpful but not required: C++20 coroutine keywords (co_await, co_return) and C++20 ranges pipe syntax

## Contents

1. [Introduction: Why Revisit Concurrent Architecture Now](#1-introduction-why-revisit-concurrent-architecture-now) — [0:00](https://www.youtube.com/watch?v=ttoQfPP_Gkg&t=0s)
2. [From Sequential Programs to Concurrent Environments](#2-from-sequential-programs-to-concurrent-environments) — [1:50](https://www.youtube.com/watch?v=ttoQfPP_Gkg&t=110s)
3. [Concurrency as Dependency Description: Non-blocking APIs, Callbacks and Futures](#3-concurrency-as-dependency-description-non-blocking-apis-callbacks-and-futures) — [4:30](https://www.youtube.com/watch?v=ttoQfPP_Gkg&t=270s)
4. [Fork, Join, Synchronization Primitives and Cancellation](#4-fork-join-synchronization-primitives-and-cancellation) — [7:15](https://www.youtube.com/watch?v=ttoQfPP_Gkg&t=435s)
5. [Motivation: What's the Point of Concurrency?](#5-motivation-whats-the-point-of-concurrency) — [10:27](https://www.youtube.com/watch?v=ttoQfPP_Gkg&t=627s)
6. [The Correctness Challenge and 'No Raw Synchronization Primitives'](#6-the-correctness-challenge-and-no-raw-synchronization-primitives) — [13:28](https://www.youtube.com/watch?v=ttoQfPP_Gkg&t=808s)
7. [Concurrency Techniques: OS Threads and Virtual Threads](#7-concurrency-techniques-os-threads-and-virtual-threads) — [17:43](https://www.youtube.com/watch?v=ttoQfPP_Gkg&t=1063s)
8. [Reactor/Proactor Thread Pools](#8-reactorproactor-thread-pools) — [20:08](https://www.youtube.com/watch?v=ttoQfPP_Gkg&t=1208s)
9. [Splitting Work into Tasks: State Machines](#9-splitting-work-into-tasks-state-machines) — [21:46](https://www.youtube.com/watch?v=ttoQfPP_Gkg&t=1306s)
10. [Callbacks / Continuation-Passing Style and the Lifetime Problem](#10-callbacks--continuation-passing-style-and-the-lifetime-problem) — [23:20](https://www.youtube.com/watch?v=ttoQfPP_Gkg&t=1400s)
11. [The Structured Concurrency Philosophy](#11-the-structured-concurrency-philosophy) — [27:21](https://www.youtube.com/watch?v=ttoQfPP_Gkg&t=1641s)
12. [Coroutines Are Structured Concurrency](#12-coroutines-are-structured-concurrency) — [31:20](https://www.youtube.com/watch?v=ttoQfPP_Gkg&t=1880s)
13. [Senders Are Structured Concurrency](#13-senders-are-structured-concurrency) — [34:05](https://www.youtube.com/watch?v=ttoQfPP_Gkg&t=2045s)
14. [The Sender/Receiver Onion](#14-the-senderreceiver-onion) — [37:45](https://www.youtube.com/watch?v=ttoQfPP_Gkg&t=2265s)
15. [Reading the Flow: Coroutine vs. Sender-Chain Implementations](#15-reading-the-flow-coroutine-vs-sender-chain-implementations) — [39:59](https://www.youtube.com/watch?v=ttoQfPP_Gkg&t=2399s)
16. [Summary So Far and Migration Tip: Use Libraries, Keep Your Main Loop](#16-summary-so-far-and-migration-tip-use-libraries-keep-your-main-loop) — [46:00](https://www.youtube.com/watch?v=ttoQfPP_Gkg&t=2760s)
17. [Where to Start: Root vs. Leaf, Function Coloring and Cancellation](#17-where-to-start-root-vs-leaf-function-coloring-and-cancellation) — [49:47](https://www.youtube.com/watch?v=ttoQfPP_Gkg&t=2987s)
18. [Final Summary and Q&A: Testing and Performance](#18-final-summary-and-qa-testing-and-performance) — [55:18](https://www.youtube.com/watch?v=ttoQfPP_Gkg&t=3318s)

## 1. Introduction: Why Revisit Concurrent Architecture Now

*Talk segment: [0:00](https://www.youtube.com/watch?v=ttoQfPP_Gkg&t=0s)*

### Who is speaking, and why it matters

The talk behind this chapter was given by **Roi Barkan**, a C++ developer since 2000 who now leads development and technology at Istra Research, a proprietary trading firm in Tel Aviv. His day job is **low-latency, high-frequency trading (HFT)**. In HFT, microseconds decide whether a trade is profitable, and systems must react to a constant stream of market events from many sources at once.

This background shapes the whole chapter. In his world, concurrency is not about making a slow program somewhat faster. It is the core of the architecture. Bugs in that architecture are expensive, and so is wasted overhead: an extra thread, an extra context switch, or an extra heap allocation. When the speaker later argues that *structured concurrency* can be both **safer** and **cheaper** than traditional techniques, he is arguing from a domain where both properties are non-negotiable.

### The goal: refactoring, not greenfield design

Most material on coroutines or senders starts from a blank page: "here is a new feature, here is a toy program that uses it." This talk takes a different angle. Its title begins with *Refactoring*, and Barkan sets himself three tasks:

1. **Explain the concepts.** What is a *concurrent system*, and what is *structured concurrency*?
2. **Make the case for migrating.** Why take an existing system that *already seems to work*, but does not follow the structured methodology, and move it towards that methodology?
3. **Give practical tips** for refactoring such a system step by step.

The second point is the provocative one. Engineers are rightly reluctant to rewrite working code. The speaker's claim is that "seems to work" is weaker than it sounds for concurrent code. Bugs can hide in rare interleavings that no test suite reaches, and lifetime management in callback-based designs is fragile. A more disciplined structure lets you be *correct by construction*. Later sections develop this argument in detail.

> **Key idea:** The question is not "how do I write a new program with coroutines?" It is "how do I move a real, running, performance-critical system towards a structure I can actually reason about?"

### Why now? A short timeline of C++ concurrency

Barkan points out that we are in the early days of C++26. C++20 is available to most developers today, and many of the mechanisms the talk relies on are either already in C++20 or arriving with C++26. That makes this a natural moment to rethink concurrent architecture.

It helps to see how the standard has evolved:

| Standard | What arrived | What it gave us |
|---|---|---|
| **C++11** | `std::thread`, `std::mutex`, `std::atomic`, `std::future`, `std::async` | A portable memory model and OS-thread-based concurrency. Everything above this layer was up to you. |
| **C++20** | Coroutines (`co_await`, `co_return`, `co_yield`), `std::jthread`, `std::stop_source` / `std::stop_token`, semaphores, latches, barriers | Language support for suspendable functions, threads that join automatically, and a standard cancellation mechanism. |
| **C++26** | `std::execution` (senders and receivers) | A standard model for describing, composing and launching asynchronous work on arbitrary execution resources. |

Notice the trend. C++11 gave us **raw building blocks**: threads and locks that you must coordinate by hand. C++20 and C++26 add **structure**: lifetimes tied to scopes, cancellation as a first-class concept, and ways to *compose* asynchronous operations rather than wire them together manually.

One caveat that recurs later: C++20 provides the *language machinery* for coroutines but almost no library support. There is no standard task type in C++20, for example. In practice you pair coroutines with a library such as Asio. Senders, by contrast, arrive in C++26 with a set of standard algorithms included.

The first example below (my own, not from the talk) shows the step from C++11 to C++20 in miniature. The task is the same, but the C++20 version ties the thread's lifetime to a scope and builds in cancellation. This is a small taste of the "structure" the chapter is about.

**Listing 1** — From C++11 std::thread to C++20 std::jthread with cancellation *(illustrative example, not from the talk)*

```cpp
#include <chrono>
#include <iostream>
#include <stop_token>
#include <thread>

void cxx11_style() {
    std::thread t([] { std::cout << "work in a C++11 thread\n"; });
    t.join();  // Forget this (or skip it via an exception) and
               // ~thread() calls std::terminate.
}

void cxx20_style() {
    // jthread passes a stop_token to the callable if it accepts one.
    std::jthread t([](std::stop_token st) {
        while (!st.stop_requested()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        std::cout << "worker observed the stop request\n";
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}   // ~jthread(): request_stop(), then join(). The scope owns the thread.

int main() {
    cxx11_style();
    cxx20_style();
}

```

The `std::thread` version relies on the programmer to remember `join()`. If you forget it, or an exception skips it, the program calls `std::terminate`. The `std::jthread` version cannot make that mistake. When the object goes out of scope, its destructor first *requests a stop* through the thread's `std::stop_token`, then *joins*. The closing brace `}` becomes a guarantee that the background work has finished. The speaker returns to this jthread-versus-thread contrast when he defines structured concurrency.

The second example jumps ahead to C++26. It shows only the *shape* of sender-based code, which later sections explain properly.

**Listing 2** — A first glimpse of C++26 senders (std::execution) *(illustrative example, not from the talk)*

```cpp
// C++26 <execution>. Before your standard library ships it, the same code
// works with NVIDIA's stdexec library (#include <stdexec/execution.hpp>,
// namespace stdexec, stdexec::sync_wait) or with beman.execution.
#include <execution>
#include <iostream>
#include <utility>

int main() {
    namespace ex = std::execution;

    // Describe the work; nothing runs yet.
    ex::sender auto work = ex::just(21)
                         | ex::then([](int x) { return x * 2; });

    // Launch it and block until it completes.
    // sync_wait returns std::optional<std::tuple<int>>.
    auto [result] = std::this_thread::sync_wait(std::move(work)).value();
    std::cout << result << '\n';  // prints 42
}

```

Don't worry if `just`, `then` and `sync_wait` are unfamiliar. The point for now is that work is *described* as a pipeline first and *run* afterwards. That separation is what makes senders composable.

### The road map

Barkan structures the talk in three parts, and this chapter follows the same order:

1. **Concurrency in high-performance systems.**
   - *Definitions:* what concurrency actually is. His answer is "a description of which dependencies really exist."
   - *Motivations:* why bother, given how hard concurrent code is to get right.
   - *Techniques:* the classic toolbox, including OS threads, virtual threads, reactor/proactor thread pools, state machines and callbacks.
2. **Structured concurrency.**
   - *What* it is: tasks as resources owned by scopes, organised as a tree.
   - *Why* it helps: lifetimes, errors and cancellation become easy to reason about locally.
   - *How* C++ provides it: through coroutines and through senders/receivers.
3. **Migration paths.** Practical advice for moving an existing system over:
   - which libraries to adopt;
   - how to keep control of your main loop;
   - whether to start converting at the root or at the leaves of your call graph;
   - how to deal with "function coloring" and cancellation.

### What you need to bring

You are expected to know ordinary C++: classes, RAII, lambdas, templates at a basic level, and perhaps `std::thread`. You do **not** need prior experience with coroutines, Asio or `std::execution`. Each concept is introduced before it is used:

- non-blocking APIs and futures;
- coroutine frames and awaiters;
- senders, receivers and operation states.

The first technical step is to go back to basics and ask what it means for a program to be *concurrent* at all. That is the subject of the next section.

## 2. From Sequential Programs to Concurrent Environments

*Talk segment: [1:50](https://www.youtube.com/watch?v=ttoQfPP_Gkg&t=110s)*

### The sequential world we started from

Early computers, and the algorithms written for them, were **sequential**. A program was a list of instructions executed one after another. Each instruction ran only after its predecessor had finished, so it could freely use that predecessor's result. In fact it could use the result of *every* instruction that came before it.

The speaker describes this as an **implicit dependency**. Whenever an instruction runs, it may assume that "everything before me is already done." Nobody writes that assumption down. It is simply how sequential execution works.

Here is the most ordinary sequential code imaginable, a loop that sums an array:

**Listing 3** — A sequential sum and its implicit dependencies *(illustrative example, not from the talk)*

```cpp
#include <iostream>
#include <vector>

int main() {
    std::vector<int> prices{3, 1, 4, 1, 5, 9, 2, 6};

    long total = 0;              // must happen before the loop
    for (int p : prices) {
        total += p;              // reads the result of the previous iteration
    }

    std::cout << total << '\n';  // assumes the whole loop is finished
}

```

Walk through it with the implicit dependency in mind:

- `long total = 0;` must happen before the loop. Otherwise the loop would add to garbage.
- Inside the loop, `total += p;` reads the value of `total` produced by the *previous* iteration. Iteration 5 silently relies on iterations 1 through 4 being complete.
- The final print relies on the entire loop having finished. If it ran halfway through, it would show a partial sum.

None of these dependencies is stated explicitly. The order of the source lines states all of them at once. That is convenient, because you never have to think about it. It is also very **coarse**: the program says "depend on everything before me" even when only a little of it is truly needed.

### The environment was never that simple

Even in those early days, the speaker notes, the world around programs was more complicated than "run instructions one after another." It has become far more complicated since. There are two sides to this complexity.

#### The compute side: many things can run at once

A machine rarely has just one place to execute instructions:

- **Multiple cores** in a single CPU.
- **Multiple servers** cooperating over a network.
- **GPUs**, which run thousands of lightweight computations in parallel.
- **Heterogeneous systems** that mix CPUs, GPUs, network cards and other accelerators, each able to make progress independently.

All of these can run instructions *at the same time*. A strictly sequential program leaves most of that hardware idle.

#### The input side: work arrives while you are working

The second complication is about *inputs*, or *events*. A classic batch program receives all its input up front, reads it, computes and prints an answer. Many real systems are **online systems** instead. A web server, a trading engine or a chat service receives inputs *while it is already running*. Those inputs arrive:

- **at unpredictable times**, whenever a user clicks or a market data packet lands;
- **in unpredictable orders**, depending on network delays and other users;
- **continuously**, with no point at which "the input is complete."

The speaker stresses that we want such systems to be as efficient as possible. That means computing **as early as possible**: doing useful work on the inputs that have already arrived instead of waiting for some future, complete picture. This is the idea behind *online calculation*.

> **Key insight:** Sequential programming assumes that all inputs are available and that one thing happens at a time. Modern systems break both assumptions. Hardware can do many things at once, and inputs keep arriving while work is in progress.

### Unrelated requests versus linked requests

Once many inputs flow into a system, a crucial question appears: *how do these inputs relate to each other?* The speaker distinguishes two cases.

| Kind | Example | What the system must do |
|---|---|---|
| **Unrelated** (independent) | Alice and Bob, two different users, each send a request | Nothing special. The requests can be processed in any order, or at the same time. |
| **Linked** (dependent) | Alice logs in, then Alice buys something | Respect the dependency. The purchase must see the effect of the login, so their relative order matters. |

For unrelated requests, in the speaker's words, there is "really no dependency", so the system can treat the events as totally separate things. For linked requests, the system must *uphold* the dependency. Processing Alice's purchase before her login completes would be a bug, even though both requests are perfectly valid on their own.

The example below captures this distinction as data. It is a deliberately simple rule: requests from the same user are linked, and requests from different users are not.

**Listing 4** — Modelling unrelated vs. linked requests *(illustrative example, not from the talk)*

```cpp
#include <iostream>
#include <string>
#include <vector>

struct Request {
    int user_id;
    std::string action;
};

// A deliberately simple dependency rule:
// requests from the same user are linked, others are independent.
bool must_preserve_order(const Request& a, const Request& b) {
    return a.user_id == b.user_id;
}

int main() {
    std::vector<Request> incoming{
        {1, "login"},     // Alice
        {2, "login"},     // Bob
        {1, "purchase"},  // Alice again
    };

    for (std::size_t i = 0; i < incoming.size(); ++i) {
        for (std::size_t j = i + 1; j < incoming.size(); ++j) {
            const auto& a = incoming[i];
            const auto& b = incoming[j];
            std::cout << a.action << "(user " << a.user_id << ") -> "
                      << b.action << "(user " << b.user_id << "): "
                      << (must_preserve_order(a, b) ? "linked" : "independent")
                      << '\n';
        }
    }
}

```

Notes on this sketch:

- `Request` carries the information needed to decide about dependencies, here the `user_id`.
- `must_preserve_order` encodes the dependency rule. Real systems use richer rules. Two users trading the same stock, for example, may be linked through a shared order book.
- The program never says *how* to run the requests. It only states *which pairs* need ordering. That separation becomes central throughout this chapter.

### Exercise: independent or dependent?

Before reading the next section, sort these events. For each pair, ask: *does the second one need the result or effect of the first?*

1. A user uploads a photo, and a different user posts a comment on an unrelated thread.
2. A user adds an item to a shopping cart, then clicks "checkout."
3. Two sensors in different rooms each report a temperature reading.
4. A trading system receives a "place order" message, then a "cancel that order" message for the same order.
5. A server receives a DNS lookup result, then opens a TCP connection to the returned address.
6. Two independent web pages are requested by the same browser at the same moment.

(Suggested answers: 1, 3 and 6 are independent. 2, 4 and 5 are dependent. Item 6 is instructive: the *same* client does not automatically mean *linked*. What matters is whether one request needs the other's result.)

### The big picture: a concurrent processing system

The speaker summarizes the setting with a diagram:

- On the left, many **clients** send requests.
- The requests enter a **request queue** or **load balancer**.
- The queue feeds a **concurrent processing system**, drawn as several **worker threads** (T1 to T4) that all access some **shared resources**, such as databases, caches or order books.
- **Responses** flow back to the clients.

This picture shows every complication from this section at once. Several workers exploit parallel hardware. Requests arrive continuously and in any order. Some requests are independent and some are linked. The shared resources are where independent-looking work can collide. The rest of the chapter is about structuring code for exactly this kind of system.

### Too much dependency, or just enough?

Go back to the summing loop. Its sequential form says "each addition depends on the previous one." Is that really true? Addition is associative, so the array could be split in half, each half summed separately (perhaps on different cores), and the two partial sums added at the end. The *real* dependency is weaker than the one the loop expresses: only the final combination needs every partial result.

Standard C++ already lets you state this weaker dependency for this particular case:

**Listing 5** — Stating a weaker dependency with std::reduce *(illustrative example, not from the talk)*

```cpp
#include <execution>  // C++17 execution policies
#include <iostream>
#include <numeric>
#include <vector>

int main() {
    std::vector<int> prices{3, 1, 4, 1, 5, 9, 2, 6};

    // std::reduce may add elements in any order and grouping,
    // and std::execution::par allows it to split the work across threads.
    long total = std::reduce(std::execution::par,
                             prices.begin(), prices.end(), 0L);

    std::cout << total << '\n';  // same result as the sequential loop: 31
}

```

Compared with the loop:

- `std::reduce` does not promise left-to-right order. You are telling the library that the order of additions does not matter. (The plain loop cannot say this.)
- `std::execution::par` is a C++17 *execution policy* from the `<execution>` header. It permits the library to run pieces of the work in parallel. (Do not confuse it with the C++26 senders facilities, which share the `std::execution` namespace.) On some toolchains, such as GCC's libstdc++, the parallel policies need an extra backend like Intel TBB to actually run in parallel.
- The result is the same. What changed is the *description of dependencies*, and with it the freedom given to the implementation.

This leads straight into the next section's definition. Sequential code over-states dependencies by default. Concurrency is the practice of describing **which dependencies actually exist**, so that a framework can exploit the freedom the remaining ones leave.

## 3. Concurrency as Dependency Description: Non-blocking APIs, Callbacks and Futures

*Talk segment: [4:30](https://www.youtube.com/watch?v=ttoQfPP_Gkg&t=270s)*

### Concurrency is about dependencies

The previous section ended with a system that receives many requests at once and has many cores available to process them. Before looking at tools, it helps to settle what "concurrent code" actually *means*. The speaker offers a definition that frames the rest of the chapter:

> **Concurrency is about describing which dependencies actually exist** between instructions and between components. A good concurrency framework is one that lets you express those dependencies precisely, and then **exploits that knowledge as much as possible**.

Consider what a purely sequential program tells the machine. When you write two statements one after the other, you are implicitly saying "the second may depend on the first". Often that is true. Often it is not. For example:

- `auto a = load_user(id);`
- `auto b = load_prices();`
- `render(a, b);`

The third line needs both `a` and `b`. The second line does not need `a` at all. A sequential program still forces the second line to wait for the first, because the language has no way of knowing they are unrelated. A concurrent framework gives you a vocabulary for saying "these two are independent, and this third thing depends on both". With that information it can overlap the first two, for example by issuing both network requests at once.

So in this view, concurrency is not about threads or locks. It is a **description problem**: how do I tell the system what really depends on what? The rest of this section introduces the most basic building blocks for writing such descriptions.

### Building block 1: the non-blocking API

A **blocking** call does not return until its work is finished. When `read(file)` returns, the data is in your hands. A **non-blocking API** is the opposite:

> A **non-blocking API** is a function (or request) that **returns before its effect or result is complete**.

Calling it *starts* an operation, such as a disk read, a network send or a computation on another core, and hands control straight back to you while the operation is still in progress.

This has an important consequence, which the speaker stresses. Because the function returns early, **the instructions written after the call cannot assume the result is ready**. The usual chain of "each line may rely on the line before it" is deliberately broken at that point. That brings two things:

1. **A responsibility for you.** You must somehow express "this code needs the result" in a way other than "put it on the next line".
2. **Freedom for the framework.** The engine running your program may now run the following instructions, or entirely different work, *while the operation is still in flight*. This is where parallelism comes from.

If the next lines may not use the result, where *does* the code that needs it go? There are two classic answers: callbacks and futures. To compare them, we will use one tiny task throughout: **read a file, then print its contents**.

### The baseline: blocking code

Here is the sequential version. It is easy to read precisely because the dependency ("print needs the data") is expressed by line order.

**Listing 6** — Blocking baseline: read a file, then print it *(illustrative example, not from the talk)*

```cpp
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

// Blocking: does not return until the whole file has been read.
std::string read_file(const std::string& path) {
    std::ifstream in(path);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

int main() {
    auto data = read_file("input.txt"); // thread waits here for the disk
    std::cout << data;                 // safe: data is guaranteed ready
}

```

While `read_file` waits on the disk, the calling thread sits idle. If this were a server handling thousands of requests, that idle time would be wasted capacity. The rest of this chapter is about recovering that capacity without losing the readability you see here.

### Building block 2: completion callbacks (continuation-passing style)

The first way to express the dependency is to **hand the dependent code to the non-blocking function itself**. You pass an extra argument, usually a lambda, a function pointer or a `std::function`, and the operation calls it when the result is ready. This is a **completion callback**.

The code you pass has a name. Everything that should happen *after* an operation finishes is called the **continuation** of that operation, meaning "the rest of the computation". Passing the continuation explicitly as an argument, instead of writing it on the following lines, is called **continuation-passing style (CPS)**.

In pseudo-C++, the blocking pair

- `auto data = read(f);`
- `print(data);`

becomes

- `async_read(f, [](auto data) { print(data); });`

The lambda body is exactly the code that *truly depends* on the result. By putting it there, you tell the infrastructure precisely what the dependency is: "when the data arrives, continue with this". Anything you write *after* the `async_read(...)` statement is, by construction, code that does **not** depend on the read.

The example below makes this concrete with a toy single-threaded event loop. `async_read_file` does not read anything when called. It only queues the work and returns. The actual read and the callback run later, when the loop drains its queue. This is the essence of the reactor-style designs discussed later in the chapter.

**Listing 7** — Callback (CPS) version with a toy event loop, including the classic bug *(illustrative example, not from the talk)*

```cpp
#include <deque>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>

// A minimal single-threaded "event loop": a queue of pending tasks.
std::deque<std::function<void()>> pending;

// Non-blocking: schedules the read and returns immediately.
// 'on_done' is the continuation: the code that truly depends on the result.
void async_read_file(std::string path,
                     std::function<void(std::string)> on_done) {
    pending.push_back([path = std::move(path), on_done = std::move(on_done)] {
        std::ifstream in(path);
        std::ostringstream ss;
        ss << in.rdbuf();
        on_done(ss.str());   // invoke the continuation with the result
    });
}

void run_event_loop() {
    while (!pending.empty()) {
        auto task = std::move(pending.front());
        pending.pop_front();
        task();
    }
}

int main() {
    std::string data;

    async_read_file("input.txt", [&data](std::string contents) {
        data = std::move(contents);
        std::cout << data;          // correct: runs once the data exists
    });

    // BUG: this line runs before the read has happened.
    // 'data' is still empty here, so this prints 0.
    std::cout << "size right after the call: " << data.size() << '\n';

    run_event_loop();               // only now are the read and callback executed
}

```

Notice the line marked `BUG`. It is the most common mistake with non-blocking APIs: code written *after* the call that silently assumes the result is already there. Here `data` is still empty when that line runs, and the program would print a size of 0. Nothing in the type system stops you. The code even "looks" sequential. Later in the chapter, the asio echo-server example from the talk contains a comment, `/* code here is probably a bug */`, that marks exactly this spot.

> **Rule of thumb:** after a non-blocking call, the next line runs *concurrently with* the operation, not *after* it. Anything that needs the result belongs in the continuation.

### Building block 3: futures

The second way to express the dependency inverts the arrangement. The non-blocking call does not take your continuation. Instead it **returns a handle that stands for the not-yet-available result**. That handle is called a **future**: an identifier, or delegate, for a value that will exist later.

The speaker lists three things you can typically do with a future:

| Operation | Meaning | In standard C++ |
|---|---|---|
| **Poll** | Ask "is it ready yet?" without waiting | `fut.wait_for(0s) == std::future_status::ready` |
| **Attach work** | Chain further operations that depend on the result | Not in `std::future`. It existed as `.then()` in the Concurrency TS (`std::experimental::future`) and in libraries such as Boost.Thread |
| **Block** | Wait until the result is ready, then take it | `fut.get()` (or `fut.wait()`) |

In pseudo-C++ our running example becomes:

- `auto fut = std::async(read, f);`
- `... other, independent work ...`
- `print(fut.get());`

The dependency is now expressed by **where you call `get()`**. Everything between `std::async` and `get()` is declared independent of the read.

**Listing 8** — Future version: poll, then block with get() *(illustrative example, not from the talk)*

```cpp
#include <chrono>
#include <fstream>
#include <future>
#include <iostream>
#include <sstream>
#include <string>

std::string read_file(const std::string& path) {
    std::ifstream in(path);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

int main() {
    using namespace std::chrono_literals;

    // Non-blocking: returns a handle (future) to a result that does not exist yet.
    std::future<std::string> fut =
        std::async(std::launch::async, read_file, std::string("input.txt"));

    // Poll: do other, independent work until the result is ready.
    while (fut.wait_for(10ms) != std::future_status::ready) {
        std::cout << "doing other work while the file loads...\n";
    }

    // Block (here it returns at once, since we polled) and take the value.
    std::cout << fut.get();
}

```

A few observations:

- `std::launch::async` asks for the function to run as if on a new thread. Without it, the implementation may defer the work until `get()`, which removes the concurrency.
- The polling loop shows the "check whether it's done" capability. Real code would do useful work there rather than print a message.
- `fut.get()` *blocks*. Futures leave you free to wait, but every blocking wait gives up some of the parallelism the non-blocking call made possible.
- A subtle point for later: a `std::future` obtained from `std::async` **blocks in its destructor** until the task finishes. The task's lifetime is tied to the lifetime of the object. This early form of "a scope cannot end before its work completes" will come back in the structured-concurrency sections.

### Comparing the three styles

| | Blocking | Callback (CPS) | Future |
|---|---|---|---|
| How the dependency is expressed | Line order | The continuation passed as an argument | Where you poll, chain or `get()` the handle |
| Calling thread while the operation runs | Idle | Free immediately | Free until it blocks on `get()` |
| Reads like sequential code? | Yes | Partially ("squint and it's sequential") | Yes, up to the wait |
| Typical hazards | Wasted resources, poor scalability | Code after the call assuming the result; object lifetimes inside callbacks | Blocking too early; no standard `.then()` for chaining |

None of these is wrong. They are different ways of telling the system the same fact: *this* piece of code depends on *that* result, and nothing else does. The more precisely a framework lets you state that fact, the more freedom it has to schedule work in parallel, and the more of the program's correctness can be seen by reading the code. The next section adds more building blocks to the same vocabulary: forks, joins, synchronization primitives and cancellation.

## 4. Fork, Join, Synchronization Primitives and Cancellation

*Talk segment: [7:15](https://www.youtube.com/watch?v=ttoQfPP_Gkg&t=435s)*

The previous section introduced the first building block of concurrency: the *non-blocking API*, delivered either through a completion callback or through a future. This section covers the rest of the speaker's toolkit: **fork**, **join**, the classic **synchronization primitives**, and **cancellation**. Each one is a different way of telling the system which pieces of work depend on which, in keeping with Barkan's definition of concurrency.

### Fork: a fork in the road of execution

Barkan defines a **fork** as *a non-blocking API to run an arbitrary piece of code*. If you "fork" some function `f`, two things become true at once:

- the call returns right away, without waiting for `f` to finish;
- `f` may run independently of whatever instructions come after the fork.

The speaker stresses that he does not mean only the POSIX `fork()` system call, which clones a whole process. He means the more general image of a *fork in the road*: one path of execution splits into two, and the two may proceed side by side.

In standard C++ there are two everyday ways to fork:

- **`std::jthread`** (C++20) starts a new thread running a callable.
- **`std::async`** (C++11) runs a callable, possibly on a new thread, and hands you back a `std::future` for its result.

### A short primer on threads

Threads are the most familiar way to fork, so it helps to be precise about what one is.

An **OS thread** is an independent stream of execution that the operating system schedules. Each thread has:

- its own **stack**, which holds its local variables and the chain of function calls it is in the middle of;
- its own instruction pointer, meaning its own place in the code.

All threads in a process share the same heap and global memory. Two threads can therefore run at the same time on different cores, or take turns on one core. Because they can touch the same data, the order in which their instructions interleave becomes something you have to reason about.

Creating a thread object is a fork. The question that follows is what happens to the forked path when the object that started it goes away. That question leads to *join*.

### Join: where paths converge

A **join** means *do not continue until another operation is complete*. Several execution paths converge at a point because the code after that point depends on all of them having finished.

Together, fork and join form the basic shape of parallel work:

1. **Main path** runs until it reaches a fork.
2. **Fork:** the work splits into path A and path B, which may run in parallel.
3. **Join:** the main path waits until both A and B are done.
4. **Main path** continues, and may now safely use the results of A and B.

The join is the *dependency statement*. It says that step 4 needs step 2's outputs. Everything between the fork and the join is free to run in any order or in parallel. That freedom is exactly what the framework can exploit.

### `std::thread` versus `std::jthread`

Barkan picks out one property of `std::jthread`: there is "some connection between the lifetime of the jthread object and the lifetime of the executing thread itself". The forked path does not silently *detach* and wander off on its own. When the object dies, the thread has finished.

The older `std::thread` (C++11) makes no such promise. Compare their destructors:

| | `std::thread` (C++11) | `std::jthread` (C++20) |
|---|---|---|
| Destroyed while still running (joinable) | Calls `std::terminate`, which kills the program | Calls `request_stop()`, then `join()` |
| Responsibility for joining | Yours, on every code path, including exceptions | Automatic (RAII) |
| Built-in cancellation | None | Owns a `std::stop_source`; the thread function can receive a `std::stop_token` |

With `std::thread` you must call either `join()` or `detach()` before the object is destroyed. That is easy to forget, especially when an exception is thrown between creating the thread and joining it:

**Listing 9** — Pitfall: std::thread destroyed while joinable calls std::terminate *(illustrative example, not from the talk)*

```cpp
#include <iostream>
#include <stdexcept>
#include <thread>

void background_work() { /* ... */ }

void process(bool fail) {
    std::thread t(background_work);        // fork
    if (fail)
        throw std::runtime_error("oops");  // t is still joinable here...
    t.join();                              // ...so this join is skipped
}   // ~thread() on a joinable thread -> std::terminate()

int main() {
    try {
        process(true);
    } catch (const std::exception& e) {
        // Never reached: the program terminates during stack unwinding.
        std::cout << "caught: " << e.what() << '\n';
    }
}

```

Here `t` is still joinable when the exception unwinds the stack, so its destructor calls `std::terminate`. This is not what the exception handler intended, and it is precisely the lifetime problem that RAII solves. The same idea of tying a task's lifetime to an object's lifetime returns later in the chapter as the core of *structured concurrency*.

Here is the `std::jthread` version of a long-running worker. It also previews cancellation:

**Listing 10** — std::jthread with cooperative cancellation via stop_token *(illustrative example, not from the talk)*

```cpp
#include <chrono>
#include <iostream>
#include <stop_token>
#include <thread>

int main() {
    using namespace std::chrono_literals;

    std::jthread worker([](std::stop_token st) {
        int ticks = 0;
        while (!st.stop_requested()) {   // cooperative cancellation point
            ++ticks;
            std::this_thread::sleep_for(10ms);
        }
        std::cout << "worker stopped after " << ticks << " ticks\n";
    });

    std::this_thread::sleep_for(100ms);
    // No explicit join: ~jthread() calls request_stop(), then join().
}

```

Walking through it:

- The lambda's first parameter is a `std::stop_token`. When `std::jthread` sees a callable that can accept one, it passes in a token connected to its own internal `std::stop_source`.
- The loop condition `!st.stop_requested()` is how the worker cooperates with cancellation. Nothing forcibly kills the thread. The thread checks the flag and exits on its own.
- `main` never calls `join()`. When `worker` goes out of scope, its destructor first requests a stop, which flips the token, and then joins. The worker sees the request, leaves the loop, prints its message and returns, and only then does the destructor complete.

> Key insight: `std::jthread` turns "fork, and remember to join later" into "fork inside a scope; the join happens when the scope ends". The *shape* of the code now guarantees the dependency.

### `std::async` and futures

`std::async` combines a fork with the future from the previous section. You fork a computation, receive a `std::future`, and later call `.get()`, which is a join on that one result:

**Listing 11** — Fork/join with std::async and std::future *(illustrative example, not from the talk)*

```cpp
#include <future>
#include <iostream>
#include <numeric>
#include <vector>

long long sum(const std::vector<int>& v, std::size_t lo, std::size_t hi) {
    return std::accumulate(v.begin() + lo, v.begin() + hi, 0LL);
}

int main() {
    std::vector<int> data(1'000'000, 1);
    const std::size_t mid = data.size() / 2;

    // Fork: two independent computations (launch::async forces a real thread).
    auto left  = std::async(std::launch::async, sum, std::cref(data), 0, mid);
    auto right = std::async(std::launch::async, sum, std::cref(data), mid, data.size());

    // Join: the total depends on both halves.
    long long total = left.get() + right.get();
    std::cout << "total = " << total << '\n';
}

```

Two details are worth knowing:

- Pass `std::launch::async` explicitly. With the default policy the implementation may choose to *defer* the work, running it lazily inside `.get()` on the calling thread. In that case no fork actually happens.
- A future returned by `std::async` with the `async` policy **blocks in its destructor** until the task finishes. That gives it a built-in join, similar in spirit to `std::jthread`.

### Mutexes, semaphores and barriers: dependencies on resources

Barkan groups the classic synchronization primitives under the same definition. They are mechanisms for describing dependencies between pieces of code or pieces of data:

- A **mutex** (`std::mutex`) says that only one strand of execution may be inside this region at a time. The dependency it expresses is "wait until the other user has released the resource".
- A **semaphore** (`std::counting_semaphore`, C++20) generalizes this to *N* holders. It is often used to bound how many tasks may use a resource at once.
- A **barrier** (`std::barrier`, C++20) says that nobody proceeds past this point until all *N* participants have arrived. It is a reusable, many-way join.

All of them tell the system that one strand of execution cannot continue until something else completes or releases a resource. They are low-level, and you have to place them correctly by hand. The chapter returns to this point shortly with Sean Parent's guideline about raw synchronization primitives.

### Cancellation: "nobody depends on this any more"

The final building block, which the speaker notes "came to C++ relatively late in the game", is **cancellation**. Cancellation signals that a result you once thought you needed is *no longer necessary*. If the work producing it has not finished yet, it may stop early.

Cancellation is itself a dependency statement. Fork and join say "this later code depends on that work". Cancellation *removes* such a dependency: it says nobody depends on this work any more. A framework that knows this can stop spending CPU, memory or network bandwidth on the work.

C++20 standardized cancellation with three cooperating types in `<stop_token>`:

| Type | Role |
|---|---|
| `std::stop_source` | The "sender" of the signal; call `request_stop()` to cancel |
| `std::stop_token` | A read-only view handed to the work; query `stop_requested()` |
| `std::stop_callback` | Registers a function to run when a stop is requested (useful for waking blocked operations) |

Cancellation in C++ is **cooperative**. Requesting a stop only sets a flag and runs any registered callbacks. The cancelled work has to check its token and wind itself down. This is safer than killing a thread outright, because the work can release its resources properly on the way out.

The following example uses one shared `std::stop_source` for two parallel searches. Whichever search finds the target cancels the other. It previews the "race several paths, let the winner win and cancel the rest" pattern that the next section presents as a motivation for concurrency:

**Listing 12** — Shared stop_source: first search to succeed cancels the other *(illustrative example, not from the talk)*

```cpp
#include <atomic>
#include <cstddef>
#include <iostream>
#include <numeric>
#include <stop_token>
#include <thread>
#include <vector>

int main() {
    std::vector<int> data(10'000'000);
    std::iota(data.begin(), data.end(), 0);
    const int target = 7'654'321;

    std::stop_source cancel;                      // "nobody needs this any more"
    std::atomic<std::ptrdiff_t> found{-1};

    auto search = [&](std::size_t lo, std::size_t hi) {
        std::stop_token st = cancel.get_token();
        for (std::size_t i = lo; i < hi; ++i) {
            if (st.stop_requested()) return;      // someone else already won
            if (data[i] == target) {
                found = static_cast<std::ptrdiff_t>(i);
                cancel.request_stop();            // the other search is now unneeded
                return;
            }
        }
    };

    {
        std::stop_callback on_cancel(cancel.get_token(),
                                     [] { std::cout << "cancellation requested\n"; });
        std::jthread a(search, std::size_t{0}, data.size() / 2);
        std::jthread b(search, data.size() / 2, data.size());
    }   // join point: both threads have finished here

    std::cout << "found at index " << found.load() << '\n';
}

```

Things to notice:

- Both searchers get tokens from the *same* `stop_source`, so one `request_stop()` reaches both of them.
- The winner calls `request_stop()`, which declares that the other half of the search is no longer needed.
- The `std::stop_callback` runs once, on the thread that requested the stop. Real frameworks use such callbacks to cancel pending I/O, for example.
- The inner braces form a scope. Leaving it joins both `std::jthread`s, so by the time we print `found`, every forked path has converged.

### Putting it together: the task dependency graph

The speaker closes this part with a picture: a **task dependency graph**. Each node is a task, such as "Task 1" through "Task 5" inside "Process 1" on his slide, with further processes alongside. Each edge reads "this must finish before that may start". In this picture:

- a **fork** is a node with several outgoing edges;
- a **join** is a node with several incoming edges;
- a **mutex or semaphore** adds edges that serialize access to a shared resource;
- **cancellation** prunes a subgraph whose results nobody needs any more.

In this view, writing concurrent code means drawing this graph, and the more accurately you draw it, the more freedom the framework has to run things in parallel.

(A side note for viewers of the recording: around this point a brief editing glitch shows the "No Raw Synchronization Primitives" slide too early. The speaker notices and defers it. That slide belongs to the correctness discussion later in the chapter.)

## 5. Motivation: What's the Point of Concurrency?

*Talk segment: [10:27](https://www.youtube.com/watch?v=ttoQfPP_Gkg&t=627s)*

Previous sections defined concurrency as the practice of *describing dependencies*. They also introduced the building blocks for doing so: non-blocking calls, callbacks, futures, fork, join, synchronization primitives and cancellation. This section asks a simpler question.

Why go to all this trouble? A plain sequential program, one instruction after another, is much easier to write and to reason about. The speaker gives four motivations for writing a system concurrently anyway. They recur throughout the rest of the chapter, because a good concurrency framework is judged by how well it delivers them.

### Motivation 1: Exploiting Parallelism

The most obvious reason is **parallelism**: a modern machine contains many things that can work at the same time.

- **Processors**: multiple cores, multiple sockets, sometimes GPUs or other accelerators.
- **Peripherals**: a disk controller can fetch data while the CPU computes something else.
- **The network**: a request to a remote server is "in flight" for a while, and the local machine has nothing to do for it during that time.

A strictly sequential program wastes nearly all of this. When it waits for the disk, the cores sit idle. When it computes, the network card sits idle.

The speaker ties this back to his definition of concurrency. *The more you tell the framework about the actual dependencies in your program, the more efficiently it can use the resources available to it.* Suppose the framework knows that computing a price does not depend on the log write that was just issued. It can then let both happen at once. If you never say so, it has to assume the worst and serialize them.

> **Key insight:** Parallelism is not something you add to a program. It is something the framework is *allowed* to exploit once you stop over-stating your dependencies.

### Motivation 2: Separation of Concerns

The second motivation is about people and code organization rather than hardware. The speaker identifies two distinct kinds of separation.

#### Between independent components

Suppose two subsystems can run in parallel and do not depend on each other. For example, one handles market data and another handles user logins. Then they can be **developed, tested and debugged separately**. A concurrent architecture makes this independence explicit. Each component is its own flow of work, and the system composes them.

In a sequential design, the two would be interleaved by hand inside one big loop. Each would be entangled with the other's timing.

#### Between the framework and the applications

The second kind of separation is more subtle. A concurrent system has to answer questions such as:

- Which pending task runs next? (**scheduling**)
- Should some tasks be preferred over others? (**priorities**)
- How do we ensure that no flow is starved forever? (**fairness**)

These questions are about *how* work runs, not *what* the work computes. In a well-structured concurrent system, they can be handled by the framework, possibly by **a dedicated team**. Other teams write the application logic on top of it, without hard-coding scheduling decisions into business code.

This mirrors a familiar idea from the standard library. Algorithms are written against iterators and know nothing about the container. In the same way, application tasks should be written against an execution framework and know little about the scheduling policy.

### Motivation 3: Tail-Latency Hiding

This motivation is especially important in the speaker's field, high-frequency trading.

#### What is tail latency?

If you measure the response time of a service many times, you get a *distribution*, not a single number. Most requests are fast, but a few are much slower. A garbage-collection pause on the server, a dropped packet that must be retransmitted, or a cache miss on a cold page can all cause this. These rare slow cases form the **tail** of the distribution.

**Tail latency** is usually reported as a high percentile:

| Metric | Meaning |
|---|---|
| p50 (median) | half of requests are faster than this |
| p99 | 99% of requests are faster; 1 in 100 is slower |
| p99.9 | only 1 in 1000 requests is slower |

Optimizing the average does not help much with p99. The slow cases are caused by rare events, not by the normal code path. Yet in many systems the tail is what matters. A trader who is slow 1% of the time loses exactly the trades that mattered. And a web page that issues 100 backend requests will almost always hit at least one p99 event.

#### Racing: let the winner win

The speaker's technique is to **race** several paths towards the same goal:

1. Start two or more independent attempts to obtain the same result.
2. Whichever finishes first provides the answer.
3. Cancel all the others.

A concrete example is querying **two mirrored servers** with the same request and taking whichever answers first. Assume each replica independently has a 1% chance of a slow response. Then the chance that *both* are slow is about 1% × 1% = 0.01%. The race turns the p99 of a single replica into roughly the p99.99 of the pair. You pay with extra load, since each request is sent twice, and you buy a much shorter tail.

The slide illustrates this with several contenders racing towards a finish line: a disk access, two network requests and a database query. As soon as one crosses the line, the rest are called off. The speaker notes that this pattern is also known as a **when-any** operation, because the combined operation completes when *any* of its children completes. It is the counterpart of **when-all**, the join from the previous section, which waits for *every* child.

#### Racing requires cancellation

Step 3 is where things get interesting, and it links directly back to cancellation from the previous section. A race without cancellation is wasteful at best and dangerous at worst:

- The losers keep consuming CPU, sockets and memory for results nobody wants.
- If a loser eventually completes and writes into a buffer that the winner's code has already reused or destroyed, you have a lifetime bug.

To see how much machinery even a simple race needs, here is a thread-based version written with only C++20 standard tools. This is an illustrative example, not code from the talk:

**Listing 13** — Racing two mirrored replicas with std::jthread and std::stop_source (C++20) *(illustrative example, not from the talk)*

```cpp
#include <chrono>
#include <iostream>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>

using namespace std::chrono_literals;

// Simulated remote query: takes `delay` unless cancelled first.
std::optional<std::string> query_replica(std::stop_token st, std::string name,
                                         std::chrono::milliseconds delay) {
    auto deadline = std::chrono::steady_clock::now() + delay;
    while (std::chrono::steady_clock::now() < deadline) {
        if (st.stop_requested()) return std::nullopt;  // someone else won
        std::this_thread::sleep_for(1ms);
    }
    return "answer from " + name;
}

int main() {
    std::stop_source race;                 // shared "cancel the losers" signal
    std::mutex m;
    std::optional<std::string> winner;

    auto contender = [&](std::string name, std::chrono::milliseconds delay) {
        if (auto r = query_replica(race.get_token(), std::move(name), delay)) {
            std::scoped_lock lock(m);
            if (!winner) {
                winner = std::move(r);
                race.request_stop();       // cancel everyone else
            }
        }
    };

    {
        std::jthread a(contender, "replica-A", 120ms);  // slow replica (tail event)
        std::jthread b(contender, "replica-B", 15ms);   // fast replica
    }   // both threads are joined here, so `winner` is safe to read

    std::cout << winner.value_or("no answer") << '\n';  // prints: answer from replica-B
}

```

Walking through it:

- `query_replica` simulates a remote call. Crucially, it **polls a `std::stop_token`** so that it can give up early. Without that check, cancellation would be impossible: the loser would run to completion no matter what.
- A single `std::stop_source` named `race` is shared by both contenders. Calling `request_stop()` on it signals every token obtained from it.
- Each contender, if it finishes, locks a mutex, checks whether someone already won, and if not records its answer and cancels the others.
- The inner `{ ... }` block owns both `std::jthread`s. Its closing brace joins them, so `winner` is safe to read afterwards.

It works, but look at what we had to coordinate by hand: a stop source, a mutex, an "is there already a winner?" check, and careful scoping so nothing outlives the data it touches. The next section argues that such raw synchronization is easy to get wrong. Structured-concurrency libraries instead offer `when_any` as a ready-made, composable building block. For comparison, here is the shape of the same race using NVIDIA's stdexec library. `exec::when_any` lives in its `exec::` extension namespace and is **not** part of the C++26 standard:

**Listing 14** — The same race with stdexec's exec::when_any (library extension, not standard C++26) *(illustrative example, not from the talk)*

```cpp
#include <stdexec/execution.hpp>
#include <exec/static_thread_pool.hpp>
#include <exec/when_any.hpp>   // NVIDIA stdexec extension, namespace exec::

#include <iostream>
#include <string>

// Stand-ins for real asynchronous queries to two mirrored servers.
auto query(stdexec::scheduler auto sched, std::string name) {
    return stdexec::schedule(sched)
         | stdexec::then([name] { return "answer from " + name; });
}

int main() {
    exec::static_thread_pool pool{2};
    auto sched = pool.get_scheduler();

    // when_any starts both children, completes with the first result,
    // requests stop on the other one, and waits for it to finish.
    auto race = exec::when_any(query(sched, "replica-A"),
                               query(sched, "replica-B"));

    auto [answer] = stdexec::sync_wait(std::move(race)).value();
    std::cout << answer << '\n';
}

```

Here the adaptor itself starts both children, forwards the first result and requests stop on the loser. It also does not complete until the loser has actually finished, so nothing outlives the operation. The application code only states *what* is being raced.

### Motivation 4: Efficient Batching and Chunking

The last motivation sounds almost the opposite of parallelism. Sometimes it is more efficient to **gather several independent requests and process them together**, possibly in a different order than they arrived. This matters most **when overall load is high**. With only one pending request there is nothing to batch. With hundreds, the system can choose a smart order.

The principle is the same as before. A system can only reorder operations if it *knows* they don't depend on each other. A sequential program that executes requests strictly in arrival order has hidden that freedom from the system.

#### The elevator algorithm

The speaker's example is the **elevator algorithm** (also called **SCAN**), classically used to schedule requests to a spinning hard disk. Moving the disk's read/write head across tracks is slow, so the total head movement dominates throughput.

- **First-come, first-served** makes the head zig-zag back and forth across the disk as requests arrive in random positions.
- **SCAN** behaves like an elevator in a building. It keeps moving in one direction, serving every request it passes, then reverses and serves the rest on the way back.

Each individual request may wait a little longer or shorter than under FIFO, but the *total* work drops dramatically:

**Listing 15** — Elevator (SCAN) ordering versus first-come-first-served *(illustrative example, not from the talk)*

```cpp
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <iterator>
#include <vector>

// Serve every request above the head while moving up, then the rest moving down.
std::vector<int> scan_order(int head, std::vector<int> requests) {
    std::ranges::sort(requests);
    auto split = std::ranges::lower_bound(requests, head);
    std::vector<int> order(split, requests.end());                 // upward sweep
    order.insert(order.end(), std::make_reverse_iterator(split),   // downward sweep
                 requests.rend());
    return order;
}

int total_travel(int head, const std::vector<int>& order) {
    int travel = 0;
    for (int track : order) {
        travel += std::abs(track - head);
        head = track;
    }
    return travel;
}

int main() {
    const int head = 50;
    const std::vector<int> arrivals{95, 10, 60, 20, 90, 55};  // arrival order

    std::cout << "FIFO travel: " << total_travel(head, arrivals) << '\n';  // 325
    std::cout << "SCAN travel: "
              << total_travel(head, scan_order(head, arrivals)) << '\n';   // 130
}

```

With the head at track 50 and the requests shown, serving in arrival order moves the head 325 tracks. The elevator order moves it only 130. That is the same set of requests with less than half the mechanical work, achieved purely by reordering.

The same idea appears in many places: network stacks coalescing small packets, databases grouping commits into one disk flush, and trading systems collecting several order updates into one message. In every case, the framework needs to know which operations are independent before it can batch or reorder them.

### Putting the Motivations Together

| Motivation | What concurrency buys | What the framework must know |
|---|---|---|
| Exploit parallelism | Use cores, devices and network simultaneously | Which operations are truly independent |
| Separation of concerns | Independent teams/components; scheduling policy decoupled from logic | Clear boundaries between flows |
| Tail-latency hiding | Shorter p99 by racing redundant paths | How to cancel the losers safely |
| Batching / chunking | Higher throughput under load via reordering | Which requests may be grouped or reordered |

All four benefits depend on the same thing: an accurate, explicit description of dependencies, plus reliable cancellation. They also come at a cost. Once operations may run in any order, at any time, the number of possible behaviours explodes. That correctness challenge is the subject of the next section.

## 6. The Correctness Challenge and 'No Raw Synchronization Primitives'

*Talk segment: [13:28](https://www.youtube.com/watch?v=ttoQfPP_Gkg&t=808s)*

The previous section listed the rewards of concurrency: parallelism, separation of concerns, tail-latency hiding and batching. This section covers the cost. The speaker puts it plainly: *the challenge is correctness*. Writing a concurrent system without bugs is hard, and the usual tools for finding bugs work poorly here. Before we look at any technique (threads, reactors, coroutines, senders), we need to understand why concurrency is so error-prone. Only then can we judge a technique by how well it avoids those errors.

### Why testing cannot save us

In a sequential program, a given input produces one sequence of executed instructions. Test that input once and you have tested that path.

A concurrent program adds a second dimension. Besides *which* inputs arrive, it also matters *in what order* they arrive and *how the steps of different tasks interleave* in time. An **interleaving** is one particular merging of the steps of several concurrent activities into a single timeline. The speaker calls this an "explosion" in the number of possible situations. The word is accurate, as a classic example shows.

**Listing 16** — A classic data race: two threads incrementing a shared int *(illustrative example, not from the talk)*

```cpp
#include <iostream>
#include <thread>

int main() {
    int counter = 0;
    {
        std::jthread a([&] { for (int i = 0; i < 1'000'000; ++i) ++counter; });
        std::jthread b([&] { for (int i = 0; i < 1'000'000; ++i) ++counter; });
    } // both jthreads join here

    // Data race on a non-atomic int: undefined behaviour.
    // In practice this frequently prints something less than 2000000.
    std::cout << counter << '\n';
}

```

This is our own illustration, not code from the talk. Two `std::jthread`s each increment the same `int` a million times. The inner braces matter: when the scope closes, both `jthread` destructors *join*, so the threads have finished before we print. The bug is the expression `++counter`. It looks like one step, but the hardware performs it as three: **load** the value, **add** one, **store** it back.

Take just *one* increment per thread. Thread A performs L1, I1, S1 and thread B performs L2, I2, S2. The number of ways to interleave two sequences of three steps is the binomial coefficient C(6,3) = 20. In only 2 of those 20 orderings does the result come out right: the ones where one thread finishes completely before the other starts. In the other 18, both threads load the same old value and one update is lost.

In practice the bug still shows up rarely, because the window between load and store is tiny. That is exactly the problem: a bug that appears in some interleavings, some of the time, under some load. (Strictly speaking, an unsynchronized data race on a non-atomic object is *undefined behaviour* in C++. The compiler may do even stranger things than lose updates.)

Now scale it up. The number of interleavings of *n* tasks with *k* steps each is (nk)! / (k!)ⁿ:

| Tasks | Steps per task | Possible interleavings |
|---|---|---|
| 2 | 3 | 20 |
| 2 | 10 | 184,756 |
| 3 | 10 | about 5.5 trillion |

A real server has thousands of requests and millions of steps. On top of that, the order in which requests *arrive* is itself unpredictable. The speaker's point follows directly: test coverage of this space is "very, very hard, potentially even impossible". Statistics will not reach the rare buggy corners. Neither will an AI asked to generate a large test suite. Tests stay useful, but they cannot be the main argument for correctness.

### The tempting fix: synchronize everything

The obvious reaction is to protect every shared piece of data with a synchronization primitive: a mutex, a barrier, a "synchronized value" wrapper.

**Listing 17** — Fixing the race with a mutex - correct, but serialized *(illustrative example, not from the talk)*

```cpp
#include <iostream>
#include <mutex>
#include <thread>

int main() {
    int counter = 0;
    std::mutex m;

    auto work = [&] {
        for (int i = 0; i < 1'000'000; ++i) {
            std::lock_guard lock(m); // lock on construction, unlock on destruction
            ++counter;               // only one thread can be here at a time
        }
    };

    {
        std::jthread a(work);
        std::jthread b(work);
    }

    std::cout << counter << '\n'; // always 2000000, but likely slower than one thread
}

```

Now every increment happens while holding `m`. `std::lock_guard` is RAII: it locks in its constructor and unlocks in its destructor, so we cannot forget to unlock. The result is always 2,000,000. Look at what we paid for it, though:

- **Serialization.** Only one thread can be inside the critical section at a time, so the two threads take turns. We now do the same work as a single thread *plus* the cost of acquiring and releasing the lock two million times *plus* the cost of the cache line holding `m` and `counter` bouncing between cores. This version is very likely *slower* than a plain sequential loop.
- **Contention grows with scale.** Add more threads and they spend more time waiting for each other.

The speaker describes this as a delicate trade-off. Systematic synchronization can make a system correct, but "they might defeat the entire performance gain that we were looking for." You can convince yourself there are no bugs and then find you have built an expensive single-threaded program.

Locks are also **easy to get wrong**, which is the speaker's second objection. Common failure modes:

- **Forgetting a lock.** One access path in a large codebase touches the data without the mutex, and you are back to Listing 16.
- **Deadlock.** Thread A locks `m1` and then waits for `m2`, while thread B locks `m2` and then waits for `m1`. Neither can proceed. Avoiding this means every author has to follow a global lock-ordering rule that no compiler checks.
- **Wrong granularity.** Lock too coarsely and you lose parallelism. Lock too finely and you can observe inconsistent intermediate states between two separately-locked operations.
- **Subtle atomics.** Replacing the mutex with `std::atomic` and relaxed memory orderings can be fast. It is also one of the hardest areas of C++ to reason about.

None of these bugs is visible in the line of code where it happens. Each depends on what *other* code, possibly in another file written by another team, does at the same time.

### "No Raw Synchronization Primitives"

At this point the speaker plays a short clip from Sean Parent's influential talk, which he dates to 2013 (*C++ Seasoning*, where Parent set out goals such as "no raw loops"). One of Parent's guidelines is **No Raw Synchronization Primitives**.

Parent lists what he means by raw synchronization primitives:

- mutexes,
- atomics,
- semaphores,
- memory fences.

He also gives the reason, in a single line: **"Because you'll likely get it wrong."**

The guideline does not say these primitives are evil or that nobody should implement them. Libraries need them internally. It says *application code* should not use them directly, the same way "no raw loops" means using a named algorithm instead of writing a hand-rolled `for` loop. The primitives should sit inside well-tested abstractions that express *intent*, such as "run these tasks and combine their results". They should not be scattered through business logic.

The speaker notes that 13 years later, ideas from that talk and elsewhere in the industry have matured. We can now realistically move away from raw primitives "into something better". The rest of the chapter is about what that something is.

Here is a taste of the idea, applied to our counter:

**Listing 18** — No shared mutable state: local work, combined after the join *(illustrative example, not from the talk)*

```cpp
#include <array>
#include <functional>
#include <iostream>
#include <thread>

int main() {
    std::array<long, 2> partial{};

    auto work = [](long& out) {
        long local = 0;                     // private to this thread
        for (int i = 0; i < 1'000'000; ++i) ++local;
        out = local;                        // single write to this thread's own slot
    };

    {
        std::jthread a(work, std::ref(partial[0]));
        std::jthread b(work, std::ref(partial[1]));
    } // the join here is the only synchronization we need

    std::cout << partial[0] + partial[1] << '\n'; // always 2000000
}

```

Each thread counts into its own *local* variable and writes its result exactly once into its own slot. No data is shared while the threads run, so there is nothing to race on and no mutex is needed. The only synchronization is the **join** at the closing brace, which guarantees that both writes are visible before we add the partial results. That join is done for us by `std::jthread`'s destructor. The correctness argument fits in one sentence: *the threads touch disjoint data, and the scope waits for them before the results are read.* There are no raw primitives anywhere in the code. This shape (spawn children inside a scope, let them work independently, combine their results when the scope ends) is the seed of structured concurrency, covered in later sections.

### The goal: correct by construction

The speaker's target is code that is **correct by construction**. You can *look* at it, review it, and know there are no concurrency bugs, because of how it is built and not because a test happened to pass.

He ties this to code review. Anyone who has reviewed a massive, complex change knows that "just by looking" verification becomes impossible beyond a certain size and tangledness. The way back is to work in an organized fashion: adopt a few guidelines and stick to them. In his words, *simple rules and simple design mean easy code reviews, regardless of who writes the code and who does the review.* He explicitly includes code written by AI agents. As more code is generated, the reviewer's ability to check it quickly and reliably becomes the bottleneck. Structural rules that make bugs visible (or impossible) help a human reviewer and an automated one alike.

### The analogy: value semantics and RAII

The speaker points out that C++ has solved a problem like this before. Object lifetimes and resource management were once a main source of bugs: leaks, double frees, dangling pointers. Testing could not reliably find them either, because they appeared only on rare paths such as early returns and exceptions.

The fix was not "be more careful". It was a *discipline* enforced by structure:

**Listing 19** — The RAII analogy: making leaks structurally impossible *(illustrative example, not from the talk)*

```cpp
#include <memory>
#include <stdexcept>

struct Widget {
    void use() { /* may throw */ }
};

bool should_stop_early();

void legacy_style() {
    Widget* w = new Widget;
    if (should_stop_early()) return; // leak: forgot to delete on this path
    w->use();                        // leak if this throws
    delete w;
}

void modern_style() {
    auto w = std::make_unique<Widget>();
    if (should_stop_early()) return; // fine: destructor frees the Widget
    w->use();                        // fine even if this throws
}                                    // freed here on the normal path

bool should_stop_early() { return false; }

int main() {
    modern_style();
}

```

In `legacy_style`, correctness depends on the programmer remembering a `delete` on every exit path, including the hidden one an exception creates. In `modern_style`, nobody has to remember anything. The `std::unique_ptr` owns the object, and its destructor runs when the scope ends, however it ends. A leak is not merely unlikely here. It is *structurally impossible*. A reviewer can verify the function without tracing every path.

The speaker's claim is that concurrency needs the same thing:

| Object lifetimes | Concurrent tasks |
|---|---|
| Raw `new` / `delete` | Raw threads, mutexes, atomics, detached work |
| "Remember to delete on every path" | "Remember to lock, to join, to not touch data after handing it off" |
| RAII and value semantics | Structured mechanism for tasks (coming up: structured concurrency) |
| Leaks impossible by construction | Races and dangling tasks impossible by construction |

> **Key insight:** If code follows value semantics and RAII correctly, the chance of memory corruption drops sharply, and nobody has to be heroically careful for that to happen. The aim is a similarly structured discipline for concurrent code, so that correctness follows from the shape of the program rather than from exhaustive testing.

### Composability and local reasoning

The speaker names two properties that the discipline should provide.

- **Local reasoning** means you can verify that a piece of code is correct by reading *only that piece*, plus the documented contracts of what it calls, without reading the rest of the program. `modern_style` above allows local reasoning about memory. The racy counter in Listing 16 does not allow local reasoning about `counter`, because its correctness depends on every other thread that might touch it.
- **Composability** means correct pieces can be combined into larger pieces that are still correct, and the combination rules are simple enough that the whole can also be reasoned about locally.

Together, they let you break a large, complex problem into parts, convince yourself each part is right on its own, and trust that putting them together does not create new bugs between them. This is how the exploding interleaving space from the beginning of the section is tamed. You do not enumerate the trillions of orderings. You arrange the program so that most of them *cannot interact*.

With this yardstick, the following sections survey concrete techniques (OS threads, virtual threads, reactor thread pools, state machines and callbacks) and ask of each one how well it supports local reasoning and composition.

## 7. Concurrency Techniques: OS Threads and Virtual Threads

*Talk segment: [17:43](https://www.youtube.com/watch?v=ttoQfPP_Gkg&t=1063s)*

With the correctness challenge in mind, the talk turns to the practical question: *how* do real systems achieve concurrency? Barkan groups the techniques into a few broad categories. This section covers the first two, **operating system threads** and **virtual threads**. They are closely related, and seeing exactly where they differ prepares the ground for the techniques that follow: reactor thread pools, state machines, callbacks and, eventually, C++20 coroutines and C++26 senders.

### Category 1: Operating System Threads

The most familiar approach, and the one Barkan associates with the early 2000s, is to let the **operating system** do the concurrency for you. You create threads (or whole processes). Inside each one you write ordinary, *blocking*, sequential code: read from a socket, wait for the result, process it, write a reply.

This works because of what happens when a thread blocks. Suppose your thread calls `read()` on a socket that has no data yet. The CPU does not sit idle. The OS notices that the thread cannot make progress, saves its state, and lets some other thread or process use the CPU. This is called a **context switch**. When the data arrives, the OS marks your thread as runnable again and eventually switches back to it. From your code's point of view, `read()` simply took a while to return.

The OS is responsible for four things:

- **Scheduling**: deciding which runnable thread gets which core, and for how long.
- **Preemption**: forcibly interrupting a thread that has run long enough, even if it never blocks, so others get a turn.
- **Context switches**: saving one thread's state and restoring another's.
- **Hardware interaction**: waiting on devices, network cards and timers, and waking the right thread when they are ready.

#### Preemptive scheduling

OS threads are **preemptively** scheduled. A thread never has to agree to give up the CPU. A timer interrupt can stop it between any two machine instructions. This is very convenient, because a long-running loop cannot starve the rest of the system. It is also the root of the interleaving explosion described in the previous section: because a switch can happen *anywhere*, shared data can be observed in any intermediate state, and you need synchronization primitives to protect it.

#### What a context switch actually costs

The speaker lists "high overhead" as the first drawback. It helps to know where that overhead comes from:

1. **Kernel entry and exit.** Blocking calls and timer interrupts move the CPU from user mode into kernel mode and back. Each transition costs time, and security mitigations on modern CPUs make it more expensive.
2. **Saving and restoring registers.** The general-purpose registers, the stack pointer, the instruction pointer and often the large floating-point/SIMD register file must be stored and reloaded.
3. **Scheduler bookkeeping.** The kernel must update run queues and choose the next thread.
4. **Cache and TLB effects.** This is often the largest cost. The new thread touches different memory, so it evicts the previous thread's data from the CPU caches. When the old thread resumes, it starts "cold". Switching between processes, rather than threads within one process, can also disturb the TLB (the cache of virtual-to-physical address translations).

The direct cost of a context switch is typically in the order of microseconds. The indirect cache cost can be larger and harder to measure. For a high-frequency trading system, where Barkan works and where whole reactions are measured in microseconds, that cost is significant.

**Listing 20** — Thread-per-request with blocking code (the OS does the concurrency) *(illustrative example, not from the talk)*

```cpp
#include <chrono>
#include <iostream>
#include <string>
#include <syncstream>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

// Simulates a blocking I/O call, e.g. a socket read.
// While this thread sleeps, the OS context-switches to other threads.
std::string blocking_fetch(int id) {
    std::this_thread::sleep_for(200ms);
    return "response " + std::to_string(id);
}

void handle_request(int id) {
    // Plain sequential code: each line may rely on the previous one.
    std::string data = blocking_fetch(id);
    std::osyncstream(std::cout) << "request " << id << " -> " << data << '\n';
}

int main() {
    auto start = std::chrono::steady_clock::now();
    {
        std::vector<std::jthread> workers;
        for (int id = 0; id < 3; ++id)
            workers.emplace_back(handle_request, id);  // fork
    }   // jthread destructors join here
    auto elapsed = std::chrono::steady_clock::now() - start;
    // About 200ms in total, not 600ms: the OS overlapped the waits.
    std::cout << "elapsed: "
              << std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count()
              << "ms\n";
}
```

This example is the classic "thread per request" design written with modern C++. Every function in it is blocking and reads top to bottom. The `std::jthread` destructors join automatically, which ties back to the earlier section on fork and join. The concurrency itself is invisible in the code: the OS overlaps the three waits.

#### Pros and cons

Barkan's slide summarizes OS threads as "Simple. Supported since C++11" versus "High overhead. Little control. Not scalable." In more detail:

| Pros | Cons |
|---|---|
| Very simple to write: plain blocking, sequential code | High overhead: each blocking call can trigger an expensive context switch |
| Standard since C++11 (`std::thread`, `std::mutex`, ...), with `std::jthread` since C++20 | The number of threads is bounded and relatively small. Each needs kernel resources and its own stack, so tens of thousands of connections means tens of thousands of threads |
| The heavy lifting is done by the OS, which is written by experts | Little control: the application cannot easily dictate how its threads are prioritized against each other or against other processes |
| | The OS optimizes for the *generic* case, not for your workload |

> **Key insight:** OS threads let you *write* sequential code while the OS *runs* it concurrently. You pay for this convenience with heavyweight switches that you do not control, made by a scheduler that knows nothing about your application.

The speaker found this approach tempting precisely because "someone else who's really expert" does the hard work. The cost is that the expert is optimizing for everybody, not for you.

### Category 2: Virtual Threads

The second approach keeps the *programming model* of threads but moves the scheduling *into the application*. It goes by many names, which can be confusing:

- **virtual threads** (the term Java adopted),
- **green threads**,
- **user(-space) threads**,
- **stackful coroutines**,
- **fibers**.

They all describe the same idea. The application maintains several execution contexts. Each looks like a thread and has **its own call stack**, but they all run inside the same process, usually on one or a few OS threads. A scheduler inside the framework switches between them. In the speaker's words, "I can control my scheduler and I can basically switch between multiple stacks."

#### Cooperative scheduling

Virtual threads are usually **cooperatively** scheduled. A virtual thread runs until it *voluntarily* gives up control. It does this either by calling something like `yield()`, or, more commonly, by calling a framework I/O function that would block. Instead of blocking the OS thread, that function parks the current virtual thread and switches to another one that is ready.

This changes the trade-offs:

- **Switches are cheap.** Switching only requires saving a handful of callee-saved registers and swapping the stack pointer, entirely in user space. There is no kernel entry and no scheduler run queue.
- **Switches happen at known points.** Between two yields, a virtual thread cannot be interrupted by another virtual thread on the same OS thread. That makes reasoning easier.
- **Misbehaving tasks hurt everyone.** A virtual thread that computes for a long time without yielding starves all the others on its OS thread, because nothing preempts it. A virtual thread that accidentally calls a *real* blocking OS function blocks the entire carrier thread.

#### Stackful versus stackless

The name **stackful coroutine** points at the most important technical property. It also sets up a distinction that matters a lot for the rest of this chapter.

- A **stackful** coroutine (fiber) owns a complete call stack. It can suspend **from any depth**. If `handle_request()` calls `parse()`, which calls `read_more()`, which decides to wait for I/O, the whole chain of frames is frozen on that fiber's stack and resumed later. None of the intermediate functions needs to know that it is running on a fiber. They are ordinary functions.
- A **stackless** coroutine saves only **its own frame**, meaning its local variables and the point where it stopped. It can suspend only at explicit points written in its own body (`co_await`, `co_yield`). It cannot suspend from inside an ordinary function it called. If a callee needs to suspend, that callee must itself be a coroutine, and the caller must `co_await` it.

C++ has no standard fibers. Libraries do provide them, for example **Boost.Context**, which offers low-level stack switching, and **Boost.Fiber**, which builds a scheduler, mutexes and channels on top of it. The talk does not name these libraries. They are mentioned here as background to show what stackful code looks like.

**Listing 21** — Stackful: a fiber suspends from inside an ordinary helper (Boost.Fiber, not the talk's code) *(illustrative example, not from the talk)*

```cpp
// Requires Boost.Fiber (a library, not standard C++); link with boost_fiber and boost_context.
#include <boost/fiber/all.hpp>
#include <iostream>

// An ordinary function: not a coroutine, no special return type.
void step(const char* name, int i) {
    std::cout << name << i << '\n';
    boost::this_fiber::yield();  // suspends the WHOLE fiber, including its caller
}

void worker(const char* name) {
    for (int i = 0; i < 3; ++i)
        step(name, i);           // suspension happens one call level deeper
}

int main() {
    boost::fibers::fiber a(worker, "A");
    boost::fibers::fiber b(worker, "B");
    a.join();   // main's fiber waits; the scheduler runs a and b cooperatively
    b.join();
    // Output interleaves A and B because each yield hands control to the other fiber.
}
```

The important line is inside `step()`. An ordinary function, called from within a fiber, calls `boost::this_fiber::yield()` and suspends the *entire* fiber, including the `for` loop of its caller. That is the stackful property.

Compare this with C++'s built-in, **stackless** coroutines:

**Listing 22** — Stackless: only the coroutine body can suspend (C++23 std::generator) *(illustrative example, not from the talk)*

```cpp
#include <generator>   // C++23
#include <print>       // C++23
#include <ranges>

// To produce values from a helper, the helper must itself be a coroutine.
std::generator<int> inner(int base) {
    co_yield base;
    co_yield base + 1;
}

// Not allowed: an ordinary function cannot suspend its caller.
// void helper() { co_yield 42; }  // error: this would turn helper into a coroutine
//                                  // with an invalid return type

std::generator<int> outer() {
    co_yield 0;
    co_yield std::ranges::elements_of(inner(10));  // explicit hand-off to the nested coroutine
    co_yield 99;
}

int main() {
    for (int v : outer())
        std::println("{}", v);   // prints 0 10 11 99
}
```

Here `co_yield` may appear only in the body of a coroutine. To produce values from a helper, the helper must itself be a coroutine (`inner` returns a `std::generator`), and the caller must explicitly hand control to it. The ability to suspend spreads to every function on the path. This is the **function coloring** problem, which the migration section returns to: converting one function into a coroutine forces its callers to change as well.

| | Stackful (fibers, virtual threads) | Stackless (C++20 coroutines) |
|---|---|---|
| What is saved on suspend | The whole call stack | Only the coroutine's own frame |
| Where it can suspend | Anywhere, at any call depth | Only at `co_await` / `co_yield` in its own body |
| Memory per task | A full stack, sized up front (often kilobytes or more) | Just the frame. Size is known to the compiler and can sometimes be optimized away |
| Effect on callers | Invisible: ordinary functions work unchanged | "Colored": callers must become coroutines too |
| Standard C++ support | None (libraries only) | Yes, since C++20 |

#### Pros and cons

- **Pro:** Code looks almost exactly like code written for OS threads, with sequential, apparently blocking calls, but with **much lower overhead** and with scheduling under the application's control.
- **Con:** As the speaker points out, there is **no such mechanism in standard C++**. The standard chose stackless coroutines instead.

The speaker believes the main reason is that stackful coroutines are **not zero-overhead**. C++ follows the *zero-overhead principle*: you should not pay for what you do not use, and what you do use should be as efficient as hand-written code. A fiber must reserve a stack big enough for the deepest call chain it *might* make. Every switch saves and restores registers that the code may not need, and the compiler cannot see across a switch to optimize it. Virtual threads are cheaper than OS threads, but, as Barkan puts it, "it's not the optimal way," whereas stackless coroutines "and other mechanisms" can come closer to optimal. Proposals for standard fiber support have been discussed over the years, but none is part of the standard.

### Where This Leaves Us

The two categories sit at different points on the same spectrum:

| | OS threads | Virtual threads |
|---|---|---|
| Who schedules | The kernel | The application/framework |
| Scheduling style | Preemptive | Cooperative |
| Switch cost | High (kernel entry, registers, cache effects) | Lower (user-space stack swap) |
| Scalability | Limited thread count | Many more concurrent tasks |
| Control | Little | Substantial |
| Code style | Blocking, sequential | Blocking-looking, sequential |
| Standard C++ | Yes (C++11) | No |

Both approaches share one thing: they rely on a **separate stack per task** so that you can keep writing sequential, blocking-style code. The high-performance techniques in the next sections take the opposite approach. They give up the per-task stack, and with it the illusion of blocking, in exchange for even lower overhead. That trade is what makes the third category, reactor/proactor thread pools, fast. It is also what makes them harder to write correctly, which is the problem structured concurrency sets out to solve.

## 8. Reactor/Proactor Thread Pools

*Talk segment: [20:08](https://www.youtube.com/watch?v=ttoQfPP_Gkg&t=1208s)*

The previous section looked at two ways of getting concurrency: OS threads, where the kernel does the scheduling, and virtual threads, where the application switches between many stacks itself. The third technique on the speaker's list is different. Here we stop pretending that each flow of work owns a thread at all. This is the **reactor/proactor** model, also called the *event loop* or *thread-pool* model. In the speaker's words, it is "the one that typically gains best performance, especially on high-throughput systems".

### The core idea: small tasks, generic threads

In the thread-based models, a request is handled by code that reads like a straight line: read, compute, write, read again. Whenever that code has to wait, the thread sleeps and something else runs.

In the reactor/proactor model, we cut that straight line into **small tasks**. A task is a chunk of code that:

- runs from start to finish **without blocking**: it never sleeps, never waits for I/O, and never waits for a lock held for a long time;
- finishes **quickly**;
- usually ends by arranging for some *other* task to run later, for example "when more data arrives on this socket".

The tasks are executed by **generic threads**. These threads know nothing about your application. They pick up whatever task or event is ready, run it, and go back for the next one. There may be a single such thread or a pool of them.

The heart of such a thread is the **event loop**. In pseudo-code it is almost embarrassingly short: `while (true) { auto ev = wait_for_event(); dispatch(ev); }`. All the interesting behaviour lives in two places:

- **`wait_for_event()`** is the *only* place where the thread is allowed to block. It waits for timers, I/O, or tasks posted by other code.
- **`dispatch(ev)`** finds the handler (the task) responsible for the event and runs it.

> **Key insight:** In this model, "waiting" is centralised. Application code never waits. Only the loop waits, and it can wait for *everything at once*.

The speaker describes the gaps between tasks like this: between two consecutive tasks, "I can do some other work or wait for I/O". The framework, not the operating system, decides what happens in those gaps.

### Reactor versus proactor

The two names describe two flavours of the same loop. They differ in *what the loop is told* about I/O.

| | Reactor | Proactor |
|---|---|---|
| The loop is notified when… | an I/O operation is **ready** to be performed without blocking ("the socket has data") | an I/O operation has **completed** ("I have read 512 bytes into your buffer") |
| Who performs the actual read/write? | the handler, inside the task, using a non-blocking call | the OS or kernel, in the background |
| Typical OS mechanisms | `select`, `poll`, Linux `epoll`, BSD `kqueue` | Windows IOCP, Linux `io_uring` |
| What the handler receives | "you may now read" | "here is the result of your read" |

A reactor handler therefore looks like "the socket is readable, so read what is there now (it won't block) and decide what to do next". A proactor handler looks like "the read you asked for finished with N bytes, so process them and start the next operation".

Libraries often hide the difference. Boost.Asio, for example, offers a *proactor-style* interface: you call `async_read_some` and get a completion. On Linux it may implement this on top of an `epoll` reactor internally. For the rest of this chapter the distinction matters less than the shared structure: **non-blocking tasks dispatched by a loop**.

### A minimal event loop

The first example strips the model down to its essence. There is no I/O at all, just a queue of tasks and one thread running them. A long computation is deliberately split into chunks, and each chunk re-posts the next one.

**Listing 23** — A minimal single-threaded event loop with a job split into chunks *(illustrative example, not from the talk)*

```cpp
#include <algorithm>
#include <deque>
#include <functional>
#include <iostream>
#include <string>
#include <utility>

// The simplest possible event loop: a queue of tasks run by one thread.
class EventLoop {
public:
    void post(std::function<void()> task) { queue_.push_back(std::move(task)); }

    void run() {
        while (!queue_.empty()) {               // "wait_for_event()" ...
            auto task = std::move(queue_.front());
            queue_.pop_front();
            task();                             // ... "dispatch(ev)"
        }
    }

private:
    std::deque<std::function<void()>> queue_;
};

// A long computation (sum of next..last) split into non-blocking chunks.
// Each chunk does a bounded amount of work, then re-posts the rest.
void sum_chunked(EventLoop& loop, std::string name,
                 long long next, long long last, long long acc) {
    const long long stop = std::min(next + 1000, last + 1);
    for (; next < stop; ++next) acc += next;

    if (next > last) {
        std::cout << name << " done: " << acc << '\n';
        return;
    }
    std::cout << name << " yielded at " << next << '\n';
    // The loop's "local variables" must travel inside the next task.
    loop.post([&loop, name = std::move(name), next, last, acc]() mutable {
        sum_chunked(loop, std::move(name), next, last, acc);
    });
}

int main() {
    EventLoop loop;
    loop.post([&] { sum_chunked(loop, "A", 1, 3500, 0); });
    loop.post([&] { sum_chunked(loop, "B", 1, 2500, 0); });
    loop.run();   // Output interleaves A and B: fairness without preemption.
}

```

Walking through it:

- `EventLoop` holds a queue of `std::function<void()>`. `post` appends a task. `run` repeatedly pops the front task and executes it. This is the `wait_for_event(); dispatch(ev);` loop, with "wait" degenerated to "take the next queued item".
- `sum_chunked` processes at most 1000 numbers and then **stops voluntarily**. It does not loop to the end. It packages its remaining state (`next`, `last`, `acc`, `name`) into a lambda and posts that lambda as a new task.
- Because the job yields after every chunk, two jobs posted together **interleave**: A, B, A, B, and so on. Neither job can starve the other, even though there is only one thread and no preemption. This is the **fairness** the speaker mentions: every task "will complete very very soon and make way for other tasks to run".
- Notice what the split cost us. The natural code would be a simple `for` loop. Instead we had to turn the loop's local variables into captured state and re-enter the function from the loop. Keep this in mind, because it is the first hint of the complexity discussed below.

### A minimal reactor over real I/O

The second example adds what makes a reactor a reactor: waiting for **I/O readiness**. It uses Linux `epoll`, which is an OS API and not part of standard C++. It watches standard input and runs a handler whenever data can be read without blocking.

**Listing 24** — A tiny epoll-based reactor (Linux only, not standard C++) *(illustrative example, not from the talk)*

```cpp
// Linux-specific: uses epoll(7) and POSIX file descriptors.
#include <sys/epoll.h>
#include <fcntl.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <functional>
#include <unordered_map>

class Reactor {
public:
    Reactor() : epfd_(epoll_create1(0)) {}
    ~Reactor() { close(epfd_); }
    Reactor(const Reactor&) = delete;
    Reactor& operator=(const Reactor&) = delete;

    void watch_readable(int fd, std::function<void()> on_ready) {
        epoll_event ev{};
        ev.events = EPOLLIN;
        ev.data.fd = fd;
        epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &ev);
        handlers_[fd] = std::move(on_ready);
    }

    void unwatch(int fd) {
        epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, nullptr);
        handlers_.erase(fd);
    }

    void run() {
        epoll_event events[16];
        while (!handlers_.empty()) {
            // The ONLY blocking call: wait until some fd is *ready*.
            int n = epoll_wait(epfd_, events, 16, -1);
            if (n < 0) {
                if (errno == EINTR) continue;
                std::perror("epoll_wait");
                return;
            }
            for (int i = 0; i < n; ++i) {
                auto it = handlers_.find(events[i].data.fd);
                if (it == handlers_.end()) continue;
                auto handler = it->second;  // copy: handler may unwatch itself
                handler();                  // dispatch: must not block!
            }
        }
    }

private:
    int epfd_;
    std::unordered_map<int, std::function<void()>> handlers_;
};

int main() {
    Reactor reactor;
    const int fd = STDIN_FILENO;  // try: echo hello | ./reactor
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);

    reactor.watch_readable(fd, [&] {
        char buf[256];
        for (;;) {
            ssize_t got = read(fd, buf, sizeof buf);  // non-blocking read
            if (got > 0) {
                std::printf("got %zd bytes\n", got);
            } else if (got == 0) {                     // EOF: this flow is done
                std::printf("EOF\n");
                reactor.unwatch(fd);
                return;
            } else {
                // EAGAIN: no more data right now; return to the loop.
                // (Other errors are treated the same way here for brevity.)
                return;
            }
        }
    });

    reactor.run();
}

```

Walking through it:

- `watch_readable` registers a file descriptor with `epoll`. It also stores the handler in an `unordered_map` keyed by that descriptor. This map lookup from "which source fired" to "who cares about it" is exactly the dispatch step. The next section shows that a real system usually stores a whole state-machine object there.
- `run` calls `epoll_wait`, the **single blocking call** in the whole program. When it returns, each ready descriptor's handler is called.
- The handler is copied before it is called. A handler may `unwatch` itself, and destroying a `std::function` while it is executing would be undefined behaviour. Small lifetime traps like this are typical of hand-written event-driven code.
- `on_stdin_ready` performs a **non-blocking** `read`. The descriptor was put in `O_NONBLOCK` mode, so if there is no more data, `read` returns `-1` with `EAGAIN` instead of sleeping. The handler then simply returns and lets the loop wait again.
- Reading 0 bytes means end-of-file. The handler unregisters itself, the map becomes empty, and `run` exits.

To try it, pipe input in (`echo hello | ./reactor`) or type into a terminal and press Ctrl-D. A regular file redirected with `<` won't work, because `epoll` refuses regular files.

### The golden rule: handlers must never block

Everything above depends on one discipline: **a handler must not block**. Consider what happens if one handler calls a blocking `read`, sleeps, or waits on a contended mutex:

- With a single loop thread, *every other connection, timer and task stops*. Nothing else can run, because the only thread is stuck inside your handler.
- With a pool of N threads, N such handlers are enough to freeze the whole system.
- Fairness disappears, and tail latency explodes for unrelated work that happened to queue up behind the slow task.

This rule is the reason the work *must* be split. Any operation that might wait, such as reading the next message, waiting for a reply, or waiting for a timer, has to become a boundary between two tasks. The code before the wait goes in one task. The code after it goes in another task, which the loop runs when the event arrives. This connects directly to the earlier section on non-blocking APIs: each non-blocking call marks a point where one task ends and the dependent work moves into a later task.

### Why it performs so well

The slide lists the pros briefly: *"Low overhead, no O/S required. Frequently, don't parallelize related tasks, so they don't need mutexes."* Each point deserves unpacking.

- **Low overhead.** Switching from one task to the next is just a function call made by the loop. There is no kernel context switch, no saving and restoring of a full register set and stack, and no separate stack per flow. Thousands of connections can be multiplexed on a handful of threads.
- **No reliance on OS scheduling.** The framework decides what runs next. It can apply its own priorities, batch similar events together, or poll instead of sleeping in latency-critical systems such as the speaker's HFT world. As the previous section noted, the OS scheduler optimises for the generic case. A reactor lets you optimise for *your* case.
- **Fewer mutexes.** In a thread pool you typically arrange for all tasks belonging to one flow (one session, one order book, one client) to run *one at a time*. One way is to pin the flow to one loop thread. Another is a serialising wrapper, which Asio calls a *strand*. Related tasks then never run in parallel with each other, so the state they share needs no locks. Parallelism comes from running *unrelated* flows on different threads. This is the "don't parallelize related tasks" point on the slide, and it sidesteps much of the correctness challenge from the earlier section on raw synchronization primitives.

### The catch: how do you split the work?

The slide is honest about the downside: *"Potentially complicated – how does one split the work to tasks?"*

The speaker calls this "a challenge of writing systems". A real program is not a sum over integers. It is a protocol with handshakes, retries, timeouts, errors, cancellation, and flows that depend on one another. Each of these has to be chopped at every point where it might wait. The state that would naturally live in local variables on a stack then has to be stored *somewhere* so the next task can find it. Listing 23 already showed the cost in miniature: a three-line loop became a self-re-posting function with hand-carried state.

In summary:

| | Reactor / proactor thread pool |
|---|---|
| Unit of work | small, non-blocking task or event handler |
| Who schedules | the application's framework (event loop) |
| Where waiting happens | only in the loop (`epoll_wait`, completion queue, …) |
| Pros | lowest overhead, full control over scheduling, related tasks need no mutexes |
| Cons | code must be split at every wait; state and lifetimes must be managed by hand |
| Typical use | high-throughput servers, networking, trading systems |

There are several established answers to "how do I split the work?". The next two sections examine the two classic ones:

1. **State machines**: each flow becomes an object that reacts to events and moves between explicit states.
2. **Callbacks / continuation-passing style**: each asynchronous call carries the next task with it as a lambda.

Both work, and both have characteristic weaknesses. Seeing those weaknesses clearly is what motivates the rest of the talk: structured concurrency, with coroutines and senders, keeps the performance of the reactor model while splitting the work in a more disciplined and readable way.

## 9. Splitting Work into Tasks: State Machines

*Talk segment: [21:46](https://www.youtube.com/watch?v=ttoQfPP_Gkg&t=1306s)*

The previous section ended on an uncomfortable question. Reactor/proactor designs give the best throughput, but only if the work has been cut into small, non-blocking tasks. *How* do you take an algorithm and break it into such tasks? The speaker surveys several answers. The first, which many students meet at university, is the **state machine pattern**.

### What a state machine is

A *finite state machine* is an object that is always in exactly one of a fixed set of **states**. It reacts to **events**, meaning things that happen to it, such as "a message arrived", "a timer fired" or "the peer closed the connection". Each reaction may do some work and may cause a **transition**, a move from the current state to a new one. The full diagram of states and transitions is called the **state chart**.

In a concurrent server, the pattern is used like this:

- Each **independent flow** gets its own state-machine object. A flow could be one client connection, one session or one order.
- That object reacts only to **its own events**.
- Handling an event must be quick and **non-blocking**. The handler looks at the current state, does a small amount of work, possibly changes state, and returns.

This fits the reactor model closely. The reactor thread never waits on any single client. The state machine keeps everything needed to resume later in its member variables, so no thread or stack has to stay parked while a client is slow to answer.

### The typical shape of the code

The speaker describes the usual structure as follows. A reactor thread receives an event. It works out which existing state machine the event belongs to, typically with a map lookup keyed by connection or session id. It hands the event to that machine and lets it do its non-blocking work, which may include a state switch. Then it repeats ("rinse and repeat") for every new event.

Here is a small protocol in that style. A client must first say hello, then authenticate, and only then may it exchange data.

**Listing 25** — A per-connection state machine driven by a reactor *(illustrative example, not from the talk)*

```cpp
#include <cstdint>
#include <iostream>
#include <string>
#include <unordered_map>

using ConnId = std::uint64_t;

enum class EventKind { Hello, Auth, Data, Close };

struct Event {
    ConnId conn;
    EventKind kind;
    std::string payload;
};

class Connection {
public:
    enum class State { AwaitingHello, AwaitingAuth, Established, Closed };

    // Called by the reactor. Must never block.
    void on_event(const Event& ev) {
        switch (state_) {
        case State::AwaitingHello:
            if (ev.kind == EventKind::Hello) {
                send("HELLO-ACK");
                state_ = State::AwaitingAuth;
            } else {
                fail("expected HELLO");
            }
            break;
        case State::AwaitingAuth:
            if (ev.kind == EventKind::Auth && ev.payload == "secret") {
                send("AUTH-OK");
                state_ = State::Established;
            } else {
                fail("bad credentials");
            }
            break;
        case State::Established:
            if (ev.kind == EventKind::Data) {
                send("ECHO " + ev.payload);   // stay in Established
            } else if (ev.kind == EventKind::Close) {
                state_ = State::Closed;
            }
            break;
        case State::Closed:
            break;                            // ignore late events
        }
    }

    bool closed() const { return state_ == State::Closed; }

private:
    // Stand-in for a non-blocking write that just queues bytes.
    void send(const std::string& msg) { std::cout << "-> " << msg << '\n'; }
    void fail(const std::string& why) {
        std::cout << "error: " << why << '\n';
        state_ = State::Closed;
    }

    State state_ = State::AwaitingHello;
};

class Reactor {
public:
    void dispatch(const Event& ev) {
        // Map lookup: find (or create) the state machine for this flow.
        auto [it, inserted] = connections_.try_emplace(ev.conn);
        it->second.on_event(ev);
        if (it->second.closed()) connections_.erase(it);
    }

private:
    std::unordered_map<ConnId, Connection> connections_;
};

int main() {
    Reactor reactor;
    // Events from two clients arrive interleaved, as from a real network.
    const Event events[] = {
        {1, EventKind::Hello, ""},
        {2, EventKind::Hello, ""},
        {1, EventKind::Auth,  "secret"},
        {2, EventKind::Auth,  "wrong"},
        {1, EventKind::Data,  "ping"},
        {1, EventKind::Close, ""},
    };
    for (const auto& ev : events) reactor.dispatch(ev);   // the event loop
}

```

Walking through it:

- `Connection::State` lists the four states: `AwaitingHello -> AwaitingAuth -> Established -> Closed`. The current state is a single member, `state_`.
- `on_event` is the only entry point. It switches on the **current state** first and on the **event kind** second. Each `case` is one small non-blocking task. `send` stands in for a non-blocking write that queues bytes and returns at once.
- A transition is simply an assignment to `state_`. In `Established`, a `Data` event leaves the state unchanged. A connection can stay in one state across many events.
- `Reactor::dispatch` is the event loop body. `try_emplace` finds the connection's machine, or creates a fresh one in `AwaitingHello` for a new id, and forwards the event to it. Finished machines are erased.
- In `main`, events from clients 1 and 2 arrive **interleaved**, as they would from a real network. Each machine still sees only its own events, in order, and neither knows the other exists.

All connections are handled on one reactor thread, and each machine only touches its own data. No mutex appears anywhere. This is the advantage of reactors from the previous section: related work is not parallelized, so it needs no locks.

### Where state machines shine

The speaker calls state machines "very simple and elegant when [they] work". They are especially suitable for **multi-stage protocols**:

- **Handshakes**, such as hello, then authentication, then ready.
- **Session control**, such as login, heartbeats and logout.
- Generally any **client/server back-and-forth** in which each side alternates between sending and waiting.

In these cases the state chart *is* the protocol specification. Many network RFCs and exchange protocol documents literally draw a state chart, so the code can mirror the document state by state.

### What we gave up: the hidden program counter

To see the cost, compare the same protocol written the "old" way: one blocking thread per connection, reading top to bottom.

**Listing 26** — Contrast: the same protocol as sequential blocking code *(illustrative example, not from the talk)*

```cpp
#include <iostream>
#include <string>

// One thread per connection: the 'state' is simply where we are in the code.
bool handle_connection(std::istream& in, std::ostream& out) {
    std::string line;

    if (!std::getline(in, line) || line != "HELLO") {      // blocks
        out << "error: expected HELLO\n";
        return false;
    }
    out << "HELLO-ACK\n";

    if (!std::getline(in, line) || line != "AUTH secret") { // blocks
        out << "error: bad credentials\n";
        return false;
    }
    out << "AUTH-OK\n";

    while (std::getline(in, line) && line != "CLOSE")      // blocks
        out << "ECHO " << line << '\n';

    return true;
}

int main() {
    handle_connection(std::cin, std::cout);
}

```

This version is easy to read. The order of the protocol is the order of the lines, and there is no explicit `state_` variable. The state is implicit in *where the code is currently executing*, which is the thread's program counter, plus the local variables on its stack. The price is the blocking `getline`, which holds an entire OS thread hostage while waiting, with all the costs listed for OS threads.

The state machine removes the waiting by **turning the program counter into data**. Every point where the sequential code would have blocked becomes a state boundary, and the code between two such points becomes one `case`. That is what makes it non-blocking, but it also means:

> The algorithm's control flow is no longer readable top to bottom. It is scattered across `case` labels, and the reader must reconstruct the sequence by following assignments to `state_`.

Loops, early returns and error paths, which the sequential version expressed with `while`, `if` and `return`, must all be encoded by hand as transitions.

### Where state machines break down

The speaker names two situations in which the state machine "stops doing its job".

**1. The state chart grows.** Real protocols add timeouts, retries, partial reads, error recovery, cancellation and reconnection. Each concern tends to multiply with the existing states: *every* state needs an answer to "what if the timer fires now?" and "what if the client disconnects now?". The chart grows combinatorially, and so does the `switch`.

**2. Flows depend on each other.** The pattern assumes each machine reacts only to *its own* events. Suppose instead that connection A may not proceed until something happens on connection B, or on some shared resource. Before reading on, ask yourself: *which object owns that waiting? Who wakes A up? What if A disconnects while it is waiting, or B fails?*

**Listing 27** — Pitfall: a cross-flow dependency between state machines *(illustrative example, not from the talk)*

```cpp
#include <iostream>
#include <unordered_map>
#include <vector>

using ConnId = int;

struct Session {
    enum class State { Established, WaitingForFeed, Trading };
    State state = State::Established;
};

class Reactor {
public:
    // A client may only start trading once the market-data feed is live.
    void on_start_trading(ConnId id) {
        auto& s = sessions_[id];
        if (feed_live_) {
            s.state = Session::State::Trading;
            std::cout << id << ": trading\n";
        } else {
            s.state = Session::State::WaitingForFeed;   // state caused by ANOTHER flow
            waiting_.push_back(id);
            std::cout << id << ": waiting for feed\n";
        }
    }

    // An event on a different flow must now wake up other state machines.
    void on_feed_live() {
        feed_live_ = true;
        for (ConnId id : waiting_) {
            auto it = sessions_.find(id);
            if (it == sessions_.end()) continue;        // client left meanwhile!
            it->second.state = Session::State::Trading;
            std::cout << id << ": trading (woken by feed)\n";
        }
        waiting_.clear();
    }

    // Note: leaves a stale id in waiting_. Easy to forget, easy to get wrong.
    void on_disconnect(ConnId id) { sessions_.erase(id); }

    // Still unanswered: feed going down again? timeouts while waiting?
    // cancelling a wait? Each adds more states and more shared bookkeeping.

private:
    std::unordered_map<ConnId, Session> sessions_;
    std::vector<ConnId> waiting_;
    bool feed_live_ = false;
};

int main() {
    Reactor r;
    r.on_start_trading(1);
    r.on_start_trading(2);
    r.on_disconnect(2);
    r.on_feed_live();
    r.on_start_trading(3);
}

```

This sketch shows the problems:

- `Session` gains a new state, `WaitingForFeed`, which exists only because of a *different* flow.
- The reactor now keeps extra bookkeeping (`waiting_`, `feed_live_`) outside any single machine. An event on the feed must reach into other sessions and change their state.
- Lifetimes get tangled. When client 2 disconnects, its id stays in `waiting_`, and `on_feed_live` must defensively check whether the session still exists. Forgetting that check means touching a destroyed object.
- The comments point to unanswered questions: the feed going down again, timeouts while waiting, and cancelling a wait. Each one adds more cross-flow states and more places where a bug can hide.

None of this is impossible, but it is the opposite of the *local reasoning* goal from earlier in the chapter. You can no longer understand one machine by reading it alone, because correctness depends on how several machines and the reactor's shared bookkeeping interact.

### Summary of the trade-off

| Aspect | State machine in a reactor |
|---|---|
| Blocking | None. Each event handler returns quickly |
| Threads/stacks per flow | None. State lives in a small object |
| Locks | Usually unnecessary on a single reactor thread |
| Best fit | Multi-stage protocols, handshakes, session control |
| Readability | Control flow scattered across states |
| Scaling the logic | Chart explodes with timeouts, errors and cancellation |
| Cross-flow dependencies | Awkward. Needs shared bookkeeping and manual lifetime checks |

> **Key insight:** a state machine is a hand-made way of saving and resuming a computation without a stack. It is efficient but manual, and it hides the sequential shape of the algorithm.

This tension motivates the rest of the chapter. Next comes the second classic approach, callbacks or continuation-passing style, which tries to keep the code looking sequential. After that, structured concurrency with coroutines and senders, which in effect let the compiler or library build the state machine while you write something close to the readable version in Listing 26.

## 10. Callbacks / Continuation-Passing Style and the Lifetime Problem

*Talk segment: [23:20](https://www.youtube.com/watch?v=ttoQfPP_Gkg&t=1400s)*

The previous section split work into tasks by turning each independent flow into a *state machine* that reacts to events. This section covers a second way to split work into non-blocking tasks: **callbacks**, also known as **continuation-passing style (CPS)**. It was the dominant style of asynchronous C++ for well over a decade, and it is the style many production systems still use today. It also shows clearly the problem that structured concurrency was designed to solve.

### The idea: pass "what comes next" as an argument

Recall from Section 3 that a *non-blocking API* returns before its work is finished. That raises an obvious question: where do we put the code that needs the result?

CPS answers: *hand it to the operation*. Every asynchronous call takes one extra argument, usually a lambda. That lambda is the **continuation**: the rest of the computation that truly depends on the result. The operation starts the work, returns immediately, and later, when the result is ready, the framework invokes the continuation with that result.

This gives a simple rule for reading such code:

- Every call to an async operation **ends one task**. It is the last thing the current chunk of non-blocking work does.
- The lambda passed to it is **the next task**. It runs later, possibly much later, from the framework's event loop.

In the speaker's words, if you "squint correctly" at CPS code that uses C++11 lambdas, it looks like ordinary sequential code. The read is followed by the processing, which is followed by the write. In reality it is a chain of separate tasks glued together by callbacks. This is how the "old-school" Boost.Asio library was designed, and lambdas and `std::function` made the style practical.

The speaker identifies the price immediately: **object lifetimes and resource management get messy very quickly.** The rest of this section shows why, using the speaker's example.

### A minimal model of CPS

Before looking at Asio, it helps to see the mechanism with nothing hidden. The following toy program is our own, not from the talk. It uses only the standard library. A queue of ready tasks plays the role of the reactor, and a fake asynchronous operation does nothing except *schedule* its callback.

**Listing 28** — A toy event loop showing CPS without stack growth (illustrative, standard C++20) *(illustrative example, not from the talk)*

```cpp
#include <deque>
#include <functional>
#include <iostream>

// The "reactor": a queue of ready-to-run tasks.
std::deque<std::function<void()>> ready;

// A fake non-blocking operation: it only *schedules* the callback and returns.
void async_tick(int n, std::function<void(int)> on_done) {
    ready.push_back([n, cb = std::move(on_done)] { cb(n + 1); });
}

void do_ping(int n);

void do_pong(int n) {
    async_tick(n, [](int m) { do_ping(m); });   // continuation = next task
}

void do_ping(int n) {
    if (n >= 1'000'000) {
        std::cout << "done at " << n << '\n';
        return;
    }
    async_tick(n, [](int m) { do_pong(m); });
    // Code here runs BEFORE the tick has completed!
}

int main() {
    do_ping(0);                       // returns almost immediately
    while (!ready.empty()) {          // the event loop
        auto task = std::move(ready.front());
        ready.pop_front();
        task();                       // stack depth stays small: no real recursion
    }
}

```

Several points in this program carry over directly to the real Asio code:

- `do_ping` and `do_pong` appear to call each other forever, one million times in total. Yet the program does not overflow the stack. Each function only *registers* a lambda and returns. The "recursive" call happens later, from `main`'s loop, on a fresh, shallow stack.
- The comment in `do_ping` marks a dangerous place. Anything written after `async_tick(...)` runs *before* the operation has completed.
- The lambdas here capture nothing, so lifetime is not yet an issue. In real code, continuations nearly always need access to some object, such as a socket, a buffer or a session. That is where the trouble starts.

### The speaker's example: an Asio echo session

The speaker shows a classic example from the Boost.Asio documentation, dating roughly from the early 2000s. A `session` object owns a TCP socket. It reads whatever the client sends and writes the same bytes back, forever. The first half of the class:

**Listing 29** — code from the talk (Continuation Passing Style), shown at [27:27](https://www.youtube.com/watch?v=ttoQfPP_Gkg&t=1647s)

```cpp
struct session : public enable_shared_from_this<session> {
    session(tcp::socket socket) : socket_(std::move(socket)) {}
    void start() { do_read(); }
  private:
    void do_read() {
        auto self(shared_from_this());
        socket_.async_read_some(
            asio::buffer(data_, max_length),
            [this, self](std::error_code ec,
                        std::size_t length) {
                if (!ec) { do_write(length); }
            });
        /* code here is probably a bug */
    }
```

Line by line:

- `struct session : public enable_shared_from_this<session>`: the class inherits from `std::enable_shared_from_this`. This means a session is *intended* to be owned by a `std::shared_ptr`, and it can produce new `shared_ptr`s to itself from inside its own member functions. Keep this in mind; it is the key to the lifetime story.
- The constructor takes ownership of an already-connected `tcp::socket` by moving it into `socket_`.
- `void start() { do_read(); }`: starting a session means issuing the first read. Note that `start()` returns almost immediately. It does not "run" the session.
- `auto self(shared_from_this());` creates a new `shared_ptr<session>` that shares ownership with whoever created the session. This increments the reference count.
- `socket_.async_read_some(asio::buffer(data_, max_length), ...)` asks the operating system (via Asio's reactor) to read *up to* `max_length` bytes into the member array `data_`. This call is **non-blocking**. It records the request and returns.
- The lambda `[this, self](std::error_code ec, std::size_t length) { ... }` is the continuation. It will be invoked later with an error code and the number of bytes read. It captures two things:
  - `this`, a raw pointer, which makes it convenient to call member functions like `do_write`.
  - `self`, the `shared_ptr`, which is never used in the body. It exists *only* to hold a reference count while the lambda is alive.
- `if (!ec) { do_write(length); }`: if the read succeeded, the next task is to echo `length` bytes back. If there was an error (for example, the client disconnected), the lambda does nothing.
- `/* code here is probably a bug */`: this comment comes from the speaker's slide. At this point `async_read_some` has already returned, but **the read has not happened yet**. `data_` still holds old contents, and `length` does not exist in this scope. Code here that assumes the read is done is wrong. The code that genuinely depends on the read lives *inside* the lambda.

The second half of the class:

**Listing 30** — code from the talk (Continuation Passing Style), shown at [27:27](https://www.youtube.com/watch?v=ttoQfPP_Gkg&t=1647s)

```cpp
    void do_write(std::size_t length) {
        auto self(shared_from_this());
        asio::async_write(
            socket_, asio::buffer(data_,
                                  length),
            [this, self](std::error_code ec,
                        std::size_t /*length*/) {
                if (!ec) { do_read(); }
            });
    }

    tcp::socket socket_;
    enum { max_length = 1024 };
    char data_[max_length];
};
```

- `do_write(std::size_t length)` mirrors `do_read`. It again creates its own `self` with `shared_from_this()`.
- `asio::async_write(socket_, asio::buffer(data_, length), ...)` is a *composed* operation. Unlike `async_read_some`, it keeps writing until all `length` bytes are sent or an error occurs. It is also non-blocking: it starts the work and returns.
- The continuation `[this, self](std::error_code ec, std::size_t /*length*/)` ignores the byte count (the parameter name is commented out to silence warnings). On success it calls `do_read()`, which closes the loop: read, write, read, write, and so on.
- The data members come last: the socket, a fixed buffer size of 1024 (via the old `enum` trick for an in-class constant), and the buffer `data_` itself. Notice that the buffer lives *inside the session*. The pending OS operation writes into, or reads from, memory owned by this object. If the session were destroyed while an operation is in flight, the operation would be using freed memory.

### It looks like recursion, but it isn't

At first glance `do_read` calls `do_write`, which calls `do_read`, which calls `do_write`... As the speaker notes, this *looks* like unbounded mutual recursion that would eventually exhaust the stack.

It doesn't, for exactly the reason shown in Listing 28. `do_read` never calls `do_write`. It creates a lambda that *will* call `do_write` and hands that lambda to Asio. Then `do_read` returns. By the time the lambda runs, `do_read`'s stack frame is long gone. The lambda is invoked from the event loop (`io_context::run()`), so the stack depth stays constant no matter how many messages are echoed.

> **Key insight:** In CPS, the textual nesting of the code (a call inside a lambda inside a call) says nothing about the *runtime* nesting of stack frames. Each async call is a task boundary. Everything after it in the same function runs *now*; everything inside the callback runs *later*.

### Tracing the timeline

To understand the lifetime problem, follow one session through time. First, here is how such a session is typically created and launched, in the style of the same Asio example (our code, not from the slides):

**Listing 31** — Launching the echo session: the temporary owner disappears immediately (Asio, illustrative) *(illustrative example, not from the talk)*

```cpp
#include <asio.hpp>   // standalone Asio; with Boost use <boost/asio.hpp>
#include <memory>

using asio::ip::tcp;

// Assumes `session` exactly as defined in the talk's slides (Listing 29 + Listing 30).

class server {
public:
    server(asio::io_context& io, unsigned short port)
        : acceptor_(io, tcp::endpoint(tcp::v4(), port)) {
        do_accept();
    }

private:
    void do_accept() {
        acceptor_.async_accept(
            [this](std::error_code ec, tcp::socket socket) {
                if (!ec) {
                    std::make_shared<session>(std::move(socket))->start();
                    // The temporary shared_ptr is destroyed at the ';' above.
                    // From now on, ONLY the `self` captured inside the pending
                    // read callback keeps the session alive.
                }
                do_accept();  // accept the next client (again: CPS)
            });
    }

    tcp::acceptor acceptor_;
};

int main() {
    asio::io_context io;
    server s(io, 12345);
    io.run();   // the event loop: runs callbacks as I/O completes
}

```

Now track who owns the session at each step. "Owners" means `shared_ptr`s that are alive at that moment.

| Time | What happens | Owners of the session |
|---|---|---|
| t0 | `make_shared<session>(...)` creates the session on the heap | the temporary returned by `make_shared` |
| t1 | `->start()` calls `do_read()`; `self` is created and copied into the read lambda, which Asio stores | temporary, `self` in `do_read`, read lambda's `self` |
| t2 | `async_read_some` returns, `do_read` returns, and `start` returns; the local `self` and then the temporary are destroyed | **only** the read lambda's `self` |
| t3 | the accept handler finishes; the event loop waits for I/O | read lambda |
| t4 | data arrives; the read lambda runs and calls `do_write(length)`, which creates a write lambda holding another `self` | read lambda, write lambda |
| t5 | the read lambda finishes and Asio destroys it | **only** the write lambda |
| t6 | the write completes; its lambda calls `do_read()`, creating a new read lambda | write lambda, new read lambda |
| ... | the cycle repeats | always exactly the pending callback |
| end | the client disconnects; the read lambda is called with an error, does *not* call `do_write`, and is destroyed | **nobody**, so the reference count drops to zero and the session is deleted |

The striking row is t2. After `start()` returns, **no named variable anywhere in the program owns the session**. It is kept alive solely by a `shared_ptr` hidden in the capture list of a callback that Asio has stored somewhere internally. Ownership is relayed from one pending callback to the next, like a baton.

This design is clever: the session is destroyed precisely when the last operation involving it finishes, which is exactly the moment the speaker says would otherwise be hard to pin down. Without `self`, as the speaker puts it, "there will be no clear point where it will be correct or known that the object or the session can be safely removed or deleted."

### Exercise: what if we drop `self`?

Before reading on, predict what happens if we "simplify" the capture lists to `[this]` only. After all, `self` is never used inside the lambda body.

**Listing 32** — Pitfall: dropping `self` from the capture (a broken variant of the talk's do_read) *(illustrative example, not from the talk)*

```cpp
// BROKEN: do not do this. Compare with the talk's do_read in Listing 29.
void do_read() {
    socket_.async_read_some(
        asio::buffer(data_, max_length),
        [this](std::error_code ec, std::size_t length) {  // no `self`!
            // Nothing kept the session alive after start() returned:
            // the session was destroyed, its socket closed, and the read
            // cancelled. This handler now runs with a dangling `this`.
            // Any access to members (socket_, data_, do_write, ...) here
            // is undefined behaviour.
            if (!ec) { do_write(length); }
        });
}

```

With this change, at t2 in the table the temporary `shared_ptr` dies and *nothing* keeps the session alive, so it is destroyed immediately after `start()` returns. Destroying it closes the socket, which cancels the pending read. Asio will still invoke the handler later, with an "operation aborted" error. That handler holds a raw `this` pointing at a deleted object. In this exact code the body happens to check `ec` first and touch nothing else, so it may *appear* to work: the connection just closes instantly. The slightest change, such as logging through a member or touching `data_` before checking `ec`, turns it into a use-after-free. Even in the "lucky" case, the server is silently broken.

The lesson is uncomfortable. The *correctness* of this program depends on a variable that the code never reads. Nothing in the type system or the visible control flow tells a reviewer that `self` is load-bearing.

### Why `shared_ptr` is a symptom, not a solution

The `enable_shared_from_this` / `self` pattern works, and it has been used successfully for years. The speaker points out what it costs:

- **Forced heap allocation.** `shared_from_this()` only works if the object is already owned by a `shared_ptr`. Calling it otherwise throws `std::bad_weak_ptr` (since C++17). So every session *must* live on the heap, even if it could otherwise live in a pre-allocated pool or on a stack.
- **Reference counting on every step.** Each async operation creates and destroys `shared_ptr` copies, which means atomic increments and decrements on the hot path. In low-latency systems, that overhead is not free.
- **Lifetime is implicit.** You cannot point at a scope in the source code and say "the session ends here". The answer is "whenever the last callback that happens to hold a reference is destroyed". That is a fact about runtime behaviour, not about program structure.
- **Reasoning is non-local.** To know whether a raw `this` or a reference to `data_` is safe inside some callback, you must know about every other callback that might still be pending. This is exactly the kind of global reasoning that Section 6 argued we should avoid.

The deeper diagnosis is this. We reach for *shared* ownership because **no single scope owns the asynchronous operation**. `do_read` returns before the read finishes, so `do_read` cannot own it. `start` returns before the session finishes, so `start` cannot own it either. The work has escaped every enclosing `{ }` block, and reference counting is what we use to track things that have escaped structure.

> **Key insight:** In CPS code, `shared_ptr` captured in callbacks is a sign that the asynchronous work has no owner in the source code. The problem is not memory management as such; it is that the *structure* of the computation is not visible in the structure of the program.

### Toward structured concurrency

Compare this with how lifetimes work in ordinary sequential C++. A local variable lives until the closing `}` of its block, RAII cleans it up, and nobody needs a reference count. The speaker frames structured concurrency, the third way to split work into tasks covered in this talk, in exactly these terms. Like state machines and callbacks, it breaks a large piece of work into small non-blocking tasks. Unlike them, it keeps "some understanding of how all the pieces fit together." Asynchronous operations are owned by scopes, scopes cannot end before their operations do, and lifetimes once again follow the visible nesting of the code.

The next section develops that philosophy. Later sections show that the echo loop above can be written with coroutines as a plain `while` loop with a `co_await` inside, with no `enable_shared_from_this`, no `self`, and no hidden owners.

## 11. The Structured Concurrency Philosophy

*Talk segment: [27:21](https://www.youtube.com/watch?v=ttoQfPP_Gkg&t=1641s)*

The previous section ended with an uncomfortable picture. In the callback-based echo session, the only way to know when a session object could safely be destroyed was to capture a `shared_ptr` to `self` in every lambda and let reference counting decide. The program's *logic* was sequential: read, then write, then read again. Its *lifetime management*, however, was scattered across heap-allocated closures with no single point in the code where you could say "the session is over here."

Structured concurrency is the answer to that problem. It is less a library than a discipline for arranging concurrent work so that lifetimes, errors and cancellation follow the shape of the source code. This section presents that discipline in general terms. The following sections show how coroutines and senders implement it.

### Blocks within blocks

The speaker introduces the idea with a diagram by Ben Weidig. It shows an outer scope containing several subtasks, and inside it a nested scope containing more subtasks, one of which has failed. Every rectangle is made of smaller rectangles that live entirely inside it. A task is composed of subtasks, and each subtask either succeeds or fails.

The point of the picture is **local reasoning**. When you look at one level of the nesting, you should not have to think much about what goes on inside your subtasks, or about which parent task you belong to. The speaker puts it this way: the structured concurrency mechanism "tries to assist with thinking locally." A rectangle is a black box with a clear boundary. Work enters at the top, and by the time control leaves at the bottom, everything that was started inside has finished.

### An old lesson: structured programming

The idea has a well-known precedent. In the 1960s and 70s, programmers moved away from unrestricted `goto` towards *structured programming*: `if`, `while`, function calls and blocks. Each of these has **one entry and one exit**. When you read

- `if (x) { ... }`, you know control comes back to the line after the closing brace;
- `f();`, you know that once the call returns, `f` is finished;

you can treat the construct as a single step and ignore its internals. `goto` breaks this, because control can leave a block and never come back to where you expect.

Unstructured concurrency has the same weakness. A *detached* task (a `std::thread` that is `detach()`ed, a callback chain kept alive by `shared_ptr`, a fire-and-forget job posted to a pool) is the concurrent version of `goto`. Control appears to come back to you, but part of the work has escaped the block and continues somewhere else, for an unknown amount of time, touching objects whose lifetime nobody guarantees. The `/* code here is probably a bug */` comment from the CPS example is exactly that situation: the function returned, but the work it started had not finished.

> **Key insight:** Structured concurrency applies the rule of structured programming to concurrent work. Every concurrent operation started inside a block must be finished, or explicitly handed to an enclosing owner, by the time control leaves that block.

### Tasks are resources: use RAII

The central claim on the speaker's slide is short: **tasks are resources with lifetimes, so use RAII to control them.**

A *task* here means any unit of asynchronous work: a thread of execution, a pending network read, a timer, a request sent to the outside world. Like memory, file handles or locks, each one is acquired, used and eventually released. C++ already has a good tool for that kind of thing. **RAII** (Resource Acquisition Is Initialization) ties a resource to the lifetime of a C++ object, so the object's destructor releases the resource.

The speaker explicitly connects this to an earlier argument in the talk. Value semantics and RAII solved object-lifetime problems by imposing a disciplined structure, and structured concurrency tries to do the same for concurrent work.

### The rules

The slide states the philosophy as a few rules:

| Rule | Meaning |
|---|---|
| Every task belongs to a scope | There is always an owner responsible for the task's completion. No task is orphaned. |
| Subtasks typically belong to a sub-scope | Ownership forms a tree. A task's children live in a nested scope inside that task. |
| Scopes outlive their tasks | A scope cannot end, and its owning object cannot be destroyed, before all its subtasks have completed. |

The third rule carries the most weight. If a scope always outlives its tasks, then a task can safely refer to anything that lives in an enclosing scope: local variables, buffers, sockets. Those things are guaranteed to still exist while the task runs. That is why, in the speaker's words, "we know at each and every point which tasks are running, which tasks are complete," and when something completes, "we know exactly what needs to be continued from and what needs to be cancelled."

### The analogy: std::thread versus std::jthread

The speaker's favourite way to think about this is the difference between two very similar standard classes. `std::thread` arrived in C++11. `std::jthread`, the "joining thread", arrived later. (In the talk it is attributed to C++14, but it was actually added in C++20.)

- A `std::thread` object and the thread of execution it represents have **independent** lifetimes. You can `detach()` the thread so that it outlives the object. If you destroy a still-joinable `std::thread` without calling `join()` or `detach()`, the program calls `std::terminate`.
- A `std::jthread` **ties** the two lifetimes together. Its destructor requests a stop (through the C++20 `std::stop_token` mechanism) and then joins. If the `jthread` object has been destroyed, you know its execution is over.

**Listing 33** — std::thread vs std::jthread: tying a task's lifetime to an object *(illustrative example, not from the talk)*

```cpp
#include <stop_token>
#include <thread>

void with_thread() {
    std::thread t([] { /* work */ });
    // If we return or throw here before join(), ~thread() sees a
    // joinable thread and calls std::terminate.
    t.join();  // must remember this on every path
}

void with_jthread() {
    std::jthread t([](std::stop_token st) {
        while (!st.stop_requested()) {
            /* do a small piece of work */
        }
    });
    // ... any code, including an early return or an exception ...
}   // ~jthread(): request_stop(), then join(). The task cannot outlive t.

int main() {
    with_thread();
    with_jthread();
}
```

In the first function, an early `return` or an exception thrown before `join()` would terminate the program. Correctness depends on remembering to write the right call on every path. In the second function, the closing brace does the work: a stop is requested, and the destructor waits for the thread to finish. That is RAII for a task.

`std::jthread` is only an analogy. It is still a heavyweight OS thread, and it handles only one task at a time. Structured concurrency generalises the same guarantee to any number of lightweight tasks, nested arbitrarily deep.

### The closing brace as a "symbol of power"

The speaker describes the closing brace `}` as "a symbol of power." With ordinary RAII, you look at a brace and know that every object declared inside the block lives until that point and no longer, unless it was returned upward. Structured concurrency gives braces the same meaning for tasks:

> Leaving a scope means every task defined within it is either **done** (joined) or **returned upward** (`std::move`'d to the enclosing owner).

In pseudo-code, using a hypothetical `scope` type (sometimes called a *nursery*, a name borrowed from Python's Trio library):

`{ scope s; s.spawn(a); s.spawn(b); }  // both a and b are finished here`

`a` and `b` may run concurrently, but they cannot outlive `s`, and `s` cannot outlive the brace. Someone reading this code does not need to trace callbacks to find out when `a` and `b` end. The brace tells them.

The second option, moving upward, is what a function does when it *returns* a task object, such as a coroutine handle wrapper or a sender, instead of running it. Ownership moves to the caller in the same way a `std::unique_ptr` can be returned. The task is still owned. It now belongs to the caller's scope.

### A minimal scope you can build today

The standard library does not yet ship a general "spawn into a scope" type for threads. C++26 adds scopes for senders (`std::execution::counting_scope`), and libraries such as NVIDIA's stdexec have offered `exec::async_scope` for some time. You can still build a small thread-based version from C++20 parts, which shows the mechanics clearly. This is an illustration, not production code: it uses a full OS thread per task.

**Listing 34** — A minimal thread-based task scope (nursery) with error propagation and cancellation *(illustrative example, not from the talk)*

```cpp
#include <chrono>
#include <cstdio>
#include <exception>
#include <mutex>
#include <stdexcept>
#include <stop_token>
#include <thread>
#include <utility>
#include <vector>

using namespace std::chrono_literals;

class task_scope {
public:
    task_scope() = default;
    task_scope(const task_scope&) = delete;             // children hold `this`:
    task_scope& operator=(const task_scope&) = delete;  // the scope must not move

    template <class F>
    void spawn(F f) {
        threads_.emplace_back([this, f = std::move(f)]() mutable {
            try {
                f(stop_.get_token());
            } catch (...) {
                {
                    std::lock_guard lock(m_);
                    if (!error_) error_ = std::current_exception();
                }
                stop_.request_stop();  // cancel all siblings
            }
        });
    }

    // Wait for every child, then surface the first error (if any).
    void join() {
        for (auto& t : threads_) t.join();
        threads_.clear();
        if (error_) std::rethrow_exception(error_);
    }

    ~task_scope() { stop_.request_stop(); }  // then threads_ joins below

private:
    std::stop_source stop_;
    std::mutex m_;
    std::exception_ptr error_;
    std::vector<std::jthread> threads_;  // declared last => destroyed (joined) first
};

// Run a block that spawns children; return only when all of them are done.
template <class Body>
void with_scope(Body body) {
    task_scope s;
    body(s);
    s.join();
}

int main() {
    try {
        with_scope([](task_scope& s) {
            s.spawn([](std::stop_token st) {  // task a: long-running
                while (!st.stop_requested()) std::this_thread::sleep_for(10ms);
                std::puts("a: cancelled, cleaning up");
            });
            s.spawn([](std::stop_token) {     // task b: fails
                std::this_thread::sleep_for(50ms);
                throw std::runtime_error("b failed");
            });
        });  // both a and b are finished here
    } catch (const std::exception& e) {
        std::printf("scope failed: %s\n", e.what());
    }
}
```

Walking through it:

- `task_scope` owns a `std::stop_source` that all of its children share, plus a vector of `std::jthread`s. The vector is declared last, so it is destroyed first. Every child is therefore joined before the stop source and the error slot disappear. This is the "scope outlives its tasks" rule enforced by member order.
- `spawn` starts a child and gives it the scope's stop token. If the child throws, the exception is stored (only the first one is kept), and `request_stop()` tells all siblings that their work is no longer needed.
- `join()` waits for every child and then rethrows the stored error. The error therefore surfaces at a predictable point, the end of the scope, rather than on some random thread.
- `with_scope` wraps this as "run a block, then join." If the body itself throws, the `task_scope` destructor still requests stop and the `jthread` destructors still join. No child is left running.

The scope uses a mutex internally, which is a raw synchronization primitive. That is acceptable here. Following Sean Parent's guideline from earlier in the chapter, the primitive is written **once**, inside a reusable abstraction, and application code never touches it.

### How errors and cancellation propagate

Trace the scenario in `main` step by step:

1. The scope spawns `a` (a long-running loop that checks its stop token) and `b`.
2. `b` throws. The scope records the exception and requests stop.
3. `a` sees `stop_requested()`, leaves its loop and runs its cleanup.
4. Both children have now finished, so `join()` completes and rethrows `b`'s error.
5. The error leaves the `with_scope` call at the closing brace and is caught by ordinary `try`/`catch` in `main`.

This is what the speaker means by **intuitive propagation of cancellation, error handling and cleanup**. Failure travels *up* the tree to the nearest owner. Cancellation travels *down* the tree from that owner to all the siblings. Cleanup happens because every node waits for its children before it ends. Because the structure is a tree, "which subtasks are involved when something fails?" has an obvious answer: all the descendants of the scope where the failure is handled.

Compare this with callbacks. If one callback chain fails, nothing in the code says which other chains should stop, or who is responsible for reporting the error.

### Why a tree means less shared_ptr and fewer allocations

`shared_ptr` exists for situations where ownership is **unclear**: several parties might be the last user of an object, so the decision is made at run time by counting. The echo session needed `shared_from_this` because no single piece of code owned the session. Each pending lambda was a temporary co-owner.

In a tree of scopes, ownership is never unclear. A child always finishes before its parent, so a child can hold a plain reference to anything its parent owns. You do not need reference counting to answer "is this still alive?", because the structure already guarantees the answer is yes. The speaker points out that, because nested lifetimes are known in advance, the state of child operations can often be stored *inline* inside the parent's state, the way local variables are stored inside a stack frame. As a result, many structured designs need no heap allocations at all. That matters a great deal in the low-latency setting the speaker comes from. Section 13 shows how senders take this furthest.

### Back to the echo session

This is what the echo session looks like when written in a structured style. The example uses Asio's C++20 coroutine support (Asio is a library, standalone or in Boost, and not part of the standard). The next section explains why coroutines provide these guarantees. For now, look at where the lifetimes are.

**Listing 35** — The echo session as a structured coroutine (Asio, no shared_from_this) *(illustrative example, not from the talk)*

```cpp
// Requires standalone Asio (or Boost.Asio with the boost:: namespaces).
#include <asio.hpp>
#include <cstddef>
#include <exception>

using asio::ip::tcp;

// The socket and buffer live in the coroutine frame for exactly
// as long as the session runs. No heap-allocated session object,
// no reference counting.
asio::awaitable<void> session(tcp::socket socket) {
    char data[1024];
    try {
        for (;;) {
            std::size_t n = co_await socket.async_read_some(
                asio::buffer(data), asio::use_awaitable);
            co_await asio::async_write(
                socket, asio::buffer(data, n), asio::use_awaitable);
        }
    } catch (const std::exception&) {
        // EOF or I/O error: fall out of the loop. The frame (socket
        // included) is destroyed when the coroutine finishes.
    }
}

asio::awaitable<void> listener(tcp::acceptor acceptor) {
    for (;;) {
        tcp::socket socket = co_await acceptor.async_accept(asio::use_awaitable);
        // Still unstructured at this level: sessions are detached.
        // A fully structured server would spawn them into a scope
        // owned by the listener.
        asio::co_spawn(acceptor.get_executor(), session(std::move(socket)),
                       asio::detached);
    }
}

int main() {
    asio::io_context ctx;
    asio::co_spawn(ctx, listener(tcp::acceptor(ctx, {tcp::v4(), 5555})),
                   asio::detached);
    ctx.run();
}
```

The whole session is one coroutine. The socket is a by-value parameter and the buffer is a local array. Both live in the coroutine frame for exactly as long as the session runs. When the loop ends, because the client disconnected or an error was thrown, the frame is destroyed and the socket is closed. There is no `enable_shared_from_this`, no `self` capture, and no mutual "recursion" between `do_read` and `do_write`. Each `co_await` marks the point where the work is split into non-blocking pieces, just as each async call did in the CPS version. The difference is that the code between them stays in one function with one lifetime.

One unstructured point remains in this sketch: the listener starts each session with `asio::detached`, so nothing waits for the sessions to finish. A fully structured server would place the sessions in a scope owned by the listener, so that shutting down the listener cancels and joins all live sessions. That is the same nursery pattern as above, one level higher in the tree.

> **Takeaway:** In the speaker's framing, structured concurrency does not change *what* work gets split into non-blocking tasks. It changes *who owns* each task. Once every task has a well-defined owner and every owner outlives its tasks, lifetimes, errors and cancellation follow from reading the braces.

## 12. Coroutines Are Structured Concurrency

*Talk segment: [31:20](https://www.youtube.com/watch?v=ttoQfPP_Gkg&t=1880s)*

The previous section described structured concurrency in the abstract. Tasks are resources owned by scopes, scopes outlive their tasks, and cancellation and errors flow along a tree. This section makes a concrete claim from the talk: **C++20 coroutines, combined with a suitable library, already are structured concurrency.** To see why, we need to look at what actually happens when a coroutine runs and suspends.

### What turns a function into a coroutine

In C++20, a function becomes a coroutine as soon as its body uses any of three keywords: `co_await`, `co_yield` or `co_return`. Nothing in the signature marks it as special. The return type does matter, though. It must name a type that has a nested `promise_type`, or one for which `std::coroutine_traits` is specialized. That promise type tells the compiler how this kind of coroutine should behave:

- Does it start running immediately or lazily?
- Where does its result go?
- What happens to exceptions?
- What happens when it finishes?

Libraries such as Asio (`asio::awaitable<T>`), cppcoro (`task<T>`), Boost.Cobalt and the upcoming C++26 `std::execution::task` all provide such return types. The standard itself only provides the low-level machinery in `<coroutine>`.

### The coroutine frame and the return object

The speaker starts with the first mechanical fact: **invoking a coroutine creates a coroutine frame and a return object.**

- The **coroutine frame** is a block of memory the compiler creates to hold everything the coroutine needs to survive a suspension. That includes its parameters (copied or moved in), its local variables, the promise object, and a record of which suspension point it is currently stopped at.
- The frame is usually heap-allocated, because it must outlive the stack frame of the call that created it. Compilers can sometimes prove that a coroutine's lifetime is nested inside its caller's. In that case they elide the allocation and place the frame in the caller's frame (often called HALO, heap allocation elision). This optimization is not guaranteed, which matters later in the talk when performance comes up.
- The **return object** is what the caller actually receives, for example an `awaitable<int>` or a `task<int>`. It typically holds a `std::coroutine_handle` to the frame, so it can be an owning RAII handle: when the return object dies, it can destroy the frame.

The ownership relationship is already visible here. A coroutine is not a free-floating thread of control. It is an object whose frame can be owned, moved and destroyed like any other resource. That is exactly the "tasks are resources with lifetimes" idea from the structured concurrency philosophy.

### co_await and the awaiter protocol

The second mechanical fact is that **`co_await` can suspend the coroutine**. When that happens the entire state (all the variables in the frame) is preserved, together with the **awaiter** object, if there is one.

An *awaitable* is anything you can write after `co_await`. The compiler turns it into an *awaiter*, an object with three member functions:

| Member | Question it answers | Typical use |
|---|---|---|
| `await_ready()` | "Is the result already available, so we can skip suspending?" | Return `true` for an already-completed operation (a fast path) |
| `await_suspend(handle)` | "We are suspending; who will resume us, and when?" | Store the handle of the suspended coroutine, start the async operation, register a callback |
| `await_resume()` | "We have been resumed; what is the value of the `co_await` expression?" | Return the result, or throw or return an error |

The handle passed to `await_suspend` is the key. It is a `std::coroutine_handle<>` pointing at the suspended coroutine's frame. Whoever holds it can later call `.resume()` to continue the coroutine, or `.destroy()` to tear down its frame.

Here is a deliberately tiny, self-contained example of the protocol. A global queue plays the role of a reactor. The awaiter parks the coroutine in that queue, and `main` acts as the event loop.

**Listing 36** — The three-method awaiter protocol, with a toy reactor queue *(illustrative example, not from the talk)*

```cpp
#include <coroutine>
#include <deque>
#include <exception>
#include <iostream>

// A toy "reactor": suspended coroutines waiting to be resumed.
std::deque<std::coroutine_handle<>> ready_queue;

// An awaiter: co_await next_tick{v} suspends, and later yields v.
struct next_tick {
    int value;
    bool await_ready() const noexcept { return false; }   // 1. always suspend
    void await_suspend(std::coroutine_handle<> h) {      // 2. park ourselves
        ready_queue.push_back(h);
    }
    int await_resume() const noexcept { return value; }  // 3. result of co_await
};

// Deliberately UNstructured coroutine type: nobody owns the frame,
// it destroys itself at the end (like std::thread::detach()).
struct detached {
    struct promise_type {
        detached get_return_object() noexcept { return {}; }
        std::suspend_never initial_suspend() noexcept { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        void return_void() noexcept {}
        void unhandled_exception() noexcept { std::terminate(); }
    };
};

detached worker(const char* name) {
    int a = co_await next_tick{1};   // locals live in the coroutine frame
    std::cout << name << " got " << a << '\n';
    int b = co_await next_tick{2};
    std::cout << name << " got " << b << '\n';
}

int main() {
    worker("A");   // runs until its first co_await, then returns here
    worker("B");
    while (!ready_queue.empty()) {   // the event loop
        auto h = ready_queue.front();
        ready_queue.pop_front();
        h.resume();
    }
    // Prints: A got 1, B got 1, A got 2, B got 2
}

```

Walk through it:

- `next_tick::await_ready` returns `false`, so every `co_await next_tick{...}` really suspends.
- `await_suspend` stores the handle in `ready_queue`. Suspension means `worker` returns control to its caller (`main`) at that point. The local `a` and the parameter `name` are safe in the frame.
- When `main` later calls `h.resume()`, execution continues right after the `co_await`, and `await_resume()` provides the value.
- The output interleaves the two workers (`A got 1`, `B got 1`, `A got 2`, `B got 2`). This is cooperative concurrency on a single thread, which is the reactor model from earlier sections with sequential-looking code.

Notice the `detached` return type. Its `final_suspend` returns `std::suspend_never`, so the frame destroys itself when the body ends. Nobody owns it and nobody is told when it finishes. This is the coroutine equivalent of `std::thread::detach()`: it works, but it is **not** structured. The structured version is the subject of the next subsection.

### Why coroutines are structured: the awaiter links parent and child

The speaker's central point is this: **once suspended, a coroutine can be resumed or destroyed, and both involve the awaiter.** Because the awaiter sits between the suspended coroutine and the operation it is waiting for, the awaiter is where the structured invariants can be enforced.

- **The awaiter can point to the parent coroutine.** The handle received in `await_suspend` is the *continuation*: the code that must run when the child finishes. The child never needs a `shared_ptr` back to its parent. It just resumes the parent when done.
- **The awaiter's destructor can ensure children are cancelled or completed.** If the parent is destroyed while suspended, for example because its own parent was cancelled, the awaiter's destructor runs as part of destroying the frame. It can cancel the child operation or wait for it, so no child outlives its scope.
- **`await_resume()` handles errors as well as success.** The child's outcome travels back up through `await_resume`. It can return a value, rethrow an exception the child captured, or return an error type. Errors therefore propagate along the same path as results.

A minimal *lazy task* type shows all three ideas in one place. This is a common pattern in coroutine libraries (cppcoro's `task<T>` follows the same design). The code below is a simplified illustration of it, not the speaker's code.

**Listing 37** — A minimal structured lazy task: continuation, error propagation, RAII frame ownership *(illustrative example, not from the talk)*

```cpp
#include <coroutine>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <utility>

template <class T>
class task {
public:
    struct promise_type {
        T value{};
        std::exception_ptr error;
        std::coroutine_handle<> continuation = std::noop_coroutine();

        task get_return_object() {
            return task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        // Lazy: the child does nothing until a parent awaits it.
        std::suspend_always initial_suspend() noexcept { return {}; }

        struct final_awaiter {
            bool await_ready() noexcept { return false; }
            // When the child finishes, transfer control back to the parent.
            std::coroutine_handle<> await_suspend(
                std::coroutine_handle<promise_type> h) noexcept {
                return h.promise().continuation;
            }
            void await_resume() noexcept {}
        };
        final_awaiter final_suspend() noexcept { return {}; }

        void return_value(T v) { value = std::move(v); }
        void unhandled_exception() { error = std::current_exception(); }
    };

    task(task&& other) noexcept : h_(std::exchange(other.h_, {})) {}
    task& operator=(task&&) = delete;
    ~task() { if (h_) h_.destroy(); }   // RAII: the frame dies with its owner

    // --- awaiter interface: lets a parent coroutine co_await this task ---
    bool await_ready() const noexcept { return false; }
    std::coroutine_handle<> await_suspend(std::coroutine_handle<> parent) noexcept {
        h_.promise().continuation = parent;  // the child remembers its parent
        return h_;                           // start the child (symmetric transfer)
    }
    T await_resume() {                       // success OR error flows upward
        if (h_.promise().error) std::rethrow_exception(h_.promise().error);
        return std::move(h_.promise().value);
    }

    // Top-level entry point; fine here because everything below is synchronous.
    T sync_wait() {
        h_.resume();
        return await_resume();
    }

private:
    explicit task(std::coroutine_handle<promise_type> h) : h_(h) {}
    std::coroutine_handle<promise_type> h_;
};

task<int> child(int x) {
    if (x < 0) throw std::invalid_argument("negative input");
    co_return x * 2;
}

task<int> parent() {
    int a = co_await child(20);   // child frame owned by a temporary task
    int b = co_await child(1);
    try {
        co_await child(-1);       // the error surfaces via await_resume()
    } catch (const std::exception& e) {
        std::cout << "child failed: " << e.what() << '\n';
    }
    co_return a + b;              // every child is finished by now
}

int main() {
    std::cout << parent().sync_wait() << '\n';   // prints 42
}

```

The structure to notice:

1. `initial_suspend` returns `suspend_always`, so `child(20)` creates a frame but does nothing until someone `co_await`s it. A task cannot run without an owner who is waiting for it.
2. In `task::await_suspend(parent)`, the child records `parent` as its `continuation` and then starts itself. This is the "awaiter points to the parent" link.
3. When the child finishes, `final_awaiter::await_suspend` returns the continuation, so the parent resumes. Because the child has merely *suspended* at its final point, its frame and its result still exist.
4. The parent's `await_resume()` either returns the value or rethrows the stored exception. Errors flow upward exactly like results.
5. `~task()` destroys the child's frame. The child's lifetime is tied to the `task` object, which lives inside the parent's frame. This gives a tree of frames with no reference counting.

Compare this with the asio callback example from the earlier section, which needed `enable_shared_from_this` and a captured `self` in every lambda just to keep the session alive. Here, lifetime follows scope. When `parent` reaches its closing brace, every child it awaited is done.

### One-to-many: composing awaitables

So far each `co_await` waited for exactly one child. The speaker points out that a single `co_await` can also spawn **several** subtasks and wait for **all** of them or for **any** of them. This is done by *composing awaitables*: combining two awaitables into a new awaitable that represents the combined operation.

Asio provides this as an experimental feature, in the header `asio/experimental/awaitable_operators.hpp`:

- `a && b`: run both, resume when **both** complete ("when-all"). The result combines both results.
- `a || b`: run both, resume when the **first** completes ("when-any"). The other is cancelled.

These operators are library features, not part of the C++ standard. They live in the `experimental` namespace, so their interface may change.

### Racing a read against a timer

The speaker illustrates `operator||` with a slide from Daniela Engert's CppCon 2022 talk *Contemporary C++ in Action*. The slide shows type signatures in a pseudo-code notation, followed by one line of real usage:

**Listing 38** — code from the talk (Daniela Engert on Coroutine Composition), shown at [34:19](https://www.youtube.com/watch?v=ttoQfPP_Gkg&t=2059s)

```cpp
async_read(socket, data) → awaitable<tuple1>;

timer::async_wait() → awaitable<tuple2>;

operator||(awaitable<tuple1>, awaitable<tuple2>)
    → awaitable<variant<tuple1, tuple2>>;


auto result = co_await (async_read(S, data) || T.async_wait());
```

Line by line:

- `async_read(socket, data) → awaitable<tuple1>;` Reading from a socket is an awaitable operation. When it completes it produces some result, here called `tuple1` (typically the error code and the number of bytes read).
- `timer::async_wait() → awaitable<tuple2>;` Waiting on a timer is also awaitable and produces its own result, `tuple2` (typically just an error code).
- `operator||(awaitable<tuple1>, awaitable<tuple2>) → awaitable<variant<tuple1, tuple2>>;` This is the composition. Combining the two with `||` produces a single new awaitable. Its result is a `std::variant` because only one of the two operations wins, and the variant records which one it was and holds that one's result.
- `auto result = co_await (async_read(S, data) || T.async_wait());` The real code. One `co_await` starts **both** child operations, suspends the coroutine, and resumes it as soon as the first one completes. The loser is **cancelled automatically**, and its cancellation is completed before the coroutine continues. `result` then holds either the read's result or the timer's result.

This is the classic **timeout pattern**: "read some data, but give up if the timer fires first." Engert's slide describes it as composing awaitables *with automatic mutual cancellation*. The speaker calls code like this "correct by design" or "correct by inspection". Reading the one line tells you everything:

- Both children belong to this `co_await`.
- Neither can outlive it.
- Exactly one result comes back, with its origin recorded in the type.

In a callback-based design, the same logic needs a timer callback that cancels the read and a read callback that cancels the timer. Both callbacks must agree on which one "won", and both must keep shared state alive until the slower one has also run. That is a lot of manual reasoning about lifetimes and races.

> **Key insight:** This is the "race several paths and cancel the losers" idea from the motivation section (the *when-any* operation used for tail-latency hiding). Here cancellation is built into the composition and is not bolted on by hand. Because the structure guarantees that the loser is cancelled and finished before the parent resumes, cancellation becomes cheap and safe instead of risky.

The same pattern written against the real Asio API looks like this. It is a sketch for illustration, not code from the talk.

**Listing 39** — Timeout via Asio's experimental operator|| (library feature, not standard) *(illustrative example, not from the talk)*

```cpp
// Requires Boost.Asio (or standalone Asio with the namespace adjusted).
// awaitable_operators is an *experimental* Asio feature, not part of ISO C++.
#include <boost/asio.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <chrono>
#include <iostream>
#include <variant>

namespace asio = boost::asio;
using namespace asio::experimental::awaitable_operators;
using namespace std::chrono_literals;

// Read some bytes, or give up after 2 seconds - whichever happens first.
asio::awaitable<void> read_with_timeout(asio::ip::tcp::socket& socket) {
    char buf[1024];
    asio::steady_timer timer(co_await asio::this_coro::executor, 2s);

    // awaitable<std::size_t> || awaitable<void>
    //   -> awaitable<std::variant<std::size_t, std::monostate>>
    auto result = co_await (
        socket.async_read_some(asio::buffer(buf), asio::use_awaitable) ||
        timer.async_wait(asio::use_awaitable));

    if (result.index() == 0)
        std::cout << "read " << std::get<0>(result) << " bytes\n";
    else
        std::cout << "timed out; the pending read was cancelled\n";
    // No manual cancellation, no shared_ptr: buf and timer simply live
    // in this coroutine's frame until both children are finished.
}

```

Note how the `void` result of `async_wait` shows up as `std::monostate` in the variant. Also note that the code never explicitly cancels anything: the `||` operator handles it. The `timer` and `buf` are ordinary locals in the coroutine frame. They stay alive exactly as long as needed because the frame cannot finish while either child is still pending.

### Putting it together

The speaker sums up: with coroutines we have a lot of power to build structured concurrency systems, with **one-to-one** relationships (a plain `co_await` of a child task) and **one-to-many** relationships (composed awaitables such as `&&` and `||`). The pieces that make this work are:

- frames that hold state across suspensions;
- return objects that own those frames;
- awaiters that link children to their parents and enforce cancellation, completion and error propagation.

The language provides these mechanics, but the useful task types and combinators come from libraries. That is why later sections recommend starting a migration with an existing library rather than building these types from scratch.

## 13. Senders Are Structured Concurrency

*Talk segment: [34:05](https://www.youtube.com/watch?v=ttoQfPP_Gkg&t=2045s)*

Coroutines gave us one road to structured concurrency. The talk now turns to the second road, which arrives in the standard with C++26: **senders and receivers**, specified in the `std::execution` library (proposal P2300). The speaker's claim is in the section title: senders are structured concurrency too. Like coroutines, they let you split work into non-blocking pieces while keeping a strict parent-child structure between those pieces. They get there by a different, lower-level route.

This section first builds the vocabulary: sender, receiver, operation state and scheduler. It then explains why composition sits at the heart of the model and how it resembles C++20 ranges. After that it looks at the speaker's comparison between senders and coroutines, and ends with the observation that makes the comparison less pressing than it first seems.

### The vocabulary: four small concepts

The senders model has four main roles. Each is easy to define on its own. The power comes from how they fit together.

| Term | What it is | Rough analogy |
|---|---|---|
| **Sender** | A *lazy description* of some asynchronous work. It says what will happen and what kinds of results it can produce, but does nothing by itself. | A recipe |
| **Receiver** | The object that is told how the work ended. It has three completion channels: `set_value(...)`, `set_error(e)` and `set_stopped()`. | The person waiting for the dish |
| **Operation state** | The object you get from `connect(sender, receiver)`. It holds everything the running operation needs. Calling `start(op)` begins the work. | The kitchen, set up and running |
| **Scheduler** | A lightweight handle to an *execution resource*: a thread pool, an event loop, a GPU stream. `schedule(sch)` returns a sender that completes *on* that resource. | The address of a kitchen |

The slide says a sender represents "an asynchronous computation, with multiple potential results". The three receiver channels make this concrete:

- **value**: the work succeeded and produced these values (possibly several, possibly none);
- **error**: the work failed, for example with an `std::exception_ptr` or an `std::error_code`;
- **stopped**: the work was cancelled. Its result is no longer needed, so it is neither a success nor a failure.

Compare this with a plain completion callback from the CPS section, which usually has one signature and leaves error handling and cancellation as ad-hoc conventions. Here, success, failure and cancellation are each a first-class part of the protocol. Every adaptor knows how to forward all three, so errors and cancellation propagate along the structure on their own. This is exactly the "cancellation, error handling and cleanup propagate intuitively along the tree" property promised by structured concurrency.

### Laziness: nothing happens until you connect and start

The most important mental shift is that **a sender is not running work**. Creating `just(1) | then(f)` does not call `f`. It builds an object that *describes* "produce 1, then apply `f` to it". Work only begins after two further steps:

1. `connect` the sender to a receiver, producing an operation state;
2. `start` that operation state.

In everyday code you rarely do these steps by hand. A *sender consumer* does them for you. One example is `std::this_thread::sync_wait`, which blocks until the result is ready. Another is `co_await` inside a coroutine. A third is a scope that launches work in the background.

If you know C++20 ranges, this laziness should feel familiar. The speaker draws the parallel explicitly: like range adaptors, sender composition builds **expression templates**.

**Listing 40** — Ranges analogy: a lazy pipeline that runs only when iterated *(illustrative example, not from the talk)*

```cpp
#include <iostream>
#include <ranges>
#include <vector>

int main() {
    std::vector<int> v{1, 2, 3, 4, 5, 6};

    // Builds a nested view object (an expression template). Nothing is computed yet.
    auto pipeline = v
        | std::views::filter([](int x) { return x % 2 == 0; })
        | std::views::transform([](int x) { return x * 10; });

    // Work happens only now, element by element, as the loop pulls values.
    for (int x : pipeline) {
        std::cout << x << ' ';   // prints: 20 40 60
    }
    std::cout << '\n';
}
```

In this example, `pipeline` is a small object that remembers "filter with this lambda, then transform with that one". Nothing is filtered or transformed until the `for` loop pulls elements through. Its type is a nested template, roughly `transform_view<filter_view<ref_view<vector<int>>, L1>, L2>`. Each adaptor wraps the previous one.

Senders work the same way.

**Listing 41** — The sender equivalent: describe first, run later (C++26 std::execution) *(illustrative example, not from the talk)*

```cpp
#include <execution>   // C++26; with NVIDIA stdexec use <stdexec/execution.hpp> and stdexec::
#include <iostream>

namespace ex = std::execution;

int main() {
    // A lazy description: "produce 1, then add 41". f has NOT been called yet.
    auto work = ex::just(1)
              | ex::then([](int x) {
                    std::cout << "running then\n";
                    return x + 41;
                });

    std::cout << "sender built, nothing ran\n";

    // sync_wait connects the sender to its own receiver, starts it, and blocks.
    // Returns std::optional<std::tuple<int>>; empty if the work was stopped.
    auto result = std::this_thread::sync_wait(std::move(work));
    auto [value] = result.value();
    std::cout << "value = " << value << '\n';   // value = 42
}
```

The sender `work` is an object whose type wraps `just`'s sender inside `then`'s sender. Only `sync_wait` connects it to an internal receiver, starts it, and waits for the result. Notice that `sync_wait` returns an `optional<tuple<...>>`. The optional is empty if the work completed with *stopped*. If it completed with an *error*, the error is rethrown. All three channels are accounted for.

### The onion: composition is the core operation

The slide states: "The core operation on senders is composition, via various sender-adaptors." A **sender adaptor** takes one or more senders and returns a new sender. Some examples:

- `then(f)`: when the previous sender produces values, call `f` on them.
- `let_value(f)`: like `then`, but `f` returns *another sender*, which becomes the next step.
- `when_all(s1, s2, ...)`: run several senders and complete when all of them have completed.
- `continues_on(sch)`: move the rest of the work onto another scheduler.
- `upon_error`, `upon_stopped`: react to the other two channels.

Because every adaptor returns a sender, adaptors nest. The result, in the speaker's words, is an "onion" object: one layer contains the next, which contains the next. From the outside you see only the outermost layer, but if you could look inside, the object describes the entire computation. The speaker also notes that composition can produce large trees of operations, and sometimes graphs, including cyclic ones such as repeat-until loops.

> **Key insight:** sender adaptors *themselves* guarantee structured concurrency. A well-implemented adaptor keeps the invariant that child operations complete before their parent completes. For example, `when_all` does not report completion while one of its children is still running. If one child fails, it requests cancellation of the others and waits for them to finish before passing the error upward.

This is why structured concurrency comes "for free" with senders. You do not enforce the discipline by hand at every call site. It is built into the building blocks, and composing correct pieces yields a correct whole. That is the composability and local reasoning the talk has been aiming for.

The next section looks at the onion in detail: how `connect` travels from the outside in, and how completions travel from the inside out.

### Under the hood: connect, start, and immovable operation states

To see where the memory benefits come from, it helps to perform the steps that `sync_wait` normally hides. The example below is our own illustration, not code from the talk. It writes a receiver by hand and drives a sender manually.

**Listing 42** — Doing connect/start by hand with a custom receiver *(illustrative example, not from the talk)*

```cpp
#include <execution>   // C++26 std::execution
#include <exception>
#include <cstdio>
#include <utility>

namespace ex = std::execution;

// A receiver: the three completion channels of the protocol.
struct print_receiver {
    using receiver_concept = ex::receiver_t;

    void set_value(int v) && noexcept { std::printf("value: %d\n", v); }
    void set_error(std::exception_ptr) && noexcept { std::puts("error"); }
    void set_stopped() && noexcept { std::puts("stopped"); }
};

int main() {
    auto snd = ex::just(20) | ex::then([](int x) { return x + 22; });
    // snd is only a description; it is movable and nothing has run.

    auto op = ex::connect(std::move(snd), print_receiver{});
    // op is the operation state: a statically-typed nested struct living on the
    // stack. It is immovable, because a running operation may be pointed into.

    ex::start(op);   // now the work runs (inline here) and prints "value: 42"
}
```

Walking through it:

- `print_receiver` declares `receiver_concept = receiver_t`. This opts in to being a receiver. It then provides the three completion functions. They are `&&`-qualified because each receiver is completed exactly once and is consumed by that completion. They are `noexcept` because a completion has nowhere left to report a failure.
- `ex::just(20) | ex::then(...)` builds the sender. It has not run.
- `ex::connect(std::move(snd), print_receiver{})` produces the operation state `op`. Its type is fully known at compile time. It is a nested struct containing the `then` stage's state, which contains the `just` stage's state and the receiver.
- `ex::start(op)` runs the work. Here everything happens inline, so `value: 42` is printed before `start` returns.

Two properties of `op` matter a great deal:

1. **Its size and layout are known statically.** Because the whole onion is a single concrete type, the compiler knows exactly how many bytes the running operation needs. The object can live on the stack, as here, or as a member inside a parent's operation state.
2. **It is immovable.** Once started, pieces of the operation may hold pointers into it. For example, an I/O operation may register its address with an event loop. Moving it would leave those pointers dangling, so operation states are deliberately neither copyable nor movable. Senders, by contrast, are just descriptions, so they are freely moved around while you compose them.

Together these explain the speaker's claim: "More control of memory layout, thus no need for heap allocations." The operation state does not move, and it lives in a place whose lifetime covers the whole operation: the enclosing scope or the parent operation. So nothing needs to be allocated on the heap, and nothing needs a `shared_ptr` to work out when it may be destroyed. Compare the asio echo session from the callbacks section, which needed `enable_shared_from_this` and a captured `self` in every lambda just to keep itself alive. With senders, the tree structure of operation states *is* the lifetime management.

The speaker is candid about the price. There are "certain rules": some objects must be immovable, others must be moved from place to place. The rules are "very organized", but implementing senders and adaptors yourself can be more challenging than writing a coroutine.

### Senders versus coroutines: bare-bones but more capable

The speaker summarizes the difference this way: senders are "more bare-bones than coroutines, and are more capable".

| Aspect | Coroutines | Senders |
|---|---|---|
| Where state lives | A coroutine frame created by the compiler, often heap-allocated unless the compiler can elide the allocation | An operation state whose layout you know exactly; no heap allocation needed |
| Who does the work | You trust the compiler to transform your function and manage the frame | You (or the library author) control each layer directly |
| Interaction with execution resources | Through awaitables and their awaiters | Directly, via schedulers and the sender/receiver protocol |
| Ease of writing | Reads like sequential code (`co_await` in a loop) | Lower level; chains of adaptors take practice to read and write |
| Structured concurrency | Enforced by awaiters that know their parent continuation | Enforced by the adaptors themselves |

"Capable" here means control: there is "less need for awaitables, frames, etc.", and you do not have to rely on the compiler doing the right thing. "Bare-bones" is the other side of that control. You work at a lower level, which, as the speaker puts it, has an impact on expressiveness and on how easy the code is to write.

### The key insight: senders are awaitables, and awaitables are senders

After weighing these trade-offs, the speaker argues that you often do not need to choose. He quotes Eric Niebler's well-known observation: **senders are awaitables and awaitables are senders.** In `std::execution`:

- A coroutine whose promise type supports it, such as a sender-aware task type, can `co_await` a sender directly. The sender's value channel becomes the result of the `co_await`, the error channel becomes a thrown exception, and the stopped channel cancels the coroutine.
- Conversely, an awaitable type, such as a coroutine task, can be used anywhere a sender is expected. You can pass it to `when_all`, pipe it into `then`, or hand it to `sync_wait`.

**Listing 43** — Interoperability: co_await a sender, and use a coroutine as a sender (stdexec library) *(illustrative example, not from the talk)*

```cpp
// Uses NVIDIA stdexec: stdexec:: mirrors C++26 std::execution,
// exec:: holds non-standard extras (task, static_thread_pool).
#include <stdexec/execution.hpp>
#include <exec/task.hpp>
#include <exec/static_thread_pool.hpp>

exec::task<int> compute(stdexec::scheduler auto sch) {
    // Senders are awaitables: this sender runs a step on the thread pool.
    int a = co_await (stdexec::schedule(sch)
                      | stdexec::then([] { return 40; }));
    co_return a + 2;
}

int main() {
    exec::static_thread_pool pool{4};

    // Awaitables are senders: the coroutine task is consumed by sync_wait
    // exactly like any other sender.
    auto [result] = stdexec::sync_wait(compute(pool.get_scheduler())).value();
    return result == 42 ? 0 : 1;
}
```

This example uses NVIDIA's **stdexec** library, the reference implementation of P2300. Its `stdexec::` namespace mirrors `std::execution`. Its `exec::` namespace holds extras that are not standard, here `exec::task` and `exec::static_thread_pool`. C++26 itself also adds a standard `std::execution::task` coroutine type. Inside `compute`, a sender built from a scheduler is simply `co_await`ed. In `main`, the coroutine task is handed to `sync_wait` as if it were any other sender.

The speaker's conclusion is practical. When you build a large concurrent system, you do not have to declare up front that it will be "a sender-based system" or "a coroutine-based system". Structured concurrency lets you reason locally about each subcomponent. So you can implement each component in whichever model suits it best, perhaps coroutines where readability matters most and hand-tuned senders on a hot path, and connect them freely. The two models are, in his words, "intentionally meant to work together with each other".

This interoperability returns later in the talk as a migration strategy. Start with coroutines, which are easier to adopt today, and bring in senders where their extra control pays off.

## 14. The Sender/Receiver Onion

*Talk segment: [37:45](https://www.youtube.com/watch?v=ttoQfPP_Gkg&t=2265s)*

The previous section claimed that composing senders builds an "onion": an object made of nested layers, each describing one step of the computation. This section opens the onion and looks at its two layers of structure. The goal is to understand *why* the sender/receiver model has two kinds of objects instead of one. Once you see that, sender chains stop looking magical.

The diagram the speaker shows comes from Bryce Adelstein Lelbach's C++Now 2022 talk *C++ Standard Parallelism*. Barkan uses it as the simplest mental model of what a sender is.

### Three stages, one pipeline

Take a pipeline of three operations:

- `before` produces some value.
- `then(f)` transforms that value with a function `f`.
- `after` does something with the transformed value.

Written with the pipe operator, this is `before | then(f) | after`. To make it concrete, we will follow one set of values through the whole section:

- `before` produces `2`.
- `f` doubles its argument.
- `after` prints the result, `4`.

The speaker stresses what the pipe means. **Each `|` is a potential break between tasks.** The three stages do not have to run back-to-back on the same thread. `before` might be a socket read that completes milliseconds later on an I/O thread. `then(f)` might run on a CPU thread pool. `after` might run somewhere else again.

Whatever happens, the pipe still guarantees two things:

- **Ordering.** `f` runs only after `before` has completed, and `after` runs only after `f` has.
- **No idle waiting.** No execution resource (thread, stack, core) is held hostage while waiting for an earlier stage. Nothing blocks. The next stage is simply not started until its input exists.

### The talk's code: two onions

Here is the slide, written in pseudo-code. Names like `then_sender` and `after_sender` are illustrative, not real library types.

**Listing 44** — code from the talk (Bryce Adelstein Lelbach - The Sender/Receiver Onion), shown at [39:53](https://www.youtube.com/watch?v=ttoQfPP_Gkg&t=2393s)

```cpp
before | then(f) | after;

sender auto before_snd = …;
sender auto then_f_snd = then_sender(before_snd, f);
sender auto after_snd  = after_sender(then_f_snd);
…
    return connect(after_snd, …);
       return connect(then_f_snd, after_rcv);
          return connect(before_snd, then_f_rcv);
…

…
    set_value(before_rcv, …);
       set_value(then_f_rcv, before_val);
          set_value(after_rcv, f(before_val));
…
```

The snippet has three blocks. Read them one at a time.

**Block 1: building the sender onion.**

- `before | then(f) | after;` is the user-facing syntax. The next three lines show what it expands to.
- `sender auto before_snd = …;` is the innermost sender, whatever produces the initial value. `sender` here is the concept from `std::execution`.
- `sender auto then_f_snd = then_sender(before_snd, f);` shows that the `then` adaptor *wraps* `before_snd`. It stores the inner sender together with `f`. Nothing runs yet; this is only a description.
- `sender auto after_snd = after_sender(then_f_snd);` wraps that description in one more layer.

So `after_snd` contains `then_f_snd`, which contains `before_snd`. These are the sender layers of the onion. **The outermost layer is the last step.**

From the outside, the speaker points out, `after_snd` simply looks like "the result of `after`". Its holder can pass it around, store it, or compose it further. Nobody needs to know what is inside. This is the expression-template idea that range adaptors also use. The whole pipeline is a single value whose type encodes the structure, typically with no heap allocation.

**Block 2: `connect`, working outside-in.**

A sender is only a recipe. To actually run it, you `connect` it to a **receiver**. A receiver is an object with three completion channels:

| Channel | Meaning |
|---|---|
| `set_value` | success, with result values |
| `set_error` | failure, with an error |
| `set_stopped` | the operation was cancelled |

`connect(sender, receiver)` returns an **operation state**. That object holds everything the running operation needs, and it does nothing until `start` is called on it.

The indentation in the snippet shows nesting: each call happens *inside* the previous one.

- `return connect(after_snd, …);` is where the outside world connects the outermost sender to its own final receiver (the `…`).
- Inside that call, `after_snd` cannot do its job without the result of `then_f_snd`. So it creates a receiver of its own, `after_rcv`, which means "when you have a value, deliver it to the `after` step". It then calls `connect(then_f_snd, after_rcv)`.
- Inside *that* call, `then_f_snd` needs the result of `before_snd`. It creates `then_f_rcv` ("when you have a value, apply `f` and pass it on to `after_rcv`") and calls `connect(before_snd, then_f_rcv)`.

`connect` therefore peels the sender onion from the outside in. As it goes, it builds a *second* onion of receivers. That onion is nested the opposite way: `then_f_rcv` holds `after_rcv`, which holds the final receiver. The operation states nest the same way, so the whole running operation can live in one object.

**Block 3: `set_value`, working inside-out.**

When the operation is started and `before` finishes, completion signals travel back out through the receivers:

- `set_value(before_rcv, …);` is the moment `before` completes. On the slide, `before_rcv` is the receiver `before` was connected to, which is exactly `then_f_rcv`. The two lines below spell out what that call does.
- `set_value(then_f_rcv, before_val);` delivers `before`'s result to the `then` layer.
- `set_value(after_rcv, f(before_val));` is how the `then` layer completes: it applies `f` and forwards the result outward to the `after` layer.

Only when `after` itself completes does the final receiver, the one given by whoever started the whole thing, see a result.

### Tracing it with real numbers

Here is the whole lifecycle for `before` produces `2`, `f` doubles, and `after` prints. Try writing both sequences yourself before reading the table.

| Step | Phase | Call | Direction |
|---|---|---|---|
| 1 | connect | `connect(after_snd, final_rcv)` | outermost sender first |
| 2 | connect | `connect(then_f_snd, after_rcv)` | moving inward |
| 3 | connect | `connect(before_snd, then_f_rcv)` | innermost sender last |
| 4 | start | `start(op)`, so `before` begins its work | |
| 5 | complete | `set_value(then_f_rcv, 2)` | innermost receiver first |
| 6 | complete | `set_value(after_rcv, 4)`, since `f(2) == 4` | moving outward |
| 7 | complete | `after` prints `4`, then `set_value(final_rcv)` | outermost receiver last |

Between any two completion steps there could be an arbitrary delay or a scheduler hop. Step 5 might happen on an I/O thread long after step 4. None of that changes the shape of the table.

An error or a cancellation travels along exactly the same path. If `before` fails, it calls `set_error(then_f_rcv, e)`. The `then` layer does not call `f`; it forwards the error to `after_rcv`, and so on outward. This is how structured concurrency gets error and cancellation propagation "for free". The path is fixed by the shape of the onion.

### Why receivers exist at all

The speaker sums up the point of this picture in one sentence. Receivers exist because **the order of execution is the inverse of how we compose the algorithm**.

> We *compose* outside-in: the last step wraps everything before it, so the outermost object stands for the whole computation. We *execute* inside-out: the first step must finish before anything else can happen. Senders describe the first view and receivers carry the second. `connect` is the moment one onion is turned into the other.

In callback (CPS) code, the two views are tangled together. The continuation of each step is written by hand inside the previous call, and you manage lifetimes yourself, as we saw with the `shared_from_this` pattern. With senders, you write only the composition (`before | then(f) | after`), and the adaptors generate the continuation chain.

### You rarely write receivers yourself

As Barkan notes, application programmers are usually barely aware of receivers. Every adaptor (`then`, `let_value`, `upon_error`, and so on) defines its own receiver type internally and creates it during `connect`. You meet a receiver directly only when you write a new sender adaptor or bridge sender code to a legacy callback API.

To make the hidden machinery visible, here is a deliberately tiny, library-free toy that mimics the onion. It is not a conforming `std::execution` implementation: it has no error or stopped channels, no customization points, no environments. It exists only to print the order of events.

**Listing 45** — A toy onion: connect outside-in, set_value inside-out (no library, not std-conforming) *(illustrative example, not from the talk)*

```cpp
#include <iostream>
#include <utility>

// Innermost layer: a sender that produces the value 2.
struct before_sender {
    template <class Rcv>
    struct op {
        Rcv rcv;
        void start() {
            std::cout << "before: producing 2\n";
            rcv.set_value(2);          // completion starts at the innermost receiver
        }
    };
    template <class Rcv>
    op<Rcv> connect(Rcv r) const {
        std::cout << "connect(before_snd, then_f_rcv)\n";
        return op<Rcv>{std::move(r)};
    }
};

// Middle layer: then(f) wraps an inner sender.
template <class Inner, class F>
struct then_sender {
    Inner inner;
    F f;

    template <class Next>
    struct rcv_t {                     // the receiver this adaptor generates
        Next next;
        F f;
        void set_value(int v) {
            std::cout << "then_f_rcv got " << v << '\n';
            next.set_value(f(v));      // apply f, pass the result outward
        }
    };

    template <class Next>
    auto connect(Next r) const {
        std::cout << "connect(then_f_snd, after_rcv)\n";
        return inner.connect(rcv_t<Next>{std::move(r), f});  // recurse inward
    }
};

// Outer layer: after wraps an inner sender.
template <class Inner>
struct after_sender {
    Inner inner;

    template <class Next>
    struct rcv_t {
        Next next;
        void set_value(int v) {
            std::cout << "after_rcv got " << v << '\n';
            next.set_value(v);
        }
    };

    template <class Next>
    auto connect(Next r) const {
        std::cout << "connect(after_snd, final_rcv)\n";
        return inner.connect(rcv_t<Next>{std::move(r)});     // recurse inward
    }
};

// Whoever consumes the whole pipeline.
struct final_receiver {
    void set_value(int v) { std::cout << "final result: " << v << '\n'; }
};

int main() {
    auto f = [](int x) { return x * 2; };

    // Build the sender onion: after wraps then(f), which wraps before.
    before_sender before_snd{};
    then_sender<before_sender, decltype(f)> then_f_snd{before_snd, f};
    after_sender<decltype(then_f_snd)> after_snd{then_f_snd};

    // connect: outside-in. Produces one nested operation state on the stack.
    auto op = after_snd.connect(final_receiver{});

    // start: completion flows inside-out through the receiver onion.
    op.start();
}

```

The program's output matches the trace table exactly:

- First the three `connect` lines appear, outer to inner.
- Then `before` produces `2`.
- Then `then_f_rcv got 2`, `after_rcv got 4`, and finally `final result: 4`, inner to outer.

Notice also the type of `op` in `main`. It is a single stack object whose type nests all the receivers. No heap allocation was needed.

For contrast, here is the same pipeline written the way you actually would, using NVIDIA's stdexec library. In C++26 the same names live in `std::execution`. The receivers and operation states from the toy are all still there, generated by `just`, `then` and `sync_wait`.

**Listing 46** — The same pipeline with real sender adaptors (stdexec library; std::execution in C++26) *(illustrative example, not from the talk)*

```cpp
#include <stdexec/execution.hpp>   // NVIDIA stdexec; in C++26: <execution>
#include <iostream>
#include <utility>

int main() {
    // before: lift the value 2 into the asynchronous world
    auto before = stdexec::just(2);

    // before | then(f) | after   -- builds the sender onion, runs nothing yet
    auto pipeline = std::move(before)
                  | stdexec::then([](int x) { return x * 2; })        // f
                  | stdexec::then([](int y) {                          // after
                        std::cout << "after: " << y << '\n';
                    });

    // sync_wait supplies the final receiver, connects (outside-in),
    // starts the operation, and blocks until completion flows back out.
    [[maybe_unused]] auto result = stdexec::sync_wait(std::move(pipeline));
}

```

In this version:

- `sync_wait` plays the role of "whoever uses the sender". It supplies the final receiver, calls `connect` and `start`, and blocks the calling thread until the outermost receiver is completed.
- Everything in between is the onion. You wrote three stages, and the library built both the sender layers and the receiver layers from them.

## 15. Reading the Flow: Coroutine vs. Sender-Chain Implementations

*Talk segment: [39:59](https://www.youtube.com/watch?v=ttoQfPP_Gkg&t=2399s)*

So far we have looked at coroutines and senders in the abstract. The best way to feel the difference is to read the *same* algorithm written both ways. Barkan does this with a small, realistic networking task. He shows two slides side by side: one in coroutine style using Asio, and one in sender style written by Robert Leahy.

### The task: filling a buffer from a socket

We want to fill a buffer, described by a `std::span<std::byte>`, with data from a socket. The catch is a fact of life in network programming. A single read call may return **partial data**. You ask for 4096 bytes and might get 1500. So we have to keep reading, each time into the part of the buffer that is still empty, until the whole span is filled.

One more rule: if a read returns **zero bytes**, the peer has closed the connection (end of file). In this scenario we expect enough data to arrive to fill the whole span, so EOF is treated as an **error**.

### The coroutine version

**Listing 47** — code from the talk (Coroutines and Sender-Chains Show the Flow), shown at [41:05](https://www.youtube.com/watch?v=ttoQfPP_Gkg&t=2465s)

```cpp
asio::awaitable<void> read(tcp::socket socket,
                   std::span<std::byte> span) {
  while (!span.empty()) {
    std::size_t bytes = co_await
                    socket.async_read_some(
                       asio::buffer(span),
                       asio::use_awaitable);
    if (bytes == 0) throw eof_error{};
    span = span.subspan(bytes);
  }
}
```

Here is the code line by line:

- **`asio::awaitable<void> read(tcp::socket socket, std::span<std::byte> span)`**: the return type `asio::awaitable<void>` makes this a coroutine. It is Asio's coroutine task type. Calling `read` does not run the loop to completion. It creates a coroutine frame and hands back an awaitable that someone must `co_await` or spawn. Both parameters are taken **by value**.
- **`while (!span.empty())`**: an ordinary loop. Keep going while there is still room left in the buffer.
- **`co_await socket.async_read_some(asio::buffer(span), asio::use_awaitable)`**: this starts a non-blocking read into the remaining buffer. The `asio::use_awaitable` completion token tells Asio to return something the coroutine can `co_await`. At this point the coroutine **suspends**. The thread is free to run other work, and the coroutine resumes when the read completes, with `bytes` holding the number of bytes received.
- **`if (bytes == 0) throw eof_error{};`**: the EOF check. The code throws a normal C++ exception, and the coroutine machinery carries it to whoever awaits `read`.
- **`span = span.subspan(bytes);`**: shrink the span so it covers only the part not yet filled. On the next iteration we read into that remainder.
- Falling off the end of the loop completes the coroutine with `void`.

If you squint, this is exactly the code you would write with a *blocking* socket. The only extra is `co_await` in front of the read. That is the readability promise of coroutines.

> A side note on real Asio: when the peer closes the connection, Asio normally reports `asio::error::eof` as an error code, and `use_awaitable` turns that into an exception. The zero-byte check on the slide follows the generic "a read of 0 means EOF" convention, which is also what the sender version relies on.

### The sender version

**Listing 48** — code from the talk (Coroutines and Sender-Chains Show the Flow), shown at [42:31](https://www.youtube.com/watch?v=ttoQfPP_Gkg&t=2551s)

```cpp
template<typename Readable>
auto read(Readable& readable, const std::span<std::byte> span) {
  return
    std::execution::just(span) |
    std::execution::let_value([&readable](
      std::span<std::byte>& span)
    {
      return ::exec::repeat_effect_until(
        std::exeutiion::just() |
        std::execution::let_value([&readable, &span]() {
          return readable.read(span);
        }) |
        std::execution::then([&span](std::size_t bytes) {
          if (!bytes) {
            throw eof_error{};
          }
          span = span.subspan(bytes);
          return span.empty();
        }));
    });
}
```

This one is longer. Barkan suggests reading it in two passes: first notice the overall shape, a pipeline of stages joined by `|` very much like range adaptors, and only then look at the details. Note that the slide contains a typo: `std::exeutiion::just()` should read `std::execution::just()`.

The details, line by line:

- **`template<typename Readable> auto read(Readable& readable, const std::span<std::byte> span)`**: the function is generic over anything that has a `read(span)` member returning a sender. `readable` is taken **by reference**. The span parameter is `const`. That means the span *object* (pointer plus length) cannot be reassigned. The bytes it points to are still writable.
- **`return std::execution::just(span) | ...`**: `just(span)` is a sender that, when started, immediately completes with the value `span`. Barkan describes it as **lifting** a value from the ordinary synchronous world into the asynchronous world.
- **`std::execution::let_value([&readable](std::span<std::byte>& span) { ... })`**: this is the key adaptor, explained in detail below. When `just` completes, `let_value` stores the span inside its operation state. It then calls the lambda with a **non-const lvalue reference** to that stored copy. This inner `span` shadows the function parameter, so from here on `span` refers to the mutable copy owned by `let_value`. The lambda must return a sender.
- **`return ::exec::repeat_effect_until( ... );`**: the sender returned here is a loop. `repeat_effect_until` takes a sender that completes with a `bool`. It runs that sender, and if the result is `false` it runs it again, until it gets `true`. This algorithm is **not** in C++26. It comes from a library: the `exec::` namespace of NVIDIA's stdexec.
- **`just() | let_value([&readable, &span]() { return readable.read(span); })`**: the body of the loop begins. `just()` completes with no values, so this `let_value` lambda takes no arguments. It returns the sender produced by `readable.read(span)`, the asynchronous read. That sender eventually completes with a `std::size_t`.
- **`then([&span](std::size_t bytes) { ... })`**: when the read completes, `then` calls this lambda with the byte count. It throws on zero bytes. Inside `then`, the exception is caught and sent down the **error channel** as an `exception_ptr`. Otherwise it shrinks the span, exactly like the coroutine does. It then returns `span.empty()`: `true` means "done, stop repeating".
- When the loop finishes, `repeat_effect_until` completes, the outer `let_value` completes, and the whole sender completes.

### Mapping one to the other

The two versions express the same algorithm, piece by piece:

| Role | Coroutine (Listing 47) | Sender chain (Listing 48) |
|---|---|---|
| Keep the buffer state alive | Parameter `span` taken by value, stored in the coroutine frame | `just(span)` followed by `let_value`, which owns a copy in its operation state |
| Loop | `while (!span.empty())` | `repeat_effect_until(...)`, with the `then` lambda returning `span.empty()` |
| Asynchronous step | `co_await socket.async_read_some(...)` | `let_value([&]{ return readable.read(span); })` |
| Error on EOF | `throw eof_error{}` inside the loop | `throw eof_error{}` inside `then`, which becomes `set_error` |
| Progress | `span = span.subspan(bytes)` | the same statement inside `then` |
| Completion | Fall off the end of the coroutine | `repeat_effect_until` completes, then the outer sender completes |

The loop shapes are not quite identical. `while` tests its condition *before* the first iteration. `repeat_effect_until` behaves like a `do ... while`: it tests *after* each run of the body. If the caller passes an already-empty span, the coroutine does nothing at all. The sender version still issues one read into a zero-length buffer. Such a read may well report 0 bytes and trigger `eof_error`. Translating control flow between the two styles is mechanical, but not completely brainless.

### Where does the work split?

One reason to rewrite code in a structured style is that the code itself shows you where the non-blocking boundaries are. Barkan gives a simple reading rule for each style:

- **Coroutines:** look for `co_await`. Each `co_await` is a point where the coroutine may suspend. The code before and after it runs as separate non-blocking chunks.
- **Sender chains:** look for the **pipe operator** `|`. Each pipe marks a *potential* break between tasks. Some adaptors run their continuation inline, so not every pipe is an actual suspension. But the pipes are where splitting *can* happen.

In both cases you can read the flow from top to bottom. That is the contrast with the callback style of earlier sections. With callbacks, the code after an async call was "probably a bug" and you had to chase lambdas to see what happened next.

### Lifetimes: who keeps the span alive?

The most instructive difference between the two versions is how they keep state alive across suspensions.

**Coroutine version.** The compiler does the work. Because `span` (and `socket`) are taken by value, they live in the **coroutine frame**, not on the thread's stack. The frame exists for as long as the coroutine exists. The read operation may be split into many separate chunks, possibly resumed on different threads, yet `span` is valid the whole time. It can be mutated freely with `subspan` and handed to library calls. You get this without writing anything special.

**Sender version.** Barkan's point is that there is "no magic place where every object lives until everything is complete." You must arrange storage yourself, and the tools for that are `just` and `let_value`:

1. `just(span)` captures the value into the sender.
2. When the chain is connected and started, the value flows into `let_value`. `let_value` **stores it in its operation state**, the object that represents the running operation and lives until the whole operation completes.
3. The lambda receives a **reference** to that stored value. Every later stage refers back to it by reference: the read lambda, the `then` lambda, and the loop condition.

The parameter of `read` is `const`, but the copy that `let_value` holds is not. That is why the chain can keep shrinking it.

### `let_value` versus `then`

These two adaptors look similar, so the difference is worth stating carefully:

- **`then(f)`**: when the predecessor completes with values `v...`, call `f(v...)` and complete with **whatever value `f` returns**. It is a synchronous transformation of a value. The arguments are temporary: once `f` returns, they are gone.
- **`let_value(f)`**: when the predecessor completes with values `v...`, **store them** in the operation state, call `f` with lvalue references to them, and expect `f` to return **a new sender**. That sender is connected and started, and its completion becomes the completion of the whole `let_value`. The stored values stay alive until that inner sender finishes.

This is what Barkan points out in the Q&A: `let_value`'s function returns a *sender*. That is how the Listing 48 code can start an asynchronous read from inside a stage (`return readable.read(span);`). A `then` could not do this, because it would complete with the sender object as a plain value instead of running it.

**Listing 49** — then versus let_value (stdexec) *(illustrative example, not from the talk)*

```cpp
#include <stdexec/execution.hpp>
#include <cstdio>
#include <string>
#include <utility>

int main() {
  // then: synchronously transform the incoming value into a new value.
  auto len1 = stdexec::just(std::string{"hello"})
            | stdexec::then([](std::string s) { return s.size(); });

  // let_value: the incoming value is stored in the operation state,
  // the function gets an lvalue reference to it and must return a SENDER.
  auto len2 = stdexec::just(std::string{"hello"})
            | stdexec::let_value([](std::string& s) {
                // s stays alive until this inner sender completes
                return stdexec::just()
                     | stdexec::then([&s] { return s.size(); });
              });

  auto [a] = stdexec::sync_wait(std::move(len1)).value();
  auto [b] = stdexec::sync_wait(std::move(len2)).value();
  std::printf("%zu %zu\n", a, b);  // prints: 5 5
}

```

In this example both pipelines compute the same number. The `let_value` version shows the two things `then` cannot do: it gives the inner work a stable `std::string&` to refer to, and it returns a sender that the framework then runs.

### Why the reference captures are safe

The sender version captures everything by reference: `[&readable]`, `[&readable, &span]`, `[&span]`. In callback code that would be alarming. Recall the `shared_from_this` dance from the Asio echo server. Here the captures are safe because of structure:

- `span` refers to the copy owned by the outer `let_value`. That copy lives until the sender returned by the lambda completes, which is the *entire* `repeat_effect_until` loop. The inner lambdas run strictly inside that window, so the reference cannot dangle.
- `readable` refers to the caller's object. The function signature (`Readable&`) states a contract: the caller must keep `readable` alive until the operation completes. In structured concurrency that is natural, because the parent scope outlives its child operations. The coroutine version avoids even this contract by taking the socket by value into its frame.

The lifetime argument is the same tree-shaped reasoning from the structured concurrency philosophy section. Storage lives in an enclosing layer of the "onion", and inner layers borrow from it. Lifetimes nest, so references point outward and are always valid. No `shared_ptr` and no reference counting are needed, and the operation state can typically live wherever the final consumer puts it, for example on its stack, with no heap allocation.

### Both versions are non-blocking

An audience member asked whether the sender `read` blocks. It does not, and neither does the coroutine one:

- Calling the coroutine `read` creates a frame and returns an `awaitable`. The loop runs only as the coroutine is awaited and resumed, and each `co_await` lets the thread go do other work.
- Calling the sender `read` returns **immediately** with a sender. A sender is just a *description* of asynchronous work, and nothing has started yet. Someone must still compose it with other senders, `co_await` it from a coroutine (senders are awaitables), or launch it, for example with `sync_wait` or by spawning it into a scope.

### Readability versus control

Put side by side, the coroutine is plainly easier to read. It is a `while` loop with one `co_await`. The sender chain is longer, introduces vocabulary (`just`, `let_value`, `then`, `repeat_effect_until`), and makes you think about where values are stored.

This is Barkan's earlier claim that senders are "more bare-bones but more capable" made concrete. The coroutine relies on compiler machinery: a frame, possibly heap-allocated depending on the compiler's ability to elide it, with all locals kept alive for you. The sender chain makes every piece of state explicit. In return it gives you precise control over memory layout. The whole operation state is one object, built from nested templates, whose size is known at compile time. You decide exactly what is stored and for how long. Because senders and awaitables interoperate, you are not forced to choose one style for the whole program.

### Exercise: what if we captured a local by reference?

Suppose someone "simplifies" Listing 48 by dropping the outer `just(span) | let_value(...)`. They declare a local copy instead, `std::span<std::byte> s = span;`, inside `read`, and capture `[&s]` in the loop lambdas. What goes wrong?

Think it through before reading on. `read` *returns the sender immediately*. Nothing has run yet. When `read` returns, the local `s` is destroyed, but the sender still holds lambdas with references to it. When the sender is later started, every access to `s` is a **dangling reference**, which is undefined behaviour. It might even appear to work in a quick test. The same bug can occur with a reference to any temporary or parameter of `read` that is captured by a lazily-run lambda.

The fix is exactly what Leahy's code does: move the state *into* the asynchronous world with `just`, and let `let_value` own it.

**Listing 50** — Pitfall: capturing a local by reference in a lazy sender *(illustrative example, not from the talk)*

```cpp
#include <stdexec/execution.hpp>

// BROKEN: the sender is returned before it runs, so 'count' dangles.
auto bad_counter() {
  int count = 0;                                   // lives in bad_counter's stack frame
  return stdexec::just()
       | stdexec::then([&count] { return ++count; }); // reference to a soon-dead local
}                                                  // count destroyed here; nothing has run yet

// FIXED: lift the state into the asynchronous world and let let_value own it.
auto good_counter() {
  return stdexec::just(0)
       | stdexec::let_value([](int& count) {       // count lives in the operation state
           return stdexec::just()
                | stdexec::then([&count] { return ++count; });
         });
}

int main() {
  // auto [x] = stdexec::sync_wait(bad_counter()).value();  // undefined behaviour!
  auto [y] = stdexec::sync_wait(good_counter()).value();
  return y == 1 ? 0 : 1;
}

```

Finally, here is the looping building block on its own. It is a small countdown that uses the same `just | let_value(... repeat_effect_until(just() | then(...)))` shape as Listing 48, without the networking.

**Listing 51** — A loop with exec::repeat_effect_until (stdexec library, not standard) *(illustrative example, not from the talk)*

```cpp
#include <stdexec/execution.hpp>
#include <exec/repeat_effect_until.hpp>   // library extension, not in C++26
#include <cstdio>
#include <utility>

int main() {
  auto countdown =
      stdexec::just(3)
    | stdexec::let_value([](int& n) {      // n owned by let_value's operation state
        return exec::repeat_effect_until(
            stdexec::just()
          | stdexec::then([&n] {
              std::printf("tick %d\n", n);
              return --n == 0;             // true => stop repeating
            }));
      });

  stdexec::sync_wait(std::move(countdown)); // prints tick 3, tick 2, tick 1
}

```

Listing 49 to Listing 51 use NVIDIA's stdexec library (`stdexec::` for the facilities standardized in C++26 as `std::execution`, and `exec::` for extras). In standard C++26 the first three adaptors are spelled `std::execution::just`, `then` and `let_value`, and `sync_wait` lives in `std::this_thread`. `repeat_effect_until` has no standard equivalent yet.

## 16. Summary So Far and Migration Tip: Use Libraries, Keep Your Main Loop

*Talk segment: [46:00](https://www.youtube.com/watch?v=ttoQfPP_Gkg&t=2760s)*

Before turning to migration, the speaker pauses to recap the argument so far. The recap explains *why* the migration tips that follow look the way they do.

### Where we stand: a recap

The speaker's summary has two halves. The first is about the problem.

- **Concurrent systems handle complex scenarios of incoming events.** Requests arrive in unknown orders. Some are independent and some depend on each other. The system should start useful work as early as possible.
- **Peak performance may mean giving up convenient helpers.** Threads, per-task stacks and heap allocations are easy to use. Each one costs something: context switches, memory, allocator traffic. A system tuned for the lowest latency often wants to remove them entirely.

The second half is about the tools.

- **Older approaches are error-prone.** Hand-written state machines, callbacks/CPS and raw synchronization primitives all demand constant attention to detail. Recall the asio echo session and its `shared_from_this()` captures. The speaker singles out three trouble spots:
  - managing object lifetimes,
  - handling complex scenarios (dependencies, race conditions),
  - errors and cancellation, which he calls "very, very messy" in older designs.
- **Structured concurrency is a composable, low-overhead approach.** Tasks are owned by scopes, scopes nest, and nothing escapes its scope unnoticed. Code becomes something you can check *locally*.
- **Both coroutines and senders (`std::execution`) provide structured concurrency.** As the previous sections showed, the two models interoperate: senders are awaitables and awaitables are senders.

> The motivation, in the speaker's words, is to "move to a world that should be simpler and easier to reason about," where each piece of code can be locally understood to be correct.

That raises the practical question this part of the chapter tackles: you already have a large, working concurrent system. How do you get from here to there? The speaker stresses there is **no silver bullet, just tips**. He points listeners to other talks that walk through concrete conversions of specific frameworks. This section covers the first two tips. The next section covers where to start.

### Tip 1: Import an existing library, don't start from scratch

The standard pieces are deliberately minimal. C++20 gave us the *language machinery* for coroutines: `co_await`, `co_return`, promise types and awaiters. It gave us essentially no ready-to-use coroutine types. There is no standard task type, no `when_all`, no timers and no sockets.

C++26 improves the picture. `std::execution` brings senders, receivers, schedulers and core adaptors such as `then`, `let_value` and `when_all`. A standard coroutine task type (`std::execution::task`) is also planned. (The transcript says "C++29 tasks", but the context makes clear the speaker means this C++26 task type.) Even so, the speaker calls the standard "very, very bare bones". As section 15 showed, something as basic as a loop (`repeat_effect_until`) still lives outside the standard.

You are not limited to what is standardized. The speaker names these libraries:

| Library | Model | What it offers |
|---|---|---|
| **ASIO** (standalone, or Boost.Asio) | Callbacks, coroutines (`awaitable`, `co_spawn`), experimental `&&`/`\|\|` operators | Mature networking, timers and an event loop (`io_context`) |
| **cppcoro** | Coroutines | Building blocks: `task`, `generator`, `when_all`, async mutexes and more |
| **Boost.Cobalt** | Coroutines | A coroutine library designed to be compatible with Asio |
| **NVIDIA stdexec** | Senders | The battle-tested reference implementation of `std::execution`, plus extras in its `exec::` namespace (e.g. `exec::repeat_effect_until`) |
| **beman.execution** | Senders | An implementation aiming at conformance with the standard wording |
| **beman.net** | Senders | Sender-based networking, proposed for standardization |

Why does the choice of library matter beyond convenience? The speaker notes that many of these libraries are written by people who also sit on the C++ committee. What their authors learn in production is likely to shape future standards. Adopting such a library today is therefore less of a bet on a dead end than it might seem. The standard will probably converge toward the same designs, which makes later migration from library to standard types smoother.

### Tip 2: Keep control of your main loop

The second tip concerns a subtle trap. Many async frameworks want to *own* the thread they run on. In Asio, the event loop lives in an `io_context` object. Its most familiar method, `run()`, blocks the calling thread. It keeps executing handlers until there is no more work, or until someone calls `stop()`. The speaker's phrase is that `run()` "enslaves your thread" to be the framework's scheduler. Once you call it, your existing reactor no longer gets a turn.

That is a problem for an existing system. A mature low-latency service usually has its own main loop: a reactor or proactor thread that polls sockets, dispatches to state machines, and perhaps busy-spins for latency. If adopting a coroutine library means giving that loop away, the migration becomes an all-or-nothing rewrite. All-or-nothing rewrites of large working systems are, as the speaker says in the next part, "bound to fail".

Most of these libraries offer a way out: entry points that do *some* work and then return control to you. For Asio, the relevant `io_context` methods are:

| Method | Blocks? | What it does |
|---|---|---|
| `run()` | Yes | Runs handlers until the context runs out of work or is stopped |
| `run_one()` | Yes | Waits for and runs at most one handler |
| `poll()` | No | Runs every handler that is *ready right now*, then returns |
| `poll_one()` | No | Runs at most one ready handler, then returns |

The speaker highlights `poll()` as a "non-blocking task drainer". In the talk he describes it as performing a single piece of work. Strictly, that is `poll_one()`. `poll()` drains all handlers that are ready at that moment. Either way, the key property is the same: control comes back to *your* loop.

### A hybrid main loop

Putting the tip into practice, one thread can serve both worlds. The legacy reactor keeps processing its events exactly as before. On every iteration, the loop also gives the new coroutine-based framework a chance to make progress. The example below is not from the talk; it is a minimal sketch of this pattern using standalone Asio.

**Listing 52** — Hybrid main loop: legacy reactor and Asio coroutines sharing one thread *(illustrative example, not from the talk)*

```cpp
// Standalone Asio (https://think-async.com); with Boost, use <boost/asio.hpp>
// and the boost::asio namespace instead.
#include <asio.hpp>
#include <chrono>
#include <exception>
#include <iostream>

// Stand-in for an existing hand-written event loop.
struct LegacyReactor {
    int iterations = 0;
    void process_events() {
        ++iterations;
        // Real code: epoll_wait(fd, events, max, /*timeout=*/0),
        // then dispatch each event to its state machine.
    }
};

// New-style code: a coroutine running on the Asio io_context.
asio::awaitable<void> heartbeat(int count) {
    auto ex = co_await asio::this_coro::executor;
    asio::steady_timer timer(ex);
    for (int i = 0; i < count; ++i) {
        timer.expires_after(std::chrono::milliseconds(10));
        co_await timer.async_wait(asio::use_awaitable);  // suspension point
        std::cout << "heartbeat " << i << '\n';
    }
}

int main() {
    LegacyReactor legacy_reactor;
    asio::io_context io_ctx;

    // Prevent io_ctx from stopping itself when it is momentarily idle.
    auto guard = asio::make_work_guard(io_ctx);

    bool running = true;
    std::exception_ptr failure;

    asio::co_spawn(io_ctx, heartbeat(5),
                   [&](std::exception_ptr e) {
                       failure = e;      // keep errors, don't lose them
                       running = false;  // new-style work is done
                   });

    // We still own the loop; neither call below blocks.
    while (running) {
        legacy_reactor.process_events();
        io_ctx.poll();  // run every Asio handler that is ready right now
    }

    if (failure) std::rethrow_exception(failure);
    std::cout << "legacy reactor iterated "
              << legacy_reactor.iterations << " times\n";
}

```

The main points of the sketch:

- **`LegacyReactor`** stands in for your existing code. In a real system, `process_events()` would perform a non-blocking `epoll_wait` (timeout 0) and dispatch to state machines.
- **`heartbeat`** is new-style code: a coroutine with a sequential-looking loop and `co_await` at each suspension point. Nothing here knows about the legacy reactor.
- **`co_spawn`** launches the coroutine onto `io_ctx`. The completion handler receives a `std::exception_ptr`, so errors from the coroutine are not lost. The completion handler also clears the `running` flag.
- **`make_work_guard`** keeps `io_ctx` from considering itself "out of work" between iterations. This matters because of the pitfall shown in the next example.
- **The loop** alternates `legacy_reactor.process_events()` and `io_ctx.poll()`. Neither call blocks, so neither framework starves the other. This is the shape `while (running) { legacy_reactor.process_events(); io_ctx.poll(); }`.

Because everything runs on one thread, code in the legacy state machines and code in the new coroutines never run in parallel with each other. They can share data without mutexes, just as tasks within a single reactor thread could before. Over time you move flows one at a time out of the state machines and into coroutines. When the legacy reactor has little left to do, you can, in the speaker's words, "pull the plug and move all in". At that point you replace the hand-written loop with `io_ctx.run()`, or with the library's own scheduler.

### A pitfall: a context that ran out of work stays stopped

The work guard in the hybrid loop is not decoration. An Asio `io_context` automatically enters the *stopped* state when its last piece of outstanding work finishes. A stopped context ignores further `poll()` calls until you call `restart()`. In a hybrid loop that polls an initially idle context, work posted later would silently never run. The following small program, which is also not from the talk, makes the behaviour visible.

**Listing 53** — poll_one, poll, and the stopped-context pitfall *(illustrative example, not from the talk)*

```cpp
#include <asio.hpp>
#include <iostream>

int main() {
    asio::io_context ctx;

    for (int i = 0; i < 3; ++i)
        asio::post(ctx, [i] { std::cout << "task " << i << '\n'; });

    std::size_t a = ctx.poll_one();  // runs task 0 only -> 1
    std::size_t b = ctx.poll();      // runs tasks 1 and 2 -> 2

    // The last outstanding work finished, so ctx is now *stopped*.
    asio::post(ctx, [] { std::cout << "late task\n"; });
    std::size_t c = ctx.poll();      // stopped: runs nothing -> 0

    ctx.restart();                   // clear the stopped state
    std::size_t d = ctx.poll();      // runs "late task" -> 1

    std::cout << a << ' ' << b << ' ' << c << ' ' << d << '\n';  // 1 2 0 1

    // In a hybrid loop, holding asio::make_work_guard(ctx) avoids this
    // trap: the context never runs out of work, so it never auto-stops.
}

```

There are two common fixes:

- hold an `executor_work_guard` for as long as the hybrid loop runs, as in Listing 52;
- check `io_ctx.stopped()` and call `restart()` before polling.

The work guard is usually clearer, because it states your intent: "this context is expected to receive more work."

### Migration is incremental, not big-bang

The two tips share one theme. Reduce the size of each step.

- Using a library means you are not simultaneously inventing task types, schedulers and adaptors *and* rewriting your application.
- Keeping your main loop means the old and new frameworks can coexist on the same thread. Each change can be small, reviewable and individually testable.

> Structured concurrency promises code that is correct by construction. The *migration* toward it should be built the same way: as a sequence of small, locally verifiable steps rather than one giant rewrite.

The next section takes up the remaining practical questions: which framework to adopt first, and whether to begin converting at the root or the leaves of the call graph.

## 17. Where to Start: Root vs. Leaf, Function Coloring and Cancellation

*Talk segment: [49:47](https://www.youtube.com/watch?v=ttoQfPP_Gkg&t=2987s)*

The previous section suggested that you adopt an existing library and keep control of your main loop. Those tips reduce the *cost* of migrating. This section asks a more practical question: in a large, working system, **which code do you touch first?** Barkan states the risk directly: a huge change across everything "is bound to fail". You need a path that can be taken gradually, one reviewable step at a time.

His advice comes down to four tips:

1. Start with coroutines.
2. Choose the *direction* of migration (root-first or leaf-first) based on what your code looks like today.
3. Respect the problem of *function coloring*, and be creative in working around it.
4. Don't be afraid of cancellation. It is your main tool at the boundary between old and new code.

### Tip 1: Try coroutines first

Coroutines have been in the standard since C++20, and several mature libraries are built on them (Asio, Boost.Cobalt, cppcoro). Barkan's argument is partly social. Coroutine code "will typically seem easier on your eye" and "on your colleagues' eyes", because a `co_await` chain reads almost like the sequential code everyone already knows.

Starting with coroutines does not lock you out of senders. Section 13 noted that *senders are awaitables and awaitables are senders*. Pieces you migrate to coroutines today can later be composed with sender-based components, or rewritten as senders, without another big-bang change.

### Background: what is function coloring?

The talk names the "function coloring issue" without defining it. The term comes from Bob Nystrom's 2015 essay *What Color Is Your Function?* Imagine every function in a language is painted one of two colors:

- **Blue** functions are ordinary synchronous functions. You call them and they return a result.
- **Red** functions are asynchronous. In C++ that means a coroutine returning something like `asio::awaitable<T>`, or a function returning a sender. Calling one gives you a *description* or *handle* of work, not the result.

The colors interact asymmetrically:

| Caller | Callee | Is it easy? |
|---|---|---|
| blue | blue | Yes, a normal call |
| red | blue | Yes, a coroutine can call ordinary functions freely |
| red | red | Yes, `co_await` it (or compose the senders) |
| blue | red | **No**. The caller cannot `co_await`, so it must block on a bridge (e.g. `sync_wait`, a future) or become red itself |

The last row causes the trouble. Suppose a call tree `main → load_settings → fetch_config`, and you turn the **leaf** `fetch_config` red. Its blue caller `load_settings` can no longer simply use the result. The natural fix is to make `load_settings` red too, then *its* caller, and so on up to `main`. Barkan describes this cascade: "everyone who calls me needs to also change ... and I find myself in a big mess with a very large change."

The asymmetry also explains the rest of his advice. Turning a function red is cheap for its **callees**, which are untouched. It is expensive for its **callers**. So the cost of a migration step depends on how many callers the converted function has, and on whether you can avoid forcing them to change.

### Tip 2a: Non-blocking code? Start at the root

If your system is already non-blocking (a reactor or proactor with callbacks or state machines, as in sections 8–10), Barkan recommends starting from the **root** of the computation: the top-level function that drives everything, typically your main loop or session entry point.

This direction avoids the coloring problem. The root has essentially no callers except `main` or the scheduler, so turning it red cascades nowhere. And because a red function can call blue functions, the converted root can keep calling all the existing callback-based code unchanged.

The recipe from the slides has two steps:

1. **Change `return` to `co_return`.** The root is now a coroutine, launched on the structured framework's scheduler (e.g. an `io_context`). Nothing else changes. You are now "exercising this IO context or scheduler that's given to me by the structured concurrency framework", while the rest of the code stays as it was.
2. **Then decide where to `co_await`.** One flow at a time, replace a callback chain with `co_await` points. Each replacement moves one piece of work from the old framework into the new one.

**Listing 54** — Root-first in a non-blocking system: return -> co_return, then add co_await gradually (standalone Asio) *(illustrative example, not from the talk)*

```cpp
#include <asio.hpp>
#include <chrono>
#include <iostream>

using namespace std::chrono_literals;

// LEGACY continuation-passing code: left untouched by the migration.
void legacy_ticker(asio::steady_timer& t, int remaining) {
    if (remaining == 0) return;
    t.expires_after(100ms);
    t.async_wait([&t, remaining](const asio::error_code& ec) {
        if (ec) return;
        std::cout << "legacy tick " << remaining << '\n';
        legacy_ticker(t, remaining - 1);
    });
}

// Before the migration the root was:
//   void root(asio::steady_timer& t) { legacy_ticker(t, 3); return; }

// STEP 1: the root becomes a coroutine. The only change: return -> co_return.
asio::awaitable<void> root_step1(asio::steady_timer& legacy_timer) {
    legacy_ticker(legacy_timer, 3); // red code can call blue/callback code freely
    co_return;
}

// STEP 2 (later): one flow is migrated to co_await; the other is still legacy.
asio::awaitable<void> root_step2(asio::steady_timer& legacy_timer) {
    legacy_ticker(legacy_timer, 3);

    auto ex = co_await asio::this_coro::executor;
    asio::steady_timer t(ex); // lives in the coroutine frame: no shared_from_this needed
    for (int i = 3; i > 0; --i) {
        t.expires_after(150ms);
        co_await t.async_wait(asio::use_awaitable);
        std::cout << "coroutine tick " << i << '\n';
    }
}

int main() {
    asio::io_context io;
    asio::steady_timer legacy_timer(io);
    // Swap root_step1 for root_step2 as the migration progresses; main stays the same.
    asio::co_spawn(io, root_step2(legacy_timer), asio::detached);
    io.run();
}

```

Here is the example step by step:

- `legacy_ticker` stands in for existing continuation-passing code. It is never modified during the migration.
- `root_step1` is the first, nearly trivial step. The body is what the old root did, but `co_return` makes it a coroutine. It is launched with `co_spawn`, so it now runs on the coroutine machinery.
- `root_step2` is a later step. One flow has been rewritten as a `while`/`for` loop with `co_await`. Its timer lives in the coroutine frame, so there is no `shared_from_this`, no captured `self`, and no heap-allocated session object (compare section 10). The legacy flow still runs alongside it on the same `io_context`.
- `main` does not care which step you are on. That stability is what makes each step small enough to review and ship on its own.

> **Key insight:** in a non-blocking system the root is the cheapest function to recolor, because nobody calls it. Once the root is red, every `co_await` you add below it is a local change.

### Tip 2b: Blocking code? Go from the leaves towards the root

If your system is built on blocking calls in OS threads (or green threads), Barkan recommends the opposite direction: pick **leaf** components that can be rewritten asynchronously, and move them gradually towards the root.

This may look contradictory, since the coloring cascade starts at the leaves. That is why the slide adds "function-coloring is an issue – *be creative*". In a blocking system, every caller *expects* to block anyway. So you can rewrite a leaf as a coroutine internally and keep its old blue signature as a thin **bridge** that launches the coroutine and waits for the result. The callers do not change, and the cascade stops at the bridge. Later you can push the red color one level up, remove the bridge, and repeat.

**Listing 55** — Leaf-first in a blocking system: a red leaf behind a blue bridge (standalone Asio) *(illustrative example, not from the talk)*

```cpp
#include <asio.hpp>
#include <chrono>
#include <future>
#include <iostream>
#include <string>
#include <thread>

asio::io_context g_io; // the new reactor, run on its own thread

// NEW: the leaf is now a coroutine ("red").
asio::awaitable<std::string> fetch_config_async() {
    auto ex = co_await asio::this_coro::executor;
    asio::steady_timer t(ex, std::chrono::milliseconds(50)); // stand-in for real async I/O
    co_await t.async_wait(asio::use_awaitable);
    co_return std::string{"threads=4"};
}

// BRIDGE: same blue signature the old blocking function had.
// It stops the coloring cascade: callers do not change.
std::string fetch_config() {
    auto fut = asio::co_spawn(g_io, fetch_config_async(), asio::use_future);
    return fut.get(); // blocks, exactly as the old code did. Never call this on g_io's thread!
}

// UNCHANGED blue callers.
// Without the bridge, load_settings would have to become asio::awaitable<int>
// and co_await, and so would every caller up to main.
int parse_threads(const std::string& s) {
    return std::stoi(s.substr(s.find('=') + 1));
}
int load_settings() { return parse_threads(fetch_config()); }

int main() {
    auto guard = asio::make_work_guard(g_io);   // keep run() alive while idle
    std::jthread reactor([] { g_io.run(); });

    std::cout << "threads: " << load_settings() << '\n';

    guard.reset(); // let run() return so the jthread can join
}

```

What the bridge does:

- `fetch_config_async` is the new red leaf. It is a coroutine that suspends instead of blocking a thread.
- `fetch_config` keeps the exact signature the old blocking function had. It launches the coroutine on a reactor thread with `asio::use_future` and blocks on `.get()`. Blocking here is fine, because the caller blocked before the migration too.
- `load_settings` and `parse_threads` are untouched. Without the bridge, `load_settings` would have to become `asio::awaitable<int>`, and so would every caller up to `main`.
- A pitfall: never call `.get()` on the same thread that runs the `io_context`. The coroutine could never make progress and you would deadlock. Here the reactor runs on its own `std::jthread`.

Senders have the same kind of bridge. C++26's `std::this_thread::sync_wait` (and `stdexec::sync_wait` in NVIDIA's library) turns a sender into a blocking call that returns its result.

#### An audience trick: compile in both colors

During Q&A, an audience member proposed a "cheap and dirty" technique for leaf-first migrations. Write the migrated code with a macro, such as a capital-letter `CO_AWAIT`, that expands either to `co_await` or to nothing. The same source then compiles into two colors: an asynchronous build and a synchronous, blocking build. As Barkan restated it, this lets you check during migration that "I can get the same functional result with different runtime overheads and behaviors."

**Listing 56** — The audience's two-color macro: the same code built as async or blocking (standalone Asio) *(illustrative example, not from the talk)*

```cpp
#include <asio.hpp>
#include <cstddef>
#include <span>

// Build with -DASYNC_COLOR=1 for the coroutine version, 0 for the blocking one.
#ifndef ASYNC_COLOR
#define ASYNC_COLOR 1
#endif

#if ASYNC_COLOR
  #define TASK(T)            asio::awaitable<T>
  #define CO_AWAIT           co_await
  #define CO_RETURN          co_return
  #define READ_SOME(sock, b) (sock).async_read_some((b), asio::use_awaitable)
#else
  #define TASK(T)            T
  #define CO_AWAIT           /* nothing */
  #define CO_RETURN          return
  #define READ_SOME(sock, b) (sock).read_some(b)
#endif

// Fill the whole span from the socket (same task as section 15).
// In both colors, Asio reports EOF by throwing asio::error::eof.
TASK(std::size_t) read_exactly(asio::ip::tcp::socket& sock, std::span<std::byte> buf) {
    std::size_t total = 0;
    while (!buf.empty()) {
        std::size_t n = CO_AWAIT READ_SOME(sock, asio::buffer(buf.data(), buf.size()));
        total += n;
        buf = buf.subspan(n);
    }
    CO_RETURN total;
}

// Compare the two builds' outputs in your tests to check functional equivalence.
// Caveat: this does not test interleavings; in the async build, other tasks
// may run at every CO_AWAIT.

```

With `ASYNC_COLOR` set to 1, `read_exactly` is a coroutine returning `asio::awaitable<std::size_t>` and suspends at each read. With it set to 0, every macro disappears and it is an ordinary blocking function. This is the same "fill a span from a socket" task as in section 15. You can run your test suite against both builds and compare the outputs.

The check has a limit. It shows *functional* equivalence for a single flow. It does not show that interleavings are safe, because in the async build other tasks may run at every suspension point. Use it as a migration aid, not as proof of correctness.

### Tip 3: Don't fear cancellation

Barkan's last tip addresses a habit. In older frameworks, cancellation either did not exist or was "considered harmful or very, very risky". Cancelling a callback-based operation raised hard questions. Will the callback still fire? Is the object it captured still alive? Who cleans up? Teams learned to design around cancellation and avoid it.

Structured concurrency changes this. A task belongs to a scope (section 11), and the scope cannot end until its children have finished. A cancellation request therefore has a defined path: the request travels down the task tree, each child finishes by completing with "stopped" (or an error), and cleanup runs through ordinary destructors on the way back up. Barkan calls cancellation in this world "relatively cost-free and relatively stable".

This matters most at the **seam between legacy and structured code**. Suppose a component is only partly migrated: the bulk of the logic is in coroutines, and some legacy code still needs to influence it. A tempting design is to have the legacy code store a `std::coroutine_handle` and call `resume()` on it manually. That reintroduces the problems structured concurrency removed: resuming on the wrong thread, resuming twice, or resuming after the frame was destroyed.

Barkan's advice is to let the legacy side send a **cancellation request** whenever it decides it no longer needs the result. He says the legacy system can do this "freely or without care", because "there's not much risk". The structured side handles the request through its normal completion path.

#### Be creative with the `co_await` result type

The slide offers an alternative: "be creative with the co_await return type (std::expected?)". Rather than letting cancellation surface as an exception, which is what Asio's `use_awaitable` does by default, you can make the awaited expression produce a `std::expected<T, cancelled_t>`. A cancelled operation then looks like an ordinary value the coroutine can test with an `if`. This is the same success/stopped split that senders have natively (section 13).

**Listing 57** — Cancellation as a value: co_await yields std::expected<T, cancelled_t> (C++23 + standalone Asio) *(illustrative example, not from the talk)*

```cpp
#include <asio.hpp>
#include <chrono>
#include <expected>
#include <iostream>
#include <system_error>

using namespace std::chrono_literals;

struct cancelled_t {}; // "nobody needs this result any more"

// Wraps an Asio wait so that cancellation is a value, not an exception.
asio::awaitable<std::expected<void, cancelled_t>> wait_for(asio::steady_timer& t) {
    auto [ec] = co_await t.async_wait(asio::as_tuple(asio::use_awaitable));
    if (ec == asio::error::operation_aborted)
        co_return std::unexpected(cancelled_t{});
    if (ec)
        throw std::system_error(ec); // real errors still propagate as errors
    co_return std::expected<void, cancelled_t>{};
}

// Structured side: cancellation is just another branch.
asio::awaitable<void> consumer(asio::steady_timer& t) {
    auto r = co_await wait_for(t);
    if (!r) {
        std::cout << "cancelled - cleaning up and finishing normally\n";
        co_return;
    }
    std::cout << "event arrived\n";
}

int main() {
    asio::io_context io;
    asio::steady_timer event_timer(io, 10s); // the event we might wait a long time for

    asio::co_spawn(io, consumer(event_timer), asio::detached);

    // LEGACY side: it knows nothing about coroutine handles.
    // When it decides the result is no longer needed, it just requests cancellation.
    asio::steady_timer legacy_deadline(io, 100ms);
    legacy_deadline.async_wait([&](const asio::error_code&) { event_timer.cancel(); });

    io.run();
}

```

How it works:

- `cancelled_t` is an empty tag type meaning "nobody needs this result any more".
- `wait_for` uses Asio's `as_tuple` completion token, so the error code comes back as a value rather than as a thrown `system_error`. It translates `operation_aborted` into `std::unexpected(cancelled_t{})`, so any `co_await wait_for(...)` yields a `std::expected`.
- `consumer` treats cancellation as a normal branch: it logs, cleans up and `co_return`s.
- `legacy_deadline` stands in for old callback code. It does not know anything about coroutine handles. It just calls `cancel()` on the shared timer when it changes its mind.

### Putting the tips together

| Current architecture | Direction | First step | How coloring is handled |
|---|---|---|---|
| Non-blocking (reactor/proactor, callbacks, state machines) | Root → leaves | Change the root's `return` to `co_return`, spawn it on the new scheduler | The root has no callers; red code calls the existing blue/callback code freely |
| Blocking (OS threads, green threads) | Leaves → root | Rewrite one leaf as a coroutine (or sender) | Keep a blocking bridge with the old signature; optionally build both colors with a `CO_AWAIT` macro |
| Partly migrated component | Either | Move the bulk of the logic into structured code | Legacy code talks to it through cancellation or `expected`-returning awaits, not manual `resume()` |

> **Key insight:** migration is mostly about managing *boundaries*. Pick a starting point where the boundary with old code is small (the root of a non-blocking system, or a leaf behind a blocking bridge), and let cancellation, not manual coroutine resumption, carry signals across it.

## 18. Final Summary and Q&A: Testing and Performance

*Talk segment: [55:18](https://www.youtube.com/watch?v=ttoQfPP_Gkg&t=3318s)*

### A Paradigm, Not a Library

Barkan closes the talk with one claim: **structured concurrency is a paradigm shift**. It is not a set of functions you call. It is a way of thinking about how the concurrent pieces of a system relate to one another. You can include `<coroutine>` or `<execution>` and still write unstructured code: detached tasks, shared pointers keeping callbacks alive, ad-hoc flags for cancellation. You can also write structured code with very little library support, as long as you follow the discipline.

The correctness section of the talk made the same point with an analogy, and it is worth returning to now. Before RAII and value semantics became idiomatic, C++ programmers managed object lifetimes by hand with `new` and `delete`. The fix was not a smarter `delete`. It was a *structural rule*: every resource is owned by an object, every object lives in a scope, and the closing brace releases everything that scope owns. Once code follows that rule, you can check a function for leaks by reading it, without tracing every path through the program.

Structured concurrency applies the same rule to *work in flight*:

> **The structured-concurrency invariant, in one sentence:** every asynchronous task is owned by a scope, and that scope cannot end until the task has completed (successfully, with an error, or by cancellation) or has been explicitly handed to an enclosing scope.

Try restating it in your own words before moving on. If your version mentions *ownership*, *scope*, and *cannot outlive*, you have the core idea. Everything else in the chapter follows from this sentence:

- coroutine awaiters that destroy or cancel their children;
- sender adaptors that nest operation states like an onion;
- cancellation that propagates down a tree;
- errors that propagate up it.

### The Three Claims

Barkan's summary slide says that, *used correctly*, structured concurrency "claims readability, safety and low overhead". The word *claims* is deliberate. These are benefits you earn by following the discipline, not guarantees a header file gives you. Each one comes from a specific property:

| Claim | Where it comes from |
|---|---|
| Readability | The code reads top-to-bottom. `co_await` points or `|` pipes mark exactly where work splits into non-blocking chunks, as in the coroutine and sender-chain versions of the socket-reading loop. |
| Safety | Lifetimes are nested, so a child can borrow from its parent by reference without `shared_ptr`. Cancellation and errors travel along the ownership tree instead of through ad-hoc flags. |
| Low overhead | Tree-shaped ownership means no reference counting and often no heap allocation. Work runs on non-blocking schedulers instead of parking OS threads. |

### Two Routes in C++26

As of C++26 there are two standard-backed routes to structured concurrency. They interoperate: a sender is awaitable, and an awaitable can be treated as a sender.

| | Coroutines (C++20 language feature) | Senders (`std::execution`, C++26) |
|---|---|---|
| What the standard gives you | The language machinery only: frames, `co_await`, promise types. C++26 adds a standard `std::execution::task`, but the set of combinators is thin. | A full vocabulary of sender adaptors (`then`, `let_value`, `when_all`, ...), schedulers and stop tokens, "fresh out of the box" in Barkan's words. |
| What you still need | Third-party helpers such as Asio, Boost.Cobalt or cppcoro for task types, `||` / `&&` combinators and I/O. | Third-party adaptors for things not yet standardized, such as `repeat_effect_until` in stdexec's `exec::` namespace, plus networking (for example beman.net). |
| Typical feel | Looks like sequential code. | Explicit composition, with more control over memory layout. |

Barkan's practical advice is not to wait. C++26 toolchains will take time to arrive, but the libraries exist today. He suggests studying them now, downloading their examples, experimenting with them, and planning how your existing code base would be re-architected. The migration tips from the previous section (start from the root or the leaves depending on your current design, keep control of your main loop, don't fear cancellation) are meant to be tried in that exploratory phase.

### Q&A: Can a Deterministic Scheduler Replace Careful Design?

An audience member asked a good question. Earlier, Barkan said concurrent code is hard to test because the number of possible interleavings explodes. A sender scheduler is just an object, though. Could we plug in a *single-threaded scheduler with a seeded random order* of dispatch and test that way?

The question names two related techniques:

- **Deterministic simulation** means running the whole concurrent system on one thread with a scheduler that you control completely. Given the same inputs and the same seed, the run is identical every time. A failing test can therefore be replayed, stepped through in a debugger, and turned into a regression test.
- **Schedule fuzzing** means running the same test many times with different seeds. Each seed makes the scheduler pick a different order among the tasks that are ready, so you explore many interleavings instead of the one your machine happens to produce.

The example below is a deliberately tiny version of this idea, using C++20 coroutines and a seeded random pick among ready tasks.

**Listing 58** — Schedule fuzzing with a seeded single-threaded coroutine scheduler *(illustrative example, not from the talk)*

```cpp
#include <coroutine>
#include <cstdio>
#include <exception>
#include <random>
#include <vector>

// A deterministic, single-threaded scheduler that resumes ready
// coroutines in a random order chosen by a seed.
class fuzz_scheduler {
public:
    explicit fuzz_scheduler(unsigned seed) : rng_(seed) {}

    auto schedule() {
        struct awaiter {
            fuzz_scheduler* s;
            bool await_ready() const noexcept { return false; }
            void await_suspend(std::coroutine_handle<> h) { s->ready_.push_back(h); }
            void await_resume() const noexcept {}
        };
        return awaiter{this};
    }

    void run() {
        while (!ready_.empty()) {
            std::uniform_int_distribution<std::size_t> pick(0, ready_.size() - 1);
            const auto i = pick(rng_);
            auto h = ready_[i];
            ready_[i] = ready_.back();
            ready_.pop_back();
            h.resume();
        }
    }

private:
    std::mt19937 rng_;
    std::vector<std::coroutine_handle<>> ready_;
};

// Test-only fire-and-forget coroutine type (NOT structured!).
// Safe here only because run() drains every task before the
// scheduler and the log go out of scope.
struct job {
    struct promise_type {
        job get_return_object() noexcept { return {}; }
        std::suspend_never initial_suspend() noexcept { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        void return_void() noexcept {}
        void unhandled_exception() { std::terminate(); }
    };
};

job worker(fuzz_scheduler& s, int id, std::vector<int>& log) {
    for (int step = 0; step < 3; ++step) {
        co_await s.schedule();          // a point where interleaving may change
        log.push_back(id * 10 + step);
    }
}

int main() {
    for (unsigned seed = 0; seed < 5; ++seed) {
        fuzz_scheduler s{seed};
        std::vector<int> log;
        worker(s, 1, log);
        worker(s, 2, log);
        s.run();

        // Property check: each worker's own steps must appear in order.
        int last[3] = {-1, -1, -1};
        bool ok = true;
        for (int e : log) {
            const int id = e / 10, step = e % 10;
            ok = ok && step == last[id] + 1;
            last[id] = step;
        }

        std::printf("seed %u:", seed);
        for (int e : log) std::printf(" %d", e);
        std::printf("  -> %s\n", ok ? "ok" : "VIOLATION");
    }
}

```

The example works as follows:

- `fuzz_scheduler::schedule()` returns an awaiter. Its `await_suspend` parks the coroutine's handle in a `ready_` list instead of resuming it.
- `run()` repeatedly picks a *random* ready handle using `std::mt19937` seeded by the test, and resumes it. Because nothing else runs, the whole execution is reproducible from the seed.
- Each `worker` yields at three points, so different seeds produce different interleavings of workers 1 and 2.
- The test checks a *property* (each worker's own steps stay in order) rather than one exact output, because the exact interleaving is supposed to vary.
- The `job` type is fire-and-forget, which is *unstructured*. That is acceptable only in this toy harness, where `run()` drains every task before `s` and `log` go out of scope. Production code would use a structured task type from a library.

Barkan's answer had two parts.

1. **Such tools help, but the space is still huge.** Even a smart exploration strategy samples only a tiny fraction of the possible orderings, and some bugs need an unusual ordering combined with unusual inputs.
2. **Structure is what makes testing tractable.** In a well-composed system, the dangerous bugs (an unexpected sequence of events between two sub-components that *looked* unrelated) become much rarer by construction, because components cannot reach into each other's lifetimes or state outside the tree. So, he argues, you can test each component heavily in isolation and rely on the composition rules for the rest. He even ventures that you may not need to test the whole system "that much".

The way to read this exchange is that the two approaches are **complementary, not alternatives**:

- Structure shrinks the space of things that can go wrong *between* components.
- Deterministic simulation and schedule fuzzing explore the remaining space *within* a component, and make any failure reproducible.

Senders make the second technique cheap. Because work is described separately from where it runs, the same component can run on a thread pool in production and on a seeded test scheduler in CI without changing its code. This is the separation of concerns between application logic and framework scheduling that Barkan listed among the motivations for concurrency.

> Testing explores the state space; structure shrinks it. Fuzzing an unstructured system can only sample an enormous space, while fuzzing a structured one lets you cover each component's much smaller space well.

### Q&A: What Happens to Performance?

The second question was practical: in real migrations, did performance get better, stay the same, or get worse? Barkan's answer depends on where you start.

| Starting point | Expected outcome (per the speaker) |
|---|---|
| Thread-per-task or blocking design | Usually easy wins in throughput or resource usage. There are fewer OS threads, fewer context switches and no stacks parked on blocking calls. |
| Already-tuned reactor/proactor | Matching the existing performance "can be a challenge". A hand-tuned reactor already avoids threads and often allocations. |

He also stresses that raw speed is *not the primary goal*. The goal is code that is more readable, has fewer bugs, and is easier to maintain and extend. Performance parity with a tuned reactor is something you may have to work for.

#### The coroutine frame and HALO

The specific cost Barkan points to for coroutine-based frameworks is the **coroutine frame**. When you call a coroutine, the compiler must store its locals and suspension state somewhere that survives suspension. By default that is a heap allocation made through the promise type's `operator new`, or the global one.

Compilers may remove this allocation through **HALO** (Heap Allocation eLision Optimization). If the compiler can prove that the frame's lifetime is strictly nested inside the caller's, it may place the frame in the caller's own storage. HALO is permitted by the standard but never required. Whether it happens depends on:

- the compiler and the optimization level;
- inlining decisions;
- whether the coroutine handle *escapes*, for example by being stored in a scheduler queue, which defeats the proof.

This is what Barkan means by relying on the compiler: performance can vary between builds, and you may need to find the bottlenecks yourself. The next example makes frame allocations visible so you can measure them instead of guessing.

**Listing 59** — Making coroutine frame allocations (and HALO) visible *(illustrative example, not from the talk)*

```cpp
#include <coroutine>
#include <cstddef>
#include <cstdio>
#include <exception>
#include <new>
#include <utility>

inline std::size_t frame_allocs = 0;

struct lazy_int {
    struct promise_type {
        int value{};

        // Called for every frame allocation that is NOT elided by HALO.
        static void* operator new(std::size_t n) {
            ++frame_allocs;
            return ::operator new(n);
        }
        static void operator delete(void* p, std::size_t n) noexcept {
            ::operator delete(p, n);
        }

        lazy_int get_return_object() {
            return lazy_int{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        std::suspend_always initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        void return_value(int v) noexcept { value = v; }
        void unhandled_exception() { std::terminate(); }
    };

    explicit lazy_int(std::coroutine_handle<promise_type> h) : h_(h) {}
    lazy_int(lazy_int&& o) noexcept : h_(std::exchange(o.h_, {})) {}
    lazy_int& operator=(lazy_int&&) = delete;
    ~lazy_int() { if (h_) h_.destroy(); }

    int get() {
        h_.resume();
        return h_.promise().value;
    }

private:
    std::coroutine_handle<promise_type> h_;
};

lazy_int answer() { co_return 42; }

int main() {
    int sum = 0;
    for (int i = 0; i < 1000; ++i)
        sum += answer().get();   // frame lifetime is strictly nested: a HALO candidate

    // Typically 1000 at -O0; may drop to 0 with optimizations on some compilers.
    std::printf("sum=%d frame allocations=%zu\n", sum, frame_allocs);
}

```

How the example works:

- A class-specific `operator new` in `promise_type` is used for every frame allocation that actually happens, so it counts them.
- `answer()` is created, resumed once and destroyed inside a single expression, which is the easiest case for HALO. Depending on your compiler and flags you may see 1000 allocations, or 0.
- Change the optimization level, or store the `lazy_int` somewhere long-lived, and the count changes. This is exactly the kind of measurement you will need when hunting allocation hot spots after a migration.

You can also take control yourself. Libraries typically give their task types a custom `operator new` backed by a recycling allocator or arena, so that a frame allocation costs a pointer bump instead of a call into `malloc`.

#### Sender operation states: allocation-free by construction

Senders take a different approach, and it explains Barkan's earlier remark that they give more control over memory layout. A sender is only a *description*. Calling `connect(sender, receiver)` produces an **operation state**, an ordinary object whose type the compiler knows exactly. It nests the operation states of every inner stage, like the layers of the onion. That object can live wherever you choose to put it: on the stack, as a class member, or inside an outer operation state.

**Listing 60** — An allocation-free sender operation state on the stack (stdexec) *(illustrative example, not from the talk)*

```cpp
// Uses NVIDIA stdexec (namespace stdexec::), the reference implementation
// of C++26 std::execution. With a C++26 standard library, replace
// stdexec:: with std::execution::.
#include <stdexec/execution.hpp>
#include <cstdio>
#include <exception>

struct print_receiver {
    using receiver_concept = stdexec::receiver_t;

    void set_value(int v) noexcept { std::printf("value: %d\n", v); }
    void set_error(std::exception_ptr) noexcept { std::printf("error\n"); }
    void set_stopped() noexcept { std::printf("stopped\n"); }
};

int main() {
    auto work = stdexec::just(20)
              | stdexec::then([](int x) { return x + 1; })
              | stdexec::then([](int x) { return x * 2; });

    // connect() returns the whole nested "onion" as one concrete object.
    // It lives on the stack: no heap allocation is needed to run the chain.
    auto op = stdexec::connect(std::move(work), print_receiver{});

    // Operation states are immovable: 'op' must stay put until completion.
    stdexec::start(op);   // just/then complete inline here, printing "value: 42"
}

```

What to notice in this example:

- `op` is a local variable. Its size and layout are fixed at compile time, and no heap allocation is needed to run the whole `just | then` chain.
- Operation states are **immovable**: once `start(op)` has run, the object must stay where it is until completion. This is the price for allocation-free composition that the talk mentioned, and it is why writing senders by hand can feel harder than writing coroutines.
- `sync_wait`, `when_all` and the other adaptors use this same mechanism internally. A well-built sender pipeline can run a complex asynchronous flow with zero allocations, which is attractive in the low-latency, high-frequency trading setting the speaker works in.

| | Coroutine frame | Sender operation state |
|---|---|---|
| Where it lives by default | Heap (`operator new`) | Wherever the caller places it |
| Allocation removal | HALO, at the compiler's discretion | Guaranteed by the type system |
| Control knobs | Custom `promise_type::operator new`, allocators | Placement of the `connect` result |
| Ergonomics | Sequential-looking code | Explicit composition, immovability rules |

Because senders and awaitables interoperate, a migration does not have to choose one model for the whole system. A practical approach is to write most logic as coroutines for readability, measure, and then rewrite only the hot paths as sender pipelines where allocation-free operation states matter.

### Check Your Understanding

1. State the structured-concurrency invariant in one sentence, and explain how it parallels RAII for memory.
2. Why does a seeded single-threaded scheduler make concurrency bugs *reproducible* but not *exhaustively* testable?
3. According to the speaker, why can a well-composed system rely more on per-component testing?
4. You migrate a tuned reactor to coroutines and latency gets worse. Name two things you would measure first. (Hint: frame allocations, and whether HALO applied on the hot path.)
5. What property of sender operation states allows them to avoid heap allocation, and what rule does that impose on the programmer?

## Key takeaways

- Concurrency is fundamentally about describing the real dependencies between pieces of work. The better a framework knows them, the more it can exploit parallelism.
- Raw synchronization primitives (mutexes, atomics, semaphores, fences) are easy to get wrong and can erase performance gains. Aim for code that is correct by construction.
- Reactor/proactor designs give the best throughput but force you to split work into non-blocking tasks. State machines and callbacks do this at the cost of scattered control flow and messy lifetimes (shared_from_this).
- Structured concurrency treats tasks as RAII resources: every task belongs to a scope, scopes outlive their tasks, and a closing brace means the tasks are done or have been moved up.
- Both C++20 coroutines (through awaiters) and C++26 senders (through adaptors) provide structured concurrency, and they interoperate: senders are awaitables and awaitables are senders.
- Coroutines are more convenient, because the frame keeps locals alive. Senders are more bare-bones but give fine control over memory and can avoid heap allocation.
- Migrate incrementally. Use existing libraries, keep control of your main loop (poll rather than run), start at the root for non-blocking code and at the leaves for blocking code, and treat cancellation as a friend.

## Glossary

| Term | Meaning |
|---|---|
| Concurrency | In Barkan's definition, the description of dependencies between instructions or components, so that independent work can proceed without waiting. |
| Non-blocking API | A function that returns before its effect or result is complete. The caller must not assume the result is available afterwards. |
| Continuation-Passing Style (CPS) | A style in which you pass the next step of the computation (a callback, the 'continuation') to an asynchronous operation, to be invoked when it completes. |
| Future | A handle returned by a non-blocking operation. It can be used to check, wait for, or chain work onto the eventual result. |
| Fork / Join | Fork starts arbitrary code that runs independently of what follows. Join waits until forked operations complete before continuing. |
| std::jthread | A C++20 joining thread. Its destructor requests stop and joins, tying the thread's execution to the object's lifetime. |
| std::stop_source / std::stop_token | C++20 cooperative cancellation primitives: a stop source requests cancellation, and holders of the matching token can observe it. |
| Raw synchronization primitive | A low-level coordination construct such as a mutex, atomic, semaphore or memory fence. Sean Parent advises against using them directly. |
| Correct by construction | Code whose design makes certain bugs impossible, so that correctness can be verified by reading it rather than by exhaustive testing. |
| Virtual (green) threads / fibers | User-space threads with their own stacks, scheduled cooperatively by the application. Also called stackful coroutines. Not part of standard C++. |
| Reactor / Proactor | Event-loop architectures in which generic threads dispatch small non-blocking tasks as events arrive. A reactor reacts to readiness; a proactor reacts to completion. |
| Tail latency | The latency of the slowest requests, at high percentiles. It can be reduced by racing redundant operations and cancelling the losers. |
| Structured concurrency | A discipline in which every asynchronous task is owned by a scope that outlives it. This gives RAII-like lifetimes and hierarchical propagation of errors and cancellation. |
| Coroutine frame | The storage the compiler creates when a C++20 coroutine is invoked. It holds the coroutine's parameters, locals and suspension state. |
| Awaiter / Awaitable | An awaitable is an object usable with co_await. Its awaiter (with await_ready, await_suspend and await_resume) controls suspension and resumption and delivers the result or error. |
| Sender | In C++26 std::execution, a lazy description of asynchronous work that can complete with a value, an error, or a stopped signal. |
| Receiver | The object that a sender's operation delivers its completion to, through set_value, set_error or set_stopped. |
| Sender adaptor | An algorithm such as then, let_value or when_all that composes senders into a new sender while preserving structured-concurrency guarantees. |
| let_value | A sender adaptor whose function receives the predecessor's values, kept alive in the operation state, and returns a new sender to run. |
| Function coloring | The problem that converting a function to async (a coroutine or sender-returning function) forces its callers to change as well, causing cascading edits. |
| io_context::poll | An asio method that runs ready handlers without blocking and then returns control to the caller. Contrast with run(), which takes over the thread. |

## Exercises

1. In the asio session example (Listing 29/Listing 30), explain why do_read and do_write calling each other does not cause a stack overflow. Then explain what would go wrong if the lambdas captured only `this` and not `self`.

   <details><summary>Hint</summary>Consider when async_read_some returns relative to when the lambda runs. Then ask who owns the session object once the original caller drops its shared_ptr.</details>

2. State the core structured-concurrency invariant in your own words and explain how std::jthread is an analogy for it. What would 'detaching' a task correspond to in this model?

   <details><summary>Hint</summary>Compare scope lifetime to task lifetime, and recall what the jthread destructor does that a std::thread destructor does not.</details>

3. Using the sender/receiver onion (Listing 44), list in order the connect calls made when `before | then(f) | after` is connected, then the set_value calls made when it completes. Why is the order inverted?

   <details><summary>Hint</summary>Connection proceeds from the outermost sender inward. Completion starts at the innermost operation and flows outward through receivers.</details>

4. Compare how the span's lifetime is managed in the coroutine version (Listing 47) and the sender version (Listing 48) of read(). Why does the sender version need `just(span) | let_value(...)` instead of simply capturing the parameter?

   <details><summary>Hint</summary>Where does a by-value coroutine parameter live? What happens to an ordinary function's parameters once the function returns its sender?</details>

5. Your team has a large blocking, thread-per-request server and a separate reactor-based market-data handler. Following Barkan's tips, where would you start converting each one to coroutines, and why?

   <details><summary>Hint</summary>Think about the direction of function coloring: root-first for non-blocking code, leaf-first for blocking code.</details>

6. Explain how `co_await (async_read(S, data) || T.async_wait())` (Listing 38) implements a timeout. How does it relate to the 'tail latency hiding' motivation, and why does structured concurrency make the losing operation safe to abandon?

   <details><summary>Hint</summary>Think about when-any semantics, automatic cancellation of the loser, and the variant result type.</details>

7. Barkan argues that heavy use of mutexes can make a concurrent system correct but slow, and that structured concurrency aims for correctness 'by construction'. Using the RAII analogy, explain what class of bugs structured concurrency eliminates and which it does not.

   <details><summary>Hint</summary>Consider lifetime and ownership bugs (dangling tasks, leaks, lost errors) versus logical data races on genuinely shared state.</details>

## Further reading

- Roi Barkan, 'Refactoring Towards Structured Concurrency' slides: https://github.com/Digital-Medium/ACCU-On-Sea-Slides-2026
- Sean Parent, 'No Raw Synchronization Primitives' guideline (talk circa 2013)
- Daniela Engert, 'Contemporary C++ in Action', CppCon 2022 (composing awaitables with automatic mutual cancellation)
- Bryce Adelstein Lelbach, 'C++ Standard Parallelism', C++Now 2022 (the sender/receiver onion)
- Ben Weidig's structured concurrency diagram
- Eric Niebler: 'senders are awaitables and awaitables are senders'
- Robert Leahy's sender-based read example
- ASIO (standalone and Boost.Asio), including experimental awaitable operators && and ||
- cppcoro coroutine library
- Boost.Cobalt coroutine library
- NVIDIA stdexec (senders reference implementation, including exec::repeat_effect_until)
- beman.execution
- beman.net
- C++26 std::execution (senders and receivers)
