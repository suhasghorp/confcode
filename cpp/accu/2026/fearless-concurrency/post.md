---
title: "Fearless Concurrency in C++: Building Rust's Send and Sync with C++26 Reflection"
date: 2026-11-14
tags: ["C++", "C++26", "concurrency", "Rust", "reflection", "thread safety", "data races", "concepts", "ACCU"]
source: "https://www.youtube.com/watch?v=-FP6tLlCxWE"
video_title: "Fearless Concurrency in C++ - Tristan Brindle - ACCU 2026"
channel: "ACCU Conference"
---

> This chapter is based on the talk [Fearless Concurrency in C++ - Tristan Brindle - ACCU 2026](https://www.youtube.com/watch?v=-FP6tLlCxWE) (ACCU Conference). Code listings marked *from the talk* are transcribed from the slides; listings marked *illustrative* were written for this chapter to explain the ideas.

Writing programs that do several things at once is one of the hardest jobs in software. In C++ it is also one of the most dangerous: a single unsynchronised write to memory that another thread is reading is a *data race*, and a data race is undefined behaviour. Nothing in a standard C++ compiler stops you from writing one.

Rust takes a different position. In safe Rust a data race is impossible: the compiler rejects any program that could contain one. The Rust community calls this **fearless concurrency**. In his ACCU 2026 talk, Tristan Brindle asks a simple question: how much of that guarantee can we get in the C++ we already have? He studies how Rust does it, with the `Send` and `Sync` traits, library types such as `Mutex`, and the borrow checker. He then rebuilds the first ingredient in C++26 as a `sendable` concept, computed automatically by static reflection, plus a constrained `spawn_thread` function.

This chapter follows the talk's experiment step by step. Along the way it teaches the background you need: what a data race really is, how type traits and concepts work, how `const` and reference semantics interact, how C++26 reflection lets a program inspect its own types, and why the thread that *spawns* work turned out to be the weak spot. The speaker presents this as an experiment and calls it incomplete. The chapter keeps that spirit: it shows what works, what does not, and why.

## What you will learn

- Define a data race precisely and distinguish it from race conditions and deadlocks
- Explain the meaning of Rust's Send and Sync traits in C++ terms (pass by value vs. pass by const reference)
- Classify common C++ types (int, std::vector, std::span, std::unique_ptr, std::shared_ptr, user-defined structs) as safe or unsafe to share across threads, and justify each answer
- Explain why the talk merges Send and Sync into a single sendable concept, and what std::unique_ptr and C++26 std::indirect have to do with it
- Read and write basic C++26 reflection code: ^^T, std::meta queries, splicers, template for and define_static_array
- Implement a reflection-based type trait that recursively inspects bases and data members
- Use a concept to constrain a thread-spawning function and predict which calls compile
- Explain why the spawning thread breaks the guarantee and how a fork-style structured API closes the gap
- List the remaining limitations of the approach compared with Rust's borrow checker

## Before you start

- Comfortable reading modern C++ (templates, lambdas, references, const)
- Basic familiarity with std::thread or std::jthread
- No prior Rust knowledge is required; the Rust ideas are explained in C++ terms
- No prior knowledge of C++26 reflection is required

## Contents

1. [Why concurrency needs a safety net](#1-why-concurrency-needs-a-safety-net) — [0:00](https://www.youtube.com/watch?v=-FP6tLlCxWE&t=0s)
2. [What "fearless concurrency" means](#2-what-fearless-concurrency-means) — [4:30](https://www.youtube.com/watch?v=-FP6tLlCxWE&t=270s)
3. [Data races, race conditions and undefined behaviour](#3-data-races-race-conditions-and-undefined-behaviour) — [7:00](https://www.youtube.com/watch?v=-FP6tLlCxWE&t=420s)
4. [How Rust prevents data races](#4-how-rust-prevents-data-races) — [9:20](https://www.youtube.com/watch?v=-FP6tLlCxWE&t=560s)
5. [Send and Sync in Rust, explained for C++ programmers](#5-send-and-sync-in-rust-explained-for-c-programmers) — [12:25](https://www.youtube.com/watch?v=-FP6tLlCxWE&t=745s)
6. [The ground rules: classifying int](#6-the-ground-rules-classifying-int) — [17:45](https://www.youtube.com/watch?v=-FP6tLlCxWE&t=1065s)
7. [Values versus views: vector, span and span<const int>](#7-values-versus-views-vector-span-and-spanconst-int) — [20:30](https://www.youtube.com/watch?v=-FP6tLlCxWE&t=1230s)
8. [Smart pointers: unique_ptr and shared_ptr](#8-smart-pointers-uniqueptr-and-sharedptr) — [23:50](https://www.youtube.com/watch?v=-FP6tLlCxWE&t=1430s)
9. [User-defined types and the mutable keyword](#9-user-defined-types-and-the-mutable-keyword) — [26:50](https://www.youtube.com/watch?v=-FP6tLlCxWE&t=1610s)
10. [One concept instead of two: sendable](#10-one-concept-instead-of-two-sendable) — [29:10](https://www.youtube.com/watch?v=-FP6tLlCxWE&t=1750s)
11. [A crash course in C++26 reflection](#11-a-crash-course-in-c26-reflection) — [34:40](https://www.youtube.com/watch?v=-FP6tLlCxWE&t=2080s)
12. [Implementing sendable, part 1: the shape and the scalar cases](#12-implementing-sendable-part-1-the-shape-and-the-scalar-cases) — [37:10](https://www.youtube.com/watch?v=-FP6tLlCxWE&t=2230s)
13. [Implementing sendable, part 2: pointers, references and classes](#13-implementing-sendable-part-2-pointers-references-and-classes) — [39:10](https://www.youtube.com/watch?v=-FP6tLlCxWE&t=2350s)
14. [spawn_thread: a constrained wrapper around std::jthread](#14-spawnthread-a-constrained-wrapper-around-stdjthread) — [44:40](https://www.youtube.com/watch?v=-FP6tLlCxWE&t=2680s)
15. [The moment of truth: what compiles and what must not](#15-the-moment-of-truth-what-compiles-and-what-must-not) — [45:50](https://www.youtube.com/watch?v=-FP6tLlCxWE&t=2750s)
16. [A hole in the fence: the spawning thread](#16-a-hole-in-the-fence-the-spawning-thread) — [49:10](https://www.youtube.com/watch?v=-FP6tLlCxWE&t=2950s)
17. [A solution: fork and structured concurrency](#17-a-solution-fork-and-structured-concurrency) — [51:20](https://www.youtube.com/watch?v=-FP6tLlCxWE&t=3080s)
18. [Limitations and a brighter future](#18-limitations-and-a-brighter-future) — [55:40](https://www.youtube.com/watch?v=-FP6tLlCxWE&t=3340s)

## 1. Why concurrency needs a safety net

*Talk segment: [0:00](https://www.youtube.com/watch?v=-FP6tLlCxWE&t=0s)*

Every modern computer runs code in parallel. Your phone has several cores, a laptop typically has eight or more, and servers have dozens. To use that hardware, programs split their work across **threads**: independent sequences of execution that run at the same time *inside the same process*. That last part is what makes concurrency both powerful and dangerous. All threads of a process share one address space. Any thread can read or write any object that it can reach through a pointer or reference.

This chapter is about one particular kind of bug that arises from that sharing, the **data race**, and about an experiment to make C++ reject data races at compile time, the way the Rust programming language does.

### The speaker and the spirit of the talk

The talk was given by Tristan Brindle at ACCU 2026. He describes himself as someone who is "really interested in the topic of safety in C++". He is the author of **Flux**, a library that aims to make iteration in C++ safer, because, as he puts it, "our iterators are very unsafe". More recently he has been exploring other ways to make the language safer, and this talk reports on one of those explorations.

He is very clear about what the talk is and is not, and he opens with a set of personal disclaimers:

- The talk represents *his ideas* about how thread safety in C++ might be improved.
- He "might be completely wrong".
- Everything is highly experimental, prototype-quality code. "Please don't look at it and go, 'Well, this is the way I should do things.'"
- He is "definitely not a Rust expert". He has read about how Rust works and checked his facts carefully, but he will describe Rust using C++ terminology, which Rust experts may find grating.

> Keep this framing in mind as you read. The goal of the chapter is not to sell you a finished library. It is to follow a careful experiment, understand *why* each design decision was made, and see clearly where the idea works and where it runs out of road.

### Why shared memory is hard

Consider the simplest possible concurrent program: two threads that each increment a shared counter a million times.

**Listing 1** — A data race on a shared counter *(illustrative example, not from the talk)*

```cpp
#include <iostream>
#include <thread>

int main()
{
    int counter = 0;   // shared, unsynchronised

    auto work = [&counter] {
        for (int i = 0; i < 1'000'000; ++i) {
            ++counter;   // read-modify-write: NOT atomic
        }
    };

    {
        std::jthread t1(work);
        std::jthread t2(work);
    }   // jthreads join automatically here

    // Undefined behaviour: the two threads race on `counter`.
    std::cout << counter << '\n';
}
```

You might expect it to print 2000000. In practice it usually prints something smaller, and a different number each run. The statement `++counter` is not one indivisible step. The machine loads the value, adds one, and stores it back. If two threads interleave those steps, one increment overwrites the other and is lost.

Losing increments is bad enough, but the real problem is worse. In C++ this program has **undefined behaviour**. The language does not say "you might get a smaller number". It says the program has no meaning at all. The compiler is entitled to assume that this situation never happens, and optimisations built on that assumption can produce results that look impossible. We will define this precisely in a later section. For now the key point is that the compiler accepted this program without a single warning.

### Three ways out

Before the talk proper, Brindle recommends a talk by David Rowland from C++ on Sea 2025, *What Can C++ Learn About Thread Safety From Other Languages?* He says, with good humour, that he only discovered it after his own talk had been accepted: "they say great minds think alike, but Dave's mind was there a year before me". The two talks start along similar lines and then go in different directions.

Rowland's talk lays out a *landscape of approaches* that languages take to make concurrency safe. It is a useful map for this whole chapter:

| Approach | Core idea | Languages mentioned on the slide |
|---|---|---|
| **Immutability** | Shared data is never modified, so sharing is always safe (pure value semantics) | functional languages |
| **Law of exclusivity** | At any moment, data has either one writer *or* any number of readers, never both; enforced by local reasoning | Rust, Circle, Carbon, Hylo, Swift |
| **Message passing** | Threads share nothing; they communicate by sending messages | Erlang, Elixir |

All three can be practised in C++ *by discipline*. Here is a small example of each style. Nothing checks that you got them right: the compiler will happily accept a version that breaks the rules.

**Listing 2** — Immutability, exclusivity and message passing in plain C++ *(illustrative example, not from the talk)*

```cpp
#include <condition_variable>
#include <iostream>
#include <memory>
#include <mutex>
#include <numeric>
#include <optional>
#include <queue>
#include <thread>
#include <vector>

// 1. Immutability: share data that nobody can modify.
void immutable_style()
{
    auto data = std::make_shared<const std::vector<int>>(std::vector{1, 2, 3, 4});
    std::jthread reader1([data] { std::cout << std::accumulate(data->begin(), data->end(), 0) << '\n'; });
    std::jthread reader2([data] { std::cout << data->size() << '\n'; });
}

// 2. Exclusivity: one writer at a time, enforced (by convention!) with a mutex.
void exclusive_style()
{
    int total = 0;
    std::mutex m;
    auto add = [&](int n) { std::scoped_lock lock(m); total += n; };
    {
        std::jthread a([&] { for (int i = 0; i < 1000; ++i) add(1); });
        std::jthread b([&] { for (int i = 0; i < 1000; ++i) add(1); });
    }
    std::cout << total << '\n';   // always 2000
}

// 3. Message passing: threads share nothing except a synchronised channel.
template <typename T>
class channel {
    std::queue<T> q_;
    std::mutex m_;
    std::condition_variable cv_;
public:
    void send(T value)
    {
        { std::scoped_lock lock(m_); q_.push(std::move(value)); }
        cv_.notify_one();
    }
    T receive()
    {
        std::unique_lock lock(m_);
        cv_.wait(lock, [this] { return !q_.empty(); });
        T value = std::move(q_.front());
        q_.pop();
        return value;
    }
};

void message_style()
{
    channel<std::optional<int>> ch;
    std::jthread consumer([&ch] {
        int sum = 0;
        while (auto msg = ch.receive()) { sum += *msg; }
        std::cout << sum << '\n';
    });
    for (int i = 1; i <= 10; ++i) ch.send(i);
    ch.send(std::nullopt);   // "no more messages"
}

int main()
{
    immutable_style();
    exclusive_style();
    message_style();
}
```

Notice what each version relies on:

- The **immutable** version relies on nobody casting away `const`.
- The **exclusive** version relies on every access to `total` going through the mutex.
- The **message-passing** version relies on threads only communicating through the channel, and on the channel itself being correctly synchronised.

Rust sits in the middle column. Its compiler *enforces* the law of exclusivity, and it adds two traits, `Send` and `Sync`, that describe which types may cross between threads. The rest of this chapter asks how much of that enforcement we can recreate in today's C++.

## 2. What "fearless concurrency" means

*Talk segment: [4:30](https://www.youtube.com/watch?v=-FP6tLlCxWE&t=270s)*

### A slogan worth stealing

When the speaker asked the audience who had heard the phrase "fearless concurrency", fewer hands went up than he expected. To him it is a well-known slogan, "primarily associated with the Rust programming language".

He is careful about calling it a *marketing* slogan, because that sounds negative, and he means the opposite: "as a marketing thing, it works really well". Everybody who has written multi-threaded code knows the feeling it addresses. Concurrent programming is hard, "there's no getting away from it", and it is even harder in a language where a mistake quietly becomes undefined behaviour. The phrase promises that with this language you do not have to be afraid of that class of mistake. "That's why I stole it for the title of my talk."

### What the slogan actually promises

Strip away the marketing and the technical claim is precise:

> In **safe Rust** code, it is impossible to write a data race.

To understand that sentence, a C++ programmer needs to know what "safe Rust" means. Rust code is divided into two worlds:

- **Safe Rust** is the default. The compiler checks every rule of the language, including the rules about references and threads. If your program compiles, a whole set of bugs, data races among them, cannot happen.
- **Unsafe Rust** is code written inside an `unsafe` block or function, or an `unsafe impl`. There you may do things the compiler cannot verify, such as dereferencing raw pointers. You, the programmer, promise that the rules still hold.

The speaker sums it up: "As long as you don't use the unsafe keyword, it doesn't matter what you do. You can use sort of crazy construction of threading primitives… you will not get a data race in Rust." His reaction is the reaction of most C++ programmers who hear this for the first time: "that sounds pretty awesome… and kind of I want that in C++."

### Compare with C++

C++ has no equivalent split. Every line of C++ is, in Rust terms, "unsafe": the compiler trusts you completely. The racy counter from the previous section compiles with no complaint. Here is a slightly more realistic version of the same mistake: a worker thread that captures a local variable by reference, while the main thread keeps modifying it.

**Listing 3** — C++ accepts a race through a reference capture *(illustrative example, not from the talk)*

```cpp
#include <iostream>
#include <thread>

int main()
{
    int progress = 0;

    std::jthread reporter([&progress] {
        // Reads `progress` while main() writes it: a data race.
        for (int i = 0; i < 5; ++i) {
            std::cout << "progress: " << progress << '\n';
        }
    });

    for (int i = 0; i < 100; ++i) {
        ++progress;   // unsynchronised write
    }
}   // compiles cleanly; behaviour is undefined
```

Every C++ compiler accepts this. Static analysers and sanitizers such as ThreadSanitizer can often *detect* the race when the program runs, but nothing in the language *prevents* it.

Here is the equivalent program in Rust. You do not need to read Rust fluently. The point is that the compiler refuses to build it.

**Listing 4** — The same idea in Rust is rejected at compile time *(illustrative example, not from the talk)*

```rust
use std::thread;

fn main() {
    let mut progress = 0;

    // error[E0373]: closure may outlive the current function, but it
    // borrows `progress`, which is owned by the current function
    let reporter = thread::spawn(|| {
        println!("progress: {}", progress);
    });

    for _ in 0..100 {
        progress += 1;
    }
    reporter.join().unwrap();
}
```

The error does not come from a special-purpose race detector. It falls out of ordinary type checking: `thread::spawn` states in its signature what kind of data it accepts, and a reference to a local variable that the main thread is still mutating does not qualify. The rest of this chapter is about recreating that *signature-level* check in C++.

### The goal of the talk

The speaker sets a modest goal. He does not claim C++ can match Rust completely. Instead: "this talk is about me seeing whether we could maybe not completely eliminate data races, but getting as close as we can with the C++ that we've got."

That framing matters. Later we will see that one of Rust's three ingredients, the borrow checker, cannot be recreated in today's C++. The question is how far the other two take us.

## 3. Data races, race conditions and undefined behaviour

*Talk segment: [7:00](https://www.youtube.com/watch?v=-FP6tLlCxWE&t=420s)*

Before we can prevent data races, we need to know exactly what one is. The speaker takes his definition straight from the Rust documentation (the Rustonomicon). It matches the C++ standard's definition closely.

### The definition

> Safe Rust guarantees an absence of data races, which are defined as:
>
> - two or more threads concurrently accessing a location of memory
> - one or more of them is a write
> - one or more of them is unsynchronized

All three conditions must hold. That gives us three ways to make a program race-free:

1. **Don't share.** If only one thread ever touches a memory location, there is no race.
2. **Don't write.** Any number of threads may read the same data at once, as long as nobody modifies it.
3. **Synchronise.** If threads must share and at least one must write, every access must be ordered, for example with a mutex or atomic operations.

Hold on to these three escape routes. Every rule we develop later is an application of one of them. Value types use route 1 (each thread gets its own copy). Const views of immutable data use route 2. Mutex-protected wrappers use route 3.

### Data races are undefined behaviour

"Data races are undefined behaviour in C++ and in Rust, actually," the speaker notes. You *can* write a data race in Rust, but only in unsafe code ("if you mess about with raw pointers and stuff"), and then the consequences are the same as in C++.

What does "undefined behaviour" mean here? In C++ terms, two accesses to the same memory location **conflict** if at least one is a write. If two conflicting accesses happen in different threads, at least one is not atomic, and neither *happens before* the other, the program has a data race and its behaviour is undefined. "Happens before" is the ordering created by synchronisation: unlocking a mutex happens before the next lock of that mutex, a thread's constructor call happens before the thread function starts, the end of a thread happens before `join()` returns, and so on.

The compiler optimises under the assumption that races do not exist. It may keep a variable in a register instead of re-reading memory, or reorder stores. That is why a racy program can behave in ways that no interleaving of its statements explains.

The fix for the counter from the first section is to make the conflicting accesses synchronised, here with `std::atomic`:

**Listing 5** — Fixing the counter with std::atomic *(illustrative example, not from the talk)*

```cpp
#include <atomic>
#include <iostream>
#include <thread>

int main()
{
    std::atomic<int> counter = 0;

    auto work = [&counter] {
        for (int i = 0; i < 1'000'000; ++i) {
            counter.fetch_add(1, std::memory_order_relaxed);   // indivisible
        }
    };

    {
        std::jthread t1(work);
        std::jthread t2(work);
    }

    std::cout << counter << '\n';   // always 2000000
}
```

### What Rust does *not* promise

"It's worth noting that Rust guarantees the absence of data races, but other kinds of concurrency problems … can still occur." A program free of data races can still be wrong:

- A **race condition** is a logic bug where the outcome depends on timing. The classic form is *check-then-act*: check that a condition holds, then act on it, while another thread changes the condition in between. Every individual access can be perfectly synchronised and the program is still wrong.
- A **deadlock** happens when threads wait for each other forever, for example when two threads lock two mutexes in opposite orders.

**Listing 6** — A race condition without a data race (check-then-act) *(illustrative example, not from the talk)*

```cpp
#include <iostream>
#include <mutex>
#include <thread>

class account {
    mutable std::mutex m_;
    int balance_ = 100;
public:
    int balance() const { std::scoped_lock lock(m_); return balance_; }
    void withdraw(int amount) { std::scoped_lock lock(m_); balance_ -= amount; }
};

int main()
{
    account acc;
    auto try_withdraw = [&acc] {
        if (acc.balance() >= 100) {   // check ...
            acc.withdraw(100);        // ... then act: another thread may get in between
        }
    };
    {
        std::jthread a(try_withdraw);
        std::jthread b(try_withdraw);
    }
    // Every access was locked, so there is no data race,
    // yet the balance can end up at -100.
    std::cout << acc.balance() << '\n';
}
```

**Listing 7** — A deadlock, and the std::scoped_lock fix *(illustrative example, not from the talk)*

```cpp
#include <mutex>
#include <thread>

std::mutex a, b;

void deadlock_prone_1() { std::lock_guard la(a); std::lock_guard lb(b); /* ... */ }
void deadlock_prone_2() { std::lock_guard lb(b); std::lock_guard la(a); /* ... */ }  // opposite order!

// Fix: lock both at once; std::scoped_lock uses a deadlock-avoidance algorithm.
void safe_1() { std::scoped_lock lock(a, b); /* ... */ }
void safe_2() { std::scoped_lock lock(b, a); /* ... */ }

int main()
{
    std::jthread t1(safe_1);
    std::jthread t2(safe_2);
}
```

These bugs are not undefined behaviour. The program does something well-defined, just not what you wanted. The speaker's conclusion is honest: "the absence of data races is awesome. It doesn't necessarily mean that suddenly multi-threaded programming is easy, especially if you're using low-level threading primitives … I'm still a little bit scared of it." So: "a great marketing term, but maybe not completely accurate."

> **Key distinction.** Data-race freedom removes *undefined behaviour* from concurrent code. It does not remove *logic errors*. That is still a big win: a logic error can be found by testing and reasoning about the program; undefined behaviour can make the program's behaviour impossible to reason about at all.

## 4. How Rust prevents data races

*Talk segment: [9:20](https://www.youtube.com/watch?v=-FP6tLlCxWE&t=560s)*

### An "absolutely foolproof" plan

With the goal defined, the speaker presents his "absolutely foolproof three-step plan":

1. Understand how Rust prevents data races.
2. Do that in C++.
3. Party.

The joke carries a real methodology. Rust has already solved the problem in a production language. Instead of inventing a C++ solution from first principles, study the working one, find the parts that rely only on concepts C++ also has, and translate those.

### Three families of features

"Rust actually uses a combination of features to do this," he explains. Three families, each doing a different job:

| Rust feature | Job | C++ analogue today |
|---|---|---|
| `Send` and `Sync` traits | Describe *which types* may be passed to another thread, and how | Type traits and concepts (we can write these) |
| Library types such as `std::sync::Mutex` | Let threads share *mutable* data safely | `std::mutex`, but see below |
| The borrow checker | Proves at compile time who may access what, and for how long | None |

Let us look at each in C++ terms.

#### 1. Send and Sync

These are traits: compile-time properties of types. A function like `std::thread::spawn` states in its signature that it only accepts data with the right property. We will spend the next several sections on them. C++ has an exact counterpart: a **type trait** checked by a **constraint**.

#### 2. Library types that own their data

Rust's `Mutex<T>` differs from C++'s `std::mutex` in one important way: the Rust mutex *contains* the data it protects. The only way to reach the `T` inside is to lock the mutex, which hands you a guard object; when the guard goes away, the lock is released. You cannot forget to lock, because without locking you have no way to name the data.

In C++, `std::mutex` and the data it protects are separate objects, connected only by a comment or a naming convention. Nothing stops a careless function from touching the data without the lock. We can, however, build a Rust-style wrapper ourselves:

**Listing 8** — A Rust-style mutex that owns its data *(illustrative example, not from the talk)*

```cpp
#include <iostream>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

template <typename T>
class synchronized {
    T value_;
    mutable std::mutex m_;
public:
    template <typename... Args>
    explicit synchronized(Args&&... args) : value_(std::forward<Args>(args)...) {}

    // The ONLY way to reach the value: run a function while the lock is held.
    template <typename Fn>
    decltype(auto) with_lock(Fn&& fn)
    {
        std::scoped_lock lock(m_);
        return std::forward<Fn>(fn)(value_);
    }

    template <typename Fn>
    decltype(auto) with_lock(Fn&& fn) const
    {
        std::scoped_lock lock(m_);
        return std::forward<Fn>(fn)(std::as_const(value_));
    }
};

int main()
{
    synchronized<std::vector<int>> shared;

    {
        std::jthread a([&] { for (int i = 0; i < 100; ++i) shared.with_lock([&](auto& v) { v.push_back(i); }); });
        std::jthread b([&] { for (int i = 0; i < 100; ++i) shared.with_lock([&](auto& v) { v.push_back(-i); }); });
    }

    std::cout << shared.with_lock([](auto const& v) { return v.size(); }) << '\n';   // 200
}
```

The speaker's own repository contains a similar `synchronized<T>` type. It did not make it into the talk; he mentions at the end that some slides were cut. Keep the idea in mind: later we will see that safely sharing *mutable* data across threads requires exactly this kind of wrapper, because our C++ version cannot rely on a borrow checker.

#### 3. The borrow checker

The third arm, in the speaker's words, "is an integral part of not just the memory safety story, which is what we usually associate the borrow checker with, but also the data race safety story."

The borrow checker enforces the *law of exclusivity* from the first section. At any point in the program, a value may have either:

- exactly one **mutable** reference (`&mut T`), or
- any number of **shared** references (`&T`),

but never both at once. It also tracks *lifetimes*, so no reference can outlive the value it refers to. Notice how directly this maps onto the definition of a data race. A race needs a write concurrent with another access. With one `&mut` and no `&`, nobody else can access the data at all; with many `&` and no `&mut`, nobody can write. The borrow checker rules out the race at its root.

### The C++ borrow checker

"In particular, the borrow checker. Well, that's no problem. Okay, I'll just use the C++ borrow checker." The next slide, titled *The C++ borrow checker*, starts out empty, and the audience laughs.

Then the serious points:

- "Obviously, standard C++ today does not have a borrow checker or anything similar."
- "This seems unlikely to change in the short term." There is work going on ("lots of people are thinking about these things, including me"), but "we're not going to suddenly have borrow-checked C++ next year".
- "So what we're going to do is we're going to try to do the best we can with the C++ that we have today."
- "And we will acknowledge that our solution is inevitably going to be incomplete."

That last point frames the rest of the talk. Of Rust's three ingredients, one cannot be replicated. The speaker promises to "work around the absence of the borrow checker as best we can, but fundamentally we are going to come up short." Watch for the places where that shortfall shows up. It will matter a great deal in the sections near the end.

## 5. Send and Sync in Rust, explained for C++ programmers

*Talk segment: [12:25](https://www.youtube.com/watch?v=-FP6tLlCxWE&t=745s)*

With the borrow checker out of reach, the speaker turns to the ingredient C++ *can* imitate: the `Send` and `Sync` traits. This section explains what they are and how they are used, in C++ vocabulary throughout.

### What the traits are for

The `Send` and `Sync` traits in Rust "are used to indicate types which are safe to use across thread boundaries". They are attached to *types*, not to variables or functions. They are also a special kind of trait.

**They are marker traits.** A typical Rust trait is like a C++ interface or concept with required functions (for example, a `Display` trait requires a formatting method). `Send` and `Sync` have no methods and no associated types. They exist only to record a yes/no fact about a type. As the speaker puts it: "you don't need to know anything about Rust traits to understand that. Just think of them as type traits. They're basically equivalent to type traits in C++."

### Type traits in C++: a quick refresher

A C++ **type trait** is a compile-time function from a type to a value, usually a `bool`. The standard library has dozens: `std::is_integral_v<T>`, `std::is_const_v<T>`, `std::is_trivially_copyable_v<T>`. Since C++17 the common form is a `constexpr bool` variable template, and since C++20 we can wrap a trait in a **concept** and use it to constrain templates.

The pattern we need has three parts:

1. A primary template that gives the default answer.
2. **Specialisations** that override the answer for particular types. This is how a library or a user opts a type in or out.
3. A concept for use in `requires` clauses.

**Listing 9** — A hand-written marker trait with opt-in and opt-out *(illustrative example, not from the talk)*

```cpp
#include <concepts>
#include <memory>
#include <string>

// 1. Primary template: the default answer.
template <typename T>
constexpr bool is_send_v = std::is_arithmetic_v<T>;   // a crude default for now

// 2. Specialisations: library or user opts types in or out.
template <>
constexpr bool is_send_v<std::string> = true;

template <typename T>
constexpr bool is_send_v<std::unique_ptr<T>> = is_send_v<T>;

// 3. A concept for constraining templates.
template <typename T>
concept send = is_send_v<T>;

template <send T>
void give_to_worker(T value) { /* ... move value into another thread ... */ }

int main()
{
    give_to_worker(42);                                 // OK: int
    give_to_worker(std::string("hello"));               // OK: opted in
    give_to_worker(std::make_unique<double>(1.5));      // OK: unique_ptr<double>
    // give_to_worker(std::make_unique<int*>(nullptr)); // error: int* is not send
}
```

That is a complete marker trait in C++. What is missing compared to Rust is the *automatic* derivation for user-defined types, which we will add later with reflection.

### Unsafe traits

"Interestingly, Send and Sync are *unsafe* traits." In Rust, writing `unsafe impl Send for MyType {}` is how you manually mark a type. The `unsafe` keyword is required "because if you get it wrong, if you claim that a type you have written meets the Send requirements and actually it doesn't, then that can lead to a data race, which is undefined behaviour."

The same is true of our C++ version, but C++ has no keyword to flag it. A specialisation like `template <> constexpr bool is_send_v<my_type> = true;` is an unchecked promise. When we build the C++ design later, treat every manual specialisation as the moral equivalent of an `unsafe` block: a place where a human is vouching for something the compiler cannot verify.

### Automatic derivation

Manual marking is rare in Rust: "for the overwhelming majority of user-defined types, the Rust compiler will automatically derive the correct value of Send and Sync." The rule is structural. A struct is `Send` if all its fields are `Send`, and `Sync` if all its fields are `Sync`. Only people who "mess about with low-level pointers and stuff" need to write `unsafe impl`. Everyone else just aggregates existing types and gets the right answer for free.

### What the traits mean

Here is the heart of it, in the speaker's C++ translation:

- **Send**: it is safe to pass an instance of the type **by value** to another thread.
- **Sync**: it is safe to pass an instance of the type **by const reference** to other threads.

"Again, apologies to the Rust people. I know they're not const references." A Rust shared reference `&T` is not quite a C++ `const T&`, but it is the closest C++ concept.

The two traits are linked by a neat rule:

> A type `T` is `Sync` if and only if `&T` (a shared reference to `T`) is `Send`.

In C++ terms: sharing `T` between threads by const reference is safe exactly when *sending a const reference* to another thread is safe. That observation lets you define one trait in terms of the other:

**Listing 10** — Defining Sync in terms of Send: T is Sync iff const T& is Send *(illustrative example, not from the talk)*

```cpp
#include <type_traits>

template <typename T>
constexpr bool is_send_v = std::is_arithmetic_v<T>;

// Sending a const reference is safe when the referred-to type is safe to share.
template <typename T>
constexpr bool is_send_v<T const&> = is_send_v<T>;

// Rust's rule: T is Sync if and only if &T is Send.
template <typename T>
constexpr bool is_sync_v = is_send_v<T const&>;

static_assert(is_sync_v<int>);
static_assert(!is_sync_v<int*>);

int main() {}
```

### Where the traits are used

Marker traits are useless unless something checks them. In Rust, the checker is `std::thread::spawn`. "The Rust thread spawn function constrains its arguments to require this Send property." You pass it a closure, typically a capturing lambda, and the closure must be `Send`. Because a closure is `Send` only if all its captures are, every captured value must be `Send`. "And because the way references work in Rust, it means that if you capture anything by reference, it requires that thing has the Sync property."

That is the whole mechanism. No runtime cost and no special compiler magic beyond trait derivation: just a constrained function and two well-chosen type properties.

### Why this is promising for C++

The speaker sums up where this leaves us: "We don't have a borrow checker, but we do have type traits. We can do that. We can write some type traits. We know how to do that. And we know how to constrain functions." So the next step is a thought experiment. Take the definitions of Send and Sync, pass by value and pass by const reference, and apply them to everyday C++ types.

## 6. The ground rules: classifying int

*Talk segment: [17:45](https://www.youtube.com/watch?v=-FP6tLlCxWE&t=1065s)*

### A show of hands

To find out what `Send` and `Sync` should mean for C++ types, the speaker runs the next part of the talk as an audience exercise. He puts a type on the screen and asks two questions:

1. Is it safe to pass an instance **by value** to another thread, for example as an argument to the `std::thread` constructor? (Send)
2. Is it safe to pass an instance **by const reference** to other threads? (Sync)

The answers are shown on each slide as a small table with a tick or a cross in each column. We will collect them as we go, and by the end of the next few sections we will have a table of our own.

### int by value

He starts with "everybody's favourite type", `int`, "the prototypical example of everything". Is it safe to pass an `int` by value to another thread? Yes. The new thread gets its own copy. Nobody else can touch that copy, so the first escape route from the data-race definition applies: no sharing, no race.

### int by const reference

Now the second question, and the audience says *no*. Why? Because a `const int&` only promises that *you* will not modify the `int` through *that reference*. It says nothing about the object itself. Another thread, typically the one that created the `int`, can still hold a non-const path to the same object and write to it while the receiving thread reads. An audience member points out that the object could also be destroyed while the other thread is still looking at it.

Here is that failure in code:

**Listing 11** — A const reference does not make the object constant *(illustrative example, not from the talk)*

```cpp
#include <iostream>
#include <thread>

void reader(const int& value)   // "I promise not to modify it" -- but someone else might
{
    for (int i = 0; i < 3; ++i) {
        std::cout << value << '\n';   // races with the writes below
    }
}

int main()
{
    int shared = 0;
    std::jthread t(reader, std::cref(shared));

    for (int i = 0; i < 1000; ++i) {
        ++shared;   // the owning thread still writes: data race (UB)
    }
}
```

This is the crucial C++ subtlety. **`const` in C++ describes the access path, not the object.** A `const T&` is a read-only *view*, and other non-const views of the same object may exist. In Rust, the borrow checker guarantees that while a shared reference `&T` exists, no `&mut T` exists anywhere. That is exactly why Rust can say "`T` is `Sync`" as a property of the *type*. C++ cannot, unless we add assumptions.

### The ground rules

So the speaker adds two assumptions to the thought experiment.

**Assumption 1: the object is born const.** "We're going to assume that the int that we're passing in this case was born const." The *sending* thread treats the object as constant for as long as it is shared. "We're going to assume that people are being nice to us."

**Assumption 2: the called function is friendly.** The code running in the other thread "was written by our friend and not our enemy. So they're not going to do `const_cast` or anything like that, you know, `reinterpret_cast`, poke at the bits, or anything."

Under these rules, an `int` passed by const reference is never written by anyone while it is shared. The second escape route applies, many readers and no writer, and so: **`int` is both Send and Sync.**

| Type | Send | Sync |
|---|---|---|
| `int` | ✅ | ✅ |

### Why we need the rules

An audience member asks the obvious question: "Do we have to make these assumptions because we don't have a borrow checker?" The answer: "Yes, we do. Yes, that is entirely why."

Another asks about lifetime: aren't we also assuming the object stays alive? "Yes, I am, but I'm going to kind of say that deleting a heap thing is modifying it." Destroying an object is the ultimate write, so the born-const assumption covers lifetime too.

The speaker then makes an observation that becomes very important later: "the only person that has to play along with this game is the sending thread." If four worker threads each receive a `const int&`, they cannot modify it without a `const_cast`, so they are not a problem. The only party who can break the rules is the thread that owns the object and still has non-const access to it. Remember this. In the section titled *A hole in the fence* we will see a real bug that comes from exactly here.

**Listing 12** — Born const: sharing an object that nobody modifies *(illustrative example, not from the talk)*

```cpp
#include <iostream>
#include <thread>
#include <vector>

void reader(const int& value, int id)
{
    std::cout << "reader " << id << " sees " << value << '\n';
}

int main()
{
    const int config = 42;   // born const: no thread can legally modify it

    std::vector<std::jthread> readers;
    for (int id = 0; id < 4; ++id) {
        readers.emplace_back(reader, std::cref(config), id);   // many readers, no writer
    }
}   // all readers join before `config` is destroyed
```

### Read the rest of the chapter with these rules in mind

From now on, every classification assumes the two rules. They are the price of not having a borrow checker. Rust's compiler *proves* these assumptions. In our C++ design they are a *contract*: the compiler checks the types, and the programmer promises the rest.

## 7. Values versus views: vector, span and span<const int>

*Talk segment: [20:30](https://www.youtube.com/watch?v=-FP6tLlCxWE&t=1230s)*

### std::vector<int>

Next on the list is a `std::vector<int>`. Can we pass it **by value** to another thread? Yes. The receiving thread gets a complete, independent copy, or takes ownership of the buffer if we move it, and nobody else can reach that buffer.

By **const reference** to several threads? Also yes, under the born-const assumption, "because the standard library has this nice thread safety guarantee." The C++ standard says (in the library's general requirements on data races) that standard library functions do not modify objects reachable through their `const` arguments, including `this`, unless the function documents otherwise. So if several threads only hold a `const std::vector<int>&`, they can only call const member functions such as `size()`, `operator[] const` and `begin() const`, and none of these causes "any internal mutation that is unprotected".

| Type | Send | Sync |
|---|---|---|
| `std::vector<int>` | ✅ | ✅ |

The speaker generalises: "ints, vectors of ints, floats, bools, generally value types, strings, value types are okay. We can send pure value types like these across threads."

### Value semantics versus reference semantics

Why are value types so well behaved? A type has **value semantics** when a copy is a completely independent object: modifying the copy never affects the original. `int`, `std::string` and `std::vector<int>` all work this way. The vector's heap buffer is an implementation detail; copying the vector copies the buffer.

A type has **reference semantics** when copying it produces another *handle to the same data*. Raw pointers, `std::span`, `std::string_view` and iterators all work this way. Copying a pointer does not copy what it points to.

That distinction decides how `const` behaves:

- For a value type, `const` is **deep**. A `const std::vector<int>` will not let you change its elements, because the elements are part of the vector's value.
- For a reference type, `const` is **shallow**. A `const` pointer cannot be re-pointed, but the data it points to is unaffected.

### std::span<int>

"What about a span of ints?" Someone suggests it is the same as a vector of ints. It is not. A `std::span<int>` is essentially a pointer and a length: a reference type, "and in particular, it's a reference to some *mutable* ints."

If we copy a span to another thread, both threads now hold handles to the same mutable ints, and either can write through its handle without synchronisation. So `std::span<int>` is **not Send**.

What about sharing a span by const reference? "For exactly the same reason, it should not have the Sync property." A top-level `const` on a span "basically does nothing". It does not stop you mutating the data, "because if nothing else, we can just take a copy of the span and operate on the non-const one."

**Listing 13** — Top-level const on a span does not protect the data *(illustrative example, not from the talk)*

```cpp
#include <span>
#include <vector>

void worker(const std::span<int> view)   // const span ...
{
    view[0] = 99;                 // ... still allows writing the elements
    std::span<int> copy = view;   // and a copy drops even the top-level const
    copy[1] = 100;
}

int main()
{
    std::vector<int> data{1, 2, 3};
    worker(data);
    // data is now {99, 100, 3}: shallow const protected nothing.
}
```

### std::span<int const>

Change the element type to `const int` and the answer flips. Passing a `std::span<int const>` by value gives the receiver read-only access to the elements, and under the born-const assumption nobody else writes them.

The speaker argues by analogy: "we already agreed … we can pass a vector by const reference, and a span of const ints is pretty much … a type-erased contiguous array, but it's morally the same thing as passing a vector by const reference." So `std::span<int const>` is **both Send and Sync**.

**Listing 14** — span<int const> gives read-only access to many threads *(illustrative example, not from the talk)*

```cpp
#include <numeric>
#include <print>
#include <span>
#include <thread>
#include <vector>

int sum(std::span<int const> view)
{
    // view[0] = 1;   // error: cannot assign to a const int
    return std::accumulate(view.begin(), view.end(), 0);
}

int main()
{
    const std::vector<int> data{1, 2, 3, 4, 5};   // born const

    std::jthread a([view = std::span<int const>(data)] { std::println("{}", sum(view)); });
    std::jthread b([view = std::span<int const>(data)] { std::println("{}", sum(view)); });
}
```

Our table now reads:

| Type | Send | Sync |
|---|---|---|
| `int` | ✅ | ✅ |
| `std::vector<int>` | ✅ | ✅ |
| `std::span<int>` | ❌ | ❌ |
| `std::span<int const>` | ✅ | ✅ |

> **Rule of thumb.** A view type is safe to send exactly when what it views is safe to *share*: the elements must be `const` (and themselves safe). That is the C++ version of Rust's "`T` is `Sync` iff `&T` is `Send`". A `span<int const>` is just a fancy `&[i32]`.

`std::string_view` is a `std::span<char const>` in disguise, so by the same reasoning it is sendable. The talk's first live demo relies on exactly that.

## 8. Smart pointers: unique_ptr and shared_ptr

*Talk segment: [23:50](https://www.youtube.com/watch?v=-FP6tLlCxWE&t=1430s)*

### std::unique_ptr<int>: an interesting one

"Next one. Unique pointer of int. This is an interesting one."

**By value?** Yes. `std::unique_ptr` *owns* its pointee and cannot be copied, only moved. When we move a `unique_ptr` into another thread, the sending thread's `unique_ptr` becomes null, and only the receiver can reach the `int`. "The unique pointer that is in thread A by value cannot touch the unique pointer that's in thread B by value." No sharing, so it is **Send**.

**By const reference?** No. "A const unique pointer doesn't prevent you from mutating the underlying int." The reason is shallow const again. `std::unique_ptr<T>::operator*` is a const member function that returns a *non-const* `T&`. `const` applies to the pointer object, which cannot be reset or released, but not to the thing it points at. If several threads hold a `const std::unique_ptr<int>&`, each can write `*p = …` with no synchronisation.

**Listing 15** — Mutating through a const unique_ptr *(illustrative example, not from the talk)*

```cpp
#include <memory>
#include <thread>

void bump(const std::unique_ptr<int>& p)
{
    // p.reset();   // error: reset() is non-const -- the pointer itself is protected
    ++*p;           // fine: operator* is const but returns int& -- the pointee is NOT
}

int main()
{
    auto p = std::make_unique<int>(0);
    {
        // Sharing one unique_ptr by const reference with two threads:
        std::jthread a(bump, std::cref(p));
        std::jthread b(bump, std::cref(p));   // data race on *p
    }
}
```

| Type | Send | Sync |
|---|---|---|
| `std::unique_ptr<int>` | ✅ | ❌ |

"So this is a rare example of a type where Send and Sync don't go together." Keep this in mind: in the section after next, this type is the "main casualty" of a design decision.

The slide adds the precise reason: *`unique_ptr<T> const&` still allows mutable access to the underlying object, so it should not be Sync.*

### std::shared_ptr<int>

"So obviously the next question is shared pointer." Does a `std::shared_ptr<int>` have the Send property? "No, it doesn't."

This surprises people, because `shared_ptr` is often described as "thread-safe". That description is half true, and it helps to be precise about which half:

- The **reference count** is updated atomically. Copying and destroying `shared_ptr`s that point to the same object from different threads is safe. "That's not going to be a problem when we're passing shared pointers by value to other threads. Reference count is going to increase. Things are going to stay alive for the whole duration of that other thread."
- The **pointee** is not protected at all. "The problem is that access to the int in this case is not synchronised." Two threads holding copies of the same `shared_ptr<int>` can both write `*p` and race.

**Listing 16** — shared_ptr: the count is atomic, the pointee is not *(illustrative example, not from the talk)*

```cpp
#include <memory>
#include <thread>

int main()
{
    auto p = std::make_shared<int>(0);

    auto work = [p] {                    // copying p: atomic refcount increment, safe
        for (int i = 0; i < 1000; ++i) {
            ++*p;                        // unsynchronised write to the shared int: data race
        }
    };

    std::jthread a(work);
    std::jthread b(work);
}   // last owner frees the int: safe, thanks to the atomic count
```

Copying a `shared_ptr` into a thread therefore *creates sharing* of mutable data, so it fails our first question. "And for similar reasons, [it] should not have the Sync property."

### What about shared_ptr<const T>?

An audience member asks about `shared_ptr` to const. Intuitively that looks like the immutability strategy from the first section: many owners, nobody can write. The speaker's answer is a model of experimental caution: "I believe a shared pointer to const should have the Sync property, but I'm not 100% convinced about it because of the presence of type-erased destructors doing weird things." A `shared_ptr` can carry a custom deleter, chosen at runtime and hidden behind type erasure, and that deleter runs on whichever thread drops the last reference. Since he could not convince himself it was always safe, he left the example off his slides: "it's something I would have to think about more carefully."

That pattern repeats through the talk. When a case is unclear, the design errs on the side of saying **no**. A false "no" costs some convenience; a false "yes" costs undefined behaviour.

**Listing 17** — Sharing read-only data with shared_ptr<const T> *(illustrative example, not from the talk)*

```cpp
#include <map>
#include <memory>
#include <print>
#include <string>
#include <thread>

int main()
{
    // Build the data, then freeze it.
    auto table = std::make_shared<const std::map<std::string, int>>(
        std::map<std::string, int>{{"one", 1}, {"two", 2}});

    std::jthread a([table] { std::println("{}", table->at("one")); });
    std::jthread b([table] { std::println("{}", table->at("two")); });
    // table->at("one") = 5;   // error: the map is const
}
```

## 9. User-defined types and the mutable keyword

*Talk segment: [26:50](https://www.youtube.com/watch?v=-FP6tLlCxWE&t=1610s)*

So far every type on the list came from the standard library. Real programs are mostly made of our own types, so the last two examples in the exercise are user-defined.

### A plain aggregate

**Listing 18** — code from the talk (Send and Sync types), shown at [26:57](https://www.youtube.com/watch?v=-FP6tLlCxWE&t=1617s)

```cpp
struct Person {
    std::string name;
    int age;
};
```

Is `Person` safe to send by value? "Yes, it is. This is purely composed of types that are … in this case, they're pure value types, but in general, purely composed of types that have the Send property." A `std::string` and an `int` are both value types, so a copy of a `Person` is fully independent.

By const reference? Also yes: "it is composed entirely of … types that have the Sync property." A `const Person&` only lets you read `name` and `age` through const access, and both members are safe to share read-only.

| Type | Send | Sync |
|---|---|---|
| `struct Person { std::string name; int age; };` | ✅ | ✅ |

The slide then states the rule that generalises this example:

> *A user-defined type is Send/Sync if all of its members are Send/Sync respectively.*

"And actually, this is exactly the way that the Rust compiler automatically generates the Send and Sync traits for user-defined types. It just looks at all the members, goes, 'Well, do they all have the Send property? Okay, then this type is Send.' Likewise for Sync."

This **structural rule** is the key to making the whole idea practical. If every user-defined type needed a hand-written specialisation, nobody would use it. With a structural rule, users only ever classify the small number of *leaf* types (the low-level building blocks), and everything built from them follows automatically. Hold that thought until the reflection sections, where we teach the C++ compiler to apply this rule.

**Listing 19** — Structural derivation, by hand *(illustrative example, not from the talk)*

```cpp
#include <string>
#include <type_traits>

template <typename T> constexpr bool is_safe_v = std::is_arithmetic_v<T>;
template <> constexpr bool is_safe_v<std::string> = true;

struct Person { std::string name; int age; };
struct Pair   { Person a; Person b; };

// Without reflection we must spell out the member-wise rule for each type:
template <> constexpr bool is_safe_v<Person> = is_safe_v<std::string> && is_safe_v<int>;
template <> constexpr bool is_safe_v<Pair>   = is_safe_v<Person> && is_safe_v<Person>;

static_assert(is_safe_v<Pair>);
// Tedious and error-prone: add a member and forget to update the trait, and it silently lies.
// The reflection sections later in the chapter automate exactly this.

int main() {}
```

### One small change: mutable

**Listing 20** — code from the talk (Send and Sync types), shown at [28:09](https://www.youtube.com/watch?v=-FP6tLlCxWE&t=1689s)

```cpp
struct Person {
    mutable std::string name;
    int age;
};
```

"Slight alteration. What about this type?" The only difference is the `mutable` keyword on `name`.

Send? "Yes, it does." The speaker had expected to catch the audience out, but did not: "You guys are really good." Passing a `Person` by value still gives the receiver full ownership of its own string and int.

Sync? No. "Because the mutable keyword means that even if we have a const instance of a person, we can still modify the name field in an unsynchronised manner."

| Type | Send | Sync |
|---|---|---|
| `struct Person { mutable std::string name; int age; };` | ✅ | ❌ |

> *A type with an unsynchronised mutable member is not safe to pass by const reference.*

### What mutable is for, and why it is dangerous here

`mutable` exists so that a member can change even inside a const member function. Two legitimate uses are common:

1. **Caches and lazy computation.** A const `area()` function computes the result once and stores it in a `mutable` member. Logically the object did not change, so the function should be const.
2. **Synchronisation objects.** A `mutable std::mutex` lets a const getter lock the mutex.

The first use is a hidden data race waiting to happen. Two threads calling the "read-only" const function at the same time both write the cache:

**Listing 21** — A mutable cache: const member function, hidden race *(illustrative example, not from the talk)*

```cpp
#include <cmath>
#include <optional>
#include <thread>

class circle {
    double radius_;
    mutable std::optional<double> area_cache_;   // logically const, physically mutable
public:
    explicit circle(double r) : radius_(r) {}

    double area() const
    {
        if (!area_cache_) {                         // two threads may both see "empty"...
            area_cache_ = M_PI * radius_ * radius_; // ...and both write: data race
        }
        return *area_cache_;
    }
};

int main()
{
    const circle c(2.0);   // even a born-const object is not safe to share!
    std::jthread a([&c] { (void)c.area(); });
    std::jthread b([&c] { (void)c.area(); });
}
```

The second use is the *fix*. Protect the mutable state with a mutex (itself `mutable`), or make it atomic. Then the type really is safe to share, even though it contains `mutable` members. Note the consequence for an automatic rule, though. A rule that says "any `mutable` member makes the type unsafe" will reject the safe version too, because it cannot tell a properly locked cache from an unlocked one. That is a recurring theme: an automatic algorithm will sometimes be too conservative, and the programmer needs a way to override it. We will see that the speaker's design has exactly such an override.

### Enough examples

The speaker originally planned many more rapid-fire examples, but "it seems like everyone's got a really good idea about this". Privately, he went through "loads" of them, "all the gnarly things I could think of". His conclusion: we understand well enough when the properties should apply to C++ types. "Type traits are a thing we can do. Constrained functions are a thing we can do … Let's start building something."

## 10. One concept instead of two: sendable

*Talk segment: [29:10](https://www.youtube.com/watch?v=-FP6tLlCxWE&t=1750s)*

### The plan for the C++ implementation

With a good feel for which types should and should not cross thread boundaries, the speaker sets out the plan:

> We're going to write type traits to mark types which are safe to send to other threads, and a `spawn_thread()` function which uses the traits to reject unsafe arguments.

That mirrors Rust exactly: marker traits plus a constrained spawning function. But there is one deliberate deviation. "We're not just going to slavishly follow the way that Rust does things." He admits to being "quite lazy … as all good programmers are", and asks: "why implement two traits when you can implement one?"

### One concept: sendable

> For our implementation, we're going to combine Rust's `Send` and `Sync` traits into a single **`sendable`** concept.
>
> A type is `sendable` if:
> - it is safe to pass to a thread by value
> - **and** it is safe to pass to a thread by const reference

"Sendable" is "a more C++-y name for this". It is also the name Swift uses for a similar single protocol, which will come up again when we handle class hierarchies.

As always in this talk, the decision comes with a caveat: "I don't know that this is the correct choice … there are good reasons in Rust that it does have these two traits. I think that this is a reasonable simplification in C++, but I could be wrong about that."

To justify it, he looks at the two kinds of types that the merged concept would treat wrongly. Those are the types with one property but not the other. Think of a 2×2 grid:

| | **Sync** | **not Sync** |
|---|---|---|
| **Send** | value types: `int`, `vector`, `span<const int>` → *sendable* | owners with shallow const: `unique_ptr` |
| **not Send** | reference types that try to be "deep const" | mutable views: `span<int>`, `shared_ptr<int>` → *not sendable* |

The diagonal is unaffected by merging the traits. The question is how much we lose in the other two cells.

### Sync but not Send

> A type can be Sync but not Send if:
> - its non-const member functions allow unsynchronised remote mutation
> - but its const member functions do not

Unpack that. The type is *not* a plain value type, because then it would be Send. Its non-const member functions can mutate some "far-away data" that it refers to, and that is shared with others. But it is also not a plain mutable view like `span<int>`, because then it would not be Sync either. So its *const* member functions refuse to give mutable access to the far-away data.

"In other words, what we're looking at is a type that has reference semantics but still tries to be deep const." That is odd in C++. "For reference types like span, top-level const doesn't prevent you from mutating the data … It's not the way language references work. It's not the way language pointers work." A shallow handle that pretends to be deep-const is "semantically questionable", and the speaker thinks such types are rare in practice.

**Listing 22** — A questionable "Sync but not Send" handle *(illustrative example, not from the talk)*

```cpp
#include <vector>

// A handle to someone else's vector that tries to be deep-const:
class weird_handle {
    std::vector<int>* target_;
public:
    explicit weird_handle(std::vector<int>& v) : target_(&v) {}

    void push(int x) { target_->push_back(x); }                // non-const: mutates far-away data
    int size() const { return static_cast<int>(target_->size()); }  // const: read-only
};

// Sharing a const weird_handle& between threads only allows size(): "Sync".
// But a *copy* sent by value lets the receiver push() into the original vector
// while others read it: not "Send". Copying a handle that claims deep const is surprising.

int main()
{
    std::vector<int> v;
    weird_handle h(v);
    weird_handle copy = h;   // shallow copy...
    copy.push(1);            // ...that mutates the original
}
```

So rejecting this cell costs very little.

### Send but not Sync

> A type can be Send but not Sync if:
> - it has ownership of its members
> - and it allows unsynchronised mutation of its data via const member functions

This sounds just as unusual, an owner whose const functions mutate its data, but there is "one fairly everyday type that does this, and we've already seen it": **`std::unique_ptr`**. The merged concept cannot accept it, because it is not safe by const reference. So "the main casualty of this simplification … is that we can't pass unique pointers to threads, at least not without some extra work."

### std::indirect to the rescue

"This might have been enough for me to reject this simplification, but C++26 brings in this `std::indirect` type."

`std::indirect<T>` (in `<memory>`) is a C++26 class template that owns a single heap-allocated `T`, like `unique_ptr`, but with **value semantics**:

- **Copying** an `indirect` copies the owned object (a deep copy). It does not share it.
- It is **const-correct**: `operator*` on a `const std::indirect<T>` returns a `const T&`. Const propagates through to the owned object.

In the speaker's words, it "behaves kind of like a value that happens to be allocated on the heap". Because of that, `std::indirect<T>` can be both Send and Sync whenever `T` is: it sits in the comfortable top-left cell of the grid.

**Listing 23** — unique_ptr versus std::indirect under const *(illustrative example, not from the talk)*

```cpp
#include <memory>
#include <type_traits>

int main()
{
    const auto up = std::make_unique<int>(1);
    *up = 2;                                      // compiles: shallow const

    const std::indirect<int> ind(1);
    // *ind = 2;                                  // error: const propagates to the owned int
    static_assert(std::is_same_v<decltype(*ind), const int&>);

    std::indirect<int> copy = ind;               // deep copy: independent heap object
    *copy = 3;                                   // does not affect `ind`
    return *ind;                                 // still 1
}
```

"So, because of the presence of indirect, I'm going to say, and again, caveat, could be wrong, that this simplification is valid, at least for my experimental thing that I want to do today. You are free to disagree and heckle me and yell at me."

> **Design lesson.** A simplification is justified when what it rules out is either rare or has a ready replacement. Sync-but-not-Send types are rare. The one common Send-but-not-Sync type, `unique_ptr`, has a replacement in `std::indirect`. Swift made the same choice with its single `Sendable` protocol.

If your standard library does not yet provide `std::indirect`, the idea is simple enough to sketch, and writing it shows exactly where const propagation happens:

**Listing 24** — A minimal const-propagating box (an indirect-like sketch) *(illustrative example, not from the talk)*

```cpp
#include <memory>
#include <utility>

template <typename T>
class box {
    std::unique_ptr<T> p_;
public:
    template <typename... Args>
    explicit box(Args&&... args) : p_(std::make_unique<T>(std::forward<Args>(args)...)) {}

    box(const box& other) : p_(std::make_unique<T>(*other.p_)) {}   // deep copy
    box& operator=(const box& other) { *p_ = *other.p_; return *this; }
    box(box&&) noexcept = default;
    box& operator=(box&&) noexcept = default;

    T&       operator*()       { return *p_; }    // mutable access only through non-const box
    const T& operator*() const { return *p_; }    // const box => const T: deep const
};

int main()
{
    const box<int> b(41);
    // *b = 42;          // error, unlike const std::unique_ptr<int>
    box<int> c = b;
    *c = 42;            // c is an independent copy
    return *b;          // 41
}
```

### Where this leaves us

The design now has two parts. First, a single type trait, `sendable`, for types that are both Send and Sync. Second, a constrained function that launches new threads only with `sendable` arguments. "We know how to do that in C++."

But one big piece of Rust's convenience is still missing: automatic derivation. "What would be really, really nice to do is if we could somehow emulate what the Rust compiler does, which is to automatically look at the members of your type and decide, 'Okay, all of these members are Send, so this composite type is going to be Send.'"

## 11. A crash course in C++26 reflection

*Talk segment: [34:40](https://www.youtube.com/watch?v=-FP6tLlCxWE&t=2080s)*

### "Reflect all the things"

The speaker asks the room how we could "somehow look at all the members of a user-defined type". Someone shouts the answer: **reflection**. "Reflection! Well, yes, we are programming in C++26, and that means that by law we must reflect all the things." The slide shows the familiar meme: *REFLECT ALL THE THINGS*.

Static reflection is one of the headline features of C++26. It lets a program ask the compiler questions about its own entities at compile time: what members a class has, what their types are, whether a type is an enum, and so on. It can then turn the answers back into types and expressions. That is exactly what we need to apply the structural rule ("a type is sendable if all its members are") automatically.

The speaker is candid that this was also an experiment in using reflection: "This was also my first use of reflection." At the time he had no local compiler that supported it, so "I was doing all of this on Compiler Explorer." And: "There are probably better, more elegant ways to do this … I might look at this code in a year's time and go, 'What on earth were you thinking?' … But I wrote something, I got there, it worked, so I was happy."

Since the next sections depend on it, here is a short tutorial on the parts of reflection the talk uses.

### The building blocks

**1. The reflection operator `^^`.** Applying `^^` to a type (or a namespace, variable, function and so on) produces a value of type `std::meta::info`, an opaque handle that describes the entity. `^^int`, `^^std::string` and `^^T` inside a template are all values of the same type.

**2. `std::meta::info` is an ordinary (compile-time) value.** You can store it in a `constexpr` variable, pass it to functions and compare it. Because it describes compile-time entities, it can only exist during constant evaluation, which is why the functions that use it are `consteval`.

**3. Query functions in `std::meta`.** The `<meta>` header provides `consteval` functions that answer questions about an `info`:

| Query | Meaning |
|---|---|
| `is_complete_type(r)` | Is the type complete (not just forward-declared)? |
| `is_fundamental_type(r)`, `is_enum_type(r)`, `is_class_type(r)` | Type categories, as in `<type_traits>` |
| `is_pointer_type(r)`, `is_reference_type(r)`, `is_member_pointer_type(r)`, `is_function_type(r)` | More categories |
| `is_const_type(r)` | Is it const-qualified? |
| `remove_pointer(r)`, `remove_reference(r)`, `remove_const(r)` | Transformations; return another `info` |
| `is_polymorphic_type(r)`, `is_final_type(r)` | Class properties |
| `bases_of(r, ctx)`, `nonstatic_data_members_of(r, ctx)` | Lists of `info`s for base classes and data members |
| `type_of(m)` | The type of a member or base |
| `is_mutable_member(m)` | Was the data member declared `mutable`? |
| `identifier_of(m)` | The member's name as a string |

Most of these mirror familiar type traits. The difference is that they work on *values*, so you can loop over them and store them in arrays.

**4. Access contexts.** Functions that list members take an `access_context`, which says whose point of view to use for access checking. `access_context::unchecked()` means "show me everything, including private members". We need that, because privacy must not hide a dangerous member from a safety check.

**5. Splicers `[: r :]`.** A splicer turns an `info` back into program syntax. `typename[: r :]` is the type that `r` reflects, so `is_sendable_v<typename[: remove_const(r) :]>` instantiates a template with the type computed by reflection. The speaker also uses a `using` alias, `using base_type = [:type_of(base):];`.

**6. Expansion statements: `template for`.** A normal `for` loop runs at run time, but members have different types, so each iteration needs its own compile-time code. `template for` expands its body once per element of a compile-time range, like a fold expression in statement form.

**7. `std::define_static_array`.** `nonstatic_data_members_of` returns a `std::vector<info>`, which cannot persist beyond constant evaluation. `define_static_array` copies the elements into a static array that a `template for` can iterate over.

Here they are working together. This program prints the names and types of a struct's members:

**Listing 25** — Listing a struct's members with C++26 reflection *(illustrative example, not from the talk)*

```cpp
#include <meta>
#include <print>
#include <string>

struct Person {
    std::string name;
    int age;
private:
    double secret = 0.0;
};

int main()
{
    constexpr auto ctx = std::meta::access_context::unchecked();   // include private members

    template for (constexpr auto m :
                  std::define_static_array(std::meta::nonstatic_data_members_of(^^Person, ctx))) {
        std::println("{} : {}", std::meta::identifier_of(m),
                     std::meta::display_string_of(std::meta::type_of(m)));
    }
}
// Build: g++ -std=c++26 -freflection
```

### A first useful trait

Combining these pieces gives a trait that applies a rule to every member, the shape of what we are about to build:

**Listing 26** — A member-wise trait: "all members are arithmetic" *(illustrative example, not from the talk)*

```cpp
#include <meta>
#include <type_traits>

template <typename T>
consteval bool all_members_arithmetic()
{
    constexpr auto ctx = std::meta::access_context::unchecked();
    template for (constexpr auto m :
                  std::define_static_array(std::meta::nonstatic_data_members_of(^^T, ctx))) {
        if constexpr (not std::is_arithmetic_v<typename[: std::meta::type_of(m) :]>) {
            return false;
        }
    }
    return true;
}

struct point { int x; int y; };
struct named_point { const char* name; int x; int y; };

static_assert(all_members_arithmetic<point>());
static_assert(not all_members_arithmetic<named_point>());

int main() {}
```

### Compiler support

At the time of writing, reflection is new. The speaker used Compiler Explorer. The diagnostics in his screenshots are in Clang's style, consistent with the experimental Clang implementation of the reflection proposal that Compiler Explorer offers. GCC 16 also implements C++26 reflection behind the `-freflection` flag, and the examples in this chapter were checked with it. Implementations still differ in details, and we will meet one such difference, how lambdas are reflected, later in the chapter.

## 12. Implementing sendable, part 1: the shape and the scalar cases

*Talk segment: [37:10](https://www.youtube.com/watch?v=-FP6tLlCxWE&t=2230s)*

### The shape of the solution

The speaker starts with the overall structure before diving into the details. "Let's start at the bottom":

**Listing 27** — code from the talk (Implementing sendable with reflection), shown at [37:11](https://www.youtube.com/watch?v=-FP6tLlCxWE&t=2231s)

```cpp
template <typename T>
consteval bool is_sendable_impl();

template <typename T>
constexpr bool is_sendable_v = is_sendable_impl<T>();

template <typename T>
concept sendable = is_sendable_v<T>;
```

Read it from the bottom up, as he does:

1. **`sendable`** is the concept that users write in `requires` clauses. It is defined entirely in terms of the next layer.
2. **`is_sendable_v<T>`** is a `constexpr bool` variable template. This is the **customisation point**. Users can specialise it "so that if the default implementation doesn't get it right for whatever reason, they can override it". They can override it in either direction: "this type actually is sendable even though the default implementation says not", or, "in hopefully rare cases", the opposite: "the default algorithm has claimed my type is sendable, but I know it's not."
3. **`is_sendable_impl<T>()`** is a `consteval` function that "actually runs the default algorithm". It is only declared at this point and defined later, which lets the algorithm call `is_sendable_v` recursively on member types.

This three-layer pattern is worth recognising, because it is a good way to design *any* customisable trait:

| Layer | Role | Who touches it |
|---|---|---|
| concept | the vocabulary used in constraints | users of the library |
| variable template | the customisation point (specialise to override) | authors of special types |
| `consteval` function | the default rule | the library author |

Why not let users specialise the concept directly? Because concepts cannot be specialised. Why not put the algorithm directly in the variable template's initialiser? Because the algorithm is long, needs `if constexpr` and loops, and should be shared by every specialisation that falls back to the default. A `consteval` function, one that *must* be evaluated at compile time, is the natural home for it, especially since reflection values can only exist at compile time.

**Listing 28** — Opting in and opting out with the customisation point *(illustrative example, not from the talk)*

```cpp
// Uses the sendable machinery from Listing 27, Listing 29, Listing 32 and Listing 33.
#include <atomic>
#include <mutex>
#include <string>

// Opt IN: a hand-rolled thread-safe type the default algorithm would reject
// (it has a mutable member). Specialising is our "unsafe impl Send + Sync".
class counter {
    mutable std::mutex m_;
    long value_ = 0;
public:
    void add(long n)   { std::scoped_lock lock(m_); value_ += n; }
    long get() const   { std::scoped_lock lock(m_); return value_; }
};
template <>
constexpr bool is_sendable_v<counter> = true;

// Opt OUT: a type that looks harmless member-wise but hands out
// mutable access to global state from its const member functions.
inline std::string global_log;
struct logger {
    int level = 0;
    std::string& log() const { return global_log; }
};
template <>
constexpr bool is_sendable_v<logger> = false;

static_assert(sendable<counter>);
static_assert(not sendable<logger>);

int main() {}
```

Remember the earlier warning, though: a specialisation to `true` is the C++ equivalent of Rust's `unsafe impl Send`. The compiler takes your word for it.

### The default algorithm, part 1

"So what does `is_sendable_impl` look like? Well, it's quite a long function. As you can see, it sort of drags off the bottom of the screen, but it's actually … pretty simple to work out what it's doing. We just go through a lot of the properties of C++ types and work out whether those properties mean that we want these things to be sendable or not."

**Listing 29** — code from the talk (Implementing sendable with reflection), shown at [38:07](https://www.youtube.com/watch?v=-FP6tLlCxWE&t=2287s)

```cpp
template <typename T>
consteval bool is_sendable_impl()
{
    using namespace std::meta;

    constexpr auto info = ^^T;

    // Incomplete types are not sendable
    if constexpr (not is_complete_type(info)) {
        return false;
    }

    // Fundamental types are sendable
    if constexpr (is_fundamental_type(info)) {
        return true;
    }

    // Enum types are sendable
    if constexpr (is_enum_type(info)) {
        return true;
    }
```

Line by line:

- **`using namespace std::meta;`** brings the reflection queries into scope, so the code can say `is_enum_type(info)` instead of `std::meta::is_enum_type(info)`.
- **`constexpr auto info = ^^T;`** reflects the type parameter. From here on the function asks questions about `info`.
- **Incomplete types are not sendable.** A forward-declared class has no visible members, so we cannot inspect it. "You can forward-declare a class, … try and pass a const pointer to that class. We don't know anything about it, so we're going to disallow that." This is the "when unsure, say no" principle again.
- **Fundamental types are sendable.** Fundamental types are the arithmetic types (`int`, `double`, `char`, `bool` and so on), `void` and `std::nullptr_t`. "Ints, floats, char types, all of those things. Those are all value types."
- **Enum types are sendable.** "Enums are just ints in disguise."

Each check uses `if constexpr`. Only the branch that applies to a given `T` is instantiated, which matters because later branches splice types that only make sense for particular kinds of `T`.

Notice also the order: completeness is checked first, because no other question can be answered about a type we cannot see.

There is a subtlety here that is easy to miss, and that showed up when the slide code was compiled for this chapter. Each check is a *separate* `if constexpr` statement, not an `else if constexpr` chain. An early `return false;` stops *execution*, but it does not stop *instantiation*. In a template, every `if constexpr` whose condition is true has its body compiled, even if an earlier branch has already returned. For an incomplete class, `is_class_type` is still true. So the class-type branch shown in the next section gets instantiated and tries to list the members of a type that has none visible, which is a hard compile error rather than a clean "not sendable". With GCC 16, asking whether a forward-declared class (or a pointer to one) is sendable fails to compile for this reason. The fix is a one-word change per branch: chain the checks with `else`.

**Listing 30** — Why the branches need else: discarded statements and instantiation *(illustrative example, not from the talk)*

```cpp
#include <meta>

template <typename T>
consteval bool separate_ifs()
{
    if constexpr (not std::meta::is_complete_type(^^T)) { return false; }
    // Still instantiated for incomplete T, because it is not in an else branch:
    if constexpr (std::meta::is_class_type(^^T)) { return sizeof(T) > 0; }
    return true;
}

template <typename T>
consteval bool chained_ifs()
{
    if constexpr (not std::meta::is_complete_type(^^T)) { return false; }
    else if constexpr (std::meta::is_class_type(^^T)) { return sizeof(T) > 0; }   // discarded
    else { return true; }
}

struct forward_declared;

static_assert(not chained_ifs<forward_declared>());      // OK
// static_assert(not separate_ifs<forward_declared>());  // error: sizeof applied to incomplete type

int main() {}
```

> **Note.** The speaker's published code also handles `const`-qualified types right after the completeness check: a `const T` is sendable if `T` is. This line is not on the slides, but you need it when the algorithm recurses into members declared `const`.

Here is a quick test harness for the cases so far. Writing `static_assert` tables like this is the natural way to develop a compile-time trait: each line is a test that runs every time you compile.

**Listing 31** — A static_assert test table for the scalar cases *(illustrative example, not from the talk)*

```cpp
// Uses the sendable machinery from Listing 27, Listing 29, Listing 32 and Listing 33.
#include <cstddef>

enum class colour { red, green, blue };

static_assert(sendable<int>);
static_assert(sendable<double>);
static_assert(sendable<char8_t>);
static_assert(sendable<bool>);
static_assert(sendable<std::nullptr_t>);
static_assert(sendable<colour>);
// Incomplete types: see the else-if-constexpr discussion above.

int main() {}
```

## 13. Implementing sendable, part 2: pointers, references and classes

*Talk segment: [39:10](https://www.youtube.com/watch?v=-FP6tLlCxWE&t=2350s)*

The second half of the default algorithm handles the cases that need judgement: pointers, references and, most importantly, classes.

### Pointers and references

**Listing 32** — code from the talk (Implementing sendable with reflection), shown at [39:15](https://www.youtube.com/watch?v=-FP6tLlCxWE&t=2355s)

```cpp
// ...continued

// Member pointer types are *not* sendable
if constexpr (is_member_pointer_type(info)) {
    return false;
}

// Pointer types are sendable if the pointee is a function type,
// or if the pointee is both const and sendable
if constexpr (is_pointer_type(info)) {
    constexpr auto pointee = remove_pointer(info);
    if constexpr (is_function_type(pointee)) {
        return true;
    } else {
        return is_const_type(pointee)
            && is_sendable_v<typename[:remove_const(pointee):]>;
    }
}

if constexpr (is_reference_type(info)) {
    constexpr auto referent = remove_reference(info);
    if constexpr (is_function_type(referent)) {
        return true;
    } else {
        return is_const_type(referent)
            && is_sendable_v<typename[:remove_const(referent):]>;
    }
}
```

**Member pointers are not sendable.** The reasoning is refreshingly honest: "This is principally because member pointers make my head hurt and I didn't want to think about it. So we're just going to say, 'Nah, you can't have those.'" It is another conservative choice. Saying "no" can never introduce a data race; it can only reject a program that might have been fine.

**Function pointers are sendable.** "A function pointer can't have any associated data … static lifetime, it's fine." A function lives for the whole program and has no state, so many threads can call through the same pointer safely. (The function might of course touch global variables; we come back to that weakness at the end of the chapter.)

**Other pointers are sendable only if the pointee is const and sendable.** This is our `span<const int>` reasoning applied to single objects: "a span is a pointer to, like, a fat pointer. So this is about pointers to single objects. Pointer to a const sendable type is sendable." Look at the implementation: `remove_pointer(info)` gets the pointee, `is_const_type` checks the const, and `remove_const` strips it before the recursive call `is_sendable_v<typename[: … :]>`. The splicer `typename[: … :]` turns the computed reflection back into a type so that it can be used as a template argument.

**References follow the same rule.** "We can't send references directly to another thread via the constructor, but you can capture them in a lambda. So we have to think about them." A lambda that captures by reference has reference-typed members, so this branch is what decides whether a capture like `[&x]` is acceptable: only if `x` is const and of a sendable type.

| Type | sendable? |
|---|---|
| `void(*)(int)` | ✅ function pointer |
| `const int*`, `const int&` | ✅ pointer/reference to const sendable |
| `int*`, `int&` | ❌ mutable access to shared data |
| `int Person::*` | ❌ member pointer |

### Class types: the meat of it

**Listing 33** — code from the talk (Implementing sendable with reflection), shown at [42:31](https://www.youtube.com/watch?v=-FP6tLlCxWE&t=2551s)

```cpp
// ...continued

if constexpr (is_class_type(info)) {
    // Polymorphic classes must also be final
    if constexpr (is_polymorphic_type(info) && not is_final_type(info)) {
        return false;
    }
    constexpr auto ctx = access_context::unchecked();

    // All bases must be sendable
    template for (constexpr auto base :
                     define_static_array(bases_of(info, ctx))) {
        using base_type = [:type_of(base):];
        if constexpr (not is_sendable_v<typename[:type_of(base):]>) {
            return false;
        }
    }

    // All NSDMs must be sendable and not declared mutable
    template for (constexpr auto member :
                    define_static_array(nonstatic_data_members_of(info, ctx))) {
        if constexpr (is_mutable_member(member)) {
            return false;
        }
        if constexpr (not is_sendable_v<typename[:type_of(member):]>) {
            return false;
        }
    }
    return true;
}
```

"This is kind of the meat of it. This is what happens if you have a user-defined type."

**Polymorphic classes must be final.** This rule does not come from Rust, which has no class inheritance. It comes from Swift, which "uses an approach that is pretty similar to the way Rust does things. They also just use a single trait called Sendable." Swift has to think about inheritance, and it requires a sendable polymorphic class to be final. The reason: "if you have a const pointer to base, and base is sendable, then the default algorithm would allow us to pass that, but we have no idea what the dynamic type is." A derived class could add mutable state or override a const virtual function with an implementation that mutates shared data without synchronisation. Requiring `final` guarantees that the static type *is* the dynamic type, so inspecting it tells the whole story. One consequence is easy to miss: because base classes are also checked recursively, a final class that derives from a polymorphic base is still rejected, since that base is itself polymorphic and not final. In practice the rule accepts polymorphic classes that are final *and* have no polymorphic bases.

An audience member (addressed as Anthony) pushes back: if you have a *value* of a polymorphic class type, not a pointer, it doesn't matter whether it is final. The speaker agrees. Our `sendable` combines Send and Sync, though, and the pointer case is the Sync side. Because of the merge, "it's going to rule out some types. That's actually … I hadn't thought of that, but you are correct." It is a nice illustration of the cost of merging the two traits, and of the experimental nature of the work: "perhaps we could deal with that with changes to this function."

**All bases must be sendable.** `bases_of(info, ctx)` lists the direct base classes. For each one, `type_of(base)` gives its type, and the algorithm recurses. (The `using base_type = …` line on the slide is unused; the check splices the type directly.) Recursion is what makes the rule structural: a class is checked by checking its parts, and their parts in turn.

**All non-static data members must be sendable and not declared mutable.** This is the structural rule from the `Person` example, plus the `mutable` lesson. With `access_context::unchecked()` the loop sees private members as well, which is essential: a private `int*` is just as dangerous as a public one.

If every check passes, the class is sendable. (The slide stops at the end of the class branch. In the speaker's repository, any type not matched by one of the branches falls through to a final `return false;`, conservative again.)

### Questions from the audience

**"What about a mutable mutex as a member variable?"** A properly synchronised class usually has one, as in the `counter` from the previous section. The default algorithm sees `mutable` and says no. "It's up to you to say that I have got this right and I'm going to specialise this `is_sendable` trait." An audience member adds that such a type is also not copyable or movable, because `std::mutex` is neither, so you would not be passing it by value anyway.

**"Does the previous slide work with unions?"** Unions are class types in C++, so they go through this branch. Should they be banned? The speaker reasons they are fine: "what's the unsafe thing you can do with unions? Changing the active member, right? But you can't do that with a const instance of the union … And if you have ownership of the union, well, it doesn't matter if you change the active member." So unions are left in, with the usual "I could be wrong."

When the slide code was compiled for this chapter, though, the answer turned out to depend on a detail of the reflection library. Unions are class types in the *core language*, but the type trait `std::is_class_v` is `false` for them (there is a separate `std::is_union_v`), and the reflection query `is_class_type` follows the trait. So with the algorithm exactly as shown, a union never enters the class branch. It falls through to the final `return false;` in the speaker's repository and is treated as not sendable. To get the behaviour the speaker argues for, you would add a branch that tests `is_union_type(info)` and checks the members the same way. It is a good example of why an experimental design should be backed by a table of tests.

### How well does it work?

"I think it's really awesome … it works and I was really happy with this." The results slide:

- With reflection, the vast majority of types have their `sendable` status determined automatically, and hopefully correctly.
- Only a few types still need to manually specialise `is_sendable_v`, "typically containers such as `std::vector`".
- In the (hopefully) very rare case that the algorithm wrongly decides a user-defined type is sendable, the user can specialise `is_sendable_v = false`.

Why does `std::vector` need help? Internally a vector stores *non-const* pointers to its buffer, so the member-wise rule says no. But the vector's interface gives that buffer value semantics with deep const, which is exactly the "library guarantee" from the vector discussion. The pointer members do not tell the whole story; the class's *behaviour* does. "And actually this is true in Rust as well": Rust's `Vec` is built on raw pointers, and its `Send`/`Sync` implementations are written by hand inside the standard library. The speaker's repository specialises vector like this:

**Listing 34** — Specialisations for containers whose behaviour hides their pointers *(illustrative example, not from the talk)*

```cpp
// Uses the sendable machinery from Listing 27, Listing 29, Listing 32 and Listing 33.
#include <string>
#include <vector>

// From the speaker's repository: a vector is sendable if its elements are.
template <typename T, typename A>
constexpr bool is_sendable_v<std::vector<T, A>> = is_sendable_v<T>;

// The same reasoning applies to std::basic_string, whose internal
// non-const char pointer otherwise makes the default answer "no".
template <typename C, typename Tr, typename A>
constexpr bool is_sendable_v<std::basic_string<C, Tr, A>> = is_sendable_v<C>;

struct Person { std::string name; int age; };

static_assert(sendable<std::vector<int>>);
static_assert(sendable<std::string>);
static_assert(sendable<Person>);                   // derived member-wise
static_assert(not sendable<std::vector<int*>>);    // elements are not sendable

int main() {}
```

When the examples in this chapter were checked with GCC 16 and libstdc++, `std::string` turned out to need the same treatment. It is also a value type built on a non-const internal pointer, so the default algorithm classifies it as not sendable until it is specialised.

Finally, a test table for this section's rules:

**Listing 35** — Testing pointers, references and class rules *(illustrative example, not from the talk)*

```cpp
// Uses the sendable machinery from Listing 27, Listing 29, Listing 32 and Listing 33.
#include <span>

struct point { int x; int y; };
struct holds_ptr { int* p; };
struct holds_cptr { const int* p; };
struct cached { mutable int cache; };

struct shape { virtual ~shape() = default; virtual double area() const = 0; };
struct square final : shape { double side; double area() const override { return side * side; } };
struct standalone final { int x; virtual int get() const { return x; } };
struct derived_point : point { int z; };

union number { int i; float f; };

static_assert(sendable<void(*)(int)>);
static_assert(sendable<const int*>);
static_assert(sendable<const int&>);
static_assert(not sendable<int*>);
static_assert(not sendable<int&>);
static_assert(not sendable<int point::*>);

static_assert(sendable<point>);
static_assert(sendable<derived_point>);         // bases are checked too
static_assert(not sendable<holds_ptr>);
static_assert(sendable<holds_cptr>);
static_assert(not sendable<cached>);            // mutable member
static_assert(not sendable<shape>);             // polymorphic, not final
static_assert(not sendable<square>);           // final, but its base `shape` is rejected
static_assert(sendable<standalone>);            // polymorphic, final, no polymorphic bases
static_assert(not sendable<number>);            // unions: is_class_type is false -> fallback
static_assert(not sendable<std::span<int>>);
static_assert(sendable<std::span<const int>>);

int main() {}
```

## 14. spawn_thread: a constrained wrapper around std::jthread

*Talk segment: [44:40](https://www.youtube.com/watch?v=-FP6tLlCxWE&t=2680s)*

### The spawning function

"So that's our type trait written. We just need to write a spawn function. This is not too difficult."

**Listing 36** — code from the talk (Spawning threads with sendable), shown at [44:51](https://www.youtube.com/watch?v=-FP6tLlCxWE&t=2691s)

```cpp
template <typename Fn, typename... Args>
    requires sendable<std::decay_t<Fn>> && (sendable<std::decay_t<Args>> && ...)
[[nodiscard]]
auto spawn_thread(Fn&& fn, Args&&... args) -> std::jthread
{
    return std::jthread(std::forward<Fn>(fn), std::forward<Args>(args)...);
}
```

It "just wraps the `jthread` constructor and constrains the arguments to be sendable". Let us go through it piece by piece.

**`template <typename Fn, typename... Args>`** accepts any callable `Fn` and any number of arguments, exactly like the `std::jthread` constructor.

**The `requires` clause** has two parts:

- `sendable<std::decay_t<Fn>>`: the function object itself must be sendable.
- `(sendable<std::decay_t<Args>> && ...)`: a **fold expression** that expands to `sendable<A1> && sendable<A2> && …` for every argument type. With zero arguments a `&&` fold is `true`.

**`[[nodiscard]]`** makes the compiler warn if the caller ignores the returned thread. With `std::jthread` that matters: if the result is discarded, the temporary `jthread` is destroyed at the end of the statement, and its destructor joins. The call would silently become synchronous.

**`-> std::jthread`** returns the thread handle, and the body simply forwards everything into the constructor. At run time this is a plain `std::jthread`; all the checking happens at compile time.

### Why constrain the function object?

"It also constrains the function object to be sendable. And that's because this can be a capturing lambda, and we want to make sure that any captures are sendable."

A lambda expression creates an object of a unique, unnamed **closure type**. Each capture becomes a data member of that type. A by-value capture `[x]` becomes a member of type `X`, and a by-reference capture `[&x]` behaves like a reference member. So "the captures are sendable" means exactly "the closure type's members are sendable", which is what our structural rule checks for every class.

"And the really nice thing is that because lambdas just generate … a function object, just a class type, the reflection algorithm just works on class types, and it just does the right thing for lambdas. I was so happy when this worked."

> **A portability note from testing this chapter.** Whether reflection can see a closure type's captures is up to the implementation. The talk's demos ran on Compiler Explorer, and the diagnostics on the slides are in Clang's style. With GCC 16 (`-freflection`), however, `nonstatic_data_members_of` reports *no* data members for a closure type. A lambda therefore looks like an empty class, and it passes the check whatever it captures. The talk's code, compiled with GCC 16, accepts the bad examples in the next sections that the speaker's compiler rejected. A named function-object class, whose members are ordinary declared members, is checked correctly on both. The illustrative examples from here on use such classes wherever a rejection matters.

**Listing 37** — Function objects: what the check sees *(illustrative example, not from the talk)*

```cpp
// Uses the sendable machinery from Listing 27, Listing 29, Listing 32 and Listing 33, and spawn_thread from Listing 36.
#include <string>
#include <string_view>

// A hand-written equivalent of  [sv] { std::println("{}", sv); }
struct print_view {
    std::string_view sv;             // by-value capture of a view of const chars
    void operator()() const { /* print sv */ }
};

// A hand-written equivalent of  [&str] { str = "goodbye"; }
struct overwrite {
    std::string& str;                // by-reference capture of a mutable string
    void operator()() const { str = "goodbye"; }
};

static_assert(sendable<print_view>);       // string_view: sendable
static_assert(not sendable<overwrite>);    // std::string&: mutable reference

int main()
{
    std::string s = "Hello";
    auto t = spawn_thread(print_view{s});  // OK
    // auto u = spawn_thread(overwrite{s}); // error: constraints not satisfied
    t.join();
}
```

### Why decay_t?

An audience member asks why the constraint uses `std::decay_t`. "Because the thread constructor decays its arguments." The `std::thread` and `std::jthread` constructors do not pass your arguments through as references. They make **decay copies**: references and cv-qualifiers are stripped, arrays become pointers, and the result is copied or moved into storage owned by the new thread. "That's what we're actually going to end up passing to the thread: the decayed version of the argument."

So the type that actually crosses the thread boundary is `std::decay_t<Args>`, and that is the type that must be sendable. Passing an lvalue `std::string` gives `Args = std::string&`, but the thread receives a `std::string` copy, which is perfectly safe.

The decay rule also explains the standard idiom for passing a reference to a thread on purpose: wrap it in `std::ref` or `std::cref`. Those produce `std::reference_wrapper<T>`, a small class that stores a pointer. Our structural rule then does the right thing automatically:

**Listing 38** — std::ref and std::cref through the sendable lens *(illustrative example, not from the talk)*

```cpp
// Uses the sendable machinery from Listing 27, Listing 29, Listing 32 and Listing 33, and spawn_thread from Listing 36.
#include <functional>

void read(const int& x)  { (void)x; }
void write(int& x)       { ++x; }

static_assert(not sendable<std::reference_wrapper<int>>);      // stores int*: mutable
static_assert(sendable<std::reference_wrapper<const int>>);    // stores const int*

int main()
{
    int value = 0;
    auto r = spawn_thread(read, std::cref(value));    // OK: shared read-only
    // auto w = spawn_thread(write, std::ref(value)); // error: not sendable
    r.join();
}
```

### Is stop_token sendable?

Another question: "Is `stop_token` sendable?" `std::jthread` can pass a `std::stop_token` to its function so that the thread can be asked to stop cooperatively. The speaker's answer: "I'll get back to you on that one."

This is a good question to think through with the tools from this chapter. A `std::stop_token` is a handle to shared stop state, so it has reference semantics. But that shared state is designed for concurrent use: requesting and checking a stop are synchronised operations. So semantically it belongs with the thread-safe types that should be opted in by specialisation. Its internal representation (a pointer to shared state) means the default algorithm would probably say no. Either way, the token is created by `std::jthread` itself, not passed through `spawn_thread`'s argument list, so the constraint never sees it in this design.

## 15. The moment of truth: what compiles and what must not

*Talk segment: [45:50](https://www.youtube.com/watch?v=-FP6tLlCxWE&t=2750s)*

"We've written our type traits. We've written our spawn function. Here we go, ladies and gentlemen. This is the moment of truth."

The speaker had planned a live demo on Compiler Explorer, "but I realised that I just don't have the confidence that I'm going to be able to do that. So instead you get Compiler Explorer screen grabs, and you're going to have to trust me that this is what happened."

### Test 1: something that should compile

**Listing 39** — code from the talk (The moment of truth), shown at [46:49](https://www.youtube.com/watch?v=-FP6tLlCxWE&t=2809s)

```cpp
int main()
{
    std::string str = "Hello";

    auto thread = spawn_thread([](auto sv) {
        std::println("{}", sv);
    }, std::string_view(str));

    thread.join();
}
```

A `std::string` holds `"Hello"`. The program spawns a thread whose lambda takes a parameter `sv` and prints it. The argument passed is a `std::string_view` pointing at the string. "String view is a const reference type, so … we agree that it's sendable. So this should, we hope, work."

Check it against the rules:

- The lambda captures nothing, so its closure type is empty and trivially sendable.
- The argument decays to `std::string_view`, a view of `const char`s, sendable just like `std::span<const int>`.
- The main thread does not modify `str` before calling `join()`, so the born-const assumption holds.

And the result:

**Listing 40** — code from the talk (The moment of truth), shown at [47:13](https://www.youtube.com/watch?v=-FP6tLlCxWE&t=2833s)

```console
ASM generation compiler returned: 0
Execution build compiler returned: 0
Program returned: 0
Hello
```

"Awesome. This works … this made me very happy."

### Why a passing test proves little

"This made me happy until I realised that, hang on, this would work just if I used `std::thread`." The program would have compiled and run without any of our machinery. A test that passes with or without the feature tells you nothing about the feature. "What we need to do is to try something that shouldn't work."

This is a general principle for any code whose job is to **reject** things: concepts, `static_assert`s, type traits, validators. You need *negative tests*, inputs that must fail, as well as positive ones.

### Test 2: something that must not compile

**Listing 41** — code from the talk (The moment of truth), shown at [47:37](https://www.youtube.com/watch?v=-FP6tLlCxWE&t=2857s)

```cpp
int main()
{
    std::string str = "Hello";

    auto thread = spawn_thread([&str] {
        str = "goodbye";
    });

    thread.join();
}
```

Now the lambda captures `str` **by mutable reference** (`[&str]`) and assigns to it from the new thread, while the main thread still owns the string. That is the classic race we want to forbid. The closure type has a member that behaves like a `std::string&`, a non-const reference, so the closure is not sendable, and neither is the call. "As the screen grab is rather handily given away, this fails":

**Listing 42** — code from the talk (The moment of truth), shown at [49:11](https://www.youtube.com/watch?v=-FP6tLlCxWE&t=2951s)

```console
<source>:16:19: error: no matching function for call to 'spawn_thread'
   16 |     auto thread = spawn_thread([&str] {
      |                   ^~~~~~~~~~~~
```

"This fails just as we wanted it to fail. So this made me very happy." (Recall the portability note from the previous section: with a compiler whose reflection does not expose lambda captures, this program would compile. The equivalent hand-written function object is rejected everywhere.)

### Error messages

"I'm going to be honest, the error message, I've clipped it there. The error messages are kind of terrible. They're not very long by C++ standards, but they give you absolutely no information about what failed." The diagnostic says only that there is no matching `spawn_thread`. It doesn't say which argument failed, or which member of which capture was the problem.

An audience member suggests using `static_assert` instead of a `requires` clause: "Probably, because the compiler spits out more information in a `static_assert` than it does in a constraint failure. Yes, you're right." Another suggests other tricks. The speaker's answer is pragmatic: "Getting it working was a priority. I haven't really thought about ways to improve the error messages."

There is a real trade-off here. A constraint removes the function from overload resolution, so other overloads can be chosen and other code can *ask* whether a call is valid. A `static_assert` inside the body cannot be detected that way, but it can carry a message. A common compromise is to keep the constraint and also add a deliberately poisoned overload for the failing case:

**Listing 43** — Keeping the constraint, adding a readable diagnostic *(illustrative example, not from the talk)*

```cpp
// Uses the sendable machinery from Listing 27, Listing 29, Listing 32 and Listing 33, and spawn_thread from Listing 36.
#include <thread>
#include <type_traits>

// Chosen only when the constrained spawn_thread is not viable.
template <typename Fn, typename... Args>
    requires (not (sendable<std::decay_t<Fn>> && (sendable<std::decay_t<Args>> && ...)))
auto spawn_thread(Fn&&, Args&&...) -> std::jthread
{
    static_assert(sendable<std::decay_t<Fn>>,
                  "spawn_thread: the function object (or one of its captures) is not sendable");
    static_assert((sendable<std::decay_t<Args>> && ...),
                  "spawn_thread: an argument is not sendable");
    return {};
}

struct overwrite { int& target; void operator()() const { ++target; } };

int main()
{
    int x = 0;
    (void)x;
    // auto t = spawn_thread(overwrite{x});
    // error: static assertion failed: spawn_thread: the function object
    //        (or one of its captures) is not sendable
}
```

### Turning negative tests into compile-time tests

A test that "must not compile" is awkward to keep in a test suite: if you write the bad call directly, your test file stops compiling. Concepts give a neat solution. Wrap "this call is valid" in a concept, then `static_assert` that it is *false*:

**Listing 44** — Testing that bad calls do NOT compile *(illustrative example, not from the talk)*

```cpp
// Uses the sendable machinery from Listing 27, Listing 29, Listing 32 and Listing 33, and spawn_thread from Listing 36.
#include <string>
#include <string_view>
#include <utility>

template <typename... Ts>
concept spawnable = requires(Ts&&... ts) { spawn_thread(std::forward<Ts>(ts)...); };

struct print_view { std::string_view sv; void operator()() const {} };
struct overwrite  { std::string& str;    void operator()() const { str = "goodbye"; } };
void takes_view(std::string_view) {}

// Positive tests
static_assert(spawnable<print_view>);
static_assert(spawnable<void(*)(std::string_view), std::string_view>);

// Negative tests: each line would be a compile error if written as a call.
static_assert(not spawnable<overwrite>);
static_assert(not spawnable<void(*)(int*), int*>);

int main() {}
```

### Where we are

"As far as it goes, we have got this working. We can pass const references and they work. Mutable references get blocked." The speaker did not stop at two examples: "I did a lot of examples, and it all seemed to be going pretty well until…"

## 16. A hole in the fence: the spawning thread

*Talk segment: [49:10](https://www.youtube.com/watch?v=-FP6tLlCxWE&t=2950s)*

### The program that broke the spell

"…until — well, actually it didn't take all that long. I wrote an example that looked like this":

**Listing 45** — code from the talk (A problem), shown at [50:01](https://www.youtube.com/watch?v=-FP6tLlCxWE&t=3001s)

```cpp
int main()
{
    std::vector<int> vec{1, 2, 3, 4, 5};

    auto thread = spawn_thread([](auto span) {
        std::print("{}", span);
    }, std::span<int const>(vec));

    vec.resize(100); // Uh oh

    thread.join();
}
```

Check it against our rules, just as the compiler does:

- The lambda captures nothing.
- The argument is a `std::span<int const>`, a read-only view of `int`s, which we established is sendable.
- So `spawn_thread` accepts the call, and the program compiles.

But look at the line commented `// Uh oh`. After spawning the thread, the *main* thread calls `vec.resize(100)`. Here is what happens, in the speaker's words: "at the moment of thread creation, span effectively takes a copy of the vector's internal data pointer and sends that pointer to another thread." Then, "as this thread is starting up, the operating system is doing whatever it does, we're going to go ahead and resize this vector to much larger, probably going to cause a reallocation. And suddenly this pointer is invalid."

Growing a vector from 5 to 100 elements usually needs a bigger buffer, so the vector allocates new memory, moves the elements and frees the old buffer. The worker's span still points into the freed buffer. Reading it is a use-after-free, and it is also a data race between the worker's reads and the main thread's writes to that memory. "And sure enough, if we try and run this, we get junk":

**Listing 46** — code from the talk (A problem), shown at [52:07](https://www.youtube.com/watch?v=-FP6tLlCxWE&t=3127s)

```console
ASM generation compiler returned: 0
Execution build compiler returned: 0
Program returned: 0
[-1042023263, 5, -1747711136, -1689989899, 5]
```

Two of the five printed numbers happen to be `5`, the rest is garbage, and a different run prints different garbage. (Running the same code for this chapter produced a similar pattern of junk.) "So this is bad in a whole load of ways … and this made me a bit sad."

### What went wrong?

Nothing in the type check was incorrect. `std::span<int const>` really is sendable *under the ground rules*. The problem is that the program broke one of those rules. Recall *born const*: the object shared by const reference must not be modified by its owner while it is shared. And recall the observation from that section: "the only person that has to play along with this game is the sending thread."

In this program the sending thread is the one that cheated. The worker received a read-only view and behaved perfectly. The main thread kept a *mutable* handle to the same data, the vector `vec` itself, and used it.

An audience member makes exactly this connection: "So this example of you taking the vector, is that similar to what you talked about at the beginning, about integers only Sync if it's a const int?" "Yes, that's exactly what I was talking about … We rely on the sending thread to cooperate with this."

This is the gap the missing borrow checker leaves. In Rust, creating the shared borrow of `vec` for the worker would forbid any mutable use of `vec` until that borrow ended, and `vec.resize(100)` would be a compile error. Our C++ check looks only at the *arguments* of `spawn_thread`. It has no way to see what the caller does with its own variables afterwards.

You do not even need a span to see this. Any sendable handle to data that the spawning thread keeps mutating will do:

**Listing 47** — The same hole with a plain pointer to const *(illustrative example, not from the talk)*

```cpp
// Uses the sendable machinery from Listing 27, Listing 29, Listing 32 and Listing 33, and spawn_thread from Listing 36.
#include <print>
#include <string>
#include <utility>

void show(const std::string* s) { std::println("{}", *s); }

template <>
constexpr bool is_sendable_v<std::string> = true;   // as discussed: a value type

int main()
{
    std::string message = "Hello";

    // const std::string* is sendable: pointer to const, sendable pointee.
    auto t = spawn_thread(show, &std::as_const(message));

    message = "a much longer string that forces a reallocation";   // the spawning thread cheats

    t.join();   // the worker may read freed memory: data race + use-after-free
}
```

### Three ways forward

The speaker lists the options:

1. **Assume the spawning thread is nice to us.** "This is kind of what we said originally … And it's what you have to do in C++ today." The spawning thread is the only one that *can* mess things up, so this is a well-localised rule to follow by discipline. "Perhaps we can just live with this. Well, but I didn't like that very much."
2. **Wait for C++ to get a borrow checker.** "When I said that the borrow checker is integral to the way Rust's data race freedom [works], this is what I meant."
3. **Something else.** That is the subject of the next section.

Before reading on, note what *does* fix this particular program: do not touch shared data until the thread that borrows it has finished. Moving the resize after `join()` makes the program correct. The next section turns that observation into an API.

**Listing 48** — Correct by ordering: mutate only after the join *(illustrative example, not from the talk)*

```cpp
// Uses the sendable machinery from Listing 27, Listing 29, Listing 32 and Listing 33, and spawn_thread from Listing 36.
#include <print>
#include <span>
#include <vector>

void print_all(std::span<int const> s) { std::println("{}", s); }

int main()
{
    std::vector<int> vec{1, 2, 3, 4, 5};

    auto thread = spawn_thread(print_all, std::span<int const>(vec));
    thread.join();        // the borrow ends here ...

    vec.resize(100);      // ... so mutating now is safe
}
```

## 17. A solution: fork and structured concurrency

*Talk segment: [51:20](https://www.youtube.com/watch?v=-FP6tLlCxWE&t=3080s)*

### Constrain the spawning thread too

"There's another thing we can do. Because what is it that we actually want to prevent? We want to prevent the spawning thread from mutating anything that we're using."

The problem with `spawn_thread` is the API shape. It hands back a thread handle, "you do whatever you like, and we join it later." Whatever the caller does between spawning and joining is invisible to the library. So change the shape: "What if we had a function that does the thread spawning, but it takes two function objects? And in this second function object, you write all the code that you would do in between the thread spawning and the join … and we constrain that function object with the `sendable` constraint."

The effect: "whatever you were doing in the main thread is subject to the same constraints as the things you do in the worker threads."

**Listing 49** — code from the talk (A solution), shown at [52:47](https://www.youtube.com/watch?v=-FP6tLlCxWE&t=3167s)

```cpp
template <typename Fn1, typename Fn2>
    requires sendable<std::decay_t<Fn1>> &&
             sendable<std::decay_t<Fn2>>
void fork(Fn1&& fn1, Fn2&& fn2)
{
    auto thread = spawn_thread(std::forward<Fn2>(fn2));
    auto{fn1}();
    thread.join();
}
```

The implementation is tiny:

1. **Constrain both callables.** `fn1` (the work for the current thread) and `fn2` (the work for the new thread) must both be sendable. The `requires` clause spells the two conditions over two lines.
2. **Spawn** a thread running `fn2`, using our `spawn_thread`.
3. **Run `fn1` on the current thread.** The expression `auto{fn1}()` uses C++23's `auto{x}` syntax, an explicit decay-copy: it makes a prvalue copy of `fn1` and calls it. The current thread therefore runs on its own copy of the callable, just as the new thread runs on its decay-copied `fn2`.
4. **Join.** When `fork` returns, both pieces of work are finished.

"So I call this function fork. I don't know that that's the correct name for a function that does something like this, but that's what I called it." (In the speaker's repository the same function appears under the name `in_parallel`.)

Now, any data the two lambdas both touch must be reachable from *both* captures. Since both captures must be sendable, neither lambda can hold a mutable reference to shared data. The rule "the spawning thread must be nice" has become a rule the compiler can check.

### The problem, revisited

**Listing 50** — code from the talk (A solution), shown at [53:45](https://www.youtube.com/watch?v=-FP6tLlCxWE&t=3225s)

```cpp
int main()
{
    std::vector<int> vec{1, 2, 3, 4, 5};

    fork(
        [&vec] {
            vec.resize(100);
        },
        [span = std::span<int const>(vec)] {
            std::print("{}", span);
        }
    );
}
```

The vector resize, which used to happen in the main thread after spawning, now lives in the first lambda, which captures `[&vec]`. The printing lives in the second, which captures a `std::span<int const>`. And "again, Compiler Explorer has handily given away that this does indeed fail to compile, because we can't capture the vector by mutable reference":

**Listing 51** — code from the talk (A solution), shown at [54:31](https://www.youtube.com/watch?v=-FP6tLlCxWE&t=3271s)

```console
<source>:26:5: error: no matching function for call to 'fork'
   26 |     fork(
      |     ^~~~
<source>:15:6: note: candidate template ignored: constraints not satisfied [with Fn1 = (lambda at <source>:27:9), Fn2 = (lambda at <source>:30:9)]
   15 | void fork(Fn1&& fn1, Fn2&& fn2)
      |      ^
<source>:13:14: note: because 'std::decay_t<(lambda at <source>:27:9)>' (aka '(lambda at <source>:27:9)') does not satisfy 'sendable'
   13 |     requires tcb::sendable<std::decay_t<Fn1>> &&
      |              ^
```

Read the diagnostic from the bottom up. The `requires` clause of `fork` (note the namespace `tcb::`, from the speaker's library) found that `std::decay_t<Fn1>`, the closure type of the first lambda at line 27, "does not satisfy `sendable`". That excluded the only candidate, so there is "no matching function for call to `fork`". Unlike the earlier `spawn_thread` error, this one at least names the failing lambda.

The bug from the previous section is now a compile error. (The portability note applies again: this relies on the compiler exposing lambda captures to reflection. The illustrative examples below use named function objects so that the rejection is visible on GCC too.)

### The cost: too strict

"Now, this is a bit more restrictive than we actually need to." `fork` stops the first function from mutating *anything* it captures by reference, "when actually we only have to be careful that we don't mutate things that are sent to the worker threads." A local accumulator that the worker never sees would be perfectly safe to update, but `fork` rejects it too. "This is what we get for not having a borrow checker." Rust can reason about each borrow individually. Our type-level check can only see "this callable holds a mutable reference", not *to what*.

### Where this leads: structured concurrency

"If we go back and look at this function, you can probably immediately think of ways that you could expand this. We could make this variadic and launch any number of threads … maybe those threading functions return values and maybe we pass a function that accumulates those values somehow." He admits this is a slippery slope: "very quickly you start … coming up with the world's worst task system that you've just handily cobbled together because you get really excited about this. It's absolutely not what I did."

The underlying idea is important, and it has a name: **structured concurrency**. Concurrent work is tied to a lexical scope. Work starts inside a block or function and is guaranteed to finish before that scope ends, just as local variables are destroyed at the end of their block. `fork` is the simplest possible structured-concurrency primitive, a *fork-join*:

- No thread outlives the call, so no borrowed data can be destroyed while a thread is still using it.
- All participants, including the "parent", are visible to the API at once, so all of them can be constrained.

That is why the speaker sees promise here: "I think it has potential to work quite nicely with task systems, with structured concurrency, with that sort of thing. I am optimistic."

Here are two of the extensions he mentions, written as illustrative examples. First, a variadic version that runs any number of sendable callables in parallel:

**Listing 52** — fork_all: a variadic fork-join *(illustrative example, not from the talk)*

```cpp
// Uses the sendable machinery from Listing 27, Listing 29, Listing 32 and Listing 33, and spawn_thread from Listing 36.
#include <array>
#include <atomic>
#include <print>
#include <thread>
#include <utility>

template <typename First, typename... Rest>
    requires sendable<std::decay_t<First>> && (sendable<std::decay_t<Rest>> && ...)
void fork_all(First&& first, Rest&&... rest)
{
    // Spawn one thread per extra callable ...
    std::array<std::jthread, sizeof...(Rest)> threads{ spawn_thread(std::forward<Rest>(rest))... };
    // ... run the first one here ...
    auto{first}();
    // ... and join them all before returning.
    for (auto& t : threads) t.join();
}

struct hello { int id; void operator()() const { std::println("hello from task {}", id); } };

int main()
{
    fork_all(hello{0}, hello{1}, hello{2}, hello{3});
}
```

Second, a version where each piece of work returns a value and the results are combined after the join:

**Listing 53** — fork with results: compute in parallel, combine after the join *(illustrative example, not from the talk)*

```cpp
// Uses the sendable machinery from Listing 27, Listing 29, Listing 32 and Listing 33, and spawn_thread from Listing 36.
#include <numeric>
#include <optional>
#include <print>
#include <span>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

template <typename Fn1, typename Fn2>
    requires sendable<std::decay_t<Fn1>> && sendable<std::decay_t<Fn2>>
auto fork_results(Fn1&& fn1, Fn2&& fn2)
{
    using R2 = std::invoke_result_t<std::decay_t<Fn2>&>;
    std::optional<R2> r2;
    {
        // The worker writes into r2, which only this function can see,
        // and the join below happens before anyone reads it.
        std::jthread worker([&r2, fn = std::decay_t<Fn2>(std::forward<Fn2>(fn2))]() mutable { r2.emplace(fn()); });
        auto r1 = auto{fn1}();
        worker.join();
        return std::pair{std::move(r1), std::move(*r2)};
    }
}

struct sum_of {
    std::span<const int> data;
    long operator()() const { return std::accumulate(data.begin(), data.end(), 0L); }
};

template <> constexpr bool is_sendable_v<std::vector<int>> = true;

int main()
{
    const std::vector<int> v{1, 2, 3, 4, 5, 6, 7, 8};
    std::span<const int> all(v);
    auto [left, right] = fork_results(sum_of{all.first(4)}, sum_of{all.last(4)});
    std::println("{} + {} = {}", left, right, left + right);
}
```

Both keep the same safety property: every callable must be sendable, and nothing escapes the call.

## 18. Limitations and a brighter future

*Talk segment: [55:40](https://www.youtube.com/watch?v=-FP6tLlCxWE&t=3340s)*

### What have we learned?

"Couple of minutes left." The speaker's summary slide is short and cheerful:

- "I had fun." ("That's what's most important.")
- "I learned about Rust."
- "I learned about reflection." ("Reflection's awesome. Don't know if anyone's said that yet.")
- "A `sendable` trait (or something like it) seems like a promising approach to reducing data races in C++ code."

In the talk he adds that "something like it" could mean "perhaps two separate traits". The Send/Sync merge is still an open question.

### Limitations

Then, true to the disclaimers at the start, an honest list of limitations.

**1. Mutable sharing always needs a lock.** "Since we don't have a borrow checker, we are forced to declare all unsynchronised mutable references as non-sendable." In Rust, "because of the borrow checker, you can send a mutable reference to exactly one thread": the borrow checker proves nobody else is using it. Our C++ version cannot prove that, so it must reject every `T&`. "Putting this the other way around, it means that even if we're sending a value to just one thread, if we want it to be sendable, we have to use a mutex." That is a run-time overhead Rust does not pay.

The way to share mutable data in this design is a type that *owns* its data and its lock, like the `synchronized<T>` wrapper from the section on how Rust prevents races. Such a type is safe to share, and you opt it in with a specialisation:

**Listing 54** — Opting a lock-owning wrapper into sendable *(illustrative example, not from the talk)*

```cpp
// Uses the sendable machinery from Listing 27, Listing 29, Listing 32 and Listing 33, and spawn_thread from Listing 36.
#include <mutex>
#include <print>
#include <utility>

template <typename T>
class synchronized {
    T value_;
    mutable std::mutex m_;
public:
    explicit synchronized(T v) : value_(std::move(v)) {}
    template <typename Fn>
    auto with_lock(Fn fn) const { std::scoped_lock lock(m_); return fn(value_); }
    template <typename Fn>
    auto with_lock(Fn fn)       { std::scoped_lock lock(m_); return fn(value_); }
};

// The default algorithm rejects it (mutable member), so we vouch for it --
// the C++ counterpart of Rust's `unsafe impl Sync`. Sharing it by reference
// is then allowed, as long as the value type itself is sendable.
template <typename T>
constexpr bool is_sendable_v<synchronized<T>&> = sendable<T>;

struct add_one {
    synchronized<int>& counter;
    void operator()() const { counter.with_lock([](int& n) { ++n; }); }
};

int main()
{
    synchronized<int> counter(0);
    static_assert(sendable<synchronized<int>&>);
    {
        auto a = spawn_thread(add_one{counter});
        auto b = spawn_thread(add_one{counter});
    }
    std::println("{}", counter.with_lock([](int n) { return n; }));   // 2
}
```

**2. `spawn_thread` alone doesn't prevent races.** "As we've seen, constraining the `spawn_thread()` function alone to require sendable arguments does not prevent data races." The spawning thread can still cheat, and the `fork` fix is "overly restrictive".

**3. Globals and statics are invisible.** "This approach does absolutely nothing to prevent other ways of getting data into threads: global variables, and that sort of thing." The check inspects what you *pass*, not what the code *reaches*. A function pointer counts as sendable, and the function behind it can use any global it likes:

**Listing 55** — A global variable slips past the check *(illustrative example, not from the talk)*

```cpp
// Uses the sendable machinery from Listing 27, Listing 29, Listing 32 and Listing 33, and spawn_thread from Listing 36.

int hits = 0;   // global mutable state: invisible to the sendable check

void count() { for (int i = 0; i < 1000; ++i) ++hits; }

int main()
{
    // A function pointer is sendable, so both calls are accepted ...
    auto a = spawn_thread(count);
    auto b = spawn_thread(count);
    // ... and the two threads race on `hits`.
}
```

**4. Friendly functions only.** The approach "still requires that the functions that are run in the threads are, you know, nice functions … they don't cast away const, or do any other myriad unsafe behaviours." A single `const_cast` inside a worker undoes the whole guarantee.

His overall verdict: "I think this is something that is interesting, exciting, has potential, but it's not perfect."

Here is how the pieces compare with Rust:

| Concern | Rust | This C++ design |
|---|---|---|
| Which types may cross threads | `Send` / `Sync`, auto-derived | `sendable`, auto-derived by reflection |
| Manual overrides | `unsafe impl` (visibly unsafe) | specialise `is_sendable_v` (no marker) |
| Mutable reference to one thread | allowed (borrow checker proves uniqueness) | rejected; use a synchronised wrapper |
| Spawning thread mutating lent data | compile error | allowed with `spawn_thread`; blocked by `fork` |
| Global mutable state | `static mut` access requires `unsafe` | unchecked |
| Escape hatches | `unsafe` blocks | `const_cast`, raw pointers, anywhere |

### A brighter future?

"Can we make it perfect? Well, hmm, maybe." The final technical slide points beyond this experiment to the C++ standards committee. At the recent WG21 meeting in Croydon, the Evolution Working Group (EWG) discussed **P3874, "Should C++ be a memory-safe language?"** by Jon Bauman and many co-authors. The speaker was in the room: "This was a very long discussion … a lot of opinions given, and at the end a poll was taken." He stresses that the results are public.

The poll, as shown on the slide:

> Encourage more work in the direction of P3874R1, which pursues a subset-of-superset strategy towards memory safety which guarantees UB-Free in a syntactically explicit and well-defined subset. We expect the author to do an audit of existing practice, strategies, etc, and return with a concrete, complete, actionable proposal.

| SF | F | N | A | SA |
|---|---|---|---|---|
| 50 | 24 | 5 | 3 | 1 |

(SF = strongly for, F = for, N = neutral, A = against, SA = strongly against.) "The people who were for and strongly for it, 74. People against, four … That is an overwhelming majority of EWG members who would be interested in seeing C++ moving in the direction of becoming a memory-safe language. So maybe we will be able to have perfection one day."

"Subset of superset" describes the shape of such a plan: first *extend* C++ with new facilities (the superset), then define a *subset* of the extended language in which undefined behaviour is impossible, so that code opts into the safe subset explicitly. A borrow-checking subset of that kind is exactly what would close the gaps listed above.

### Links

The code from the talk is at **github.com/tcbrindle/safer_concurrency**. The speaker warns it is "in a particularly rough state", but you can "use it on Compiler Explorer and have a bit of a play with it". As this chapter showed in several places, testing a few cases of your own is the best way to understand both what the design guarantees and where it stops.

## Key takeaways

- A data race needs three ingredients: concurrent access to the same memory, at least one write, and at least one unsynchronised access. In C++ and Rust it is undefined behaviour.
- Rust's Send means 'safe to pass by value to another thread'; Sync means 'safe to pass by const reference'. Both are marker traits, derived automatically from a type's members.
- In C++, value types (int, std::vector, strings, aggregates of values) are safe to share; mutable views (std::span<int>) and shallow-const owners (std::unique_ptr) are not.
- The talk merges Send and Sync into one sendable concept, on the grounds that types with only one of the two properties are rare or replaceable (std::indirect instead of std::unique_ptr).
- C++26 reflection can compute sendable automatically by walking a type's bases and non-static data members, much like the Rust compiler does.
- A spawn_thread constrained on sendable rejects mutable captures, but cannot stop the spawning thread from mutating data it lent out.
- Constraining the parent's work as well (a fork-join style API) closes that hole at the cost of being over-restrictive, which points towards structured concurrency.
- Without a borrow checker the approach stays incomplete: globals, const_cast and single-owner mutable references remain outside its reach.

## Glossary

| Term | Meaning |
|---|---|
| Data race | Two or more threads access the same memory location concurrently, at least one access is a write, and at least one is unsynchronised. Undefined behaviour in C++ and Rust. |
| Race condition | A bug where the result depends on the timing of threads (e.g. check-then-act). Can happen without any data race. |
| Send (Rust) | Marker trait: a value of the type can be safely moved (passed by value) to another thread. |
| Sync (Rust) | Marker trait: the type can be safely shared by shared/immutable reference between threads; T is Sync iff &T is Send. |
| Marker trait | A trait with no methods that only records a property of a type; the C++ analogue is a type trait. |
| Borrow checker | Rust's compile-time analysis that enforces 'one mutable reference XOR many shared references' and lifetimes. |
| sendable | The talk's single C++ concept: a type is sendable if it is safe to pass to a thread both by value and by const reference. |
| Born const | The talk's assumption that an object shared by const reference is never modified by its owner while shared. |
| Shallow const | const that applies only to the object itself, not to data it points to (pointers, span, unique_ptr). |
| Deep const | const that propagates to owned or referenced data, as with std::vector or C++26 std::indirect. |
| std::indirect | C++26 owning, heap-allocated wrapper with value semantics and const propagation. |
| Reflection operator (^^) | C++26 operator that yields a std::meta::info value describing a type, variable, member, etc. |
| Splicer [: :] | C++26 syntax that turns a std::meta::info back into a type or expression, e.g. typename[:r:]. |
| template for | C++26 expansion statement that repeats its body for each element of a compile-time range. |
| Structured concurrency | A style where concurrent work is scoped to a block or function and always joined before the scope ends. |
| Decay-copy | What std::thread/std::jthread do with arguments: strip references and cv-qualifiers and copy or move into the new thread. |

## Exercises

1. For each type, decide whether it should be sendable under the talk's rules and justify: double, std::string, std::string_view, std::span<double>, std::array<int, 4>, std::optional<std::vector<int>>.

   <details><summary>Hint</summary>Ask two questions: is it safe to pass by value, and is it safe to pass by const reference? Views of mutable data fail the first.</details>

2. Why is std::unique_ptr<int> Send but not Sync, while C++26 std::indirect<int> is both?

   <details><summary>Hint</summary>Compare what operator* returns on a const object of each type.</details>

3. Write a struct whose default is_sendable_impl result is false because of a mutable std::mutex member, then specialise is_sendable_v so it is accepted. What must you guarantee for this to be correct?

   <details><summary>Hint</summary>Every access to the mutable state, even through const member functions, must hold the mutex.</details>

4. Explain, step by step, why the program in the 'A problem' section printed garbage values even though every argument passed to spawn_thread was sendable.

   <details><summary>Hint</summary>When is the span's pointer captured, and what does vec.resize(100) do to the vector's buffer?</details>

5. Modify spawn_thread to use static_assert instead of a requires-clause and compare the error messages for a mutable capture. What do you lose by doing this?

   <details><summary>Hint</summary>A static_assert fires after overload resolution has chosen the function; a constraint removes it from overload resolution.</details>

6. The fork function forbids the main-thread lambda from mutating anything at all. Sketch a design that would allow it to mutate data that is not shared with the worker.

   <details><summary>Hint</summary>Think about which objects the worker can see; could the API take the shared data explicitly as const arguments?</details>

7. List three ways a program can still have a data race even when every thread is started with spawn_thread or fork.

   <details><summary>Hint</summary>Globals and statics, const_cast, and types whose is_sendable_v was specialised incorrectly.</details>

## Further reading

- Rust documentation on Send and Sync and data races (The Rustonomicon)
- David Rowland, "What Can C++ Learn About Thread Safety From Other Languages?", C++ on Sea 2025
- Tristan Brindle, Flux library (safer iteration for C++)
- Tristan Brindle, safer_concurrency: github.com/tcbrindle/safer_concurrency
- P3874, "Should C++ be a memory-safe language?", Jon Bauman et al.
- Swift's Sendable protocol (mentioned in the talk as using a single trait)
- C++26 std::indirect
- Compiler Explorer (godbolt.org), used for all of the talk's demos
