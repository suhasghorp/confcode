---
title: "Is C++ Actually Faster Than Java? Allocation, Cache Lines and Tail Latency"
date: 2026-11-17
tags: ["cpp", "java", "performance", "jit", "jvm", "benchmarking", "cpu-caches", "garbage-collection", "low-latency"]
source: "https://www.youtube.com/watch?v=KnqADllKdBw"
video_title: "Is C++ Actually Faster Than Java?"
channel: "Hot Path C++"
---

> This chapter is based on the talk [Is C++ Actually Faster Than Java?](https://www.youtube.com/watch?v=KnqADllKdBw) (Hot Path C++). Code listings marked *from the talk* are transcribed from the slides; listings marked *illustrative* were written for this chapter to explain the ideas.

"C++ is faster than Java" is one of the oldest claims in programming, and one of the least precise. Faster at what? Allocating small objects? Calling virtual functions? Scanning millions of records? Staying fast at the 99.99th percentile while a garbage collector runs in the background?

This chapter follows Episode 13 of the *Hot Path C++* YouTube series, "Is C++ Actually Faster Than Java?", which puts C++20 (GCC 13.3, `-O3 -march=native`) and OpenJDK 21 through three benchmarks on a 16-core AMD Ryzen 9 9950X. The speaker opens with a result that surprises most C++ programmers: textbook, pointer-heavy C++ loses to Java by 22.9x to 31.7x. Then, one benchmark at a time, the talk shows when the picture flips, and argues that each result comes down to hardware (allocators, branch prediction, cache lines, page tables) and not to the language.

Along the way we explain the machinery the talk relies on (JIT tiers, escape analysis, vtables, the cache hierarchy, garbage collectors), walk through every listing from the talk, and finish by re-running the whole experiment on a 2017 laptop with a very different CPU to see which conclusions survive.

## What you will learn

- Explain how HotSpot's tiered JIT (C1, C2) uses runtime profiles to inline virtual calls and remove allocations.
- Describe what escape analysis and scalar replacement do, and when they fail.
- Explain why a C++ virtual call through a pointer usually cannot be inlined, even with -flto.
- Compute the in-memory size of a C++ struct and a Java object, and relate it to 64-byte cache lines.
- Recognise the fresh-allocation benchmark trap and reproduce heap churn in a benchmark.
- Read Linux perf stat counters (LLC misses, dTLB misses, IPC) to explain a slowdown.
- Compare stop-the-world G1 pauses, generational ZGC load barriers and a pre-allocated C++ slab pool at p99.99.
- Explain speculative deoptimization and why it hurts a rare, latency-critical code path.

## Before you start

- Comfortable reading C++ (classes, virtual functions, std::unique_ptr, std::vector) and Java (classes, records, generics).
- A rough idea of what a compiler and a garbage collector do.
- Optional: some exposure to x86-64 assembly; every listing is explained line by line.

## Contents

1. [Background: How C++ and Java Turn Code into Machine Instructions](#1-background-how-c-and-java-turn-code-into-machine-instructions) — [0:00](https://www.youtube.com/watch?v=KnqADllKdBw&t=0s)
2. [Battle 1, Part 1: Allocation, TLABs and Escape Analysis](#2-battle-1-part-1-allocation-tlabs-and-escape-analysis) — [1:08](https://www.youtube.com/watch?v=KnqADllKdBw&t=68s)
3. [Battle 1, Part 2: Virtual Calls, Speculation and the Megamorphic Cliff](#3-battle-1-part-2-virtual-calls-speculation-and-the-megamorphic-cliff) — [4:00](https://www.youtube.com/watch?v=KnqADllKdBw&t=240s)
4. [Battle 2: Four Million Orders, Cache Lines and the Benchmark Trap](#4-battle-2-four-million-orders-cache-lines-and-the-benchmark-trap) — [6:33](https://www.youtube.com/watch?v=KnqADllKdBw&t=393s)
5. [Structure of Arrays, SIMD and the Valhalla Question](#5-structure-of-arrays-simd-and-the-valhalla-question) — [9:58](https://www.youtube.com/watch?v=KnqADllKdBw&t=598s)
6. [Battle 3: Tail Latency, Garbage Collectors and Off-Heap Java](#6-battle-3-tail-latency-garbage-collectors-and-off-heap-java) — [11:26](https://www.youtube.com/watch?v=KnqADllKdBw&t=686s)
7. [Deoptimization, the Verdict and When to Pick Each Language](#7-deoptimization-the-verdict-and-when-to-pick-each-language) — [14:53](https://www.youtube.com/watch?v=KnqADllKdBw&t=893s)
8. [Auditing Benchmarks, and Re-running Them on a Different Machine](#8-auditing-benchmarks-and-re-running-them-on-a-different-machine) — [17:58](https://www.youtube.com/watch?v=KnqADllKdBw&t=1078s)

## 1. Background: How C++ and Java Turn Code into Machine Instructions

*Talk segment: [0:00](https://www.youtube.com/watch?v=KnqADllKdBw&t=0s)*

### A benchmark that starts an argument

The episode opens with two versions of the same program on screen. Both implement a small "order pipeline" of the kind found in trading systems: create an order event, ask it to compute a result, and add that result to a running total called the *net exposure*. The left version is C++20 compiled with GCC 13.3 at `-O3 -march=native`. The right version is Java running on OpenJDK 21. The testbed is a 16-core AMD Ryzen 9 9950X (a Zen 5 CPU).

The numbers the speaker reports:

| Version | Time per event |
|---|---|
| Textbook C++20, separate `.cc` files | 16.8 ns |
| OpenJDK 21, events pre-built | 0.73 ns |
| OpenJDK 21, events allocated fresh inside the loop | 0.53 ns |

Java is between 22.9 and 31.7 times *faster*. The speaker is careful about what this means: Java "isn't just keeping up with naive C++ here". The word *naive* matters, and most of the chapter is about why it is there.

### The two programs

Here is the C++ version as it appears on the slide.

**Listing 1** — code from the talk (Object-Oriented Order Pipeline Benchmark: C++20 vs. OpenJDK 21), shown at [0:39](https://www.youtube.com/watch?v=KnqADllKdBw&t=39s)

```cpp
struct OrderResult { int64_t gross_notional; int64_t fee_ticks; };
class OrderEvent {
 public:
  virtual ~OrderEvent() = default;
  virtual std::unique_ptr<OrderResult> ProcessHeapAlloc() const = 0;
};
int64_t RunOrderPipeline(size_t n) {
  int64_t net_exposure = 0;
  for (size_t i = 0; i < n; ++i) {
    auto ev = CreateOrderEvent(i);
    auto res = ev->ProcessHeapAlloc();
    net_exposure += res->gross_notional - res->fee_ticks;
  }
  return net_exposure;
}
```

Line by line:

- `struct OrderResult` holds two 64-bit integers, the gross notional (price times quantity) and the fee, measured in ticks. It is 16 bytes.
- `class OrderEvent` is an abstract base class. Its destructor is `virtual` so that deleting a derived object through a base pointer is correct. `ProcessHeapAlloc` is a *pure virtual* function (`= 0`): every concrete event type must implement it.
- `ProcessHeapAlloc` returns a `std::unique_ptr<OrderResult>`, which means each call allocates a new `OrderResult` on the heap and hands ownership to the caller.
- In `RunOrderPipeline`, `CreateOrderEvent(i)` (not shown on this slide) is a factory that returns a `std::unique_ptr<OrderEvent>`. Again, a heap allocation.
- `ev->ProcessHeapAlloc()` is a *virtual call*: the program decides at run time which implementation to run.
- When `ev` and `res` go out of scope at the end of each iteration, their `unique_ptr` destructors call `delete`, freeing both objects.

So each iteration performs two heap allocations, one virtual call and two heap deallocations. This is how many people were taught to write object-oriented C++, and it is how Java code naturally looks. Here is the Java version.

**Listing 2** — code from the talk (Object-Oriented Order Pipeline Benchmark: C++20 vs. OpenJDK 21), shown at [0:39](https://www.youtube.com/watch?v=KnqADllKdBw&t=39s)

```java
record OrderResult(long grossNotional, long feeTicks) {}

abstract class OrderEvent {
  abstract OrderResult processHeapAlloc();
}

long runOrderPipeline(long n) {
  long netExposure = 0;
  for (long i = 0; i < n; ++i) {
    OrderEvent ev = createOrderEvent(i);
    OrderResult res = ev.processHeapAlloc();
    netExposure += res.grossNotional() - res.feeTicks();
  }
  return netExposure;
}
```

The structure is identical. `record OrderResult` is Java's compact syntax (since Java 16) for an immutable data class; it generates the constructor and the accessor methods `grossNotional()` and `feeTicks()`. `OrderEvent` is an abstract class with one abstract method. Every Java method that is not `static`, `private` or `final` is virtual, so `ev.processHeapAlloc()` is a virtual call too, and both `createOrderEvent(i)` and `processHeapAlloc()` use `new`, so on paper Java allocates two objects per iteration as well.

How can the same algorithm be 20 to 30 times faster in a language that is supposedly slower? To answer that, we need to know how each language turns this source code into machine instructions.

### How C++ becomes machine code: ahead of time, one file at a time

C++ is compiled **ahead of time (AOT)**. Before the program ever runs, the compiler translates it into machine code for a particular CPU. Two details of that process matter for this chapter.

1. **The compiler sees one translation unit at a time.** A *translation unit* is a single `.cc` file after `#include` directives are expanded. When the compiler builds `order_service.cc`, it sees only the *declaration* of a function defined in `order_factory.cc`. It cannot look inside, so it cannot know what type of object comes back, whether the object could be kept on the stack instead, or which virtual function will run. It must emit a real call and assume the worst.
2. **Everything is decided before the program sees any data.** If a virtual call might reach any of several classes, the compiler must emit code that works for all of them: load the object's vtable pointer, load the function address, call it.

**Link-time optimization** (`-flto`) relaxes the first rule. Object files then carry the compiler's intermediate representation, and the linker re-runs the optimizer across all of them. The second rule remains: even with LTO, the compiler knows nothing about the *data* the program will process, unless you feed it a profile from an earlier run (profile-guided optimization, PGO).

A quick reminder of how virtual calls work in C++. Every object of a class with virtual functions carries a hidden pointer, the **vptr**, usually in its first 8 bytes. The vptr points to the class's **vtable**, a table of function addresses. A virtual call loads the vptr, loads an address from a fixed offset in the vtable, and makes an *indirect* call to it. The CPU's branch predictor usually guesses the target correctly, so the call is not slow in itself. What really costs is that the compiler cannot *inline* the function, that is, copy its body into the caller. Inlining is what unlocks almost every other optimization.

### How Java becomes machine code: watch first, compile later

Java source code is compiled by `javac` to **bytecode**, a portable instruction set for the Java Virtual Machine. The JVM (here HotSpot, the JVM in OpenJDK) then executes that bytecode in stages called **tiers**:

| Tier | Who runs the code | What it is for |
|---|---|---|
| 0 | The interpreter | Starts immediately, slowly |
| 1-3 | The **C1** compiler | Fast compilation; tier 3 adds *profiling* code |
| 4 | The **C2** compiler | Slow, aggressive compilation using the profile |

A method starts in the interpreter. Once it has been called often enough, C1 compiles it at tier 3, with extra instructions that record what happens at run time: which branches are taken, and, crucially for us, which concrete classes show up at each virtual call site (the **receiver type profile**). After many more calls, C2 recompiles the method at tier 4, using that profile to make bets: "this call site has only ever seen `LimitOrderEvent`, so I will inline `LimitOrderEvent.processHeapAlloc` directly."

This is **just-in-time (JIT)** compilation, and it has two advantages over AOT that the talk returns to again and again:

- C2 compiles the *whole running program*. There are no translation-unit walls; it can inline across classes, packages and libraries.
- C2 knows the *data*. It can specialise code for the traffic it actually observes, and protect each bet with a cheap check that falls back to the interpreter if the bet ever turns out wrong.

You can watch the tiers yourself. The following program is our own illustration, not code from the talk.

**Listing 3** — Watching a method move through HotSpot's tiers *(illustrative example, not from the talk)*

```java
// Tiers.java - run with: java -XX:+PrintCompilation Tiers.java
public class Tiers {
    interface Event { long process(); }

    record Limit(long px, long qty) implements Event {
        public long process() { return px * qty; }
    }

    static long step(Event e) {
        return e.process();          // a virtual (interface) call
    }

    public static void main(String[] args) {
        Event e = new Limit(10125, 100);
        long sum = 0;
        for (int i = 0; i < 5_000_000; i++) {
            sum += step(e);          // hot: compiled at tier 3, then tier 4
        }
        System.out.println(sum);
    }
}
```

Run with `java -XX:+PrintCompilation Tiers.java`, the output contains lines such as `Tiers::step (7 bytes)` with a tier number column: `step` first appears at tier 3 and, a few milliseconds later, at tier 4, after which the tier-3 version is marked "made not entrant" (retired). If you add `-XX:+UnlockDiagnosticVMOptions -XX:+PrintInlining`, C2 also reports that it inlined `Limit::process` into `step` (typically with the note "inline (hot)").

### Two lazy myths

With that background, the speaker's framing makes sense. "For 30 years, systems engineers have repeated two lazy myths":

1. *Java is a slow, interpreted toy.* It is not. Hot Java code runs as native machine code produced by C2, and that compiler has information a static compiler never gets.
2. *C++ is automatically faster because it is compiled ahead of time.* It is not automatically anything. AOT compilation gives you control, but if you write "Java-style pointer-heavy OOP in C++", the speaker says, "the JVM's tier 4 C2 just-in-time compiler will routinely embarrass your binary."

> The speaker's thesis: "Both camps are wrong." Performance comes from what the hardware has to do, and which language is "faster" depends on whether the code (and the compiler) lets the hardware do less.

The rest of the episode tests that thesis with three benchmarks, which the talk calls "battles". Each one isolates a different piece of hardware: the memory allocator and branch predictor (Battle 1), the cache hierarchy (Battle 2), and the latency tail under garbage collection (Battle 3).

## 2. Battle 1, Part 1: Allocation, TLABs and Escape Analysis

*Talk segment: [1:08](https://www.youtube.com/watch?v=KnqADllKdBw&t=68s)*

### Three battles, three instruments

The speaker frames the episode as a question: to decide whether C++ is faster than Java, "we have to ask what fast means in production." The answer is split into three benchmarks, each measured with tools built for the job:

- **Google Benchmark** for the C++ side, a library that runs a function repeatedly and reports time per iteration.
- **JMH** (the Java Microbenchmark Harness) for the Java side. JMH matters because naive Java timing loops are easily fooled by the JIT; JMH handles warm-up, forking fresh JVMs and preventing dead-code elimination.
- **Linux `perf stat`**, which reads the CPU's hardware performance counters (cache misses, TLB misses, instructions per cycle).

The three battles are, in the speaker's words: "dynamic allocation and virtual dispatch where the JIT genuinely wins", "cache line physics across 4 million orders where pointer indirection collapses", and "P99.99 tail latency and speculative deoptimization under live market load". This section and the next cover the first.

### The Battle 1 workload

Imagine "a textbook enterprise service". A factory creates a polymorphic order event, the program calls the event's virtual `ProcessHeapAlloc` method, which returns a heap-allocated `OrderResult` with the gross notional and the fee, and the loop accumulates the net exposure. Here is the C++ service.

**Listing 4** — code from the talk (Battle 1: Polymorphic Event Factory & Result Accumulation), shown at [2:23](https://www.youtube.com/watch?v=KnqADllKdBw&t=143s)

```cpp
struct OrderResult { int64_t gross_notional; int64_t fee_ticks; };
class OrderEvent {
 public:
  virtual ~OrderEvent() = default;
  virtual std::unique_ptr<OrderResult> ProcessHeapAlloc() const = 0;
};
std::unique_ptr<OrderEvent> CreateOrderEvent(size_t index);

int64_t RunOrderService(size_t n) {
  int64_t net_exposure = 0;
  for (size_t i = 0; i < n; ++i) {
    auto ev = CreateOrderEvent(i);
    auto res = ev->ProcessHeapAlloc();
    net_exposure += res->gross_notional - res->fee_ticks;
  }
  return net_exposure;
}
```

Compared with the opening slide, one line is new and it is the important one:

- `std::unique_ptr<OrderEvent> CreateOrderEvent(size_t index);` is only a *declaration*. The definition lives in another file, `order_factory.cc`. This is what the speaker means by "separate .cc translation units".
- In the loop, `CreateOrderEvent(i)` is therefore an opaque call. The compiler does not know which subclass comes back, how big it is, or what its `ProcessHeapAlloc` does.
- `ev->ProcessHeapAlloc()` must be compiled as a virtual call through the vtable.
- `res->gross_notional - res->fee_ticks` reads two fields from the heap object.
- At the closing brace, both `unique_ptr` destructors run: two calls to `operator delete`.

The Java service looks the same, but now we see the factory.

**Listing 5** — code from the talk (Battle 1: Polymorphic Event Factory & Result Accumulation), shown at [2:23](https://www.youtube.com/watch?v=KnqADllKdBw&t=143s)

```java
record OrderResult(long grossNotional, long feeTicks) {}
abstract class OrderEvent {
  abstract OrderResult processHeapAlloc();
}
OrderEvent createOrderEvent(long index) {
  return new LimitOrderEvent(index, 10125L, 100L);
}

long runOrderService(long n) {
  long netExposure = 0;
  for (long i = 0; i < n; ++i) {
    OrderEvent ev = createOrderEvent(i);
    OrderResult res = ev.processHeapAlloc();
    netExposure += res.grossNotional() - res.feeTicks();
  }
  return netExposure;
}
```

- `createOrderEvent` returns `new LimitOrderEvent(index, 10125L, 100L)`: a price of 10125 ticks and a quantity of 100.
- The loop body is identical to the C++ one.

The measured results: C++ with `std::make_unique` across translation units takes **16.8 ns** per operation. Java takes **0.73 ns**, and **0.53 ns** "with fresh in-loop event allocation". The rest of this section explains both numbers.

### Why C++ pays 16.8 ns

The next slide shows the concrete event class and annotates the loop with what the hardware does.

**Listing 6** — code from the talk (Why C++ Lost by 22.9x: Separate .cc Units & glibc tcache), shown at [3:07](https://www.youtube.com/watch?v=KnqADllKdBw&t=187s)

```cpp
class LimitOrderEvent final : public OrderEvent {
 private:
  int64_t id_, price_ticks_, qty_;
};

int64_t RunOrderService(size_t n) {
  int64_t net_exposure = 0;
  for (size_t i = 0; i < n; ++i) {
    auto ev = CreateOrderEvent(i);
    auto res = ev->ProcessHeapAlloc();
    net_exposure += res->gross_notional - res->fee_ticks;
  }
  return net_exposure;
}
```

- `class LimitOrderEvent final : public OrderEvent` holds three 64-bit fields: `id_`, `price_ticks_` and `qty_`. With the 8-byte vptr in front, an object is 8 + 24 = **32 bytes**.
- Line `auto ev = CreateOrderEvent(i);` calls `operator new(32)`, step ① on the slide.
- `ev->ProcessHeapAlloc()` loads the vptr and calls through the vtable (②), and inside, `make_unique<OrderResult>` calls `operator new(16)` (③).
- The closing brace of the loop runs two `operator delete` calls (④).

The slide shows only the class's data members. The real class must also override `ProcessHeapAlloc`; compiled exactly as printed, Clang warns that a class marked `final` is still abstract.

As the speaker puts it, "the C++ compiler cannot see across the separate translation unit boundary to prove that those standard unique pointer objects never escape the loop." Nothing here is exotic. It is simply what the code asks for: two allocations, one indirect call and two frees on every iteration.

Those allocations are not system calls. glibc's `malloc` keeps a **tcache**, a small per-thread cache of recently freed chunks sorted by size, so `new` usually pops a chunk off a singly linked list and `delete` pushes it back. That is fast, but it is still work: computing the size class, reading and writing chunk headers and list pointers, and doing it four times per iteration. The speaker measures 16.8 ns without LTO and 15.2 ns "even with whole program `-flto` enabled". LTO helps a little because it lets the compiler see into the factory, but the allocations and the vtable call through the base-class pointer survive.

It is worth seeing that the allocations are *not* inherent to C++. They exist because of what the compiler cannot see. In the following example (ours, not the talk's), the factory is in the same file and returns the concrete `final` type.

**Listing 7** — The same pipeline, with the factory visible: GCC removes new and delete *(illustrative example, not from the talk)*

```cpp
#include <cstdint>
#include <cstdio>
#include <memory>

struct OrderResult { int64_t gross_notional; int64_t fee_ticks; };

class OrderEvent {
 public:
  virtual ~OrderEvent() = default;
  virtual std::unique_ptr<OrderResult> ProcessHeapAlloc() const = 0;
};

class LimitOrderEvent final : public OrderEvent {
 public:
  LimitOrderEvent(int64_t id, int64_t px, int64_t qty)
      : id_(id), price_ticks_(px), qty_(qty) {}
  std::unique_ptr<OrderResult> ProcessHeapAlloc() const override {
    return std::make_unique<OrderResult>(OrderResult{price_ticks_ * qty_, id_ & 7});
  }
 private:
  int64_t id_, price_ticks_, qty_;
};

// The factory is visible in this translation unit and returns the
// concrete, final type, so the compiler knows exactly what runs.
inline std::unique_ptr<LimitOrderEvent> CreateOrderEvent(size_t i) {
  return std::make_unique<LimitOrderEvent>(static_cast<int64_t>(i), 10125, 100);
}

int64_t RunOrderService(size_t n) {
  int64_t net_exposure = 0;
  for (size_t i = 0; i < n; ++i) {
    auto ev = CreateOrderEvent(i);
    auto res = ev->ProcessHeapAlloc();   // devirtualized: the class is final
    net_exposure += res->gross_notional - res->fee_ticks;
  }
  return net_exposure;
}

int main() { std::printf("%lld\n", static_cast<long long>(RunOrderService(1000))); }
```

Compiled with `g++-13 -O3 -S`, the body of `RunOrderService` in this file contains no call to `operator new` (`_Znwm`) or `operator delete` (`_ZdlPv`), and no indirect call. Since C++14 the standard explicitly allows a compiler to omit a `new`/`delete` pair when the program cannot observe the difference, and both GCC and Clang do it here. With the allocations gone, GCC even vectorizes the loop with SSE2 instructions. The same source code that costs 16.8 ns behind a `.cc` wall costs a fraction of a nanosecond when the optimizer can see everything. Keep this in mind, because it is exactly what the JVM's JIT does by default.

### Java's first weapon: the thread-local allocation buffer

Java's `new` is a different operation from C++'s `operator new`. The HotSpot heap is **generational**: new objects are created in a region called **Eden**, and the garbage collector periodically copies the few survivors out and treats the whole of Eden as free again. Because dead objects are never individually freed, the allocator never has to maintain free lists.

On top of that, each Java thread owns a private slice of Eden, its **thread-local allocation buffer (TLAB)**. Allocating in a TLAB needs no lock and no search: the thread keeps a pointer to the next free byte (`tlab_top`) and the end of its slice (`tlab_end`), and `new` just moves the pointer. The slide shows the fast path in x86-64 assembly.

**Listing 8** — code from the talk (Java's First Weapon: Thread-Local Allocation Buffer (TLAB)), shown at [3:37](https://www.youtube.com/watch?v=KnqADllKdBw&t=217s)

```asm
; R15 holds JavaThread* on x86-64 OpenJDK 21
mov   rax, QWORD PTR [r15 + 0x118]
lea   rbx, [rax + 24]
cmp   rbx, QWORD PTR [r15 + 0x128]
ja    .L_tlab_slow_refill
mov   QWORD PTR [r15 + 0x118], rbx
mov   QWORD PTR [rax], 0x1
mov   DWORD PTR [rax + 8], 0x248a1c00
; Scope exit: 0 instructions! Short-lived objects die in Eden.
```

- The comment states the convention: on x86-64, HotSpot keeps a pointer to the current `JavaThread` in register `r15`. The TLAB's top and end live inside that structure, here at offsets `0x118` and `0x128`.
- `mov rax, QWORD PTR [r15 + 0x118]` loads the current top: this will be the address of the new object.
- `lea rbx, [rax + 24]` computes the new top by adding the object's size. `lea` ("load effective address") is just arithmetic here; nothing is read from memory.
- `cmp rbx, QWORD PTR [r15 + 0x128]` and `ja .L_tlab_slow_refill` check that the object fits; if not, the thread jumps to a slow path that fetches a new TLAB.
- `mov QWORD PTR [r15 + 0x118], rbx` stores the new top. The allocation is done: four instructions.
- The two dimmed lines initialise the object header: the **mark word** (8 bytes, holding lock state and GC age; `0x1` means "unlocked, no hash") and the 4-byte **compressed class pointer** identifying the class.
- The final comment is the punchline: when the object dies, nothing happens. "Short-lived objects simply die in Eden at zero per object deallocation cost."

One detail on the slide is simplified: the bump is drawn as 24 bytes for a new `OrderResult`. On a default 64-bit HotSpot, an object with two `long` fields occupies 32 bytes, a 12-byte header, 4 bytes of padding so the first `long` is 8-byte aligned, and 16 bytes of fields (we checked the field offsets with `sun.misc.Unsafe` on OpenJDK 21: `grossNotional` at offset 16, `feeTicks` at 24). The instruction sequence is the same either way, only the constant changes.

### Java's second weapon: allocation that never happens

The TLAB makes heap allocation cheap. In this loop, the speaker says, "Java doesn't even touch the TLAB."

**Listing 9** — code from the talk (Scalar Replacement: Bypassing TLAB into RAX & RDX), shown at [4:07](https://www.youtube.com/watch?v=KnqADllKdBw&t=247s)

```java
record OrderResult(long notional, long feeTicks) {}

final class LimitOrderEvent extends OrderEvent {
  @Override
  OrderResult processHeapAlloc() {
    return new OrderResult(price * qty, fee);
  }
}

long runBattle1Step(OrderEvent ev) {
  OrderResult res = ev.processHeapAlloc();
  return res.notional() - res.feeTicks();
}
```

This slide is a condensed excerpt; `price`, `qty` and `fee` stand for the event's fields, which the slide omits. The annotations describe what C2 does to it, in three steps:

1. **① C2 inlines.** The call site `ev.processHeapAlloc()` has only ever seen `LimitOrderEvent` (more on how C2 knows this in the next section), so C2 copies the body of `LimitOrderEvent.processHeapAlloc` into `runBattle1Step`. Now the `new OrderResult(...)` is *inside* the method that uses it.
2. **② Escape analysis finds no escape.** **Escape analysis** asks: can this object be seen by anyone outside this compiled method, because it is stored in a field, passed to a non-inlined call, returned or shared with another thread? Here, `res` is created, two fields are read, and it is dropped. It does not escape (`NoEscape`).
3. **③ Scalar replacement.** An object that does not escape does not need to exist as an object. C2 replaces it with its fields, plain `long` values that live in CPU registers. The slide labels them `RAX - RDX`: the notional in one register, the fee in another. The `new` instruction is deleted entirely.

Zero bytes are allocated on the heap, and what is left is a multiply and a subtract. That is the 0.73 ns. It also explains the oddity that the *fresh* version, which allocates a new event object every iteration, is faster still at 0.53 ns. When the event is created inside the loop, C2 can scalar-replace the event *and* the result, and there is no array of events to read from memory at all.

You can verify scalar replacement yourself without JMH, using the JVM's per-thread allocation counter. The following program is our own.

**Listing 10** — Counting the bytes that escape analysis saves *(illustrative example, not from the talk)*

```java
import java.lang.management.ManagementFactory;

public class EscapeDemo {
    record OrderResult(long grossNotional, long feeTicks) {}

    static final class LimitOrderEvent {
        final long id, price, qty;
        LimitOrderEvent(long id, long price, long qty) { this.id = id; this.price = price; this.qty = qty; }
        OrderResult process() { return new OrderResult(price * qty, id & 7); }
    }

    static long run(long n) {
        long net = 0;
        for (long i = 0; i < n; ++i) {
            LimitOrderEvent ev = new LimitOrderEvent(i, 10125, 100);  // never escapes
            OrderResult res = ev.process();                           // never escapes
            net += res.grossNotional() - res.feeTicks();
        }
        return net;
    }

    public static void main(String[] args) {
        var mx = (com.sun.management.ThreadMXBean) ManagementFactory.getThreadMXBean();
        long tid = Thread.currentThread().threadId();
        for (int warm = 0; warm < 20; ++warm) run(1_000_000);      // let C2 compile run()
        long before = mx.getThreadAllocatedBytes(tid);
        long sum = run(10_000_000);
        long after = mx.getThreadAllocatedBytes(tid);
        System.out.printf("%,d bytes allocated for 10,000,000 iterations (%.2f bytes/iteration), sum=%d%n",
                after - before, (after - before) / 1e7, sum);
    }
}
```

On OpenJDK 21 it prints `0 bytes allocated for 10,000,000 iterations`. Run it again with `-XX:-DoEscapeAnalysis` and it prints 720,000,000 bytes, 72 per iteration: a 40-byte `LimitOrderEvent` (12-byte header, 4 bytes of padding, three `long`s) plus a 32-byte `OrderResult`. Same code, same JIT, and the only difference is whether C2 is allowed to prove the objects do not escape.

> Escape analysis is the JVM's version of the optimization in the C++ example above. The difference is that C2 gets the visibility for free, because it inlines across classes at run time, while the C++ compiler only gets it if you arrange your code (or your build) so that it can.

## 3. Battle 1, Part 2: Virtual Calls, Speculation and the Megamorphic Cliff

*Talk segment: [4:00](https://www.youtube.com/watch?v=KnqADllKdBw&t=240s)*

### Making it fair

A C++ programmer watching the first half of Battle 1 will object, and the speaker anticipates it: "Now, let's make it fair." Nobody who cares about nanoseconds allocates two heap objects per event. So the next experiment removes per-iteration allocation from C++. The events are created once, up front, in a ring buffer, and a new virtual method returns the result *by value*.

**Listing 11** — code from the talk (Making It Fair: Pre-Allocated C++ Ring & The vtable Tax), shown at [4:55](https://www.youtube.com/watch?v=KnqADllKdBw&t=295s)

```cpp
class Event {
 public:
  virtual ~Event() = default;
  virtual OrderResult ProcessNoAlloc() const = 0;
};

int64_t RunPreallocatedRing(
    const std::vector<std::unique_ptr<Event>>& ring,
    size_t n) {
  int64_t net_exposure = 0;
  for (size_t i = 0; i < n; ++i) {
    OrderResult res = ring[i & kMask]->ProcessNoAlloc();
    net_exposure += res.gross_notional - res.fee_ticks;
  }
  return net_exposure;
}
```

Walking through it:

- `class Event` is the same idea as before, but `ProcessNoAlloc` returns an `OrderResult` directly. `OrderResult` is two `int64_t`s, and the x86-64 calling convention returns such a 16-byte struct in two registers, `RAX` and `RDX`. No heap, no `unique_ptr`.
- `RunPreallocatedRing` takes a `std::vector<std::unique_ptr<Event>>`: a vector of pointers to events that were allocated before the timed loop started.
- `ring[i & kMask]` cycles through the ring. `kMask` is the ring size minus one (a power of two), so `i & kMask` is a cheap `i % size`.
- The only remaining cost per event is the virtual call and the arithmetic.

The result is a large improvement: **1.52 ns** per operation without LTO, and **1.32 ns** when "we recompile the entire C++ binary with link time optimization enabled". But Java's 0.73 ns is still faster. Why does C++ not go further?

### The vtable tax

The ring "holds pointers to multiple derived classes, limit order event and cancel order event." The slide draws `ring[0]` and `ring[1]` pointing at `LimitOrderEvent` objects and `ring[2]` at a `CancelOrderEvent`. Even with the whole program visible, the compiler cannot know which class sits in `ring[i]` for a given `i`; that depends on data that only exists at run time. So, "without runtime profile guided optimization, the static C++ compiler must emit an indirect vtable call."

**Listing 12** — code from the talk (Making It Fair: Pre-Allocated C++ Ring & The vtable Tax), shown at [4:55](https://www.youtube.com/watch?v=KnqADllKdBw&t=295s)

```asm
; RDI = Event* from ring[i]
mov   rax, QWORD PTR [rdi]
call  QWORD PTR [rax + 24]
sub   rax, rdx
```

- The comment says `RDI` holds the `Event*` from `ring[i]` (in the System V calling convention, the first argument, here the implicit `this` pointer, is passed in `RDI`).
- `mov rax, QWORD PTR [rdi]` loads the vptr from the first 8 bytes of the object. This is hop 1: a memory load.
- `call QWORD PTR [rax + 24]` loads a function address from the vtable and calls it. This is hop 2, an indirect call.
- `sub rax, rdx` computes `gross_notional - fee_ticks` from the two returned registers.

A note on the offset: for the class exactly as printed on the slide, GCC uses slot `+16`, because under the Itanium C++ ABI the virtual destructor occupies the first two vtable slots. We compiled the slide's code with GCC 13 and got `call [QWORD PTR 16[rax]]`. The `+24` on the slide suggests the real benchmark class has one more virtual function before `ProcessNoAlloc`. It does not change the argument.

Two hops are not expensive on their own. Modern CPUs predict indirect branch targets well, especially when most targets are the same. The real cost is what the call *prevents*: the compiler cannot inline `ProcessNoAlloc` into the loop, so it cannot fold the arithmetic into the caller, keep values in registers across iterations, or vectorize. The slide calls this "blocked by static polymorphism".

### Java's answer: speculate, then guard

The JVM has the information the static compiler lacks. "Java's C2 JIT compiler is watching your live production traffic at runtime." At tier 3, the profiling code records the **receiver type profile** at the call site: which classes `ev` has actually been. If it has been `LimitOrderEvent` 99.9 to 100% of the time, C2 **speculatively devirtualizes**: it assumes the class, inlines that class's method, and protects the assumption with a cheap check.

**Listing 13** — code from the talk (Speculative Devirtualization: Watching Live Traffic at Runtime), shown at [5:39](https://www.youtube.com/watch?v=KnqADllKdBw&t=339s)

```asm
; RSI = OrderEvent reference inside hot loop
cmp   DWORD PTR [rsi + 8], 0x248a1c00
jne   .L_uncommon_trap
; --- Speculatively Inlined LimitOrderEvent Arithmetic ---
mov   rax, QWORD PTR [rsi + 16]
imul  rax, QWORD PTR [rsi + 24]
sub   rax, QWORD PTR [rsi + 32]
```

Line by line:

- `RSI` holds the reference to the current event object.
- `cmp DWORD PTR [rsi + 8], 0x248a1c00` compares the object's class word with the expected class. Recall from the TLAB listing that every HotSpot object starts with an 8-byte mark word followed by a 4-byte **compressed class pointer** at offset 8. Comparing 32 bits against a constant is a single instruction.
- `jne .L_uncommon_trap`: if the class is anything else, jump to an **uncommon trap**, a stub that abandons the compiled code and lets the interpreter handle this call. The label says it: cold path. We will see what that costs in Battle 3.
- The comment marks the inlined body of `LimitOrderEvent`'s method. The next three instructions read the fields directly from the object (`+16`, `+24`, `+32`, after the 12-byte header and 4 bytes of alignment padding), multiply price by quantity with `imul`, and subtract the fee.

No vtable, no call. Combined with escape analysis from the previous section, nothing is allocated either. The speaker measures **0.73 ns** per operation (0.89 ns under JMH), "nearly two times" faster than the pre-allocated virtual C++ ring.

> The general principle: a static compiler must generate code that is correct for every possible input. A JIT can generate code that is fast for the inputs it has *seen*, plus a guard that falls back to slower code if it ever sees something else.

C++ can sometimes do the same thing statically. If the compiler can prove only one target is possible, it devirtualizes on its own. The simplest way to give it that proof is `final`, as in this example of our own:

**Listing 14** — `final` lets the compiler devirtualize and inline *(illustrative example, not from the talk)*

```cpp
#include <cstdint>
#include <cstdio>
#include <memory>
#include <vector>

struct OrderResult { int64_t gross_notional; int64_t fee_ticks; };

class Event {
 public:
  virtual ~Event() = default;
  virtual OrderResult ProcessNoAlloc() const = 0;
};

class LimitOrderEvent final : public Event {
 public:
  LimitOrderEvent(int64_t px, int64_t qty, int64_t fee) : px_(px), qty_(qty), fee_(fee) {}
  OrderResult ProcessNoAlloc() const override { return {px_ * qty_, fee_}; }
 private:
  int64_t px_, qty_, fee_;
};

// The element type is the final class, so ProcessNoAlloc has exactly one
// possible target: GCC calls (and inlines) it directly, no vtable load.
int64_t RunLimitRing(const std::vector<std::unique_ptr<LimitOrderEvent>>& ring) {
  int64_t net_exposure = 0;
  for (const auto& ev : ring) {
    OrderResult res = ev->ProcessNoAlloc();
    net_exposure += res.gross_notional - res.fee_ticks;
  }
  return net_exposure;
}

int main() {
  std::vector<std::unique_ptr<LimitOrderEvent>> ring;
  for (int i = 0; i < 1024; ++i) ring.push_back(std::make_unique<LimitOrderEvent>(10125, 100 + i % 7, i & 7));
  std::printf("%lld\n", static_cast<long long>(RunLimitRing(ring)));
}
```

Because the vector holds `std::unique_ptr<LimitOrderEvent>` and `LimitOrderEvent` is `final`, there is only one possible `ProcessNoAlloc`. GCC 13 at `-O3` emits no call instruction in `RunLimitRing` at all: the method is inlined. GCC also implements *speculative* devirtualization (with `-fdevirtualize-speculatively`, enabled at `-O2`) when it can see that a class hierarchy has a likely target, and PGO (`-fprofile-generate` / `-fprofile-use`) gives it the run-time profile the speaker mentions. But these only work when the build gives the compiler the information, while C2 collects it from production traffic by default.

### When the bet stops paying: the megamorphic cliff

Speculation has a weakness: it depends on the traffic being predictable. The speaker shows what happens when it is not.

| Event types at the call site | Java (C2) | What C2 does |
|---|---|---|
| 1 (100% one type) | 0.73 ns (0.89 ns JMH) | one class guard, full inlining, escape analysis |
| 2 (50% / 50%) | 2.66 ns (2.55 ns JMH) | two guards, both bodies inlined (a *bimorphic* inline cache) |
| 5 (20% each) | 3.32 ns (3.52 ns JMH) | gives up: a real virtual call |
| C++ virtual call, for comparison | 1.32 ns | vtable call, every time |

An **inline cache** is the JIT's record of the types it expects at a call site. With one type it is *monomorphic*, with two *bimorphic*. HotSpot's C2 will inline at most `TypeProfileWidth` receiver types (the default is 2). Beyond that the call site is **megamorphic**, and C2 falls back to an ordinary virtual (or interface) call through the vtable or itable. With two 50/50 types, the guard's branch is also hard to predict, which is part of the 2.66 ns.

At five types, Java is slower than C++'s plain vtable call, and the speaker identifies two reasons. The first is that it falls back to a full virtual dispatch. The second, more important, is that "loss of inlining also defeats escape analysis": once `processHeapAlloc` is not inlined, the `new OrderResult` happens inside a method C2 cannot see into from the loop, so the object escapes and must be allocated in the TLAB again. The slide calls this the *megamorphic cliff*: performance does not degrade smoothly as traffic diversifies, it falls off an edge.

### The C++ answer: stop using pointers for small values

The speaker's fix on the C++ side is not a better vtable. It is to stop modelling events as heap objects at all.

**Listing 15** — code from the talk (The Megamorphic Cliff (TypeProfileWidth=2) vs. Flat C++20 Struct), shown at [6:31](https://www.youtube.com/watch?v=KnqADllKdBw&t=391s)

```cpp
enum class EventKind : uint8_t {
  kLimit, kCancel, kModify, kFill, kPeg
};

struct FlatOrderEvent {
  int64_t price_ticks;
  int64_t qty;
  int32_t fee_bps;
  EventKind kind;
};

inline OrderResult ProcessFlat(const FlatOrderEvent& e) {
  return {e.price_ticks * e.qty, e.fee_bps};
}
```

- `enum class EventKind : uint8_t` lists five event kinds (limit, cancel, modify, fill, peg) in a single byte. The `: uint8_t` sets the underlying type, so the enum is one byte instead of the default `int`.
- `struct FlatOrderEvent` holds the event's data directly: two 64-bit fields, a 32-bit fee in basis points, and the kind. That is 8 + 8 + 4 + 1 = 21 bytes, padded to **24 bytes** so the next element in an array is 8-byte aligned. The slide's label "24 Bytes Total" matches `sizeof`.
- `ProcessFlat` is an `inline` free function taking the event by `const&`. There is no vptr and no indirection: the compiler sees the body at every call site and inlines it ("100% Inlined").

The slide's `ProcessFlat` does not look at `kind` at all. It shows the data layout rather than a full dispatcher. In real code you would branch on `kind`, and a `switch` over a one-byte enum in contiguous memory is something the compiler can inline, reorder and often turn into branch-free code.

The measured result is **0.55 to 0.56 ns**, "regardless of traffic mix": one, two or five event types make no difference, because there is no type-based dispatch for the traffic to confuse. The slide's verdict: the flat C++20 struct is 6.0x faster than megamorphic Java.

When the event kinds carry genuinely different data, modern C++ has a standard tool for "one of several value types": `std::variant`. Here is our own example of the five-events idea in that style.

**Listing 16** — Value-type events with std::variant: no vptr, no heap *(illustrative example, not from the talk)*

```cpp
#include <cstdint>
#include <cstdio>
#include <type_traits>
#include <variant>
#include <vector>

struct OrderResult { int64_t gross_notional; int64_t fee_ticks; };

// Each event type is a plain value: no base class, no vptr, no heap.
struct Limit  { int64_t price_ticks, qty, fee; };
struct Cancel { int64_t order_id; };
struct Fill   { int64_t price_ticks, filled; };

using OrderEvent = std::variant<Limit, Cancel, Fill>;

OrderResult Process(const OrderEvent& ev) {
  return std::visit([](const auto& e) -> OrderResult {
    using T = std::decay_t<decltype(e)>;
    if constexpr (std::is_same_v<T, Limit>)  return {e.price_ticks * e.qty, e.fee};
    if constexpr (std::is_same_v<T, Cancel>) return {0, e.order_id & 3};
    if constexpr (std::is_same_v<T, Fill>)   return {e.price_ticks * e.filled, 2};
  }, ev);
}

int main() {
  std::vector<OrderEvent> events;  // contiguous, sizeof(OrderEvent) bytes each
  for (int64_t i = 0; i < 1'000'000; ++i) {
    switch (i % 3) {
      case 0: events.emplace_back(Limit{10125, 100, i & 7}); break;
      case 1: events.emplace_back(Cancel{i}); break;
      default: events.emplace_back(Fill{10125, i % 50}); break;
    }
  }
  int64_t net_exposure = 0;
  for (const auto& ev : events) {
    OrderResult r = Process(ev);
    net_exposure += r.gross_notional - r.fee_ticks;
  }
  std::printf("sizeof(OrderEvent) = %zu, net exposure = %lld\n",
              sizeof(OrderEvent), static_cast<long long>(net_exposure));
}
```

Each alternative is a plain struct. A `std::variant<Limit, Cancel, Fill>` stores the largest alternative plus a small index, here 32 bytes, directly inside the vector, with no heap allocation per event. `std::visit` dispatches on the index, which compilers typically lower to a jump table or a chain of compares that they can inline, and `if constexpr` selects the code for each type at compile time. It prints `sizeof(OrderEvent) = 32` with GCC 13, GCC 16 and Clang 20.

> Battle 1's lesson, in the speaker's framing: Java beats *pointer-heavy* C++ because the JIT can undo pointer-heavy design at run time. C++ beats Java when the design does not need undoing in the first place.

## 4. Battle 2: Four Million Orders, Cache Lines and the Benchmark Trap

*Talk segment: [6:33](https://www.youtube.com/watch?v=KnqADllKdBw&t=393s)*

### From the L1 cache to main memory

Everything in Battle 1 happened in a tiny working set. A ring of a thousand events fits easily in the CPU's **L1 data cache** (48 KB per core on Zen 5), the fastest memory the core has, a few cycles away. Battle 2 changes the scale: a risk engine scans **4,000,000 active orders** to compute the total notional exposure, the sum of price times quantity. That is well over 100 MB of data, far larger than any cache, so most of it must come from DRAM.

Some background on how a CPU reads memory, because the whole battle turns on it:

- Memory moves between DRAM and the caches in **cache lines** of 64 bytes. Ask for one byte and you get the 64-byte line it lives in.
- The caches are layered: L1 (tens of KB, ~1 ns), L2 (around a MB), L3, also called the **last-level cache** or LLC (tens of MB), then DRAM (around 70-100 ns away).
- **Hardware prefetchers** watch your access pattern. If you read addresses at a constant stride, they fetch the next lines before you ask, so a sequential scan runs at memory bandwidth instead of memory latency.
- Programs use *virtual* addresses. The CPU translates them to physical addresses page by page (4 KB pages by default), caching recent translations in the **TLB** (translation lookaside buffer; `dTLB` for data). A TLB miss forces a **page-table walk**: up to four dependent memory reads on x86-64 before the real load can even start.

### The two scans

The Java risk engine uses the idiomatic collection.

**Listing 17** — code from the talk (Battle 2: Scanning 4,000,000 Active Orders in a Risk Engine), shown at [7:07](https://www.youtube.com/watch?v=KnqADllKdBw&t=427s)

```java
record OrderRecord(
    long orderId,
    long price,
    int quantity,
    int symbolId,
    int flags) {}

long computeTotalExposure(
    ArrayList<OrderRecord> orders) {
  long totalNotional = 0;
  for (int i = 0; i < orders.size(); ++i) {
    OrderRecord ord = orders.get(i);
    totalNotional += ord.price() * ord.quantity();
  }
  return totalNotional;
}
```

- `record OrderRecord(...)` has five fields: `orderId` and `price` are `long` (8 bytes each), `quantity`, `symbolId` and `flags` are `int` (4 bytes each). That is 28 bytes of actual data.
- `computeTotalExposure` takes an `ArrayList<OrderRecord>` with 4,000,000 elements.
- `orders.get(i)` returns a *reference* to an `OrderRecord` object somewhere on the heap; the slide marks it "Pointer Chase!".
- `ord.price() * ord.quantity()` then reads two fields from that object.

The C++ engine uses a vector of structs.

**Listing 18** — code from the talk (Battle 2: Scanning 4,000,000 Active Orders in a Risk Engine), shown at [7:07](https://www.youtube.com/watch?v=KnqADllKdBw&t=427s)

```cpp
struct Order {
  int64_t order_id;
  int64_t price;
  int32_t quantity;
  int32_t symbol_id;
  int32_t flags;
};

int64_t ComputeTotalExposure(
    const std::vector<Order>& orders) {
  int64_t total_notional = 0;
  for (size_t i = 0; i < orders.size(); ++i) {
    total_notional += orders[i].price * orders[i].quantity;
  }
  return total_notional;
}
```

- `struct Order` has the same five fields with fixed-width types.
- `ComputeTotalExposure` takes `const std::vector<Order>&`. A `std::vector` stores its elements by value in one contiguous array, so the 4 million orders are one 128 MB block ("Contiguous 128 MB").
- `orders[i].price * orders[i].quantity` reads directly from that block. There is no pointer to follow ("0 Indirection").

The speaker's numbers: Java after "realistic order book churn" takes **32.21 ms** per scan (8.05 ns per order). C++ takes **4.74 ms**, "6.8 times faster". Same algorithm, same data, roughly the same machine code for the arithmetic. The difference is entirely in how the data is laid out in memory.

### Counting bytes

The speaker's slide counts the bytes. The 28 bytes of payload are the same in both languages.

**C++:** `sizeof(Order)` is 32. The two `int64_t` fields require 8-byte alignment, so the struct's size is rounded up from 28 to the next multiple of 8, adding 4 bytes of padding at the end. Nothing else: no header, no hidden pointer, because `Order` has no virtual functions. 28 of 32 bytes are payload, 87.5% density. Four million of them occupy exactly 128 MB, and every 64-byte cache line holds two complete orders. You can make the compiler check all of this, as in this example of ours:

**Listing 19** — Letting the compiler check the C++ layout *(illustrative example, not from the talk)*

```cpp
#include <cstddef>
#include <cstdint>

struct Order {
  int64_t order_id;
  int64_t price;
  int32_t quantity;
  int32_t symbol_id;
  int32_t flags;
};

// 28 bytes of fields, rounded up to the 8-byte alignment of int64_t.
static_assert(sizeof(Order) == 32);
static_assert(alignof(Order) == 8);
static_assert(offsetof(Order, flags) == 24);   // 4 bytes of tail padding follow
static_assert(64 / sizeof(Order) == 2);        // two complete orders per cache line

int main() {}
```

**Java:** every object on the HotSpot heap starts with a header: an 8-byte **mark word** (lock state, identity hash code, GC age) and a **class pointer**, compressed to 4 bytes on 64-bit JVMs by default. On top of the object itself, the `ArrayList` stores a 4-byte **compressed reference** (a "compressed oop") to it in its internal `Object[]` array.

The speaker's slide adds this up as 8 + 4 + 28 + 8 bytes of alignment padding = **48 bytes** per object, plus the 4-byte reference: **52 bytes** per order, "a 62.5% memory bloat", and 208 MB for 4 million orders.

We checked this on OpenJDK 21 and got a smaller number. HotSpot does not have to lay out fields in declaration order. After the 12-byte header, the next free offset is 12, which is not 8-byte aligned, so HotSpot puts one of the `int` fields there and starts the `long`s at offset 16. The layout we measured with `sun.misc.Unsafe.objectFieldOffset` on an equivalent class is:

| Offset | Field |
|---|---|
| 0 | mark word (8 bytes) |
| 8 | compressed class pointer (4 bytes) |
| 12 | `quantity` (int) |
| 16 | `orderId` (long) |
| 24 | `price` (long) |
| 32 | `symbolId` (int) |
| 36 | `flags` (int) |

That is 40 bytes, already a multiple of 8, so no padding is needed. Measuring the allocation of a million actual `OrderRecord` records confirms it:

**Listing 20** — Measuring the real heap size of OrderRecord *(illustrative example, not from the talk)*

```java
import java.lang.management.ManagementFactory;

public class RecordSize {
    record OrderRecord(long orderId, long price, int quantity, int symbolId, int flags) {}

    static Object[] keep = new Object[1_000_000];

    public static void main(String[] args) {
        var mx = (com.sun.management.ThreadMXBean) ManagementFactory.getThreadMXBean();
        long tid = Thread.currentThread().threadId();
        long before = mx.getThreadAllocatedBytes(tid);
        for (int i = 0; i < keep.length; ++i) keep[i] = new OrderRecord(i, 1, 2, 3, 4);
        long after = mx.getThreadAllocatedBytes(tid);
        // Default JDK 21: 40.00. With -XX:-UseCompressedClassPointers: 48.00.
        System.out.printf("%.2f bytes per OrderRecord%n", (after - before) / (double) keep.length);
    }
}
```

It prints `40.00 bytes per OrderRecord` with default flags. Only with `-XX:-UseCompressedClassPointers`, which makes the class pointer 8 bytes and the header 16, does it print 48. So on a default JDK 21 the per-order cost is 40 + 4 = **44 bytes**, +37.5% rather than +62.5%, and about 176 MB rather than 208 MB for the whole book. The direction of the speaker's argument is unchanged: Java's layout is significantly bigger than the C++ struct, and every byte of overhead means fewer orders per cache line.

### Size is not the main problem: double indirection

"Worse than the size bloat is the double indirection." To read order *i* in Java, the CPU must:

1. read `ref[i]` from the `ArrayList`'s backing array (hop 1), then
2. read the `OrderRecord` object at the address that reference points to (hop 2).

The second read cannot start until the first completes, because its address is not known until then. If the objects are scattered around the heap, every hop 2 is a likely cache miss, and the prefetcher cannot help, because it sees no pattern in the addresses.

The C++ vector needs only one read per order, at an address that increases by exactly 32 bytes each time. The speaker describes the Zen 5 "hardware streamer prefetcher", which detects that stride and "pulls upcoming cache lines into L1D before the loop even asks for them." Each line fetched from DDR5 delivers two complete orders.

### The benchmark trap

Here is the part of the talk that the speaker says "fools 90% of blog benchmarks". Two ways of building the Java list:

**Listing 21** — code from the talk (The Benchmark Trap That Fools 90% of Blogs: Fresh TLAB vs. Churn), shown at [9:09](https://www.youtube.com/watch?v=KnqADllKdBw&t=549s)

```java
for (int i = 0; i < 4_000_000; ++i) {
  orders.add(new OrderRecord(i, p, q, s, 0));
}
long exposure = computeTotalExposure(orders);
```

**Listing 22** — code from the talk (The Benchmark Trap That Fools 90% of Blogs: Fresh TLAB vs. Churn), shown at [9:09](https://www.youtube.com/watch?v=KnqADllKdBw&t=549s)

```java
for (int step = 0; step < kChurnOps; ++step) {
  orders.set(randomSlot(), new OrderRecord(step, p, q, s, 0));
}
long exposure = computeTotalExposure(orders);
```

- The first listing (`NaiveBlogBenchmark.java`) fills the list in one loop of 4,000,000 iterations (note Java's `4_000_000` digit separators), then immediately measures the scan. Result: **5.24 ms**. Almost as fast as C++.
- The second (`ProductionOrderBookChurn.java`) simulates a live order book: for `kChurnOps` steps, it replaces the order at a random slot with a new `OrderRecord`, as cancels and modifications would. Then it measures the same scan. Result: **32.21 ms**, 6.1 times slower.

Why does the first one look so good? Because of the TLAB from Battle 1. Objects allocated one after another in a loop are placed one after another in memory, each one object-size further on. The slide calls this an "accidental +48-byte TLAB stride" (with the 40-byte object size we measured, the stride is 40 bytes; the effect is identical). The list's references point at addresses in increasing order with a constant gap, which is exactly the pattern the prefetcher is designed to follow. The fresh Java list behaves almost like a C++ array.

Real systems do not look like that. Orders are cancelled, modified and replaced in random order, garbage collections move the survivors, and after a while the references in the list point all over the heap. Every `orders.get(i)` is then a jump to an unrelated address, often on a different page.

> A benchmark that builds its data and then measures it immediately is measuring the allocator's layout, not the steady state of a running system. If production data churns, the benchmark has to churn too.

### Proof from the hardware counters

The speaker does not ask us to take the explanation on faith. Linux `perf stat` reads counters built into the CPU. Over 40 scans of the churned list:

- **LLC-load-misses: 477.1 million.** A 49.4% miss rate: nearly half of all loads that reach L3 miss it and go to DRAM.
- **dTLB-load-misses: from 7.06 million (fresh list) to 204.6 million (churned)**, nearly 29-fold. Scattered objects span many more pages than the TLB can cover, so the CPU is constantly walking page tables.
- **IPC: 0.38.** A core that can retire around six instructions per cycle averages less than one every two cycles: it is waiting for memory almost all the time.

Then the speaker turns the experiment around, and this is the key move of the battle. Is this a Java problem? To find out, make C++ chase pointers too.

**Listing 23** — code from the talk (Linux perf stat Proof: 477.1M L3 Misses, 29x dTLB & 0.38 IPC), shown at [9:57](https://www.youtube.com/watch?v=KnqADllKdBw&t=597s)

```cpp
void BM_Battle2_PointerArray_ChurnedHeap(
    benchmark::State& state) {
  std::vector<const Order*> ptrs = MakeShuffledHeap();
  for (auto _ : state) {
    int64_t total_notional = 0;
    for (size_t i = 0; i < ptrs.size(); ++i) {
      total_notional += ptrs[i]->price * ptrs[i]->quantity;
    }
    benchmark::DoNotOptimize(total_notional);
  }
}
```

- This is a Google Benchmark function. `benchmark::State& state` controls the run, and `for (auto _ : state)` is the timed loop; Google Benchmark decides how many times it runs.
- `MakeShuffledHeap()` (not shown) allocates each `Order` separately on the heap and returns a vector of `const Order*` in shuffled order, the C++ equivalent of a churned `ArrayList`.
- The inner loop reads `ptrs[i]->price * ptrs[i]->quantity`: one load for the pointer, one dependent load for the data. Exactly Java's double indirection.
- `benchmark::DoNotOptimize(total_notional)` tells the compiler the result is used, so it cannot delete the loop.

Result: **26.6 ms** and **245.6 million dTLB misses**, "the exact same hardware stall". The slide's caption sums it up: "It's silicon physics, not language." Contiguous data is fast in both languages, and scattered pointers are slow in both. Idiomatic C++ simply defaults to the contiguous version, and idiomatic Java defaults to the scattered one.

If you want to collect these counters yourself, the commands are short. For a C++ benchmark binary:

`perf stat -e LLC-load-misses,dTLB-load-misses,instructions,cycles ./battle2 --benchmark_filter=PointerArray`

For a JMH benchmark, the `perfnorm` profiler runs `perf` for you and divides the counts by the number of operations: `java -jar benchmarks.jar Battle2 -prof perfnorm`. Event names vary between CPUs; `perf list` shows what yours supports. We use both in the last section of this chapter.

## 5. Structure of Arrays, SIMD and the Valhalla Question

*Talk segment: [9:58](https://www.youtube.com/watch?v=KnqADllKdBw&t=598s)*

### Strip the objects away

Battle 2 showed that the expensive part of scanning orders is not the arithmetic but getting each order's bytes into the core. So the speaker asks the obvious follow-up: what if both languages stop storing *orders* and store *columns* instead? "Strip away object headers in both languages and store prices, quantities, and masks in three flat contiguous arrays, a structure of arrays, or SoA."

The two layouts have names:

- **Array of structures (AoS)**: `std::vector<Order>`, one struct after another. Reading `price` pulls in the whole 32-byte order, including fields the scan never uses.
- **Structure of arrays (SoA)**: one array per field. A scan that needs only prices and quantities reads only those arrays, and every byte loaded is a byte used.

This scan needs one more column, a *mask*: `-1` (all bits set) for an order that should count, `0` for one that should not, for example a cancelled order. `(x & mask)` is then either `x` or `0`, which lets the loop include or exclude orders without an `if`, keeping it branch-free.

**Listing 24** — code from the talk (Structure of Arrays (SoA): AVX-512 & Auto-Vectorized Dead Heat), shown at [10:45](https://www.youtube.com/watch?v=KnqADllKdBw&t=645s)

```cpp
struct OrderBookSoA {
  const int64_t* __restrict prices;
  const int64_t* __restrict quantities;
  const int64_t* __restrict masks;
};

int64_t ScanOrderBookSoA(const OrderBookSoA& b, size_t n) {
  int64_t total = 0;
  for (size_t i = 0; i < n; ++i) {
    total += (b.prices[i] * b.quantities[i]) & b.masks[i];
  }
  return total;
}
```

- `struct OrderBookSoA` holds three pointers to `const int64_t` arrays (each "Contiguous 32 MB": 4 million times 8 bytes).
- `__restrict` is a compiler extension (GCC, Clang and MSVC all support it; C has the keyword `restrict`, standard C++ does not). It promises the compiler that the three arrays do not overlap, which removes any doubt about whether the loop can be reordered and vectorized.
- The loop computes `(prices[i] * quantities[i]) & masks[i]` and adds it to `total`. The slide annotates the line with "AVX-512 vpmullq".

### SIMD: one instruction, many orders

Modern x86 CPUs have **SIMD** (single instruction, multiple data) registers: 128-bit `xmm`, 256-bit `ymm` (AVX2) and 512-bit `zmm` (AVX-512). A 512-bit register holds eight 64-bit integers, and one instruction can multiply all eight pairs at once. Compilers **auto-vectorize** simple loops like this one, turning the scalar loop into one that processes a whole register of elements per iteration.

The instruction that matters here is `vpmullq`, "packed multiply low, quadword": it multiplies 64-bit integers lane by lane and keeps the low 64 bits of each product, exactly `prices[i] * quantities[i]` in C++. It exists only in the AVX-512DQ extension. Zen 4 and Zen 5 have it, so on the speaker's Ryzen 9 9950X, GCC 13.3 turns the loop into this (our compilation of the slide's function with `-O3 -march=znver5`, inner loop only):

**Listing 25** — GCC 13 inner loop for ScanOrderBookSoA with -O3 -march=znver5 *(illustrative example, not from the talk)*

```asm
.L4:
    vmovdqu64  zmm3, ZMMWORD PTR [r8+rax]            ; 8 prices
    vpmullq    zmm0, zmm3, ZMMWORD PTR [rsi+rax]     ; x 8 quantities (64-bit low products)
    vpandq     zmm0, zmm0, ZMMWORD PTR [rcx+rax]     ; & 8 masks
    add        rax, 64                               ; advance 64 bytes = 8 elements
    vpaddq     zmm1, zmm1, zmm0                      ; 8 running totals
    cmp        rax, rdx
    jne        .L4
```

Three loads, a multiply, an AND and an add handle eight orders. The speaker reports **1.56 ms** for all 4 million orders. The slide quotes "57.26 GB/s": the loop reads 3 × 32 MB = 96 MB, and 96 × 10⁶ bytes in 1.56 ms is about 61.5 × 10⁹ bytes per second, or 57.3 GiB/s, so the slide's figure is in binary gigabytes. Either way, the loop is running close to what the memory system can deliver, which is the ceiling for any scan this size.

AVX2 has no `vpmullq`. On an AVX2-only CPU the compiler has to build each 64-bit product out of 32-bit multiplies (`vpmuludq`), shifts and adds, four lanes at a time. We return to this in the last section, because the laptop we re-ran the benchmarks on is exactly such a CPU.

### Java ties

Now give Java the same layout.

**Listing 26** — code from the talk (Structure of Arrays (SoA): AVX-512 & Auto-Vectorized Dead Heat), shown at [10:45](https://www.youtube.com/watch?v=KnqADllKdBw&t=645s)

```java
final class OrderBookSoA {
  final long[] prices;
  final long[] quantities;
  final long[] masks;

  long scanOrderBookSoA(int n) {
    long total = 0L;
    for (int i = 0; i < n; ++i) {
      total += (prices[i] * quantities[i]) & masks[i];
    }
    return total;
  }
}
```

- `final class OrderBookSoA` holds three `long[]` arrays. A Java primitive array is a single object, a header followed by the values packed contiguously. There are no per-element headers and no references.
- The loop is the same expression as the C++ one. The slide labels it "C2 Auto-Vectorized".
- The slide leaves out the constructor. Compiled exactly as printed, `javac` rejects the class ("variable prices not initialized in the default constructor"), because `final` fields must be assigned exactly once. The real class needs a constructor that takes or allocates the three arrays.

HotSpot's C2 has an auto-vectorizer called **SuperWord**, which recognises loops over primitive arrays and emits SIMD instructions for them. The speaker measures **1.55 ms** (1.56 ms under JMH): "a dead heat with C++, and 20.8 times faster than array list of order record."

This result is as important as any in the talk. For flat, primitive data, there is no meaningful speed difference between C++ and Java. Both compilers generate essentially the same SIMD loop, and both are limited by the same memory bandwidth.

### So why doesn't all Java run at 1.55 ms?

The speaker asks the natural question: "If Java primitive long arrays tie C++, why doesn't every Java code base run at 1.55 milliseconds?" The answer is that idiomatic Java cannot express this layout for structured data.

- **Generics work only with references.** Java generics are implemented by *erasure*: `List<OrderRecord>` and `List<Long>` are both, at run time, lists of `Object` references. There is no `List<long>`, and no way to say "a list of structs stored inline".
- **Libraries are built on that.** `List`, `Map<Long, Order>` (boxed keys plus an entry object per mapping), ORM frameworks and JSON mappers such as Jackson all produce graphs of small heap objects.

So in Java 21, to get the SoA performance you have to give up the type system's help and write something like this:

**Listing 27** — code from the talk (Why Every Java Codebase Doesn't Run at 1.55 ms: OOP vs. JEP 401), shown at [11:23](https://www.youtube.com/watch?v=KnqADllKdBw&t=683s)

```java
final class ManualSoABook {
  final long[] orderIds = new long[kMaxOrders];
  final long[] prices = new long[kMaxOrders];
  final int[] quantities = new int[kMaxOrders];
  final int[] symbolIds = new int[kMaxOrders];

  void updateOrder(int slot, long px, int qty) {
    prices[slot] = px;
    quantities[slot] = qty;
  }
}
```

- Four parallel arrays, one per field, sized to `kMaxOrders`. An "order" is no longer an object; it is an *index*.
- `updateOrder(int slot, long px, int qty)` writes into two of the arrays at the same index. The slide marks it "No Type Encapsulation": nothing ties `prices[slot]` and `quantities[slot]` together except the programmer's discipline, and nothing stops code from passing a slot from one book into another.

That is the cost the speaker names: to get 1.55 ms "you must abandon OOP and index parallel primitive arrays". In C++, `std::vector<Order>` is both idiomatic *and* flat, and if you want SoA for a hot scan you can still give it a typed interface. Here is a small example of ours:

**Listing 28** — A typed structure of arrays with std::span *(illustrative example, not from the talk)*

```cpp
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <span>
#include <vector>

// Owns three parallel columns and keeps them the same length.
class OrderColumns {
 public:
  void Add(int64_t price, int64_t qty, bool active) {
    prices_.push_back(price);
    quantities_.push_back(qty);
    masks_.push_back(active ? -1 : 0);
  }
  std::size_t size() const { return prices_.size(); }
  std::span<const int64_t> prices() const { return prices_; }
  std::span<const int64_t> quantities() const { return quantities_; }
  std::span<const int64_t> masks() const { return masks_; }

 private:
  std::vector<int64_t> prices_, quantities_, masks_;
};

int64_t TotalActiveNotional(const OrderColumns& book) {
  auto px = book.prices();
  auto qty = book.quantities();
  auto mask = book.masks();
  int64_t total = 0;
  for (std::size_t i = 0; i < px.size(); ++i) {
    total += (px[i] * qty[i]) & mask[i];   // vectorizes at -O3
  }
  return total;
}

int main() {
  OrderColumns book;
  for (int i = 0; i < 1000; ++i) book.Add(10000 + i % 97, 1 + i % 13, i % 10 != 0);
  std::printf("%zu orders, active notional %lld\n", book.size(),
              static_cast<long long>(TotalActiveNotional(book)));
}
```

The `OrderColumns` type owns the three columns and enforces that they have the same length. `std::span` (C++20) gives the scan a bounds-aware, non-owning view of each column without copying, and the hot loop is the same one GCC vectorizes above.

### Project Valhalla: flattening in the future

The long-term Java answer is **Project Valhalla**, and specifically **JEP 401: Value Classes and Objects**. The slide shows what the order record might look like:

**Listing 29** — code from the talk (Why Every Java Codebase Doesn't Run at 1.55 ms: OOP vs. JEP 401), shown at [11:23](https://www.youtube.com/watch?v=KnqADllKdBw&t=683s)

```java
value record OrderRecord(
    long orderId,
    long price,
    int quantity,
    int symbolId,
    int flags) {}
```

- `value record OrderRecord(...)` declares a record *without identity*. An identity object can be compared with `==`, locked with `synchronized`, and mutated in place through any reference to it; a value object promises none of that. Two value objects with the same fields are the same value.
- Without identity, the JVM is free to **flatten** value objects: store their fields directly inside an array or another object, with no header and no reference, like a C++ struct in a vector. The slide's label: "0-Byte Header Goal".

Note that this is preview syntax from Valhalla early-access builds. It does not compile on Java 21 (or on any released JDK at the time of writing), which is why the slide is labelled "JEP 401 Preview".

The speaker also points out the hard part, the "silicon constraint" of **tearing**. On x86-64, an aligned 64-bit store is atomic: another thread sees either the old value or the new one. This record is 28 bytes. If one thread writes a flattened `OrderRecord` into an array slot while another thread reads it, the reader could see the new `orderId` and `price` with the old `quantity`, `symbolId` and `flags`: a value that was never written. Today this cannot happen in Java: the array holds a reference, reading a reference is atomic, and the record's fields are `final`, so a reader always sees one complete record. Flattening removes the reference, so the JVM has to deal with tearing in some other way: either by keeping some values unflattened, by using atomic wide accesses where the hardware has them, or by letting programmers opt in to non-atomic behaviour. C++ has the same hazard, it just leaves preventing it (with a mutex, or `std::atomic` where possible) to the programmer.

> The speaker's summary of this point: until Valhalla flattens "identity-free composite structs directly inside arrays without object headers and without tearing multi-word updates, getting C++ memory layout in Java requires abandoning idiomatic OOP."

## 6. Battle 3: Tail Latency, Garbage Collectors and Off-Heap Java

*Talk segment: [11:26](https://www.youtube.com/watch?v=KnqADllKdBw&t=686s)*

### A different question: how slow is the slowest request?

The first two battles measured *throughput*: average nanoseconds per event, average milliseconds per scan. Trading systems, games and databases care just as much about **tail latency**: how long the slowest requests take. If a market data handler processes a burst in 1 µs on average but occasionally takes 10 ms, the occasional 10 ms is when you lose money.

Latency is reported as **percentiles**. Sort all measurements; p50 (the median) is the value half of them are below, p99 the value 99% are below, p99.99 the value all but one in ten thousand are below, and max is the single worst. A system that handles a million bursts a day sees its p99.99 a hundred times a day.

Battle 3 is designed to stress exactly this. The program keeps "a live order book of 2,097,152 resting orders" (2²¹ slots) and fires **50,000 bursts of 256 order replacements**, 12.8 million state mutations in total. Each burst is timed separately.

### The two implementations

The C++ side uses a **slab pool**.

**Listing 30** — code from the talk (Battle 3: Tail Latency Under 12.8M Live Order-Book Mutations), shown at [11:59](https://www.youtube.com/watch?v=KnqADllKdBw&t=719s)

```cpp
void ApplyMutationBurst(
    SlabPool<Order>& pool,
    std::span<Order*> book,
    uint32_t base_slot) {
  for (uint32_t j = 0; j < 256; ++j) {
    uint32_t idx = (base_slot + j) & kBookMask;
    pool.Free(book[idx]);
    book[idx] = pool.Allocate(idx, 10125, 100);
  }
}
```

- `SlabPool<Order>& pool` is a pre-allocated pool of `Order` slots (its code is not shown on the slide). "Pre-faulted" means every page of the pool was touched at startup, so the operating system has already mapped physical memory for it and no page fault can happen during a burst.
- `std::span<Order*> book` is a C++20 non-owning view of the book: an array of pointers, one per slot.
- For each of the 256 replacements, `idx` wraps around the book with `& kBookMask`, `pool.Free(book[idx])` returns the old order's slot to the pool, and `pool.Allocate(idx, 10125, 100)` takes a slot and constructs a new order in it. The slide marks both as O(1).

A slab pool is easy to build and worth knowing. Here is a minimal one of our own:

**Listing 31** — A minimal pre-faulted slab pool with an intrusive free list *(illustrative example, not from the talk)*

```cpp
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>
#include <utility>

template <typename T>
class SlabPool {
 public:
  explicit SlabPool(std::size_t capacity) : slots_(new Slot[capacity]) {
    std::memset(static_cast<void*>(slots_), 0, capacity * sizeof(Slot));  // pre-fault every page
    for (std::size_t i = 0; i + 1 < capacity; ++i) slots_[i].next = &slots_[i + 1];
    slots_[capacity - 1].next = nullptr;
    free_ = slots_;
  }
  ~SlabPool() { delete[] slots_; }
  SlabPool(const SlabPool&) = delete;
  SlabPool& operator=(const SlabPool&) = delete;

  template <typename... Args>
  T* Allocate(Args&&... args) {
    if (free_ == nullptr) return nullptr;            // pool exhausted
    Slot* s = std::exchange(free_, free_->next);     // pop the free list
    return ::new (static_cast<void*>(s->storage)) T{std::forward<Args>(args)...};
  }

  void Free(T* p) {
    if (p == nullptr) return;
    p->~T();
    Slot* s = reinterpret_cast<Slot*>(p);            // storage is at offset 0 of the union
    s->next = std::exchange(free_, s);               // push onto the free list
  }

 private:
  union Slot {
    Slot* next;                                      // while free
    alignas(T) unsigned char storage[sizeof(T)];     // while in use
  };
  Slot* slots_;
  Slot* free_ = nullptr;
};

struct Order {
  int64_t order_id;
  int64_t price;
  int32_t quantity;
  int32_t symbol_id;
  int32_t flags;
};

int main() {
  SlabPool<Order> pool(4);
  Order* a = pool.Allocate(int64_t{1}, int64_t{10125}, 100, 7, 0);
  pool.Free(a);
  Order* b = pool.Allocate(int64_t{2}, int64_t{10130}, 50, 7, 0);   // reuses a's slot
  std::printf("same slot reused: %s, price %lld\n", a == b ? "yes" : "no",
              static_cast<long long>(b->price));
}
```

- The constructor allocates all slots in one block and touches every byte with `memset`, which pre-faults the pages.
- Each slot is a `union`: while it is free, its bytes hold a pointer to the next free slot (an *intrusive* free list, costing no extra memory); while it is in use, the same bytes hold a `T`.
- `Allocate` pops the head of the free list and constructs a `T` there with placement `new`. `Free` destroys the object and pushes the slot back. Both are a few instructions, with no locks, no system calls and no size-class lookup, unlike a general-purpose `malloc`.

The Java side does what idiomatic Java does: allocate a new record and let the old one become garbage.

**Listing 32** — code from the talk (Battle 3: Tail Latency Under 12.8M Live Order-Book Mutations), shown at [11:59](https://www.youtube.com/watch?v=KnqADllKdBw&t=719s)

```java
void applyMutationBurst(
    OrderRecord[] book,
    int baseSlot) {
  for (int j = 0; j < 256; ++j) {
    int idx = (baseSlot + j) & BOOK_MASK;
    book[idx] = new OrderRecord(
        idx, 10125L, 100, 1, 0);
  }
}
```

- `OrderRecord[] book` is an array of references.
- `book[idx] = new OrderRecord(idx, 10125L, 100, 1, 0);` is a TLAB bump allocation (fast), and the overwritten record becomes unreachable. The slide's annotations: "Old Order Dies", "Fast TLAB Bump".

At the median, both look fine: **0.40 µs** per burst for the C++ slab pool, **1.54 µs** for Java 21. "Both look fast," says the speaker. "Now, let's look at the tail."

### G1: stop the world

A program that allocates 256 objects per burst eventually fills Eden. Then the garbage collector must run. OpenJDK's default collector is **G1** ("Garbage-First"). Its young-generation collections are **stop-the-world**: all application threads are brought to a **safepoint** (a point in the code where the JVM knows the exact state of every reference) and paused while GC threads copy the live objects out of Eden into survivor or old regions.

The trouble here is how much is live. The book keeps 2,097,152 records reachable at all times, so each young collection has to find and copy a large number of survivors. The speaker's measurements with `-XX:+UseG1GC -Xms4g -Xmx4g -XX:+AlwaysPreTouch` (a fixed 4 GB heap, touched at startup so page faults do not distort the result):

| G1, 4 GB | Latency per burst |
|---|---|
| p50 | 1.54 µs |
| p99 | 5.05 µs |
| p99.99 | 58.35 µs |
| max | 11,163.8 µs (11.16 ms) |

Three stop-the-world young evacuations happen during the run, and the burst unlucky enough to coincide with one takes 11.16 ms. That is **463 times** the C++ slab pool's worst burst, 24.10 µs.

### Generational ZGC: concurrent, but not free

"Java's answer to stop-the-world pauses is generational ZGC, introduced in Java 21." ZGC does its marking and its *relocation* (moving objects) **concurrently**, while the application keeps running. Application threads pause only for very short synchronisation points. On Java 21, generational mode is opt-in with `-XX:+UseZGC -XX:+ZGenerational`; it became the default in JDK 23, and from JDK 24 the non-generational mode is gone and the flag is ignored.

With the same 4 GB heap, the worst burst drops from 11.16 ms to **268.69 µs**, 41.5 times better. But moving objects while the program reads them requires cooperation from every read. The JIT inserts a **load barrier** after each reference load:

**Listing 33** — code from the talk (Generational ZGC (Java 21): 41.5x Lower Max vs. Load Barrier Tax), shown at [13:33](https://www.youtube.com/watch?v=KnqADllKdBw&t=813s)

```asm
; 64-Bit ZGC Colored OOP: [16b Color Metadata | 46b Object Addr]
mov   rax, QWORD PTR [rsi + rdx*8 + 16]
test  rax, QWORD PTR [r15 + 0x28]
jne   .L_zgc_load_barrier_slow_stub
shr   rax, 16
mov   rcx, QWORD PTR [rax + 16]
```

- The comment describes a ZGC **colored pointer**: a 64-bit reference that carries GC metadata bits ("color") in addition to the address.
- `mov rax, QWORD PTR [rsi + rdx*8 + 16]` loads `book[idx]`: the array base in `rsi`, the index in `rdx`, 8 bytes per uncompressed reference (ZGC does not use compressed oops), and 16 bytes of array header.
- `test rax, QWORD PTR [r15 + 0x28]` ANDs the loaded pointer with a "bad color" mask kept in the thread structure (`r15` again). If any bad bit is set, the pointer may refer to an object that has moved or not yet been marked.
- `jne .L_zgc_load_barrier_slow_stub`: in that case, take the slow path, which fixes up (and possibly relocates) the object and repairs the reference in memory.
- `shr rax, 16` removes the color. In generational ZGC the metadata lives in the low 16 bits of the stored reference and the address above it, so a right shift by 16 turns the stored value into a usable address.
- `mov rcx, QWORD PTR [rax + 16]` finally reads `OrderRecord.price`.

The fast path is three extra instructions per reference load. In a loop that reads references from a 2-million-entry book, that adds up, and so does the slow path whenever ZGC is relocating. The speaker's numbers for generational ZGC with 4 GB: p99 rises from 5.05 µs to **8.18 µs**, p99.99 from 58.35 µs to **90.73 µs**. ZGC trades a much better worst case for a somewhat worse typical tail.

### The iron law: concurrent GC races your allocation rate

A concurrent collector must reclaim memory at least as fast as the program allocates it. If it falls behind, threads stall waiting for memory. The speaker demonstrates by shrinking the heap from 4 GB to 1.5 GB (`-Xms1536m -Xmx1536m`):

| Generational ZGC | 4 GB | 1.5 GB |
|---|---|---|
| GC cycles | few | 15 concurrent cycles, 94 ms of GC CPU |
| p50 | 1.53 µs | 2.84 µs |
| p99 | 8.18 µs | 16.69 µs |
| p99.99 | 90.73 µs | 111.00 µs |
| max | 268.69 µs | 618.73 µs |

With less headroom, ZGC collects more often, its threads compete with the application for CPU, and every percentile gets worse. Meanwhile, the C++ slab pool holds **0.40 µs** at p50, **1.74 µs** at p99 and **8.17 µs** at p99.99, "with zero background GC threads", because it never produces garbage: every slot is reused in place.

> Garbage collection is not a fixed tax. Its cost depends on how fast you allocate, how much is live, and how much spare memory and spare CPU you give the collector. A slab pool removes all three variables.

### How low-latency Java avoids all of this

The speaker closes the battle with what latency-sensitive Java shops actually do. Frameworks such as **Aeron**, **Agrona** and **Chronicle** "never allocate order objects on the hot path". Instead, they allocate one large block of memory *outside* the Java heap at startup and read and write fields at byte offsets through a reusable cursor object, the **flyweight** pattern.

**Listing 34** — code from the talk (How Low-Latency Java Trading Firms Dodge GC: Off-Heap Flyweights), shown at [14:55](https://www.youtube.com/watch?v=KnqADllKdBw&t=895s)

```java
final class OffHeapOrderBook {
  private static final long ORDER_STRIDE = 32L;
  private final MemorySegment slab =
      Arena.ofShared().allocate(2_097_152L * ORDER_STRIDE, 64L);

  void replaceOrderAtSlot(int slot, long id, long px, int qty) {
    long offset = (long) slot * ORDER_STRIDE;
    slab.set(ValueLayout.JAVA_LONG_UNALIGNED, offset + 0L, id);
    slab.set(ValueLayout.JAVA_LONG_UNALIGNED, offset + 8L, px);
    slab.set(ValueLayout.JAVA_INT_UNALIGNED, offset + 16L, qty);
  }
}
```

- `MemorySegment` and `Arena` come from the Foreign Function & Memory API (Project Panama), in `java.lang.foreign`. `Arena.ofShared().allocate(bytes, alignment)` allocates native memory, outside the heap, so the GC never scans or moves it.
- The size is 2,097,152 slots times `ORDER_STRIDE` (32 bytes), aligned to 64 bytes (a cache line).
- `replaceOrderAtSlot` computes the slot's byte offset by hand, `(long) slot * ORDER_STRIDE`, and writes the id at `+0`, the price at `+8` and the quantity at `+16`, using `ValueLayout` constants to say how to encode each value.
- Nothing is allocated per order. The slide's label: "0 Heap Objects".

One version detail matters if you try this: in Java 21 the FFM API was still a *preview* API. Compiling this class with JDK 21 fails with "Arena is a preview API and is disabled by default"; it needs `--enable-preview` at both compile and run time. It became final in Java 22. Before Panama, the same pattern used `ByteBuffer.allocateDirect` or `sun.misc.Unsafe`. Here is the slide's class as a complete program:

**Listing 35** — The off-heap order book as a runnable program *(illustrative example, not from the talk)*

```java
import java.lang.foreign.Arena;
import java.lang.foreign.MemorySegment;
import java.lang.foreign.ValueLayout;

// JDK 21: javac --release 21 --enable-preview ...; java --enable-preview ...
// JDK 22 and later: no flags needed.
final class OffHeapOrderBook {
  private static final long ORDER_STRIDE = 32L;
  private final MemorySegment slab =
      Arena.ofShared().allocate(2_097_152L * ORDER_STRIDE, 64L);

  void replaceOrderAtSlot(int slot, long id, long px, int qty) {
    long offset = (long) slot * ORDER_STRIDE;
    slab.set(ValueLayout.JAVA_LONG_UNALIGNED, offset + 0L, id);
    slab.set(ValueLayout.JAVA_LONG_UNALIGNED, offset + 8L, px);
    slab.set(ValueLayout.JAVA_INT_UNALIGNED, offset + 16L, qty);
  }

  long priceAt(int slot) {
    return slab.get(ValueLayout.JAVA_LONG_UNALIGNED, (long) slot * ORDER_STRIDE + 8L);
  }

  public static void main(String[] args) {
    OffHeapOrderBook book = new OffHeapOrderBook();
    for (int s = 0; s < 2_097_152; ++s) book.replaceOrderAtSlot(s, s, 10125, 100);
    System.out.println("price at slot 42 = " + book.priceAt(42));
  }
}
```

It prints `price at slot 42 = 10125` (with `--enable-preview` on JDK 21, or as is on JDK 22 and later).

The speaker's verdict on this approach is pointed: it "achieves zero GC pauses by effectively writing manual C++ memory management inside Java syntax." Raw offsets, manual lifetimes, no type checking of the layout: everything Java's object model normally gives you is set aside, in exchange for the predictability that C++ gives by default.

## 7. Deoptimization, the Verdict and When to Pick Each Language

*Talk segment: [14:53](https://www.youtube.com/watch?v=KnqADllKdBw&t=893s)*

### The bet that won Battle 1, revisited

Battle 3 has a second part, and it targets the very mechanism that made Java win Battle 1. "Remember how C2 won battle one by betting that 100% of events were limit order event?" A speculative optimization is a bet. Most of the time it pays. The question for a latency-sensitive system is what happens on the one occasion it does not, and *when* that occasion tends to arrive.

The experiment: warm the event loop for 100,000 iterations on `LimitOrderEvent` only, so that C2 compiles it with a monomorphic class guard, and then send a single `MarketHaltEvent`, "the exact kind of packet that arrives when a market volatility circuit breaker trips". A market halt is rare by definition, and when it happens it is the most important message of the day.

**Listing 36** — code from the talk (C2 Speculative Deoptimization: The 35.41 us Circuit-Breaker Stall), shown at [15:57](https://www.youtube.com/watch?v=KnqADllKdBw&t=957s)

```asm
; Warmed 100,000x on LimitOrderEvent (100% Monomorphic Bet)
cmp   DWORD PTR [rsi + 8], 0x248a1c00
jne   .L_uncommon_trap_blob
call  Deoptimization::uncommon_trap
```

This is the compiled guard from Battle 1, seen from the failing side:

- The comment states the bet: 100,000 warm-up packets, 100% `LimitOrderEvent`.
- `cmp DWORD PTR [rsi + 8], 0x248a1c00` compares the incoming object's compressed class word with `LimitOrderEvent`'s. For packet #100,001, a `MarketHaltEvent`, the comparison fails.
- `jne .L_uncommon_trap_blob` jumps to the uncommon-trap stub.
- `call Deoptimization::uncommon_trap` enters the JVM's deoptimization code.

**Deoptimization** is the reverse of compilation. The JVM takes the state of the compiled frame (values in registers and stack slots, laid out the way C2 chose) and rebuilds equivalent *interpreter* frames (the slide mentions the `vframeArray`, the structure HotSpot uses for this), then resumes execution in the interpreter at the right bytecode. Depending on the trap's *action*, the compiled code is either thrown away and the method recompiled later with the new information, or (for `maybe_recompile`) kept, with the trap counted, until it has fired often enough to justify recompiling.

All of this takes time: the speaker measures **35,407 ns** for that single packet, 35.41 µs with G1 and 37.95 µs with ZGC. "In ahead-of-time O3 C++, that rare branch is already compiled into native machine code."

**Listing 37** — code from the talk (C2 Speculative Deoptimization: The 35.41 us Circuit-Breaker Stall), shown at [15:57](https://www.youtube.com/watch?v=KnqADllKdBw&t=957s)

```cpp
int64_t HandleMarketEvent(const FlatOrderEvent& ev) {
  if (__builtin_expect(ev.kind == EventKind::kMarketHalt, 0)) {
    return TriggerCircuitBreakerHalt(ev);
  }
  return ev.price_ticks * ev.qty - ev.fee_bps;
}
```

- `HandleMarketEvent` takes the flat event from Battle 1 by reference.
- `__builtin_expect(ev.kind == EventKind::kMarketHalt, 0)` is a GCC and Clang builtin that tells the compiler the condition is expected to be false (0), so it lays out the common path as the straight-line fall-through and moves the halt path out of the way. The halt path is still compiled, still present in the binary.
- If the event is a halt, `TriggerCircuitBreakerHalt(ev)` runs; otherwise the order arithmetic.

(As printed, the slides' `EventKind` from Battle 1 lists five kinds and has no `kMarketHalt`; read this listing as a sketch that adds one.)

The speaker's figure for the C++ rare branch is **0.14 ns**: "Pre-Compiled AOT", "0 traps". It is worth being precise about what such a number means. A single event cannot be timed to a fraction of a nanosecond; 0.14 ns is less than one clock cycle, so it must come from timing many calls and dividing. The meaningful point is qualitative: the C++ rare path costs about the same as the common path, a predicted or mispredicted branch, while the JIT's rare path costs tens of microseconds the first time it is taken.

Since C++20, the standard way to express the same hint is the `[[likely]]` / `[[unlikely]]` attribute pair, which works on any conforming compiler. Here is the circuit breaker written that way (our example):

**Listing 38** — The circuit breaker with the C++20 [[unlikely]] attribute *(illustrative example, not from the talk)*

```cpp
#include <cstdint>
#include <cstdio>

enum class EventKind : uint8_t { kLimit, kCancel, kModify, kFill, kPeg, kMarketHalt };

struct FlatOrderEvent {
  int64_t price_ticks;
  int64_t qty;
  int32_t fee_bps;
  EventKind kind;
};

int64_t TriggerCircuitBreakerHalt(const FlatOrderEvent&) {
  std::puts("circuit breaker: trading halted");
  return 0;
}

int64_t HandleMarketEvent(const FlatOrderEvent& ev) {
  if (ev.kind == EventKind::kMarketHalt) [[unlikely]] {
    return TriggerCircuitBreakerHalt(ev);   // already machine code, never "first-time" slow
  }
  return ev.price_ticks * ev.qty - ev.fee_bps;
}

int main() {
  int64_t total = 0;
  for (int i = 0; i < 100'000; ++i) {
    total += HandleMarketEvent({10125, 100, 3, EventKind::kLimit});
  }
  total += HandleMarketEvent({0, 0, 0, EventKind::kMarketHalt});
  std::printf("total %lld\n", static_cast<long long>(total));
}
```

And here is a Java program of our own that reproduces the trap, so you can watch it happen:

**Listing 39** — Reproducing a speculative-deoptimization trap *(illustrative example, not from the talk)*

```java
// Deopt.java - run with: java -Xlog:deoptimization=debug Deopt.java
public class Deopt {
    abstract static class MarketEvent { abstract long handle(); }

    static final class LimitOrderEvent extends MarketEvent {
        final long px, qty, fee;
        LimitOrderEvent(long px, long qty, long fee) { this.px = px; this.qty = qty; this.fee = fee; }
        long handle() { return px * qty - fee; }
    }

    static final class MarketHaltEvent extends MarketEvent {
        long handle() { return -1; }
    }

    static long sink;

    static long onPacket(MarketEvent ev) { return ev.handle(); }  // C2 bets on LimitOrderEvent

    static void warm(MarketEvent[] limits, int n) {
        for (int i = 0; i < n; ++i) sink += onPacket(limits[i & 1023]);
    }

    static long timeOne(MarketEvent ev) {
        long t0 = System.nanoTime();
        sink += onPacket(ev);
        return System.nanoTime() - t0;
    }

    public static void main(String[] args) throws Exception {
        MarketEvent halt = new MarketHaltEvent();     // load the class before warm-up
        MarketEvent[] limits = new MarketEvent[1024];
        for (int i = 0; i < limits.length; ++i) limits[i] = new LimitOrderEvent(10125, 100 + i % 7, i & 3);

        warm(limits, 100_000);
        Thread.sleep(200);                            // let C2 finish compiling onPacket
        warm(limits, 100_000);

        timeOne(limits[0]);                           // first call of timeOne is cold: discard it
        long limitNs = timeOne(limits[1]);
        long haltNs = timeOne(halt);                  // fails the class guard: deoptimizes
        System.out.printf("LimitOrderEvent: %d ns, first MarketHaltEvent: %d ns%n", limitNs, haltNs);
    }
}
```

Run it as `java -Xlog:deoptimization=debug Deopt.java` and, among the output, you will see a line like `[debug][deoptimization] ... level=4 Deopt.onPacket(LDeopt$MarketEvent;)J trap_bci=1 class_check maybe_recompile ...` at the moment the first `MarketHaltEvent` arrives: the class guard failed and the frame was deoptimized. The timings it prints are noisy, because a single event is being timed, but the first halt is consistently tens of microseconds or more, while an ordinary event is well under a microsecond. Note the design choices that make the experiment honest: `MarketHaltEvent` is loaded *before* the warm-up, so the guard is a type-profile bet rather than a class-hierarchy assumption (otherwise loading the class would invalidate the code before the event even arrives), and the warm-up loop is in its own method, so the timed calls hit the C2-compiled `onPacket` rather than an on-stack-replaced copy of `main`.

> The speaker frames this as a "tail latency hazard that even zero allocation Java cannot easily escape". Off-heap memory removes GC pauses; it does not remove speculation.

### All three battles, side by side

"Put all three battles side by side and the religious war evaporates into pure hardware physics."

| Battle | Faster | Numbers from the talk | The hardware reason |
|---|---|---|---|
| 1. Pointer-heavy OOP with per-event allocation | Java | 0.73 ns vs 16.8 ns | JIT inlines across files, deletes allocations, devirtualizes from profiles |
| 1. Same, written as a flat C++ value struct | C++ | 0.55 ns vs 0.73 ns (3.32 ns megamorphic Java) | no dispatch, nothing to speculate on |
| 2. Scan of 4M orders, idiomatic collections | C++ | 4.74 ms vs 32.21 ms | contiguous 32-byte structs vs scattered objects |
| 2. Scan of 4M orders, flat arrays (SoA) | tie | 1.56 ms vs 1.55 ms | both vectorize, both hit memory bandwidth |
| 3. Tail latency under mutation | C++ | p99.99 8.17 µs vs 58-111 µs; max 24.1 µs vs 0.27-11.16 ms | no GC, no barriers, no pauses |
| 3. First rare event | C++ | 0.14 ns vs 35.41 µs | no speculation, so no deoptimization |

### When to choose Java 21

The speaker's production guidance is notably even-handed. Java 21 is the right choice "when developer velocity, memory safety, and rapid domain evolution matter, your working sets fit comfortably with two to three times RAM headroom for generational ZGC, and your SLA is measured in hundreds of microseconds, around 269 microseconds max, rather than single-digit microseconds." The slide recommends `-XX:+UseZGC -XX:+ZGenerational -XX:+AlwaysPreTouch` for production. In that regime, "HotSpot's runtime profiling and escape analysis turn clean, idiomatic code into remarkably fast machine code."

**Listing 40** — code from the talk (Production Architecture Guide: When to Choose OpenJDK 21 LTS), shown at [17:19](https://www.youtube.com/watch?v=KnqADllKdBw&t=1039s)

```java
record OrderResult(long grossNotional, long feeTicks) {}

long processCustomerOrder(OrderEvent event) {
  OrderResult res = event.processHeapAlloc();
  return res.grossNotional() - res.feeTicks();
}
```

This is the Battle 1 pattern in its natural habitat:

- `record OrderResult` is a small immutable result type.
- `processCustomerOrder` makes a virtual call and builds a result object. Written plainly, with no tuning at all.
- The annotations say what C2 does to it when the traffic is monomorphic: devirtualizes the call and scalar-replaces the result into `RAX` and `RDX`.

The point of the slide is that ordinary enterprise code like this does not need to be contorted to be fast in Java, as long as the call sites stay monomorphic or bimorphic.

### When C++ is non-negotiable

The speaker lists four regimes where C++20 or C++23 is "non-negotiable":

1. **Strict single-digit-microsecond p99.99 SLAs.** The slab pool's 8.17 µs against the JVM's 58 to 111 µs.
2. **Multi-gigabyte in-memory order books**, "where a 52-byte object layout and 49.4% LLC miss rate would halve your server density." (With default JDK 21 flags the object is 40 bytes plus a 4-byte reference, as we measured in Battle 2. The density gap is smaller than the slide says, but it is real.)
3. **Every core pinned to a deterministic worker thread**, "with zero room for concurrent GC threads". A concurrent collector needs CPU time; if every core is already spoken for, ZGC has nowhere to run.
4. **Cold crisis paths** where "the very first packet... cannot afford a 35.41 microsecond JIT deoptimization trap".

The slide's sketch of such an engine:

**Listing 41** — code from the talk (Production Architecture Guide: When C++20 / C++23 Is Non-Negotiable), shown at [18:01](https://www.youtube.com/watch?v=KnqADllKdBw&t=1081s)

```cpp
static_assert(sizeof(Order) == 32);

class MatchingEngine {
 public:
  int64_t OnPacket(const FlatOrderEvent& ev) noexcept {
    if (__builtin_expect(ev.kind == kMarketHalt, 0)) {
      return ExecuteCircuitBreaker(ev);
    }
    Order* slot = slab_pool_.Allocate(ev);
    return slot->price * slot->quantity;
  }
 private:
  SlabPool<Order> slab_pool_;
};
```

- `static_assert(sizeof(Order) == 32);` pins the layout from Battle 2 at compile time: if someone adds a field and the struct grows past half a cache line, the build fails.
- `OnPacket` takes the 24-byte flat event by reference and is `noexcept`: no exception tables, no unwinding paths.
- The rare halt path is marked with `__builtin_expect` and compiled ahead of time, as in the circuit breaker listing.
- `slab_pool_.Allocate(ev)` takes an order slot from the pre-faulted slab pool (an O(1) free-list pop), and the method returns the notional.
- `SlabPool<Order> slab_pool_;` is the only allocator on the hot path.

As before, this is a sketch: the slide writes `kMarketHalt` unqualified, which would need `using enum EventKind;` (C++20) if `EventKind` is the scoped enum from Battle 1, and its `Allocate` takes an event rather than the field values used in the Battle 3 listing.

> Neither language "wins" in this guidance. The speaker's criteria are all about the *workload*: latency budget, memory budget, CPU budget, and how much a rare event is allowed to cost.

## 8. Auditing Benchmarks, and Re-running Them on a Different Machine

*Talk segment: [17:58](https://www.youtube.com/watch?v=KnqADllKdBw&t=1078s)*

### Four rules for auditing a C++ vs Java benchmark

The speaker closes with a checklist: "Whenever someone hands you a C++ versus Java benchmark, audit it against four rules."

1. **Check whether C++ was crippled** "by naive per iteration malloc or missing LTO". A benchmark of `make_unique` behind a virtual call across translation units mostly measures glibc's allocator. The slide's wording: it "benchmarks glibc tcache, not C++".
2. **Check whether Java's `ArrayList` was benchmarked fresh** "out of a sequential TLAB at 5.24 milliseconds instead of a realistic churned heap at 32.21 milliseconds."
3. **Demand p99.99, max and cold-path numbers**: "P99.99 max and cold path uncommon trap numbers, not just warmed up P50 averages."
4. **Verify every claim with `perf stat`**: L1D, LLC and dTLB counters explain *why* a number is what it is.

The speaker also states that the benchmarks, the JMH harness, the C++ slab pool and the `perf stat` scripts are open source in the channel's repository under episode 13, runnable with one Bazel command or a forensics script:

**Listing 42** — code from the talk (Open-Source Suite: Reproduce Episode 13 on Your Silicon), shown at [19:13](https://www.youtube.com/watch?v=KnqADllKdBw&t=1153s)

```bash
git clone https://github.com/hotpathcplusplus/hotpathcplusplus.git
cd hotpathcplusplus

bazel run --config=opt //episodes/13_cpp_vs_java_jit:ep13_suite
./episodes/13_cpp_vs_java_jit/run_ep13_forensics.sh
```

- The first two lines clone the repository and enter it. (The URL shown in the terminal, `github.com/hotpathcplusplus/hotpathcplusplus`, did not resolve when we tried it. The repository linked from the video description, `github.com/hotpathcpp/hotpathcplusplus`, also returned "not found". The channel's companion repository we could find is `github.com/ktgorekore/hotpathcplusplus`, Apache-2.0 licensed, and at the time of writing it did not yet contain an episode 13 directory.)
- `bazel run --config=opt //episodes/13_cpp_vs_java_jit:ep13_suite` builds and runs the C++ and Java suites through Bazel 9.
- `./episodes/13_cpp_vs_java_jit/run_ep13_forensics.sh` runs them pinned to a core, with the `perf stat` counters.

The summary it prints is the whole episode in three lines:

**Listing 43** — code from the talk (Open-Source Suite: Reproduce Episode 13 on Your Silicon), shown at [19:13](https://www.youtube.com/watch?v=KnqADllKdBw&t=1153s)

```console
[B1] C++ OOP:     16.8 ns  | Java C2:   0.73 ns  | C++ Flat: 0.55 ns
[B2] C++ Vector:  4.74 ms  | Java List: 32.21 ms | SoA SIMD: 1.55 ms
[B3] C++ p99.99:  8.17 μs  | G1 Max:    11.16 ms | ZGC Max:  268.7 μs
```

The speaker's final line: "Your CPU does not execute programming languages. It executes 64-byte cache lines, branch target predictions, and hardware page table walks. Respect the silicon, measure the tail."

### Taking the advice: re-running the battles on different hardware

Rule 4 says to verify claims on silicon, so we did. Since the episode's code was not available, we wrote our own versions of each benchmark from the listings in this chapter and ran them on a very different machine:

| | The talk | Our re-run |
|---|---|---|
| CPU | AMD Ryzen 9 9950X (Zen 5, 16 cores, AVX-512) | Intel Core i7-7820HQ (Kaby Lake laptop, 4 cores / 8 threads, 3.9 GHz turbo, **AVX2 only**) |
| C++ | GCC 13.3, `-O3 -march=native` | GCC 13.4, `-std=c++20 -O3 -march=native`, Google Benchmark 1.9.4 |
| Java | OpenJDK 21 | Liberica OpenJDK 21.0.6, JMH 1.37 (plus OpenJDK 25.0.2 for one check) |
| Counters | `perf stat` | `perf` 7.0; JMH `-prof perfnorm` and `-prof gc` |

C++ benchmarks ran pinned to one core (`taskset -c 2`), Java to two (`taskset -c 2,3`), with the CPU governor left at `powersave`. All numbers below are medians or means over repeated runs. Absolute times are not comparable with the talk: this CPU is from 2017, seven years older than the 9950X, with a smaller cache, slower DDR4 memory and no AVX-512. What we can compare is the *shape* of each result: ratios, and which side wins.

The JMH benchmark that matters most is Battle 2's, because of rule 2. Here it is; note the `churned` setup, which replaces 12 million random slots (three times the list size) with new records and forces garbage collections along the way, so that the list's references end up scattered:

**Listing 44** — Our JMH benchmark for Battle 2, with a churned heap *(illustrative example, not from the talk)*

```java
package bench;

import java.util.ArrayList;
import java.util.SplittableRandom;
import java.util.concurrent.TimeUnit;
import org.openjdk.jmh.annotations.*;

/** Battle 2: scan 4,000,000 orders and sum price * quantity. */
@BenchmarkMode(Mode.AverageTime)
@OutputTimeUnit(TimeUnit.MILLISECONDS)
@Warmup(iterations = 5, time = 2)
@Measurement(iterations = 5, time = 2)
@Fork(value = 2, jvmArgs = {"-Xms4g", "-Xmx4g"})
@State(Scope.Benchmark)
public class Battle2Bench {

  record OrderRecord(long orderId, long price, int quantity, int symbolId, int flags) {}

  static final int ORDERS = 4_000_000;

  /** fresh: objects allocated in one loop. churned: then 3 replacements per slot plus GCs. */
  @Param({"fresh", "churned"})
  String heap;

  ArrayList<OrderRecord> orders;

  static OrderRecord order(long i) {
    return new OrderRecord(i, 10000 + i % 97, (int) (1 + i % 13), (int) (i % 512), 0);
  }

  @Setup(Level.Trial)
  public void setup() {
    orders = new ArrayList<>(ORDERS);
    for (int i = 0; i < ORDERS; ++i) orders.add(order(i));
    if (heap.equals("churned")) {
      SplittableRandom rnd = new SplittableRandom(42);
      for (int step = 0; step < 3 * ORDERS; ++step) {
        orders.set(rnd.nextInt(ORDERS), order(step));
        if (step % ORDERS == 0) System.gc();
      }
      System.gc();
    }
  }

  static long computeTotalExposure(ArrayList<OrderRecord> orders) {
    long totalNotional = 0;
    for (int i = 0; i < orders.size(); ++i) {
      OrderRecord ord = orders.get(i);
      totalNotional += ord.price() * ord.quantity();
    }
    return totalNotional;
  }

  @Benchmark
  public long arrayListScan() {
    return computeTotalExposure(orders);
  }
}
```

The build needs no Maven project: the JMH jars from a local Maven repository plus the JMH annotation processor are enough for `javac`.

**Listing 45** — Building and running the suites without Maven or Bazel *(illustrative example, not from the talk)*

```bash
# C++: separate translation units, then whole-program LTO
g++-13 -std=c++20 -O3 -march=native cpp/battle1_bench.cc cpp/battle1_factory.cc \
    -lbenchmark -lpthread -o battle1_o3
g++-13 -std=c++20 -O3 -march=native -flto cpp/battle1_bench.cc cpp/battle1_factory.cc \
    -lbenchmark -lpthread -o battle1_flto
taskset -c 2 ./battle1_o3 --benchmark_repetitions=5 --benchmark_report_aggregates_only=true

# Java: compile against the JMH jars, running the JMH annotation processor
M2=$HOME/.m2/repository
CP=$M2/org/openjdk/jmh/jmh-core/1.37/jmh-core-1.37.jar:$M2/net/sf/jopt-simple/jopt-simple/5.0.4/jopt-simple-5.0.4.jar:$M2/org/apache/commons/commons-math3/3.6.1/commons-math3-3.6.1.jar
AP=$M2/org/openjdk/jmh/jmh-generator-annprocess/1.37/jmh-generator-annprocess-1.37.jar
javac --release 21 -cp "$CP" -processorpath "$AP:$CP" -d out java/bench/*.java

# Allocation per operation, then hardware counters per operation
taskset -c 2,3 java -cp "out:$CP" org.openjdk.jmh.Main Battle1 -prof gc
taskset -c 2,3 java -cp "out:$CP" org.openjdk.jmh.Main Battle2Bench \
    -prof 'perfnorm:events=LLC-load-misses,dTLB-load-misses,instructions,cycles'

# Tail latency, once per collector
java -XX:+UseG1GC -Xms4g -Xmx4g -XX:+AlwaysPreTouch -Xlog:gc -cp out bench.Battle3Tail G1-4g
java -XX:+UseZGC -XX:+ZGenerational -Xms4g -Xmx4g -XX:+AlwaysPreTouch -cp out bench.Battle3Tail ZGC-4g
```

### What we measured

**Battle 1** (nanoseconds per event):

| | Talk | Our laptop |
|---|---|---|
| C++ naive OOP (`make_unique` + virtual call, separate `.cc`) | 16.8 | 35.6 |
| same, with `-flto` | 15.2 | 35.1 |
| C++ pre-allocated ring (virtual call), `-O3` / `-flto` | 1.52 / 1.32 | 1.53 / 1.89 |
| C++ flat 24-byte struct | 0.55 | 1.08 (0.82 with `-flto`) |
| Java, event allocated fresh in the loop | 0.53 | 1.47 (0 B/op) |
| Java, pre-built ring, 1 type | 0.73 (0.89 JMH) | 2.00 (0 B/op) |
| Java, 2 types | 2.66 (2.55 JMH) | 4.67 (**32 B/op**) |
| Java, 5 types | 3.32 (3.52 JMH) | 5.88 (32 B/op) |

The headline survives. Naive C++ against Java's fresh in-loop version is **24x** here (35.6 / 1.47), inside the talk's 22.9x to 31.7x range. JMH's `-prof gc` confirms the mechanism: the monomorphic and fresh versions allocate 0 bytes per operation (`gc.alloc.rate.norm ≈ 10⁻⁵ B/op`, i.e. nothing), as escape analysis predicts. Unlike in the talk, LTO did not help the naive C++ version on our build at all. We did not dig into why.

Two results differ from the talk:

- **The pre-allocated C++ virtual ring was faster than monomorphic Java** on our machine (1.53 ns against 2.00 ns), where the talk has Java nearly 2x ahead. At this scale, a nanosecond or two per call, the harnesses themselves matter: a Google Benchmark loop iteration and a JMH invocation with its result going to a blackhole do not cost the same, and our C++ ring mixes in 1 `CancelOrderEvent` per 8 while the Java ring holds a single type. We would not read much into a difference this small across two different harnesses.
- **The bimorphic cliff came early, and the JIT has since fixed it.** On JDK 21, the two-type case already allocates 32 bytes per operation. C2 inlines both receivers, but the `OrderResult` now comes from one of two allocation sites, the two paths merge, and JDK 21's escape analysis cannot scalar-replace an object that comes out of such a merge. That is why it is more than twice as slow as one type, before the megamorphic cliff even starts. On **OpenJDK 25** the same benchmark gives 1.82 ns and 0 B/op for two types (newer C2 can scalar-replace allocations that merge), while five types still allocate 32 B/op at 5.53 ns. If your hot call sites are bimorphic, the JDK version matters.

**Battle 2** (milliseconds per scan of 4,000,000 orders):

| | Talk | Our laptop |
|---|---|---|
| C++ `std::vector<Order>` | 4.74 | 9.00 |
| C++ shuffled `Order*` (churned heap) | 26.6 | 50.2 |
| Java `ArrayList<OrderRecord>`, fresh | 5.24 | 12.2 |
| Java `ArrayList<OrderRecord>`, churned | 32.21 | 54.0 |
| C++ SoA | 1.56 (AVX-512) | 6.82 (AVX2) |
| Java SoA `long[]` | 1.55 | 7.27 |

Hardware counters per scan (from `perf stat` for C++, using the difference between a 10-scan and a 50-scan run to cancel out setup; from JMH `perfnorm` for Java):

| | LLC-load-misses | dTLB-load-misses | IPC |
|---|---|---|---|
| C++ vector | 0.61 M | 0.03 M | 0.75 |
| C++ shuffled `Order*` | 5.18 M | 3.90 M | 0.15 |
| C++ SoA | 0.10 M | 0.02 M | 0.57 |
| Java `ArrayList`, fresh | 0.23 M | 0.04 M | 1.02 |
| Java `ArrayList`, churned | 4.82 M | 3.67 M | 0.24 |

This battle reproduced closely. Churned Java is **6.0x** slower than the C++ vector (talk: 6.8x). Shuffled C++ pointers are **5.6x** slower than the vector, exactly the talk's ratio. The benchmark trap is real: the fresh `ArrayList` scans 4.4x faster than the churned one (talk: 6.1x), and its counters look almost like the C++ vector's. Churning multiplies the dTLB misses by **84x** here (the talk saw 29x), and the C++ pointer chase shows the same collapse in IPC as churned Java. Silicon physics, not language: on both machines.

The SoA result needs one more sentence. Java and C++ still tie (7.27 vs 6.82 ms), but the SoA layout is only 1.3x faster than the C++ vector here, not 3x. The reason is bandwidth: the vector scan reads 128 MB in 9.0 ms and the SoA scan reads 96 MB in 6.8 ms, both about 14 GB/s, which is roughly what one core of this laptop can pull from DDR4. Both loops wait on memory, so the 512-bit multiply the talk shows would not help here. On the 9950X, with far more bandwidth per core, the arithmetic matters more. As the speaker says, it is the hardware that decides.

**Battle 3** (microseconds per burst of 256 replacements, range over three runs):

| | p50 | p99 | p99.99 | max |
|---|---|---|---|---|
| C++ slab pool | 1.27-1.33 | 3.8-6.1 | 20-34 | 25-45 |
| Java G1, 4 GB | 1.96-2.04 | 3.9-9.1 | 30-53 | **45,500-53,400** |
| Java generational ZGC, 4 GB | 1.61-1.68 | 9.9-18.7 | 35-92 | 202-2,732 |
| Java generational ZGC, 1.5 GB | 1.88-2.87 | 14.7-18.0 | 110-2,913 | 150-3,040 |

The pattern holds, with bigger numbers. The medians are within a factor of two of each other, all "fast". G1's GC log shows three young evacuation pauses of 46 to 48 ms each, and the worst burst is over a thousand times the C++ slab pool's worst. ZGC removes those pauses but roughly doubles G1's p99, the load-barrier cost the speaker describes. Two caveats specific to our run: it is short (about 640 MB allocated in total), so ZGC completed only one or two cycles, not the 15 in the talk; and we confined each JVM, including its GC threads, to two logical CPUs. The occasional multi-millisecond ZGC outliers are concurrent GC threads competing with the application for those two CPUs, which is the speaker's third "non-negotiable" regime (cores with no room for GC threads) happening by accident.

**Deoptimization.** With our harness from the previous section (warm-up 2 × 100,000 events, then single timed calls from interpreted `main`), an ordinary `LimitOrderEvent` call cost about 0.65 µs including timer overhead, and the first `MarketHaltEvent` cost **100 to 175 µs** (around 145 µs typically) on both G1 and ZGC, with `-Xlog:deoptimization` confirming a `class_check` trap in the C2-compiled `onPacket`. The second halt event cost around 55 µs more: because the trap's action is `maybe_recompile`, the compiled code stays in place and traps again until HotSpot decides to recompile. An earlier version of our harness, which timed the calls inside an on-stack-replaced `main`, measured 16 to 68 µs instead. Single-event timings like this depend heavily on the harness and on the CPU's frequency state, so treat the talk's 35.41 µs and our ~145 µs as the same order of magnitude: tens to hundreds of microseconds, on the first event of exactly the kind you least want to be slow.

### Did the conclusions survive?

Yes, all of them that concern *direction*:

- Pointer-heavy OOP with per-event allocation: Java wins by over 20x, because of escape analysis.
- Scanning idiomatic collections: C++ wins by about 6x after churn, and fresh-heap Java benchmarks hide it.
- Flat arrays: a tie, set by memory bandwidth.
- Tails: G1's worst burst is more than a thousand times the slab pool's worst; ZGC trades the pauses for a worse p99.
- Rare events: a speculative JIT pays a deoptimization cost that ahead-of-time code does not.

The details that changed are themselves instructive. A Java object was 40 bytes, not 48. The bimorphic penalty depends on the JDK version. SIMD width stops mattering once a loop is bandwidth-bound. And LTO did nothing for our naive C++. Each of these is the kind of thing the speaker's fourth rule exists to catch.

All of our benchmark sources (C++, JMH and the Battle 3 harnesses), the build script and the raw results are in the `bench/` folder next to this post.

## Key takeaways

- Language is not the variable; memory layout, allocation strategy and what the compiler can see are.
- Per-object heap allocation behind an opaque virtual call is where C++ loses: the JIT can inline across it and delete the allocation.
- C2's speed comes from speculation on a runtime profile; diverse traffic (megamorphic call sites) and rare events (deoptimization) are where that bet costs.
- Contiguous value types win on scans: 32-byte structs in a vector beat 52-byte objects behind references by several times once the heap is churned.
- Flat primitive arrays put Java level with C++ on SIMD scans; idiomatic Java collections cannot use them until Valhalla.
- Median latency hides garbage-collection pauses; look at p99.99 and max, and know what each collector trades.
- A benchmark is only as good as its setup: fresh heaps, missing LTO and p50-only charts all mislead. Measure with hardware counters.

## Glossary

| Term | Meaning |
|---|---|
| AOT compilation | Ahead-of-time compilation: the program is compiled to machine code before it runs, as C++ is. |
| JIT compilation | Just-in-time compilation: the runtime compiles hot code to machine code while the program runs, using what it has observed. |
| C1 / C2 | HotSpot's two JIT compilers. C1 compiles quickly and can insert profiling (tier 3); C2 (tier 4) compiles optimised code using that profile. |
| Translation unit | One .cc file after preprocessing; the unit a C++ compiler optimises at a time unless link-time optimization is enabled. |
| LTO (-flto) | Link-time optimization: the compiler keeps an intermediate representation in object files and optimises across translation units at link time. |
| vtable / vptr | The per-class table of virtual function addresses, and the hidden pointer in each polymorphic object that points to it. |
| Devirtualization | Replacing an indirect virtual call with a direct (and usually inlined) call because the target is known. |
| Receiver type profile | HotSpot's record of which concrete classes have been seen at a call site. |
| Inline cache | A call-site cache of expected receiver types: monomorphic (one), bimorphic (two) or megamorphic (more than TypeProfileWidth, default 2). |
| Uncommon trap | A branch in C2 code that leaves compiled code when a speculative assumption fails, handing control back to the interpreter. |
| Deoptimization | Discarding compiled code whose assumptions no longer hold and rebuilding interpreter frames from its register state. |
| TLAB | Thread-local allocation buffer: a per-thread chunk of Eden where new is a pointer bump. |
| Eden | The part of a generational heap where new objects are allocated. |
| Escape analysis | A compiler analysis that decides whether an object can be seen outside the method (or thread) that created it. |
| Scalar replacement | Replacing a non-escaping object with its individual fields in registers or stack slots, eliminating the allocation. |
| tcache | glibc malloc's per-thread cache of recently freed small chunks, organised by size. |
| Mark word | The first 8 bytes of every HotSpot object header, holding lock state, hash code and GC age. |
| Compressed oops | 32-bit object references used by 64-bit HotSpot for heaps under about 32 GB. |
| Cache line | The 64-byte unit in which x86 CPUs move data between memory and caches. |
| Hardware prefetcher | CPU logic that detects regular access patterns (such as a constant stride) and fetches lines before they are requested. |
| TLB / dTLB | Translation lookaside buffer: a cache of virtual-to-physical page translations; a miss forces a page-table walk. |
| LLC | Last-level cache: the largest, slowest on-chip cache (L3 here). |
| IPC | Instructions per cycle retired; low IPC on a simple loop usually means the core is waiting on memory. |
| Structure of arrays (SoA) | Storing each field in its own contiguous array instead of an array of structs (AoS). |
| SIMD / auto-vectorization | Single instruction, multiple data: one instruction processes several values; compilers can generate it from plain loops. |
| Project Valhalla / JEP 401 | OpenJDK work on value classes: objects without identity that the JVM may flatten into arrays and fields. |
| Tearing | Seeing a value half-updated because a write wider than the hardware's atomic size was interleaved with a read. |
| Safepoint / stop-the-world | A point at which all Java threads are paused so the GC can work on a consistent heap. |
| G1 | Garbage-First, HotSpot's default collector: region-based and generational, with stop-the-world evacuation pauses. |
| Generational ZGC | A concurrent collector (generational from Java 21) that relocates objects while the program runs, using load barriers. |
| Load barrier | Code the JIT inserts after loading a reference, to check and if necessary fix the pointer for a concurrent collector. |
| Slab pool | A pre-allocated array of fixed-size slots with a free list; allocation and release are O(1) and never call the OS. |
| Flyweight | A reusable cursor object that reads and writes fields at byte offsets in a shared buffer instead of owning the data. |
| p50 / p99 / p99.99 | Latency percentiles: the value below which 50%, 99% or 99.99% of measurements fall. |

## Exercises

1. In the C++ pipeline, list every operation that happens per iteration in the naive version and say which one the JIT removes and why it can.

   <details><summary>Hint</summary>Count operator new, the virtual call and operator delete. Which of them depend on the compiler seeing CreateOrderEvent?</details>

2. Mark `LimitOrderEvent` as `final` and put `CreateOrderEvent` in the same translation unit as the loop. What can GCC now do that it could not before? Check with -S.

   <details><summary>Hint</summary>Look for a call through [rax + offset] in the assembly, and for calls to operator new.</details>

3. A JMH benchmark reports 0.73 ns/op for one event type and 3.3 ns/op for five. Explain the two separate effects that cause the slowdown.

   <details><summary>Hint</summary>One is about dispatch (TypeProfileWidth). The other is about allocation.</details>

4. Compute the size of `struct { int64_t a; int32_t b; int64_t c; int32_t d; }` and then reorder the fields to minimise it. What would the equivalent Java record cost per element in an ArrayList?

   <details><summary>Hint</summary>Alignment of int64_t is 8. Java: 12-byte header with compressed class pointers, 8-byte object alignment, plus a 4-byte reference.</details>

5. Why does a freshly built ArrayList scan nearly as fast as a C++ vector, and how would you write a benchmark that avoids this trap?

   <details><summary>Hint</summary>Think about where the TLAB placed the objects, and what replacing random elements does to that order.</details>

6. A service has a p50 of 1.5 us and a p99.99 of 60 us under G1. What extra measurements would you ask for before choosing between G1, ZGC and an off-heap design?

   <details><summary>Hint</summary>Max latency, GC log pause counts, heap headroom, CPU spent in GC threads, cold-path behaviour.</details>

7. Why does the first MarketHaltEvent cost tens of microseconds in Java but the second one much less? What could you do at startup to avoid paying it during a real market halt?

   <details><summary>Hint</summary>Deoptimization happens once per assumption. Consider warming the rare path, or ahead-of-time training.</details>

## Further reading

- Google Benchmark (github.com/google/benchmark)
- JMH, the Java Microbenchmark Harness (OpenJDK)
- Linux perf stat
- JEP 401: Value Classes and Objects (Project Valhalla)
- Generational ZGC (JEP 439, Java 21)
- Aeron, Agrona and Chronicle (off-heap Java messaging and data structures)
- Project Panama: the Foreign Function & Memory API (MemorySegment, Arena)
- Hot Path C++ companion repository: github.com/ktgorekore/hotpathcplusplus
