---
title: "The C++ Memory Model: From Compiler Tricks to std::latch"
date: 2026-11-14
tags: ["C++", "memory model", "concurrency", "atomics", "threads", "C++20", "CppCon"]
source: "https://www.youtube.com/watch?v=SVEYNEWZLo4"
video_title: "C++ Memory Model: from C++11 to C++23 - Alex Dathskovsky - CppCon 2023"
channel: "CppCon"
---

> This chapter is based on the talk [C++ Memory Model: from C++11 to C++23 - Alex Dathskovsky - CppCon 2023](https://www.youtube.com/watch?v=SVEYNEWZLo4) (CppCon). Code listings marked *from the talk* are transcribed from the slides; listings marked *illustrative* were written for this chapter to explain the ideas.

When you write a program, you probably picture the machine running it line by line, in the order you wrote it. It doesn't. Compilers move, merge, delete and vectorise your code; processors execute instructions out of order, speculate past branches and buffer writes in caches. For a single thread none of this is visible, because everything is arranged to give the same result *as if* the program ran as written. With several threads sharing memory, that illusion breaks, and the rules that say what one thread may observe of another's writes are the **memory model**.

In this CppCon 2023 talk, Alex Dathskovsky of Speedata walks from the bottom up: what compilers and CPUs do to your code, why that breaks naive multithreaded code, what sequential consistency and the "data-race-free" guarantee mean, why `volatile` is not a synchronisation tool, and how acquire and release semantics work. He then tours the concurrency toolbox that C++11 to C++20 provide, from `std::thread`, `thread_local` and `std::atomic` to futures, `std::jthread`, stop tokens, semaphores, latches, barriers and `std::call_once`, and ends with a disassembly showing what goes wrong when a spinlock uses relaxed ordering.

This chapter follows the talk and adds the background a student needs: data races and happens-before, why each example is correct (or not), and how to choose a memory order. The talk's examples were compiled and run for this chapter; where today's compilers behave differently, or where a slide's explanation needs a footnote, the chapter says so.

## What you will learn

- Describe common compiler optimisations (hoisting, vectorisation, constant folding, loop unrolling, dead-code removal) and why they are legal under the as-if rule
- Explain in-order versus out-of-order execution, branch prediction and caches, and how they can reorder memory operations
- Define a data race, sequential consistency and the SC-DRF guarantee, and explain happens-before
- Explain why volatile is not a synchronisation mechanism and what it is still for
- Explain acquire, release and sequentially consistent ordering, and use them with std::atomic and std::atomic_thread_fence
- Use thread_local correctly and recognise how passing its address defeats it
- Write a simple spinlock with std::atomic and explain the role of acquire/release
- Use futures, promises, packaged_task, std::jthread and stop tokens to pass results and cancellation between threads
- Use mutexes and lock wrappers, counting/binary semaphores, latches and barriers, and std::call_once
- Read a short disassembly and tell whether a lock protects a shared variable

## Before you start

- Comfortable reading modern C++ (lambdas, references, templates)
- A rough idea of what a thread is
- Reading a few lines of x86 assembly helps but is not required

## Contents

1. [Does the processor run the program you wrote?](#1-does-the-processor-run-the-program-you-wrote) — [0:00](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=0s)
2. [Compiler optimisations: hoisting, vectorisation and constant folding](#2-compiler-optimisations-hoisting-vectorisation-and-constant-folding) — [3:00](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=180s)
3. [Loop unrolling and dead-code removal](#3-loop-unrolling-and-dead-code-removal) — [6:15](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=375s)
4. [Inside the CPU: pipelines and branch prediction](#4-inside-the-cpu-pipelines-and-branch-prediction) — [8:10](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=490s)
5. [Out-of-order execution and dynamic scheduling](#5-out-of-order-execution-and-dynamic-scheduling) — [10:14](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=614s)
6. [Caches](#6-caches) — [14:07](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=847s)
7. [Reordering and the as-if rule](#7-reordering-and-the-as-if-rule) — [15:32](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=932s)
8. [Multithreading: reordering, sequential consistency and SC-DRF](#8-multithreading-reordering-sequential-consistency-and-sc-drf) — [17:26](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=1046s)
9. [volatile is not for threads](#9-volatile-is-not-for-threads) — [22:09](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=1329s)
10. [Barriers: compiler fences, acquire and release](#10-barriers-compiler-fences-acquire-and-release) — [24:36](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=1476s)
11. [std::thread and thread_local](#11-stdthread-and-threadlocal) — [28:19](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=1699s)
12. [std::atomic and memory orders](#12-stdatomic-and-memory-orders) — [31:05](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=1865s)
13. [A spinlock, and atomic shared_ptr](#13-a-spinlock-and-atomic-sharedptr) — [33:57](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=2037s)
14. [Futures, promises, jthread and mutexes](#14-futures-promises-jthread-and-mutexes) — [38:55](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=2335s)
15. [Semaphores and stop tokens](#15-semaphores-and-stop-tokens) — [43:14](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=2594s)
16. [Latches, barriers, static initialisation, call_once and packaged_task](#16-latches-barriers-static-initialisation-callonce-and-packagedtask) — [48:01](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=2881s)
17. [Bonus: what relaxed ordering does to a spinlock](#17-bonus-what-relaxed-ordering-does-to-a-spinlock) — [52:24](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=3144s)
18. [Questions](#18-questions) — [56:23](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=3383s)

## 1. Does the processor run the program you wrote?

*Talk segment: [0:00](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=0s)*

Alex Dathskovsky works at Speedata, an Israeli start-up building hardware for big-data query acceleration, and he writes educational C++ posts on LinkedIn. He opens with a "conspiracy": two trips to North American conferences, two long delays (25 hours, then 20), and lost luggage both times. "But don't worry, I brought a tinfoil cap."

### An important question

"Does the processor execute the program you wrote?" A few hands go up. "You're in the right [place]. You know it's not right."

- **Short version:** usually not.
- **Longer version:** "It will run what you intended it to run. … The outcome will be the same. But compilers can reorder your instructions, they can change the instructions, they can give you different instructions, something you haven't even thought about, they can remove your code. CPUs do out-of-order execution, do threading."

That is the theme of the whole talk. Your program has a meaning: its **observable behaviour**, such as what it prints, what it writes to files and how it interacts with the outside world. Everything below that level is negotiable. The compiler and the CPU are free to rearrange the work however they like, as long as a single thread can't tell the difference.

A single thread usually *can't* tell. Two threads looking at the same memory often *can*. The **memory model** is the set of rules that says what one thread is allowed to see of another thread's memory operations, and therefore which rearrangements are still legal once threads are involved.

### What "the same outcome" means

Here is a small illustration. The function below can be compiled in many ways: the stores to `a` and `b` might happen in either order, `c` might never be stored at all, and the loop might be replaced by a formula. None of that changes what the program prints, so all of it is allowed.

**Listing 1** — A program whose instructions are negotiable but whose output is not *(illustrative example, not from the talk)*

```cpp
#include <iostream>

int a, b;

int compute(int n)
{
    int c = 0;                 // may never be stored to memory at all
    for (int i = 1; i <= n; ++i) c += i;   // may become n * (n + 1) / 2
    b = 2;                     // these two stores may be emitted in either order...
    a = 1;
    return c;
}

int main()
{
    int r = compute(100);
    std::cout << a << ' ' << b << ' ' << r << '\n';   // ...because only this output is observable
}
```

The rest of the talk moves upward from there. First come the compiler's and the CPU's tricks. Then it covers what they mean for threads, and finally the tools C++ provides to keep them in check.

## 2. Compiler optimisations: hoisting, vectorisation and constant folding

*Talk segment: [3:00](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=180s)*

### Playing compiler

"Let's play a compiler role. I will be a very stupid compiler." The code:

**Listing 2** — code from the talk (If you were the compiler, what would you do?), shown at [4:49](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=289s)

```cpp
constexpr std::size_t N = 100;
auto data = get_int_array<N>();
std::size_t p_sum{};
for (std::size_t i{}; i < N; i++){
    auto p = get_pivot(N);
    p_sum += data[i] * data[p];
}
```

`get_int_array<N>()` returns an array filled with 0, 1, 2, … (using `std::iota`), and `get_pivot` is a function in the same translation unit, so the compiler can see its body. A human playing compiler already spots several improvements:

1. **`get_pivot(N)` always returns the same value**, so call it once before the loop and keep the result in a register. This is *loop-invariant code motion*.
2. **`data[p]` is also loop-invariant**, so load it once.
3. **Multiplication is expensive.** `Σ data[i] * data[p]` equals `data[p] * Σ data[i]`, so sum first and multiply once. This is a form of *strength reduction*.

"That's a very simple compiler, but already you can see that we can do many, many, many things."

### Vectorisation

"Newer CPUs have vectorisation." x86 processors have **SIMD** registers (SSE: 128 bits; AVX: 256 bits; AVX-512: 512 bits) that hold several integers at once, with instructions that operate on all of them. Compiled with Clang 16, the loop was split into chunks of four. The assembly on the slide shows tables of constants (`.LCPI` labels holding consecutive integers) feeding vector instructions. The compiler had worked out that the array holds sequential numbers and processed them several at a time.

### Doing it all at compile time

"If the optimisation is high enough, it will say: I know everything during compile time. I don't need to do anything at runtime."

**Listing 3** — code from the talk (Compiler can do even better), shown at [6:15](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=375s)

```asm
main:                                   # @main
        mov     eax, 247500
        ret
```

The whole program became one instruction that puts the answer in the return register. The value is easy to check: the array holds 0…99, whose sum is 4950, so 247500 means `data[p]` is 50. Presumably the pivot is the middle element, `N / 2`; the talk doesn't show `get_pivot`'s body.

This is **constant folding** taken to its limit. Because everything in the computation was visible and free of side effects, the compiler could run it during compilation.

### Seeing it for yourself

The best way to build intuition is to look at the assembly the compiler produces. Use Compiler Explorer (godbolt.org), or `g++ -O2 -S -masm=intel file.cpp` locally. Try this function, a fully visible computation, and watch the loop disappear at `-O2`:

**Listing 4** — A loop the optimiser turns into a constant *(illustrative example, not from the talk)*

```cpp
#include <array>
#include <cstddef>
#include <numeric>

template <std::size_t N>
constexpr std::array<int, N> get_int_array()
{
    std::array<int, N> a{};
    std::iota(a.begin(), a.end(), 0);
    return a;
}

constexpr std::size_t get_pivot(std::size_t n) { return n / 2; }   // an assumption: the talk doesn't show it

int main()
{
    constexpr std::size_t N = 100;
    auto data = get_int_array<N>();
    std::size_t p_sum{};
    for (std::size_t i{}; i < N; i++) {
        auto p = get_pivot(N);
        p_sum += data[i] * data[p];
    }
    return static_cast<int>(p_sum % 256);   // look at the -O2 assembly: no loop remains
}
// g++ -O2 -S -masm=intel: main is just "mov eax, <constant>; ret"  (247500 % 256 = 204)
```

## 3. Loop unrolling and dead-code removal

*Talk segment: [6:15](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=375s)*

### Loop unrolling

"Looping is bad sometimes. Why is it bad? Because each loop creates a branch," a jump back to the start of the loop body. Branches can disrupt the CPU's pipeline (next section). So "compilers may look at your loops and say: OK, I can open it up a little bit."

**Listing 5** — code from the talk (Loop unrolling), shown at [6:55](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=415s)

```cpp
for (int i{}; i<5; i++){
    std::cout << "Hello World!\n";
}
```

For this five-iteration loop, the compiler produced straight-line code with five consecutive calls to the stream-insertion function and no loop at all. "It created one basic block with sequential operations."

Unrolling trades code size for fewer branches and more opportunities to schedule instructions in parallel. Compilers unroll short loops with known trip counts fully, and longer loops partially (doing, say, four iterations per jump).

### Dead-code removal

**Listing 6** — code from the talk (Dead Code Removal), shown at [7:45](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=465s)

```cpp
int dead_code(int size){
    int i{}, j{};
    for (; i < size; i++){
        j++;
    }

    return i;
}
```

`j` is incremented on every iteration, but its value is never used. "And the compiler will not warn you about it, because `j` is actually used inside the function. It's not unused." Yet the computation of `j` has no observable effect, so it is **dead code**. Once `j` is gone, the loop only counts `i` up to `size`. The compiler can compute that directly:

**Listing 7** — code from the talk (Dead Code Removal), shown at [8:09](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=489s)

```asm
dead_code(int):
        xor     eax, eax
        test    edi, edi
        cmovg   eax, edi
        ret
```

Read the assembly:

- `xor eax, eax` sets the result to 0.
- `test edi, edi` compares the argument `size` with zero.
- `cmovg eax, edi` means: if `size > 0`, the result becomes `size`.
- `ret` returns.

So the function returns `max(size, 0)`, with no loop and no branch. (`cmovg` is a *conditional move*, which avoids a branch altogether.)

### The benchmarking trap

Dead-code elimination is the reason many naïve benchmarks measure nothing. If the result of the code under test is never used, the optimiser may delete the code, and the benchmark reports an impossibly fast time. Make the result observable:

**Listing 8** — Keeping a benchmarked computation alive *(illustrative example, not from the talk)*

```cpp
#include <chrono>
#include <iostream>

int dead_code(int size)
{
    int i{}, j{};
    for (; i < size; i++) j++;
    return i;
}

int main()
{
    auto t0 = std::chrono::steady_clock::now();
    long long total = 0;
    for (int n = 0; n < 1'000'000; ++n) total += dead_code(n % 1000);   // the result is used...
    auto t1 = std::chrono::steady_clock::now();

    volatile long long sink = total;                                     // ...and made observable
    (void)sink;
    std::cout << std::chrono::duration<double, std::milli>(t1 - t0).count() << " ms\n";
}
```

Benchmark libraries provide helpers for this, such as Google Benchmark's `benchmark::DoNotOptimize`.

## 4. Inside the CPU: pipelines and branch prediction

*Talk segment: [8:10](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=490s)*

"Now let's go to CPU optimisations." The speaker focuses on the **von Neumann** architecture, the design of nearly all general-purpose CPUs, with a simplification warning. He also points to his talk from the previous year on query acceleration for more detail.

### The pipeline

A CPU doesn't execute one instruction from start to finish before starting the next. It works like an assembly line. The slide shows four stages:

1. **Fetch** the instruction from memory.
2. **Decode** it to work out what it means.
3. **Execute** it in a functional unit such as the ALU or FPU.
4. **Write back** the result to a register or memory.

While one instruction executes, the next is being decoded and the one after that fetched. Ideally one instruction finishes every cycle, even though each takes four cycles in total.

The slide's diagram shows in-order execution over clock cycles: waiting instructions enter, flow through the stages, and come out completed. The weakness is **stalls**. "If you have an instruction that needs to write to the memory, it will stall for a long time." With in-order execution, "instructions that have nothing to do with" it are stuck behind it "for no reason".

### Branch prediction

A branch (an `if`, or a loop's back edge) is a problem for a pipeline. The CPU doesn't know which instruction comes next until the condition is evaluated, which happens several stages later. Waiting would leave the pipeline empty.

So "most CPUs have something that's called **branch prediction**". The slide shows the simplest scheme, a **2-bit saturating counter** per branch with four states: *strongly not taken*, *weakly not taken*, *weakly taken*, *strongly taken*. Each actual outcome moves the counter one step. The prediction follows the current state, and a single surprise doesn't flip a strongly established habit. The CPU fetches and starts executing instructions from the predicted path. If the prediction was wrong, "it will just need to flush everything and start over".

Modern predictors are far more sophisticated, using branch history and pattern tables, but the principle is the same. Predictable branches are almost free; unpredictable ones cost a pipeline flush each time they are mispredicted. Here is the classic demonstration, the same loop over sorted and unsorted data:

**Listing 9** — Branch prediction: sorted versus unsorted data *(illustrative example, not from the talk)*

```cpp
#include <algorithm>
#include <chrono>
#include <iostream>
#include <random>
#include <vector>

long long sum_big(const std::vector<int>& v)
{
    long long s = 0;
    for (int rep = 0; rep < 20; ++rep)
        for (int x : v)
            if (x >= 128) s += x;          // predictable only if the data is sorted
    return s;
}

int main()
{
    std::vector<int> v(1 << 20);
    std::mt19937 rng(1);
    for (auto& x : v) x = static_cast<int>(rng() % 256);

    auto time = [&](const char* label) {
        auto t0 = std::chrono::steady_clock::now();
        long long s = sum_big(v);
        auto t1 = std::chrono::steady_clock::now();
        std::cout << label << ": " << std::chrono::duration<double, std::milli>(t1 - t0).count()
                  << " ms (sum " << s << ")\n";
    };
    time("unsorted");
    std::sort(v.begin(), v.end());
    time("sorted  ");
}
```

Run on the machine used for this chapter at `-O0`, the unsorted pass took about 137 ms and the sorted pass about 37 ms. At `-O1` and above, both GCC 16 and Clang 20 turned the `if` into branch-free code (a conditional move or vector instructions), and the two passes ran in about the same time. That is the compiler removing the branch altogether: one more reason to measure the code you actually ship.

## 5. Out-of-order execution and dynamic scheduling

*Talk segment: [10:14](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=614s)*

### Out of order

"Why is out-of-order execution so important? With in-order execution, we used to stall all the time."

An **out-of-order** (OOO) CPU fetches and decodes instructions in program order, but executes them in whatever order their inputs become available. The diagram (from a paper by Yazdanpanah et al. on hybrid dataflow/von Neumann architectures) shows the parts:

- **Fetch** and **decode** (guided by the branch predictor).
- **Reservation stations** or an **issue queue**, where decoded instructions wait.
- Several **functional units** that execute in parallel.
- A **reorder buffer** (ROB) and **commit**: results are retired in the original program order.

The second diagram makes it concrete with three instructions: `MOV M[R1], R2` (a store to memory), `ADD R4, R5` and `SUB R6, R7`. In order, the ADD and SUB stall behind the slow store although they don't depend on it. Out of order, they execute immediately; "everything can run together", and when the store completes, their results are ready to retire.

### Dynamic scheduling

The slides list the steps of **dynamic scheduling**:

- An instruction is fetched and dispatched to an instruction queue.
- It waits in the queue until its input operands are available.
- If its operands are available, it may leave the queue **before** older instructions.
- It is issued to a functional unit.
- Only once all **older** instructions have completed is its result written to the register file.

It checks two kinds of **hazard**:

- **Structural hazards.** An instruction can be issued only if a suitable functional unit is free; otherwise it stalls.
- **Data hazards.** An instruction can execute only when its operands have been computed or loaded; otherwise it stalls.

"Instruction can be executed out of order iff there is no dependency on previous instructions, … ready instructions can execute before earlier instructions that are stalled."

### Why this matters for threads

The reorder buffer makes all of this invisible **to the thread itself**: registers and the thread's own view of memory always look as if the instructions ran in order. Another core sees something else. Stores sit in a **store buffer** before reaching the cache, and loads may be satisfied early. Another core can observe one thread's memory operations in a different order from the one the program specifies.

The classic demonstration is the **store-buffering** test. Each thread writes its own variable and then reads the other's. Intuitively at least one thread must see the other's write, yet with relaxed ordering, even x86 lets both reads return 0.

**Listing 10** — Store buffering: both threads can read the old value *(illustrative example, not from the talk)*

```cpp
#include <atomic>
#include <iostream>
#include <thread>

std::atomic<int> x{0}, y{0};
int r1, r2;

int main()
{
    int both_zero = 0;
    for (int i = 0; i < 200'000; ++i) {
        x = 0; y = 0;
        std::thread a([] { x.store(1, std::memory_order_relaxed); r1 = y.load(std::memory_order_relaxed); });
        std::thread b([] { y.store(1, std::memory_order_relaxed); r2 = x.load(std::memory_order_relaxed); });
        a.join(); b.join();
        both_zero += (r1 == 0 && r2 == 0);
    }
    // Thread creation overhead makes the window small, so the count is often low, but nonzero is allowed.
    std::cout << "r1 == r2 == 0 happened " << both_zero << " times\n";
}
```

When this was run for this chapter on an x86 machine, the "impossible" outcome appeared 3 times in 200 000 trials. Rare, because creating threads leaves only a tiny window, but real. With `std::memory_order_seq_cst` on all four operations, the "both zero" outcome is forbidden; the next sections explain why. This is the hardware side of the two-flag example that comes up a few sections later.

## 6. Caches

*Talk segment: [14:07](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=847s)*

### The memory hierarchy

"Another optimisation that is very important for CPUs is caching." Main memory is far slower than the CPU, so CPUs keep copies of recently used data in small, fast **caches** close to the cores. The slide's hierarchy:

| Level | Typical size | Typical latency |
|---|---|---|
| Registers | < 500 B | 0.25 ns |
| L1 cache | 64 KB | 1–2 ns |
| L2 cache | 8 MB | 5 ns |
| Main memory (DRAM) | 4 GB | 100 ns |
| Disk | 100 GB | 1 ms |

(Those are the slide's illustrative figures. Modern machines have bigger caches and memories, but the ratios are what matter.) "The closer the cache is to the CPU, the smaller it gets and the faster it gets." Data evicted from one level moves to a farther one, so a later access is still cheaper than going to RAM.

"Cache can accelerate reads and make writes longer and more complex." With several cores, each with its own caches, a write must eventually be made visible to every other core that has a copy. A **cache-coherence protocol** (MESI and its variants) keeps the copies consistent. That has costs: a cache line written by one core must be invalidated in the others.

"Using cache-friendly algorithms is really important." Caches move whole **cache lines**, typically 64 bytes. Accessing memory sequentially uses every byte of each line fetched. Jumping around wastes most of them.

### False sharing

Coherence works on whole lines, not individual variables. Two threads writing *different* variables that happen to share a line still fight over that line, which bounces between their caches. This is **false sharing**. It's invisible in the source code and can make "independent" threads dramatically slower. Aligning each variable to its own cache line fixes it:

**Listing 11** — False sharing and the alignas fix *(illustrative example, not from the talk)*

```cpp
#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>

struct packed  { std::atomic<long> a{0}; std::atomic<long> b{0}; };                 // same cache line
struct padded  { alignas(64) std::atomic<long> a{0}; alignas(64) std::atomic<long> b{0}; };  // separate lines

template <typename Counters>
double run()
{
    Counters c;
    auto t0 = std::chrono::steady_clock::now();
    std::thread t1([&] { for (int i = 0; i < 20'000'000; ++i) c.a.fetch_add(1, std::memory_order_relaxed); });
    std::thread t2([&] { for (int i = 0; i < 20'000'000; ++i) c.b.fetch_add(1, std::memory_order_relaxed); });
    t1.join(); t2.join();
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

int main()
{
    std::cout << "same line:      " << run<packed>() << " ms\n";
    std::cout << "separate lines: " << run<padded>() << " ms\n";
}
```

On the machine used for this chapter, two threads each doing 20 million relaxed increments took about 836 ms when the counters shared a cache line, and about 103 ms when they were 64 bytes apart: eight times faster, with identical source logic. C++17 also provides `std::hardware_destructive_interference_size` as a portable hint for the right alignment.

## 7. Reordering and the as-if rule

*Talk segment: [15:32](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=932s)*

### What C++ says

"What does it mean in C++, all that we said right now?" The slide's three rules:

- **Data dependencies must be honoured.** If one operation uses the result of another, they stay in order.
- **The C++ compiler may reorder any memory access under the as-if rule.**
- **Different processors have different reordering guarantees.** x86 reorders very little (essentially only a store followed by a load of a different location); ARM, POWER and RISC-V reorder much more. "You have to know with which processor you're working."

### The as-if rule

The **as-if rule** says the compiler may transform the program in any way, "as long as the outcome will stay the same". "Outcome" has a precise meaning: the program's **observable behaviour**. The slides spell out what that includes:

- **Accesses to `volatile` objects** "occur strictly according to the semantics of the expressions in which they occur. In particular, they are not reordered with respect to other volatile accesses on the same thread."
- **At program termination**, data written to files is exactly as if the program had executed as written.
- **Prompting text** sent to interactive devices is shown before the program waits for input.
- **Calls to external library functions** that the compiler cannot see into are kept, in order. They might have consequences the compiler can't analyse.

Everything else is fair game: temporaries, the order of independent stores, whether a variable lives in a register or in memory, even whether a loop runs at all.

### When the as-if rule doesn't protect you

"As-if rule is not always there for us":

- **Programs with undefined behaviour.** "Surprise, like everything with undefined behaviour: you don't have as-if rule anymore, and anything can happen." The compiler assumes UB never occurs, and optimises on that assumption. As later sections show, a **data race is undefined behaviour**, which is why racy code can break in such surprising ways.
- **Copy elision.** "The compiler may remove calls to move and copy constructors and destructors of temporary objects even if those calls have side effects." This is a deliberate exception that saves work, and in C++17 it became **mandatory** for prvalues. Code that counts copies or prints from copy constructors may print less than you expect:

**Listing 12** — Copy elision skips side effects in copy constructors *(illustrative example, not from the talk)*

```cpp
#include <iostream>

struct noisy {
    noisy() { std::cout << "construct\n"; }
    noisy(const noisy&) { std::cout << "copy\n"; }
    noisy(noisy&&) noexcept { std::cout << "move\n"; }
    ~noisy() { std::cout << "destroy\n"; }
};

noisy make() { return noisy{}; }   // prvalue: guaranteed elision since C++17

int main()
{
    noisy n = make();              // prints "construct" once and "destroy" once; no copy, no move
}
```

### Why this matters for threads

The as-if rule is defined in terms of a **single thread's** observable behaviour. Another thread reading shared memory is *not* part of that observable behaviour, unless the program uses the synchronisation tools that the memory model defines. That gap is where multithreaded bugs come from.

## 8. Multithreading: reordering, sequential consistency and SC-DRF

*Talk segment: [17:26](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=1046s)*

### A mutex for poor people

"What does it mean for multithreading? Let's look at those two threads."

**Listing 13** — code from the talk (Reordering between two threads), shown at [18:37](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=1117s)

```cpp
// Thread 1
flag1 = 1;
if (flag2 == 0){
    critical section}

// Thread 2
flag2 = 1;
if (flag1 == 0){
    critical section}
```

Each thread raises its own flag, then checks whether the other thread's flag is down. If it is, the thread enters the critical section. The idea, "a mutex for poor people", is that both threads can't see the other's flag down, because each raised its own first. (It is the core of Dekker's and Peterson's mutual-exclusion algorithms.)

The problem: "the compiler may reorder this." The store to `flag1` and the load of `flag2` touch different variables, so there is no data dependency between them, and either the compiler or the CPU (via its store buffer, as in the out-of-order section) may perform the load first. If both threads do that, **both see 0 and both enter the critical section**. The speaker also describes the opposite failure: with unlucky scheduling, both threads raise their flags, both see the other's flag up, and neither proceeds.

### Sequential consistency

What we intuitively expect is **sequential consistency** (SC):

> The result of any execution is the same as if the operations of all the processors were executed in some sequential order, and the operations of each individual processor appear in this sequence in the order specified by its program.

That is, a single global interleaving that respects every thread's program order. Under SC, the two-flag protocol works.

"SC is very expensive", because it forbids most of the reorderings that make compilers and CPUs fast, and the speaker's slide says "modern compilers do not offer it". More precisely, nothing gives you SC *for arbitrary code* for free.

### SC-DRF: the deal C++ offers

C++ (like Java, Rust and others) offers **sequential consistency for data-race-free programs**, SC-DRF:

> If your program has no data races, it behaves as if it were sequentially consistent.

A **data race** is two accesses to the same memory location from different threads, where at least one is a write, at least one is not atomic, and neither **happens before** the other. A data race is undefined behaviour.

To make a program race-free, the language provides **synchronising operations**: atomics, mutexes, thread creation and join, and so on. They create **happens-before** relationships between threads, and the slide's example shows the idea. Thread 1 does `Foo(Flag)`, then both threads reach a synchronising instruction `S(a)`. After that point, thread 2 knows that everything thread 1 did before `S(a)` has happened, so both can safely run `Bar(Flag)`.

Applied to the two-flag protocol: make the flags `std::atomic<bool>` with the default `seq_cst` ordering, and the reordering is forbidden. All seq_cst operations take part in one global order consistent with each thread's program order.

**Listing 14** — The two-flag protocol made correct with seq_cst atomics *(illustrative example, not from the talk)*

```cpp
#include <atomic>
#include <iostream>
#include <thread>

std::atomic<bool> flag1{false}, flag2{false};
int in_critical = 0, overlaps = 0;

void enter(std::atomic<bool>& mine, std::atomic<bool>& other)
{
    mine.store(true);                       // seq_cst by default
    if (!other.load()) {                    // seq_cst: cannot be reordered before the store
        if (++in_critical > 1) ++overlaps;  // the critical section
        --in_critical;
    }
    mine.store(false);
}

int main()
{
    for (int i = 0; i < 100'000; ++i) {
        std::thread a(enter, std::ref(flag1), std::ref(flag2));
        std::thread b(enter, std::ref(flag2), std::ref(flag1));
        a.join(); b.join();
    }
    std::cout << "overlapping entries: " << overlaps << '\n';   // always 0 with seq_cst
}
```

### Pop quiz

Four threads: thread 1 writes `x = 1`, thread 2 writes `x = 2`, thread 3 reads `x` twice into `y1` and `y2`, thread 4 reads it twice into `y3` and `y4`. Can the result be `y1=1, y2=2, y3=2, y4=1`, with threads 3 and 4 seeing the two writes in opposite orders? "Who thinks it may happen? Almost everyone. Very good." The speaker shows an interleaving that produces it, in which thread 4's reads are themselves reordered.

The answer depends on the kind of variable:

- With a **plain `int`**, the program has data races. It is undefined behaviour, and anything can happen.
- With a **`std::atomic<int>`**, even with `memory_order_relaxed`, this outcome is **forbidden**. All atomic writes to a single variable form one *modification order* that every thread observes consistently (*coherence*). Thread 3 seeing 1 then 2 means 1 comes before 2 in that order, so thread 4 cannot see 2 and then 1.

That rule is a useful anchor: even the weakest atomic ordering agrees on the history of each individual variable. What weak orderings give up is agreement on the relative order of **different** variables.

## 9. volatile is not for threads

*Talk segment: [22:09](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=1329s)*

### The elephant in the room

"First of all, I want to address the elephant in the room":

- `volatile` is **not** a synchronisation tool.
- `volatile` doesn't affect threading.
- It's **not** an atomic value.
- It **doesn't add barriers**.

"Please don't use them [for threads]." What `volatile` does is tell the compiler: "please don't omit it out, please don't reorder it". Every volatile read and write must happen, in order, relative to other volatile accesses in the same thread. "The CPU can do whatever it wants with it." The hardware may still reorder, and nothing makes the value atomic or ensures another thread sees it in order.

### A bad example

**Listing 15** — code from the talk (Volatile: Bad Example), shown at [23:53](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=1433s)

```cpp
int main(){
    int y = 1;
    volatile int* x = &y;

    auto w =std::thread([x](){ for (int i = 0; i < 50; i++){
        *x += 1;}});

    auto r =std::thread([x](){for (int i = 0; i < 50; i++){
        fmt::print("{}\n", *x);}});

    r.join();
    w.join();
}
```

One thread increments `*x` 50 times through a `volatile int*`, while another prints `*x` 50 times. The speaker says that on x86, "adding to integer is an atomic operation, so nothing wrong will happen here. But if you're on a different machine … you will have torn reads and writes."

That needs a correction. Compiled for x86-64 with GCC, `v += 1` on a `volatile int` becomes a single instruction, `add DWORD PTR v[rip], 1`, **without** a `lock` prefix. A plain `add` to memory is a read-modify-write that other cores can interleave with. It is *not* atomic across cores; only `lock add` is. (Aligned 32-bit loads and stores on x86 don't tear, which is probably what the speaker had in mind.) More fundamentally, in C++ terms this program has a **data race**: two threads access `y`, one writes, and nothing orders them. That makes it undefined behaviour on every machine, whatever the generated code. Here it would most likely only print stale values, but "nothing wrong will happen" is not a guarantee the language gives.

The fix is to use the tool designed for the job:

**Listing 16** — The same program with std::atomic *(illustrative example, not from the talk)*

```cpp
#include <atomic>
#include <iostream>
#include <thread>

int main()
{
    std::atomic<int> x{1};

    auto w = std::thread([&x] { for (int i = 0; i < 50; i++) x.fetch_add(1); });   // lock add / lock xadd
    auto r = std::thread([&x] { for (int i = 0; i < 50; i++) std::cout << x.load() << '\n'; });

    r.join();
    w.join();
    std::cout << "final: " << x << '\n';   // always 51
}
```

### Deprecated in C++20

"Most uses [are] deprecated with C++20":

- Is `+=` on a volatile a single atomic instruction? What about `++`? These compound operations look atomic but aren't, so they were deprecated.
- How many reads and writes does a compare-exchange on a volatile need, and what happens if it fails?
- `foo(int volatile n)` and `int volatile foo()`, meaning volatile parameters and return types, are meaningless.

For the full story, the speaker recommends **JF Bastien's CppCon 2019 talk "Deprecating volatile"**.

### What volatile is still for

In the Q&A, someone asks about reading hardware registers or shared memory with another device. That use is fine, and is what `volatile` was designed for. Each access must really happen, in order, because the "memory" is a device. Accesses within one thread keep their order (as the as-if rule says), and `volatile` is also needed for local variables modified between `setjmp` and `longjmp`. Just never use it to synchronise threads.

## 10. Barriers: compiler fences, acquire and release

*Talk segment: [24:36](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=1476s)*

### Compiler barriers

"Now let's talk about things that actually are synchronisation tools." The lowest level is a **barrier** (or fence): an instruction that says "don't move memory operations across this point".

**Listing 17** — code from the talk (Compiler Code Barries), shown at [25:53](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=1553s)

```cpp
// Thread 1
value = very_long_calc()
asm volatile("mfence" ::: "memory")
done = true

// Thread2
while (not done){}
asm volatile("mfence" ::: "memory")
//do something
```

Thread 1 computes a value and then sets `done`. Thread 2 spins until `done` is true and then uses the result. Without a barrier, the compiler or the CPU might make `done = true` visible before `value`. The slide's barrier is inline assembly:

- `asm volatile(...)`: an assembly block the compiler must not remove (that's what `volatile` means here).
- `"mfence"`: an x86 instruction that orders all earlier loads and stores before all later ones.
- `::: "memory"`: the *clobber* that tells the compiler this block may read or write any memory. That makes it a **compiler barrier**: the compiler cannot keep values in registers across it, or move memory accesses past it.

Note the difference. `asm volatile("" ::: "memory")` alone is a pure compiler barrier and emits no instruction. Adding `mfence` also stops the **CPU** from reordering. The code is still not correct C++, though: `done` is a plain variable read in a loop, so the program has a data race, and the compiler may legitimately hoist the read out of the loop.

The next slide covers the code with a sign: **"PLEASE DON'T USE THE FORCE."** "The standard has so many standardised tools that we don't need to write it." And: "Most locks and atomic operations will act like barriers."

### Acquire and release

"There's so much to say about memory barriers that I could fill a full hour." The speaker focuses on two ideas that make "everything else" understandable.

**Acquire semantics** apply to operations that **read** shared memory, whether plain loads or read-modify-writes (RMWs). Such an operation is a *read-acquire*. Acquire semantics "prevent memory reordering of the read-acquire with any read or write operation that **follows** it in program order." In the diagram, the read-acquire is a bar on top, and "all memory operations stay below the line".

**Release semantics** apply to operations that **write** shared memory, whether plain stores or RMWs. Such an operation is a *write-release*. Release semantics prevent reordering of the write-release "with any read or write operation that **precedes** it in program order". In the diagram, the write-release is a bar at the bottom, and "all memory operations stay above the line". (The slide is titled "Acquire Semantics" again, a copy-paste slip; it shows release.)

**Together.** A read-acquire at the top and a write-release at the bottom create a region: "all memory operations stay between the lines". Operations can't escape up past the acquire or down past the release. Inside the region, they can still be reordered among themselves.

This is exactly the shape of a **critical section**. Taking a lock is an acquire, so nothing inside leaks out above it. Releasing a lock is a release, so nothing inside leaks out below it.

### Message passing, done right

The pattern from the barrier slide, written with standard tools: the producer writes data, then does a **release** store to a flag. The consumer does an **acquire** load of the flag, then reads the data. If the acquire load sees the value written by the release store, everything before the release *happens before* everything after the acquire. The consumer is guaranteed to see the data.

**Listing 18** — Message passing with release and acquire *(illustrative example, not from the talk)*

```cpp
#include <atomic>
#include <cassert>
#include <iostream>
#include <thread>

int value;                       // plain data, published by the flag
std::atomic<bool> done{false};

int very_long_calc() { return 42; }

int main()
{
    std::thread producer([] {
        value = very_long_calc();
        done.store(true, std::memory_order_release);     // everything above stays above
    });
    std::thread consumer([] {
        while (!done.load(std::memory_order_acquire)) {} // everything below stays below
        assert(value == 42);                             // guaranteed: no data race
        std::cout << "got " << value << '\n';
    });
    producer.join();
    consumer.join();
}
```

## 11. std::thread and thread_local

*Talk segment: [28:19](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=1699s)*

### std::thread

"The first and the most important tool that you have from C++11 is `thread`." It's a portable thread class, and the standard library picks the platform's implementation (pthreads, Windows threads, …). "Who used pthreads here? Almost everyone. So you know how horrible it is." With `std::thread`:

**Listing 19** — code from the talk (std::thread), shown at [29:17](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=1757s)

```cpp
std::thread([](){ std::this_thread::sleep_for(1s); }).join();
```

Start a thread with any callable, then `join()` it. (A `std::thread` that is still joinable when destroyed calls `std::terminate`, so you must join or detach. That is one reason C++20 added `std::jthread`, covered later.)

### thread_local

`thread_local` (also C++11) gives a variable **one instance per thread**. "That's a good thing if you need something that all threads will use, the same variable, but they will have their own copy." Because each thread touches only its own copy, there is no data race.

**Listing 20** — code from the talk (thread_local), shown at [30:57](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=1857s)

```cpp
thread_local uint64_t inc = 0;

void do_count(std::string_view name, int count, uint64_t& ret){
    for (int i=0; i<count; i++){
        inc++;
    }

    ret = inc;
}

int main(){
    uint64_t ra{},rb{};
    std::thread a(do_count, "A", 10, std::ref(ra));
    std::thread b(do_count, "A", 20, std::ref(rb));
    a.join(); b.join();
    fmt::print("ret a: {}\nret b: {}\nmain inc: {}\n", ra, rb, inc);
}
```

- `inc` is `thread_local`, so threads `a` and `b` each get their own counter, starting at 0.
- Arguments passed to the `std::thread` constructor are **copied** into the new thread. To return a result through a reference parameter, the caller wraps the variable in `std::ref`: "we are just saying, hey, it's not something that you copy … it's a real reference."
- Thread `a` counts to 10 and thread `b` to 20, each in its own `inc`.

**Listing 21** — code from the talk (thread_local), shown at [31:11](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=1871s)

```console
ret a: 10
ret b: 20
main inc: 0
```

"It's really important to remember that the main thread is a thread. It has its own copy." That is why `main inc` prints 0.

### Please don't abuse it

"Of course, like everything with C++, we like to abuse everything, and I actually [have] seen something very similar in the code base":

**Listing 22** — code from the talk (thread_local), shown at [32:21](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=1941s)

```cpp
thread_local uint64_t inc = 0;

void do_count(std::string_view name, int count,
              uint64_t& ret, uint64_t* val){
    for (int i=0; i<count; i++){
        (*val)++;
    }

    ret = *val;
}

int main(){
    uint64_t ra{},rb{};
    std::thread a(do_count, "A", 10, std::ref(ra), &inc);
    std::thread b(do_count, "A", 20, std::ref(rb), &inc);
    a.join(); b.join();
    fmt::print("ret a: {}\nret b: {}\nmain inc: {}\n", ra, rb, inc);
}
```

Now `do_count` takes a pointer and increments through it, and `main` passes `&inc`, **the address of main's own `inc`**. Both worker threads increment the same variable. "You just broke what thread_local [is] supposed to be doing."

**Listing 23** — code from the talk (thread_local), shown at [32:25](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=1945s)

```console
ret a: 30
ret b: 20
main inc: 30
```

The slide pairs this with the "This is fine" dog in a burning room. Worse than defeating `thread_local`, the program now has a **data race**: two threads perform unsynchronised `++` on the same variable. The printed numbers depend on timing. When the talk's code was run for this chapter, it printed `ret a: 10`, `ret b: 30`, `main inc: 30`, the opposite assignment from the slide's run. With larger counts, increments would also be lost.

### A cleaner way to return results

The reference out-parameter in these examples is a workaround. "It will get a little bit better," the speaker promises, and futures (later in this chapter) are one answer. Another is to let each thread write into its own slot of a result container that the caller reads after joining:

**Listing 24** — Per-thread results without out-parameters or shared counters *(illustrative example, not from the talk)*

```cpp
#include <cstdint>
#include <iostream>
#include <thread>
#include <vector>

int main()
{
    std::vector<std::uint64_t> results(2);                       // one slot per thread: no sharing
    {
        std::jthread a([&results] { std::uint64_t n = 0; for (int i = 0; i < 10; ++i) ++n; results[0] = n; });
        std::jthread b([&results] { std::uint64_t n = 0; for (int i = 0; i < 20; ++i) ++n; results[1] = n; });
    }                                                            // joined here: writes happen before reads
    std::cout << "a: " << results[0] << ", b: " << results[1] << '\n';   // a: 10, b: 20
}
```

## 12. std::atomic and memory orders

*Talk segment: [31:05](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=1865s)*

### std::atomic

`std::atomic<T>` (C++11) "provides a portable way to perform low-level atomic operations". "We can do everything we used to do with ASM, or [where] we had to use a compiler intrinsic." Its guarantees:

- **No torn reads and no torn writes.** Every load sees a value that some store wrote in full.
- **Ordering on request.** It provides read-acquire, write-release and full memory barriers.

### Memory orders

Every atomic operation takes an optional `std::memory_order`:

| Order | Guarantees |
|---|---|
| `memory_order_relaxed` | "No sync or ordering constraints, only this operation is guaranteed to be atomic" |
| `memory_order_acquire` | On loads/RMWs: later operations stay after it |
| `memory_order_release` | On stores/RMWs: earlier operations stay before it |
| `memory_order_acq_rel` | On RMWs: both |
| `memory_order_seq_cst` | Acquire/release **plus** a single global order of all seq_cst operations. "This will give us sequential consistency, and this is the default mode" |

(There is also `memory_order_consume`, which compilers treat as acquire and which the speaker doesn't mention.)

### Three ways to write the same handoff

All three slides publish `a = 42` from thread 1 to thread 2 through `std::atomic<int> ready`. (The slides write `Int a = 0` with a capital I; read it as `int`.)

**With fences:**

**Listing 25** — code from the talk (Atomic Memory Fence), shown at [34:39](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=2079s)

```cpp
Int a = 0
std::atomic<int> ready(0)

// Thread 1
a = 42;
atomic_thread_fence(memory_order_release)
ready.store(1, memory_order_relaxed)

// Thread 2
int r1 = ready.load(memory_order_relaxed)
atomic_thread_fence(memory_order_acquire)
int r2 = a;
```

`std::atomic_thread_fence(memory_order_release)` before a relaxed store, paired with a relaxed load followed by `atomic_thread_fence(memory_order_acquire)`. If the load reads 1, the release fence *synchronises with* the acquire fence, and thread 2 is guaranteed to see `a == 42`. Fences are useful when one fence should order many atomic operations.

**With ordered operations, no fences:**

**Listing 26** — code from the talk (std::atomic Without a Fence), shown at [35:09](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=2109s)

```cpp
Int a = 0
std::atomic<int> ready(0)

// Thread 1
a = 42;
ready.store(1, memory_order_release)

// Thread 2
int r1 = ready.load(memory_order_acquire)
int r2 = a;
```

"The same program as before, less wording." The release store and acquire load carry the ordering themselves.

**With the default:**

**Listing 27** — code from the talk (std::atomic: Default), shown at [35:43](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=2143s)

```cpp
Int a = 0
std::atomic<int> ready(0)

// Thread 1
a = 42;
ready.store(1)

// Thread 2
int r1 = ready
int r2 = a;
```

Plain `ready.store(1)` and a plain read of `ready` are `seq_cst`. "It's a little bit easier to read, but you have to understand: if you have a full barrier, you pay the price for that."

In all three versions, thread 2 is only guaranteed to see `a == 42` **if it observed `ready == 1`**. Real code checks `r1` (or loops until it is 1) before using `a`. And because `a` is a plain `int`, reading it when `r1 == 0` would be a data race.

### Cost on x86

- "On x86, atomic loads are just loads (primitive types)." This holds for every memory order, because x86 loads already have acquire semantics. "Can be more expensive and cause locking on other systems", such as large types or other architectures.
- "On x86, atomic stores use `xchg`, and it is a full barrier." That is true for **seq_cst** stores, the default. When checked for this chapter with GCC, `a.store(x)` compiled to `xchg`, while `a.store(x, std::memory_order_release)` compiled to a plain `mov`, since x86 stores already have release semantics. Relaxing a store from seq_cst to release is therefore a real saving on x86.

You can verify the generated instructions yourself:

**Listing 28** — What each memory order compiles to (inspect with -S -masm=intel) *(illustrative example, not from the talk)*

```cpp
#include <atomic>

std::atomic<int> a;
int data;

void store_seq_cst(int x) { a.store(x); }                                // x86: xchg
void store_release(int x) { a.store(x, std::memory_order_release); }     // x86: mov
int  load_seq_cst()       { return a.load(); }                           // x86: mov
int  load_acquire()       { return a.load(std::memory_order_acquire); }  // x86: mov

void publish_with_fence(int x)
{
    data = x;
    std::atomic_thread_fence(std::memory_order_release);   // x86: no instruction, but a compiler barrier
    a.store(1, std::memory_order_relaxed);
}

int main() { publish_with_fence(42); return load_acquire() == 1 ? 0 : 1; }
```

## 13. A spinlock, and atomic shared_ptr

*Talk segment: [33:57](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=2037s)*

### A spinlock from one atomic

"Here I wanted to illustrate an example of using `std::atomic` for a spin lock."

**Listing 29** — code from the talk (std::atomic Spinlock Example), shown at [37:15](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=2235s)

```cpp
void spin_bool_foo(std::atomic<bool>& flag){
    while(flag.exchange(true)){
        //spin;
    }
    fmt::print("Worker Spin lock opened doing stuff \n");
    //doing stuff
    fmt::print("Worker releasing lock \n");
    flag = false;
}
```

`flag.exchange(true)` atomically stores `true` and returns the **previous** value. "If we are returning true, that means some other thread already acquired [the lock] … and if we will get false, that means that thread has released us," and we now hold it, because we just stored `true`. So the `while` loop spins until the lock was free at the moment we swapped. This is the classic **test-and-set** lock. Releasing is simply `flag = false`.

**Listing 30** — code from the talk (std::atomic Spinlock Example Cont), shown at [37:45](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=2265s)

```cpp
int main(){
    std::atomic<bool> flag(true);
    auto holder = std::thread([&flag](){
        std::this_thread::sleep_for(1s);
        fmt::print("Holder releasing lock \n");
        flag = false;});

    auto worker = std::thread(spin_bool_foo, std::ref(flag));

    holder.join();
    worker.join();
}
```

The flag starts as `true`, so the lock is held from the beginning. The `holder` thread sleeps for one second, announces it's releasing, and clears the flag. The `worker` spins in `spin_bool_foo` until then. (`std::ref(flag)` is needed because `std::thread` copies its arguments, and atomics can't be copied anyway.)

**Listing 31** — code from the talk (std::atomic Spinlock Example), shown at [37:55](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=2275s)

```console
Holder releasing lock
Worker Spin lock opened doing stuff
Worker releasing lock
```

The output order shows that the threads synchronised. Running the talk's code for this chapter printed the same three lines.

### Why it works, and how to make it cheaper

The slide uses the defaults, so `exchange` and the assignment are both `seq_cst`. A lock needs only:

- **acquire** on the operation that takes the lock, so nothing in the critical section moves above it, and
- **release** on the operation that frees it, so nothing in the critical section moves below it.

That is exactly the "stay between the lines" picture from the barriers section. Two further refinements are standard practice:

- **Test before test-and-set.** Spin on a cheap `load(relaxed)` until the lock looks free, and only then try `exchange`. An `exchange` always takes the cache line in exclusive mode, so spinning with it makes waiting cores fight over the line.
- **Wait instead of burning CPU.** C++20 added `atomic::wait` and `notify_one`/`notify_all`, which block efficiently until the value changes.

**Listing 32** — A spinlock with acquire/release, test-and-test-and-set, and atomic wait *(illustrative example, not from the talk)*

```cpp
#include <atomic>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

class spinlock {
    std::atomic<bool> locked_{false};
public:
    void lock()
    {
        while (locked_.exchange(true, std::memory_order_acquire)) {   // try to take it
            locked_.wait(true, std::memory_order_relaxed);            // sleep until it changes (C++20)
        }
    }
    void unlock()
    {
        locked_.store(false, std::memory_order_release);
        locked_.notify_one();
    }
};

int main()
{
    spinlock m;
    long counter = 0;
    {
        std::vector<std::jthread> ts;
        for (int t = 0; t < 4; ++t)
            ts.emplace_back([&] { for (int i = 0; i < 100'000; ++i) { std::lock_guard g(m); ++counter; } });
    }
    std::cout << counter << '\n';   // 400000
}
```

(A spinlock is the right tool only for very short critical sections. For anything else, `std::mutex` is better, because it puts waiting threads to sleep.)

### std::atomic<std::shared_ptr>

"A very good thing that was added in C++20 is `std::atomic<std::shared_ptr>`." A `std::shared_ptr`'s **control block** (its reference count) is updated atomically, so copying and destroying different `shared_ptr` objects that point to the same thing is safe. But a single `shared_ptr` *object*, read and written by several threads, is not: assigning to it while another thread copies it is a data race. `std::atomic<std::shared_ptr<T>>` makes those operations on one shared pointer atomic.

"The only problem with it: all implementations use locks, so it's not lock-free." For more, the speaker recommends Timur Doumler's CppCon 2022 talk on a lock-free atomic `shared_ptr`, and Daniel Anderson's CppCon 2023 talk *Lock-free Atomic Shared Pointers Without a Split Reference Count? It Can Be Done!*, which uses hazard pointers.

## 14. Futures, promises, jthread and mutexes

*Talk segment: [38:55](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=2335s)*

### Futures and promises

"Futures and promises were added in C++11. [They are] a good way to synchronise and basically transfer data between threads."

- `std::future<T>` is a value you may wait for.
- `std::promise<T>` produces a future: the producer side, where the value is put.

**Listing 33** — code from the talk (std::future and std::promise), shown at [40:03](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=2403s)

```cpp
int main(){
    std::promise<int> p;
    std::thread t1([&p](){
        std::this_thread::sleep_for(1s);
        p.set_value(42);
    });

    std::thread t2([](auto f){
        fmt::print("The value is {}", f.get());}, p.get_future());

    t1.join();
    t2.join();
}
```

Thread `t1` sleeps for a second, then fulfils the promise with 42. Thread `t2` receives the **future**, passed as an argument (futures are move-only, and `p.get_future()` returns a temporary that gets moved into the thread). It calls `f.get()`, which blocks until the value is available. "Get … means I will wait forever. You have other functions that can wait for a couple of seconds" (`wait_for`, `wait_until`). The talk's code printed `The value is 42` when run for this chapter.

Points to remember, from the slides:

- They "can help communication between threads" and "help build task-oriented utilities for executing work on different threads".
- "Future and promise are a one-shot operation." A promise can be fulfilled once, and `get()` can be called once.

Compared with the `thread_local` example: "We don't need to do the `std::ref` thing. We can just have a promise put into the thread."

Two things the slides don't show. If the producer throws, it can call `set_exception`, and `get()` rethrows the exception in the consumer. And if a promise is destroyed unfulfilled, the waiting future receives a `broken_promise` error instead of blocking forever.

### A small pipeline

**Listing 34** — code from the talk (Futures and Promises: another example), shown at [41:11](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=2471s)

```cpp
int main(){
    std::promise<int> prom1, prom2;
    auto future = prom1.get_future();
    std::vector<int> numbers(500);
    std::iota(numbers.begin(), numbers.end(), 0);
    auto sumTrd = std::thread([&prom1, &numbers](){
        auto sum = std::accumulate(numbers.begin(), numbers.end(), 0);
        prom1.set_value(sum);});
    auto doTrd = std::thread([&future, &prom2](){
        auto sum = future.get();
        prom2.set_value(sum > 100 ? sum/2 : sum + 200);});
    fmt::print("answer is {} \n", prom2.get_future().get());
    sumTrd.join();
    doTrd.join();
}
```

One thread sums the numbers 0…499 and fulfils `prom1`. A second thread waits on that future, applies a rule, and fulfils `prom2`. `main` waits on `prom2`'s future and prints the result. The sum is 124 750, which is greater than 100, so the answer is 124 750 / 2 = **62 375**. Running the code printed `answer is 62375`.

### std::jthread

"Another good tool that we received in C++20 is `jthread`":

- It is like `std::thread`, but **joins automatically** in its destructor. "Who here forgot to put join after he used threads? Pretty much a lot of people. So yeah, you will never forget again."
- It supports **cooperative cancellation** through `std::stop_source` and `std::stop_token` (the next section). Stop tokens also work without `jthread`.

**Listing 35** — code from the talk (Jthread: Example), shown at [42:15](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=2535s)

```cpp
int main(){
    std::promise<int> prom1, prom2;
    auto future = prom1.get_future();
    std::vector<int> numbers(500);
    std::iota(numbers.begin(), numbers.end(), 0);
    auto sumTrd = std::jthread([&prom1, &numbers](){
        auto sum = std::accumulate(numbers.begin(), numbers.end(), 0);
        prom1.set_value(sum);});
    auto doTrd = std::jthread([&future, &prom2](){
        auto sum = future.get();
        prom2.set_value(sum > 100 ? sum/2 : sum + 200);});
    fmt::print("answer is {} \n", prom2.get_future().get());
}
```

"The same example that we saw before, but … we don't have the joins." Note one subtlety: in this version, `main` waits for the answer *before* the `jthread`s are destroyed, and the threads are joined automatically at the end of `main`.

### Mutexes and lock wrappers

"Mutexes, condition variables and lock guards: I hope everyone knows about them."

**Listing 36** — code from the talk (Synchronization Tools), shown at [42:47](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=2567s)

```cpp
int main(){
    std::mutex m;
    auto doer = [&m](int idx){
        std::lock_guard l(m);
        fmt::print("doer{} thread doing stuff\n", idx);
    };
    std::jthread t1(doer, 0);
    std::jthread t2(doer, 1);
}
```

`std::lock_guard l(m);` locks the mutex and releases it when `l` goes out of scope (RAII), so you can't forget to unlock, even if an exception is thrown. The `{fmt}` call inside is therefore serialised between the two threads.

- `std::unique_lock` is like `lock_guard` but movable, and it can be **deferred**, unlocked early and relocked. It is the lock type `condition_variable` needs.
- `std::scoped_lock` (C++17) "takes ownership of multiple locks at once … with a deadlock avoidance algorithm". Locking two mutexes in different orders in two threads is the classic deadlock; `scoped_lock(m1, m2)` avoids it.

**Listing 37** — std::async, exceptions through futures, and scoped_lock *(illustrative example, not from the talk)*

```cpp
#include <future>
#include <iostream>
#include <mutex>
#include <numeric>
#include <stdexcept>
#include <thread>
#include <vector>

int main()
{
    // std::async: the promise/future plumbing is done for you.
    std::vector<int> numbers(500);
    std::iota(numbers.begin(), numbers.end(), 0);
    auto sum = std::async(std::launch::async, [&] { return std::accumulate(numbers.begin(), numbers.end(), 0); });
    std::cout << "sum = " << sum.get() << '\n';                    // 124750

    // Exceptions travel through the future.
    auto bad = std::async(std::launch::async, []() -> int { throw std::runtime_error("boom"); });
    try { bad.get(); } catch (const std::exception& e) { std::cout << "caught: " << e.what() << '\n'; }

    // Two mutexes, locked in opposite orders by two threads, without deadlock.
    std::mutex m1, m2;
    long a = 0, b = 0;
    {
        std::jthread t1([&] { for (int i = 0; i < 10000; ++i) { std::scoped_lock l(m1, m2); ++a; ++b; } });
        std::jthread t2([&] { for (int i = 0; i < 10000; ++i) { std::scoped_lock l(m2, m1); ++a; ++b; } });
    }
    std::cout << a << ' ' << b << '\n';                              // 20000 20000
}
```

## 15. Semaphores and stop tokens

*Talk segment: [43:14](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=2594s)*

### Semaphores

"More tools for C++20: counting semaphore. We finally have semaphores in the standard."

- `std::counting_semaphore<N>` holds a counter. `acquire()` decrements it, blocking while it is zero; `release()` increments it, waking a waiter.
- `std::binary_semaphore` is a counting semaphore with a maximum of 1.

Semaphores have no owner: one thread can `acquire` and another `release`. That makes them a natural tool for **signalling** between threads, which a mutex is not.

**Listing 38** — code from the talk (Synchronization Tools), shown at [45:13](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=2713s)

```cpp
std::binary_semaphore start_speaking{0}, person1_spoke{0},
                      person2_spoke{0};


void person_speak(std::string_view text, std::binary_semaphore& w, std::binary_semaphore& r){
    w.acquire();
    fmt::print("{}", te/* ? */
    std::this_thread::s/* ? */
    r.release();
}


int main(){
    std::jthread p1(per/* ? */ref(start_speaking), std::ref(person1_spoke));
    std::jthread p2(per/* ? */ref(person1_spoke), std::ref(person2_spoke));
    start_speaking.release();
    person2_spoke.acquire();

    fmt::print("GoodBye!");

    return 0;
}
```

Three binary semaphores, all starting at 0, form a relay. Each `person_speak` waits on its "go" semaphore `w`, prints, sleeps, and releases the next person's semaphore `r`. `main` releases `start_speaking` to let person 1 go, then waits on `person2_spoke` before saying goodbye. (A results pop-up hides part of the slide; the hidden parts are marked `/* ? */`.)

**Listing 39** — code from the talk (Synchronization Tools), shown at [45:13](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=2713s)

```console
Hello Person2
Hello person1
GoodBye!
```

Like a mutex, semaphore operations synchronise memory: everything before a `release()` happens before everything after the `acquire()` that it unblocks.

### stop_source and stop_token

C++20's **cooperative cancellation** has three parts:

- A `std::stop_source` can `request_stop()`.
- A `std::stop_token`, obtained from a source, can be polled with `stop_requested()`.
- A `std::stop_callback` can register a function to run when a stop is requested.

Cancellation is cooperative: the thread decides when to check its token and stop.

**Listing 40** — code from the talk (Synchronization Tools : stop_source), shown at [46:19](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=2779s)

```cpp
std::binary_semaphore start_speaking{0}, p1_done{0}, p2_done{0};

void person_speak(std::stop_token s, std::string_view text, std::binary_semaphore& w, std::binary_semaphore& r){
    w.acquire();
    while (not s.stop_requested()){
        fmt::print("{}", text);
        std::this_thread::sleep_for(1s);
    }
    r.release();
}

void stop_speaking(std::array<std::stop_source, 2> ssrc){
    for (auto i = 0u; i < ssrc.size(); i++){
        std::this_thread::sleep_for(2s);
        ssrc[i].request_stop();
    }
}


int main() {
    std::jthread p1(person_speak, "Hello Peson2 im Person1\n",
                    std::ref(start_speaking), std::ref(p1_done));
    std::jthread p2(person_speak, "Hello Peson1 im Person2\n",
                    std::ref(p1_done), std::ref(p2_done));
    start_speaking.release();
    std::jthread sched(stop_speaking, std::array{p1.get_stop_source(), p2.get_stop_source()});
    p2_done.acquire();
    fmt::print("Goodby!");
}
```

Each person now loops, printing every second, "until someone asks you to stop". A scheduler thread `stop_speaking` receives the two threads' stop sources and requests a stop on each after two seconds. `main` gets the sources from the `jthread`s themselves via `get_stop_source()`. "As I mentioned, `jthread` has the source already in." If a thread function's **first parameter** is a `std::stop_token`, `jthread` passes its own token automatically, "so you don't even need to think about it."

**Listing 41** — code from the talk (Synchronization Tools : stop_source), shown at [46:37](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=2797s)

```console
Hello Peson2 im Person1
Hello Peson2 im Person1
Hello Peson2 im Person1
Hello Peson2 im Person1
Hello Peson1 im Person2
Hello Peson1 im Person2
Hello Peson1 im Person2
Hello Peson1 im Person2
Goodby!
```

How many lines each person prints depends on timing. The slide shows four each; when the talk's code was run for this chapter, it printed two each. Each person prints roughly once per second for as long as it is allowed to speak, and person 2 can only start after person 1 has been stopped.

### Does it cost anything?

The speaker benchmarked the alternatives (`std::thread`, `std::jthread`, raw pthreads and several synchronisation variants) and found them "almost the same". The first bar was slower because of his own implementation, not the thread type. The benchmark is in one of his LinkedIn posts. The abstractions are free in practice.

### A cleaner stop-token loop

A polling loop with `sleep_for` wastes up to a second before noticing a stop request. `std::condition_variable_any` can wait on a stop token directly, waking as soon as a stop is requested:

**Listing 42** — jthread with an automatic stop_token and an interruptible wait *(illustrative example, not from the talk)*

```cpp
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <stop_token>
#include <thread>

int main()
{
    std::mutex m;
    std::condition_variable_any cv;
    using namespace std::chrono_literals;

    std::jthread worker([&](std::stop_token st) {          // first parameter: jthread passes its token
        std::unique_lock lock(m);
        int ticks = 0;
        while (!st.stop_requested()) {
            ++ticks;
            cv.wait_for(lock, st, 100ms, [] { return false; });   // returns early on stop request
        }
        std::cout << "stopped after " << ticks << " ticks\n";
    });

    std::this_thread::sleep_for(350ms);
    worker.request_stop();                                  // wakes the wait immediately
}                                                           // jthread joins here
```

## 16. Latches, barriers, static initialisation, call_once and packaged_task

*Talk segment: [48:01](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=2881s)*

### Latches and barriers

"More tools: C++20 gave us latches and barriers."

- A **`std::latch`** is a countdown. Threads call `count_down()`, and threads calling `wait()` block until the count reaches zero. It is **one-shot**.
- A **`std::barrier`** is reusable. Each phase, a fixed number of threads `arrive_and_wait()`; when the last one arrives, an optional **completion function** runs once, and then everyone is released into the next phase.

### Latch example

**Listing 43** — code from the talk (Synchronization Tools - Latch Example), shown at [49:31](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=2971s)

```cpp
int main(){
    std::vector<std::string_view> ppl{"Alex", "Dani", "Benny", "Guy"};
    std::vector<std::jthread> workers;
    std::latch jobs{ppl.size()}, go_home{1};
    auto job = [&jobs, &go_home](std::string_view name){
        fmt::print("{} is doing a job\n", name);
        jobs.count_down();
        go_home.wait();
        fmt::print("{} is going home\n", name);
    };

    for (const auto& p : ppl) {
        workers.push_back(std::jthread(job, p));
    }
    jobs.wait();
    fmt::print("Go home!\n");
    go_home.count_down();
}
```

`jobs` starts at the number of people, so each worker counts it down once. `go_home` starts at 1, so only `main` counts it down. Each worker announces its job, counts down `jobs`, then waits on `go_home`. `main` waits on `jobs` (all workers finished), announces "Go home!", and releases them all at once.

**Listing 44** — code from the talk (Synchronization Tools - Latch Example), shown at [49:45](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=2985s)

```console
Alex is doing a job
Dani is doing a job
Guy is doing a job
Benny is doing a job
Go home!
Alex is going home
Dani is going home
Guy is going home
Benny is going home
```

The order of names within each block varies from run to run, but the structure doesn't: every "doing a job" comes before "Go home!", and every "going home" after it.

### Barrier example

**Listing 45** — code from the talk (Synchronization Tools - Barrier Example), shown at [50:53](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=3053s)

```cpp
int main(){
    std::vector<std::string_view> ppl{"Alex", "Dani", "Benny", "Guy"};
    std::vector<std::jthread> workers;
    auto on_complete = [](){
        static int x = 0;
        not x++ ? fmt::print("Go Home!\n") : fmt::print("Done!\n"); };
    std::barrier sync{ppl.size(), on_complete};
    auto job = [&sync](std::string_view name){
        fmt::print("{} is doing a job\n", name);
        sync.arrive_and_wait();
        fmt::print("{} is going home\n", name);
        sync.arrive_and_wait();
    };

    for (const auto& p : ppl) {
        workers.push_back(std::jthread(job, p));
    }
}
```

One barrier does the work of both latches. The completion function runs **once per phase**, when the last thread arrives. Here it uses a `static` counter to print "Go Home!" after the first phase and "Done!" after the second. Each worker arrives twice.

**Listing 46** — code from the talk (Synchronization Tools - Barrier Example), shown at [51:13](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=3073s)

```console
Dani is doing a job
Guy is doing a job
Benny is doing a job
Alex is doing a job
Go Home!
Benny is going home
Alex is going home
Dani is going home
Guy is going home
Done!
```

**Two problems found when compiling this code.** Both apply to the latch example as well.

1. **Destruction order.** `workers`, a vector of `jthread`s, is declared *before* the barrier `sync`. Local variables are destroyed in reverse order of declaration, so at the end of `main` the barrier is destroyed **first**, while the worker threads may still be waiting on it, and only then are the `jthread`s joined. That is undefined behaviour. With GCC 16 the barrier program simply **hung**. The latch example has the same latent bug, but there `main` waits on `jobs` first, so the window is small and it happened to work. The fix is to declare the vector of threads *after* the synchronisation objects, so the threads are joined before the objects they use are destroyed.
2. **Narrowing.** `std::barrier sync{ppl.size(), on_complete}` (and `std::latch jobs{ppl.size()}`) converts `size_t` to `ptrdiff_t` inside braces. That narrowing conversion is ill-formed: Clang 20 rejects it, and GCC only warns. Use a cast, or parentheses instead of braces.

With both fixes, the barrier example ran correctly under GCC and Clang:

**Listing 47** — The barrier example with destruction order and narrowing fixed *(illustrative example, not from the talk)*

```cpp
#include <barrier>
#include <cstddef>
#include <print>
#include <string_view>
#include <thread>
#include <vector>

int main()
{
    std::vector<std::string_view> ppl{"Alex", "Dani", "Benny", "Guy"};
    auto on_complete = []() noexcept {
        static int x = 0;
        not x++ ? std::print("Go Home!\n") : std::print("Done!\n");
    };
    std::barrier sync{static_cast<std::ptrdiff_t>(ppl.size()), on_complete};   // no narrowing
    auto job = [&sync](std::string_view name) {
        std::print("{} is doing a job\n", name);
        sync.arrive_and_wait();
        std::print("{} is going home\n", name);
        sync.arrive_and_wait();
    };

    std::vector<std::jthread> workers;          // declared AFTER sync: joined before sync is destroyed
    for (const auto& p : ppl) {
        workers.push_back(std::jthread(job, p));
    }
}
```

### Thread-safe static initialisation

"Static is magical since C++11." If several threads call a function that initialises a function-local `static` for the first time, exactly one performs the initialisation and the others wait. These are "magic statics".

**Listing 48** — code from the talk (Synchronization Tools), shown at [51:43](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=3103s)

```cpp
struct X{
    int x;
};
```

**Listing 49** — code from the talk (Synchronization Tools), shown at [51:43](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=3103s)

```asm
get_once(int):                          # @get_once(int)
        movzx   eax, byte ptr [rip + guard variable for get_once(int)::single]
        test    al, al
        je      .LBB3_1
        lea     rax, [rip + get_once(int)::single]
        ret
```

**Listing 50** — code from the talk (Synchronization Tools), shown at [51:43](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=3103s)

```cpp
        std::jthread t1([](){auto& x = get_once(10);}),
        t2([](){auto& x = get_once(11);});
}
```

In the assembly, you can see the **guard variable**: `movzx eax, byte ptr [rip + guard variable for get_once(int)::single]`. The fast path checks the guard and returns the already-initialised object's address. Only the first call takes the slow path, which locks and initialises. (Overlapping boxes on the slide hide the body of `get_once` and the start of `main`.)

### call_once

**Listing 51** — code from the talk (Synchronization Tools), shown at [52:09](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=3129s)

```cpp
int main(){
    std::once_flag flag;
    auto f = [&flag](){
        static int x{10};
        std::call_once(flag, [](){x++;});
        fmt::print("{}\n", x);
    };

    std::jthread t1(f);
    std::jthread t2(f);
}
```

`std::call_once(flag, f)` runs `f` exactly once across all threads that use the same `std::once_flag`. Both threads run the lambda, but only one increments `x`, so both print 11:

**Listing 52** — code from the talk (Synchronization Tools), shown at [52:09](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=3129s)

```console
11
11
```

### packaged_task

"Packaged tasks are lazy threads, you may call [them]."

**Listing 53** — code from the talk (Synchronization Tools), shown at [52:29](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=3149s)

```cpp
int main(){
    std::packaged_task sum_task([](int a, int b){
        return a+b; });

    std::packaged_task mul_task([](int a, int b){ return a*b; });

    mul_task(10, 2);
    sum_task(10, mul_task.get_future().get());

    fmt::print("tot: {}", sum_task.get_future().get());

    return 0;
}
```

A `std::packaged_task` wraps a callable together with a promise. Calling the task runs the callable (here on the current thread) and stores the result, which you get through `get_future()`. Here `mul_task(10, 2)` computes 20, and `sum_task(10, 20)` computes 30, so it prints `tot: 30`, as it did when run for this chapter. A packaged task does nothing until it is invoked, so you can hand it to a thread or a thread pool to run later.

## 17. Bonus: what relaxed ordering does to a spinlock

*Talk segment: [52:24](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=3144s)*

"And here I want to talk about a special example that is really important to understand."

### A spinlock with relaxed ordering

**Listing 54** — code from the talk (Bonus : No Acquire), shown at [53:15](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=3195s)

```cpp
std::atomic<int> flag{0};
uint64_t shared_val{};
std::mt19937_64 eng{std::random_device{}()};
std::uniform_int_distribution dist{100, 500};

void IncsharedXtimeNoAquire(){
    for (int count{0}; count < 10000000;) {
        int expected{};
        if (flag.compare_exchange_strong(expected, 1,
        std::memory_order_relaxed)){
            //lock successfull
            shared_val++;
            count++;
            flag.store(0, std::memory_order_relaxed);
        }
    }
}
```

Two threads each run `IncsharedXtimeNoAquire`. They take a spinlock with `compare_exchange_strong(expected, 1, …)` and release it by storing 0, and between the two they increment the plain `uint64_t shared_val`. Each thread succeeds 10 000 000 times, so the expected final value is 20 000 000. (The random-number declarations at the top aren't used by this function.)

Both lock operations use `std::memory_order_relaxed`. "As I mentioned, when you say memory order relaxed, you actually mean: this operation is atomic, everything else is not guaranteed."

### The x86 disassembly

**Listing 55** — code from the talk (Bonus : No Acquire), shown at [53:41](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=3221s)

```asm
IncsharedXtimeNoAquire():                # @IncsharedXtimeNoAquire()
        xor     ecx, ecx
        mov     rdx, qword ptr [rip + shared_val]
        mov     esi, 1
        jmp     .LBB0_1
.LBB0_3:                                #   in Loop: Header=BB0_1 Depth=1
        cmp     ecx, 10000000
        jge     .LBB0_4
.LBB0_1:                                # =>This Inner Loop Header: Depth=1
        xor     eax, eax
        lock            cmpxchg dword ptr [rip + flag], esi
        jne     .LBB0_3
        inc     rdx
        mov     qword ptr [rip + shared_val], rdx
        inc     ecx
        mov     dword ptr [rip + flag], 0
        jmp     .LBB0_3
.LBB0_4:
        ret
```

"How nice, the compiler didn't reorder anything. We have the compare-exchange, then we are using the shared variable, and then we are putting zero to flag. How many of you think that this program will work? No one. Wow, good crowd."

Look at the third line: `mov rdx, qword ptr [rip + shared_val]` is **before the loop**. The compiler loads `shared_val` once, keeps it in register `rdx`, and in the highlighted block increments the register and stores it back (`inc rdx`, `mov [shared_val], rdx`). It never re-reads memory. Each thread therefore counts on its own private copy and overwrites the other's progress.

**Listing 56** — code from the talk (Bonus : No Acquire surprise), shown at [53:49](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=3229s)

```console
run0 shared_val = 10266917
run1 shared_val = 10258969
run2 shared_val = 10174515
run3 shared_val = 10001845
```

About 10 million instead of 20 million.

**Why is the compiler allowed to do this?** With relaxed operations, the lock creates no happens-before relationship between one thread's increment and the other thread's next increment. The two threads' accesses to `shared_val` are therefore a **data race**, which is undefined behaviour. The compiler may assume no other thread changes `shared_val` in between, and caching it in a register is then a perfectly valid optimisation. The bug isn't the compiler's. The lock never protected anything.

### With acquire and release

**Listing 57** — code from the talk (Bonus : With Acquire), shown at [54:19](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=3259s)

```cpp
void IncsharedXtimeAquire(){
    for (int count{0}; count < 10000000;) {
        int expected{};
        if (flag.compare_exchange_strong(expected, 1,
        std::memory_order_acquire)){
            //lock successfull
            shared_val++;
            count++;
            flag.store(0, std::memory_order_release);
        }
    }
}
```

"The code is the same except those two things": `memory_order_acquire` on the compare-exchange, `memory_order_release` on the store.

**Listing 58** — code from the talk (Bonus : With Acquire), shown at [54:43](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=3283s)

```asm
IncsharedXtimeAquire():                 # @IncsharedXtimeAquire()
        xor     ecx, ecx
        mov     edx, 1
        jmp     .LBB1_1
.LBB1_3:                                #   in Loop: Header=BB1_1 Depth=1
        cmp     ecx, 10000000
        jge     .LBB1_4
.LBB1_1:                                # =>This Inner Loop Header: Depth=1
        xor     eax, eax
        lock            cmpxchg dword ptr [rip + flag], edx
        jne     .LBB1_3
        inc     qword ptr [rip + shared_val]
        inc     ecx
        mov     dword ptr [rip + flag], 0
        jmp     .LBB1_3
.LBB1_4:
        ret
```

Now the highlighted line is `inc qword ptr [rip + shared_val]`, a fresh read-modify-write of memory on every iteration inside the critical section. The acquire forbids moving the access above the lock, and the release forbids moving it below the unlock, so caching it across iterations is no longer possible. The deck's next slide shows all four runs printing exactly 20 000 000.

One detail in the explanation needs care. The speaker says that "because it's x86, it is an atomic operation on integers". An `inc` on memory without a `lock` prefix is **not** atomic across cores. It doesn't need to be here, because the spinlock ensures only one thread executes it at a time. The atomicity comes from the lock, not from the instruction.

### Reproducing it

The talk used Clang 16. For this chapter, the same code was compiled at `-O2` and run three times per configuration:

| Compiler | relaxed | acquire/release |
|---|---|---|
| Clang 20 | 10 001 773, 10 001 361, 10 000 000 | 20 000 000 ×3 |
| GCC 16 | 20 000 000 ×3 | 20 000 000 ×3 |

Clang 20 still hoists the load, exactly as in the talk. GCC happens not to, and gets the right answer by luck. **A racy program that works on one compiler is still broken.**

### RISC-V

"I know that x86 disassembly is kind of hard to understand … so I wanted to show you the disassembly for RISC-V", a load/store architecture with explicit ordering annotations.

**Listing 59** — code from the talk (Bonus: RISCV Disassembly), shown at [55:29](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=3329s)

```asm
IncsharedXtimeNoAquire():
        li      a4,9998336
        addi    a4,a4,1664
        lui     a3,%hi(flag)
        lui     a2,%hi(shared_val)
        li      a1,1
.L6:
        addi    a0,a3,%lo(flag)
        1: lr.w a5,0(a0); bne a5,zero,1f; sc.w a6,a1,0(a0); bnez a6,1b; 1:
        sext.w  a5,a5
        bnez    a5,.L6
        ld      a5,%lo(shared_val)(a2)
        addi    a5,a5,1
        sd      a5,%lo(shared_val)(a2)
        addi    a5,a3,%lo(flag)
        amoswap.w zero,zero,0(a5)
        addiw   a4,a4,-1
        bnez    a4,.L6
        ret
```

The relaxed version's compare-exchange is a plain `lr.w` / `sc.w` loop (load-reserved, store-conditional), and the unlock is a plain `amoswap.w`. Neither carries any ordering. The increment of `shared_val` (`ld`, `addi`, `sd`) sits between them, free to be reordered by the hardware.

**Listing 60** — code from the talk (Bonus: RISCV Disassembly), shown at [56:05](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=3365s)

```asm
IncsharedXtimeAquire():
        li      a4,9998336
        addi    a4,a4,1664
        lui     a3,%hi(flag)
        lui     a2,%hi(shared_val)
        li      a1,1
.L27:
        addi    a0,a3,%lo(flag)
        1: lr.w.aq a5,0(a0); bne a5,zero,1f; sc.w.aq a6,a1,0(a0); bnez a6,1b; 1:
        sext.w  a5,a5
        bnez    a5,.L27
        ld      a5,%lo(shared_val)(a2)
        addi    a5,a5,1
        sd      a5,%lo(shared_val)(a2)
        addi    a5,a3,%lo(flag)
        fence iorw,ow; amoswap.w zero,zero,0(a5)
        addiw   a4,a4,-1
        bnez    a4,.L27
        ret
```

The acquire version uses `lr.w.aq` and `sc.w.aq`, meaning load-reserved and store-conditional **with acquire**. The release store becomes `fence iorw,ow` followed by `amoswap.w`: the fence keeps all earlier memory operations before the store that frees the lock. "You can see from this disassembly exactly what the compiler did and why this program works and the other doesn't."

### The correct versions

The fixed spinlock is the slide's code with acquire and release. If all you need is a counter, the simplest correct and fast choice is a `std::atomic` counter with `fetch_add`:

**Listing 61** — The relaxed-ordering bug, fixed two ways *(illustrative example, not from the talk)*

```cpp
#include <atomic>
#include <cstdint>
#include <iostream>
#include <thread>

std::atomic<int> flag{0};
std::uint64_t shared_val{};

void IncsharedXtimeAquire()
{
    for (int count{0}; count < 10000000;) {
        int expected{};
        if (flag.compare_exchange_strong(expected, 1, std::memory_order_acquire)) {
            shared_val++;                                   // protected by the lock
            count++;
            flag.store(0, std::memory_order_release);
        }
    }
}

std::atomic<std::uint64_t> counter{0};
void IncAtomic() { for (int i = 0; i < 10000000; ++i) counter.fetch_add(1, std::memory_order_relaxed); }

int main()
{
    { std::jthread a(IncsharedXtimeAquire), b(IncsharedXtimeAquire); }
    { std::jthread a(IncAtomic), b(IncAtomic); }
    std::cout << shared_val << ' ' << counter << '\n';      // 20000000 20000000
}
```

## 18. Questions

*Talk segment: [56:23](https://www.youtube.com/watch?v=SVEYNEWZLo4&t=3383s)*

"So thank you, everyone. We have three minutes for questions."

### volatile and hardware

*"Sometimes we use the volatile keyword if we have a register that we don't want the value cached, and we want to always be reading from a hardware-associated register, … maybe shared-memory buffers between two different processors. Did you say it's being deprecated, and what would be a better method?"*

"Most of the usages are being deprecated, but using `volatile` for something that you don't want the compiler to reorder, that's fine. … Between two threads, it will not help you." Within one thread, the as-if rule guarantees volatile accesses happen as written. It is also still needed for `setjmp`/`longjmp`. For memory shared with another processor or device, `volatile` keeps the compiler honest, but ordering against the other side usually also needs hardware fences or the platform's I/O primitives.

### stop_source versus an atomic flag

*"For stop source, is it fundamentally different than just using an atomic that you would pass in, or is it just an abstraction?"*

"It's an abstraction layer that we got in C++20, so it's not something that special, but you have it in the language, so use it. … You don't need to implement it by yourself." It does offer more than a flag: `std::stop_callback` lets you run code when a stop is requested, `jthread` wires it up automatically, and `condition_variable_any` can wait on it.

### Choosing the right tool

The talk covered a lot of tools. Here is a compact guide, roughly from simplest to most specialised:

| You need to… | Use |
|---|---|
| Run work concurrently and join | `std::jthread` |
| Get a result back | `std::async`, or `std::promise`/`std::future`, `std::packaged_task` |
| Protect shared data | `std::mutex` with `std::lock_guard` / `std::scoped_lock` |
| Readers and writers | `std::shared_mutex` with `std::shared_lock` |
| Wait for a condition | `std::condition_variable(_any)` |
| Signal between threads | `std::binary_semaphore` / `std::counting_semaphore` |
| Wait for N events, once | `std::latch` |
| Repeated phases | `std::barrier` |
| Initialise once | function-local `static`, or `std::call_once` |
| Cancel cooperatively | `std::stop_token` |
| Per-thread state | `thread_local` (and don't pass its address around) |
| A single shared counter or flag | `std::atomic` (default `seq_cst` unless you can justify weaker) |

The deck also has a reader/writer example that wasn't shown in the video. It pairs `std::shared_mutex` with `std::shared_lock` so that several readers can hold the lock at once. A small version of the same idea:

**Listing 62** — Readers and writers with std::shared_mutex *(illustrative example, not from the talk)*

```cpp
#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <vector>

int main()
{
    std::shared_mutex m;
    int config = 0;

    auto reader = [&](int id) {
        for (int i = 0; i < 3; ++i) {
            std::shared_lock lock(m);            // many readers at once
            std::cout << "reader " << id << " sees " << config << '\n';
        }
    };
    auto writer = [&] {
        for (int i = 1; i <= 3; ++i) {
            std::unique_lock lock(m);            // exclusive
            config = i;
        }
    };

    std::vector<std::jthread> ts;
    ts.emplace_back(writer);
    for (int id = 0; id < 3; ++id) ts.emplace_back(reader, id);
}
```

## Key takeaways

- Compilers and CPUs reorder, merge and delete operations whenever a single thread cannot tell the difference: that is the as-if rule.
- Another thread can tell the difference, so concurrent code needs synchronisation that creates happens-before relationships.
- C++ promises sequential consistency only for data-race-free programs (SC-DRF); a data race is undefined behaviour.
- volatile prevents some compiler optimisations but provides neither atomicity nor ordering between threads.
- Acquire (on loads) keeps later operations after it; release (on stores) keeps earlier operations before it; together they pass data safely between threads.
- std::atomic defaults to seq_cst, which is simplest; weaker orders are faster but must be justified.
- C++20 adds jthread, stop tokens, semaphores, latches and barriers, which replace many hand-written synchronisation patterns.
- A lock built with relaxed operations does not protect anything: the compiler may legally keep the protected data in registers.

## Glossary

| Term | Meaning |
|---|---|
| As-if rule | The compiler may transform a program in any way that does not change its observable behaviour. |
| Out-of-order execution | A CPU technique that executes instructions as soon as their inputs are ready and retires them in program order. |
| Data race | Two threads access the same memory location, at least one writes, at least one access is non-atomic, and neither happens before the other. Undefined behaviour in C++. |
| Sequential consistency | Every execution behaves as if all threads' operations were interleaved in one global order that respects each thread's program order. |
| SC-DRF | Sequential consistency for data-race-free programs: the guarantee C++ (and most languages) provide. |
| Happens-before | The ordering relation, created by program order and synchronisation, that determines which writes a read must see. |
| Acquire | An ordering on loads (and RMWs): no later memory operation in the thread may be moved before it. |
| Release | An ordering on stores (and RMWs): no earlier memory operation in the thread may be moved after it. |
| memory_order_relaxed | Atomicity only; no ordering with respect to other memory operations. |
| Fence | std::atomic_thread_fence: imposes ordering without being tied to a particular atomic variable. |
| Spinlock | A lock that busy-waits by repeatedly trying an atomic operation until it succeeds. |
| thread_local | Storage duration in which each thread has its own instance of the variable. |
| Latch / barrier | C++20 one-shot (latch) and reusable (barrier) synchronisation points for a group of threads. |
| Stop token | C++20 cooperative cancellation: a stop_source requests stop, threads poll a stop_token. |

## Exercises

1. Rewrite the two-flag 'mutex for poor people' with std::atomic<bool> and explain which memory order makes it correct and why memory_order_relaxed does not.

   <details><summary>Hint</summary>Each thread's store must be ordered before its subsequent load of the other flag: that needs seq_cst.</details>

2. Why is the 'Volatile: Bad Example' program a data race even on x86? Fix it in two different ways.

   <details><summary>Hint</summary>add [mem],1 without lock is not atomic across cores; use std::atomic<int> or a mutex.</details>

3. In the message-passing example with a = 42, explain exactly which happens-before edges guarantee that r2 == 42 when r1 == 1.

   <details><summary>Hint</summary>Sequenced-before in thread 1, synchronizes-with from the release store to the acquire load, sequenced-before in thread 2.</details>

4. Predict the output of the thread_local abuse example if each thread counted to 10 million instead of 10 and 20. Why?

   <details><summary>Hint</summary>Both threads update main's inc without synchronisation.</details>

5. What does 'Futures and Promises: another example' print? Show the arithmetic.

   <details><summary>Hint</summary>sum of 0..499 is 124750.</details>

6. Change the barrier example so that the completion function prints the phase number instead of using a static counter.

   <details><summary>Hint</summary>Capture a counter by reference in the completion function; it runs once per phase.</details>

7. Explain why the relaxed spinlock result can be close to 10,000,000 rather than some random number between 10M and 20M.

   <details><summary>Hint</summary>Each thread keeps its own register copy of shared_val and writes it back.</details>

## Further reading

- JF Bastien, "Deprecating volatile", CppCon 2019
- Timur Doumler, "A Lock-Free Atomic shared_ptr", CppCon 2022
- Daniel Anderson, "Lock-free Atomic Shared Pointers Without a Split Reference Count? It Can Be Done!", CppCon 2023
- cppreference: std::memory_order, std::atomic_thread_fence, std::latch, std::barrier, std::counting_semaphore, std::jthread, std::stop_token
- Slides: CppCon2023 repository, Presentations/Cpp_Memory_Model.pdf
- Alex Dathskovsky, https://www.cppnext.com
