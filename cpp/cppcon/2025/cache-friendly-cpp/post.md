---
title: "Cache-Friendly C++"
date: 2026-11-15
tags: ["C++", "performance", "CPU cache", "cache lines", "prefetching", "memory layout", "alignment", "strict aliasing", "data-oriented design", "false sharing", "std::vector", "CppCon"]
source: "https://www.youtube.com/watch?v=g_X5g3xw43Q"
video_title: "Cache-Friendly C++ - Jonathan Müller - CppCon 2025"
channel: "CppCon"
---

> This chapter is based on the talk [Cache-Friendly C++ - Jonathan Müller - CppCon 2025](https://www.youtube.com/watch?v=g_X5g3xw43Q) (CppCon). Code listings marked *from the talk* are transcribed from the slides; listings marked *illustrative* were written for this chapter to explain the ideas.

"When you need a container, pick `std::vector<T>` by default." Most C++ programmers have heard this advice, and most have seen a benchmark in which a plain vector beats a container with a better big-O complexity. The usual explanation is that `std::vector` is *cache-friendly*. This chapter explains, from the ground up, what that phrase means and how to write code that deserves it.

It follows Jonathan Müller's CppCon 2025 talk *Cache-Friendly C++*. Müller works at think-cell, where he maintains the company's core C++ libraries, and chairs the ranges work in the C++ standards committee. The talk starts from a puzzling benchmark and builds the hardware model needed to explain it. Main memory is about a hundred times slower than the CPU, so CPUs keep small, fast caches. Caches move data in 64-128 byte *cache lines*, and a *prefetcher* guesses which lines you will need next. The talk then turns each fact into programming advice: make types smaller, order struct members to avoid padding, prefer contiguous containers, remember that code is cached too, organise data around the algorithms that use it (data-oriented design), and keep data written by different threads on different cache lines.

The speaker is open about one thing, and so is this chapter. His benchmarks were *deliberately* written and run on machines (his laptop and an Apple M1) where they show the effect he wanted to demonstrate. The effects are real, but the numbers are not a substitute for measuring your own code on your own hardware. The talk's code is taken from the published slides, and the chapter adds runnable examples that let you reproduce the effects yourself.

## What you will learn

- Explain why main memory access is slow relative to computation and how a hierarchy of CPU caches hides that latency
- Read a memory-access benchmark and relate the drops in throughput to L1, L2 and main-memory sizes
- Distinguish write-through and write-back caches
- Shrink data with smaller primitive types, enum underlying types, bitfields, member reordering and pointer tag bits, and know when each costs access speed
- Explain why char, signed char, unsigned char and std::byte can make generic loops slower through strict aliasing, and how to avoid it
- Describe cache lines and hardware prefetching, and why sequential access is fast regardless of data size
- Rank standard containers by cache-friendliness from their memory layout
- Recognise instruction-cache effects and the code-side rules (avoid long branches, indirect jumps, big hot loops)
- Apply data-oriented design: structure of arrays, functions over N elements, multiple homogeneous arrays instead of polymorphism
- Diagnose and fix false sharing in a parallel fold

## Before you start

- Comfortable reading modern C++ (templates, lambdas, std::vector, enum class; a requires-clause at reading level)
- Basic notions of big-O complexity
- For the last section: basic threads (std::thread or a thread pool)
- Benchmarks in the talk use Google Benchmark; the chapter introduces the parts it uses

## Contents

1. [A set benchmark that defies big-O](#1-a-set-benchmark-that-defies-big-o) — [0:00](https://www.youtube.com/watch?v=g_X5g3xw43Q&t=0s)
2. [Measuring random memory access](#2-measuring-random-memory-access) — [4:01](https://www.youtube.com/watch?v=g_X5g3xw43Q&t=241s)
3. [CPU caches and the cache hierarchy](#3-cpu-caches-and-the-cache-hierarchy) — [6:02](https://www.youtube.com/watch?v=g_X5g3xw43Q&t=362s)
4. [Writing to cached memory](#4-writing-to-cached-memory) — [10:00](https://www.youtube.com/watch?v=g_X5g3xw43Q&t=600s)
5. [Smaller types: the average-age benchmark](#5-smaller-types-the-average-age-benchmark) — [11:09](https://www.youtube.com/watch?v=g_X5g3xw43Q&t=669s)
6. [Five one-byte types and strict aliasing](#6-five-one-byte-types-and-strict-aliasing) — [13:00](https://www.youtube.com/watch?v=g_X5g3xw43Q&t=780s)
7. [Bitfields](#7-bitfields) — [17:00](https://www.youtube.com/watch?v=g_X5g3xw43Q&t=1020s)
8. [Alignment, padding and reordering members](#8-alignment-padding-and-reordering-members) — [18:00](https://www.youtube.com/watch?v=g_X5g3xw43Q&t=1080s)
9. [Spare bits in pointers](#9-spare-bits-in-pointers) — [20:30](https://www.youtube.com/watch?v=g_X5g3xw43Q&t=1230s)
10. [Sequential access and the prefetcher](#10-sequential-access-and-the-prefetcher) — [23:00](https://www.youtube.com/watch?v=g_X5g3xw43Q&t=1380s)
11. [Cache lines](#11-cache-lines) — [24:20](https://www.youtube.com/watch?v=g_X5g3xw43Q&t=1460s)
12. [Cache-friendly containers](#12-cache-friendly-containers) — [26:40](https://www.youtube.com/watch?v=g_X5g3xw43Q&t=1600s)
13. [Instruction caches: code is data too](#13-instruction-caches-code-is-data-too) — [31:30](https://www.youtube.com/watch?v=g_X5g3xw43Q&t=1890s)
14. [Data-oriented design: records of people](#14-data-oriented-design-records-of-people) — [36:00](https://www.youtube.com/watch?v=g_X5g3xw43Q&t=2160s)
15. [Data-oriented design: heterogeneous shapes](#15-data-oriented-design-heterogeneous-shapes) — [40:40](https://www.youtube.com/watch?v=g_X5g3xw43Q&t=2440s)
16. [False sharing](#16-false-sharing) — [44:50](https://www.youtube.com/watch?v=g_X5g3xw43Q&t=2690s)
17. [Conclusion and audience questions](#17-conclusion-and-audience-questions) — [51:00](https://www.youtube.com/watch?v=g_X5g3xw43Q&t=3060s)

## 1. A set benchmark that defies big-O

*Talk segment: [0:00](https://www.youtube.com/watch?v=g_X5g3xw43Q&t=0s)*

### A word of warning about benchmarks

Before showing a single number, the speaker opens with a disclaimer that is worth repeating at the start of any chapter about performance: **this is not a lesson in how to benchmark properly.** He explains that he worked "the entirely wrong way around". He knew which effects he wanted to demonstrate and kept rewriting the benchmarks until they showed them. He also picked the machines on purpose: some results come from his laptop, others from an Apple M1, depending on where an effect was most visible.

> The effects are real and the results are true, but each benchmark was chosen to show its effect. Always benchmark on your own target hardware before making an optimization decision.

Keep this in mind throughout the chapter. Every chart describes a *mechanism* that exists on modern CPUs. How large the effect is in your program depends on your CPU, compiler, data sizes and access pattern.

### Four ways to write a set

The motivating problem is an everyday one: you need a *set* of integers, a collection in which you insert values and later ask "is `x` in here?". There are four reasonable ways to write one in C++:

| Implementation | Lookup | Insert one | Insert n at once |
|---|---|---|---|
| `std::set` (balanced binary search tree) | O(log n), walk down the tree | O(log n) | O(n log n), n insertions |
| `std::unordered_set` (hash table) | O(1), hash to a bucket | O(1) amortised | O(n) |
| unsorted `std::vector` | O(n), linear search | O(1), `push_back` | O(n) |
| sorted `std::vector` | O(log n), binary search | O(n): find the position, shift the rest | O(n log n): append everything, then `std::sort` |

The speaker notes that the sorted-vector bulk insert can be done even better: sort only the new elements, then merge them with the old ones, for example with `std::inplace_merge`. That refinement is not needed for this benchmark.

### What big-O predicts

The benchmark is simple:

1. **Setup:** fill the container with N random integers.
2. **Lookup:** repeatedly look up a random integer and measure the time per lookup.

Reading only the complexity column, you would expect this ranking:

1. `std::unordered_set` is fastest: O(1).
2. `std::set` and the sorted vector come next: O(log n).
3. The unsorted vector is slowest: O(n), and it gets worse linearly.

Recall what big-O notation actually says. It describes how the cost *grows* as n grows and deliberately drops constant factors. An O(1) operation that costs 15 ns is "better" than an O(log n) operation that costs 7 ns, in the sense that for *some* large enough n the O(1) one wins. Big-O says nothing about which one wins for the n you actually have.

### What was measured

The measured lookup times (in nanoseconds, for N from 64 to 1024) only partly match that prediction:

- The unsorted vector really does grow linearly, from about 5 ns at N = 64 to over 50 ns at N = 1024. Big-O is right about its *shape*.
- At **N = 64, the unsorted vector, the "worst" container, is the fastest of all four.** A linear search through 64 integers beats a tree walk and a hash lookup.
- The **sorted vector consistently beats `std::unordered_set`**, even though O(log n) is supposed to lose to O(1). (The slide's title says `std::unordered_map`, but the benchmarked container is `std::unordered_set`.)

So the container with the best complexity loses to a plain array with a binary search. The rest of the talk explains why. The short answer, in the speaker's words, "lies in CPU caches".

### Try it yourself

The example below reproduces the shape of the experiment without any benchmarking library. It uses `std::chrono`, sums the results so the compiler cannot discard the lookups, and prints nanoseconds per lookup for each container. The absolute numbers will differ on your machine. What to look for is the *relative* order, and in particular how close the sorted vector comes to, or how far it beats, `std::unordered_set`.

**Listing 1** — Reproducing the set-lookup comparison with std::chrono (C++20) *(illustrative example, not from the talk)*

```cpp
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <random>
#include <set>
#include <unordered_set>
#include <vector>

volatile std::size_t sink;

template <typename Lookup>
double ns_per_lookup(const std::vector<int>& queries, Lookup lookup)
{
    auto start = std::chrono::steady_clock::now();
    std::size_t hits = 0;
    for (int rounds = 0; rounds < 200; ++rounds)
        for (int q : queries)
            hits += lookup(q);
    auto stop = std::chrono::steady_clock::now();
    sink = hits;
    return std::chrono::duration<double, std::nano>(stop - start).count()
           / (200.0 * queries.size());
}

int main()
{
    std::mt19937 engine(42);
    std::uniform_int_distribution<int> dist(0, 1 << 20);

    std::printf("%6s %10s %14s %10s %10s\n", "N", "set", "unordered_set", "unsorted", "sorted");
    for (int n : {64, 256, 1024}) {
        std::vector<int> values(n);
        for (int& v : values) v = dist(engine);

        std::set<int> tree(values.begin(), values.end());
        std::unordered_set<int> hash(values.begin(), values.end());
        std::vector<int> unsorted = values;
        std::vector<int> sorted = values;
        std::ranges::sort(sorted);

        // half the queries hit, half (almost certainly) miss
        std::vector<int> queries(10'000);
        for (std::size_t i = 0; i < queries.size(); ++i)
            queries[i] = i % 2 ? values[engine() % n] : dist(engine);

        std::printf("%6d %10.1f %14.1f %10.1f %10.1f\n", n,
            ns_per_lookup(queries, [&](int q) { return tree.contains(q); }),
            ns_per_lookup(queries, [&](int q) { return hash.contains(q); }),
            ns_per_lookup(queries, [&](int q) { return std::ranges::find(unsorted, q) != unsorted.end(); }),
            ns_per_lookup(queries, [&](int q) { return std::ranges::binary_search(sorted, q); }));
    }
}
```

On the x86-64 machine used to test this chapter (GCC 16, `-O2`), the output was:

| N | `std::set` | `std::unordered_set` | unsorted | sorted |
|---|---|---|---|---|
| 64 | 24.0 ns | 12.2 ns | 32.7 ns | 28.4 ns |
| 256 | 35.1 ns | 19.5 ns | 109.7 ns | 35.1 ns |
| 1024 | 51.6 ns | 19.5 ns | 419.5 ns | 44.0 ns |

Here, unlike on the speaker's machine, `std::unordered_set` wins at every size, and the unsorted vector is not fastest even at 64. Half of these queries miss, and a miss forces the linear search to scan all 64 elements. The sorted vector still beats `std::set` as N grows, and its time grows far more slowly than the tree's. This is the speaker's disclaimer in action: the *mechanisms* are the same everywhere, but which container wins depends on the hardware, the data and the queries.

A few details deserve comment. `std::ranges::binary_search` on the sorted vector performs exactly the O(log n) search from the table. `std::ranges::find` is the linear search. The `volatile` sink and the accumulated `hits` stop the optimizer from removing lookups whose results are never used, which is the job `benchmark::DoNotOptimize` does in the talk's code (introduced in the next section).

## 2. Measuring random memory access

*Talk segment: [4:01](https://www.youtube.com/watch?v=g_X5g3xw43Q&t=241s)*

### Background: reading a Google Benchmark function

Every benchmark in the talk is written with [Google Benchmark](https://github.com/google/benchmark), a widely used open-source C++ micro-benchmarking library. You need only a handful of its pieces to read the slides:

| Piece | Meaning |
|---|---|
| `void bm(benchmark::State& state)` | A benchmark is a function that takes a `State`. You register it with `BENCHMARK(bm)`, optionally with arguments: `->Arg(16)`, `->Range(16, 10240)`. |
| `for (auto _ : state) { ... }` | The *timed loop*. The library decides how many iterations to run to get a stable measurement. Code before the loop is setup and is not timed. |
| `state.range(0)` | The first argument the benchmark was registered with, here the data size in KB. |
| `benchmark::DoNotOptimize(expr)` | Forces the compiler to assume the value of `expr` is used, so it cannot delete the computation as dead code. |
| `state.SetBytesProcessed(n)` | Tells the library how many bytes the run processed, so it reports throughput (bytes per second) next to the time. |
| `state.PauseTiming()` / `state.ResumeTiming()` | Exclude part of an iteration from the measurement. |

The slides also use `stdr::` as a short namespace alias, the usual `namespace stdr = std::ranges;`.

### How fast is random memory access?

To investigate the mystery from the previous section, the speaker steps back and asks a more basic question: how long does it take to touch one random location in memory?

**Listing 2** — code from the talk (How fast is random memory access?), shown at [4:55](https://www.youtube.com/watch?v=g_X5g3xw43Q&t=295s)

```cpp
void bm_memory_access(benchmark::State& state)
{
    std::default_random_engine engine;
    auto bytes = state.range(0) * 1024;
    std::vector<unsigned> data(bytes / sizeof(unsigned));
    stdr::generate(data, engine);
    std::uniform_int_distribution<std::size_t> dist(0, data.size() - 1);
    for (auto _ : state) {
        benchmark::DoNotOptimize(++data[dist(engine)]);
    }
    state.SetBytesProcessed(state.iterations() * data.size());
}
```

Line by line:

- `std::default_random_engine engine;` is a pseudo-random number generator. Default-constructed, it always produces the same sequence, which keeps runs repeatable.
- `auto bytes = state.range(0) * 1024;` The benchmark is run for different sizes. The argument is in kilobytes and converted to bytes.
- `std::vector<unsigned> data(bytes / sizeof(unsigned));` allocates that many bytes' worth of `unsigned` values.
- `stdr::generate(data, engine);` fills the buffer with random numbers. `std::ranges::generate` calls the generator once per element, and an engine is callable, so `engine` itself works as the generator.
- `std::uniform_int_distribution<std::size_t> dist(0, data.size() - 1);` produces uniformly distributed indices into the buffer. The bounds are inclusive, hence `- 1`.
- Inside the timed loop, `++data[dist(engine)]` picks a random index and increments that element: one random read and one write. `DoNotOptimize` keeps the compiler from proving the increments are useless.
- `state.SetBytesProcessed(state.iterations() * data.size());` reports a throughput figure. Strictly, each iteration touches a single `unsigned`, so the absolute "MB per s" values are a relative score, not literally the bytes moved. Only the *trend* across sizes matters.

Every iteration does the same amount of *work*: generate one random number, increment one integer. If memory were a uniform array where every address costs the same to reach, the result would not depend on the size of `data` at all.

### The result: memory gets slower the more of it you use

On the Apple M1, the measured throughput falls into three regions:

| Data size | Throughput (MB/s, approx.) | Behaviour |
|---|---|---|
| 16-64 KB | ~94 | flat |
| 80-96 KB to ~2 MB | ~85 down to ~77 | gentle decline |
| 3-10 MB | ~70 down to ~33 | steep drop that flattens out |

So "memory access" does not have one cost. The same instruction, `++data[i]`, becomes about three times slower once the buffer passes a few megabytes. The sizes where the curve bends are not random. As the next section shows, they line up with the sizes of the CPU's caches.

### Try it without a benchmarking library

The next example runs the same experiment with nothing but the standard library. It times a fixed number of random increments for buffers from 16 KB to 64 MB and prints nanoseconds per access. On most desktop CPUs you will see two or three plateaus with steps between them. Because it uses a fast inline random generator (a 64-bit linear congruential generator) instead of `std::uniform_int_distribution`, the memory effect dominates more clearly than in the talk's version.

**Listing 3** — Random increments over growing buffers, timed with std::chrono (C++20) *(illustrative example, not from the talk)*

```cpp
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <vector>

int main()
{
    constexpr std::size_t accesses = 20'000'000;
    for (std::size_t kb = 16; kb <= 64 * 1024; kb *= 2) {
        std::vector<unsigned> data(kb * 1024 / sizeof(unsigned), 1);
        std::uint64_t x = 88172645463325252ull;   // LCG state
        auto next_index = [&] {
            x = x * 6364136223846793005ull + 1442695040888963407ull;
            return (x >> 33) % data.size();
        };

        auto start = std::chrono::steady_clock::now();
        for (std::size_t i = 0; i < accesses; ++i)
            ++data[next_index()];
        auto stop = std::chrono::steady_clock::now();

        unsigned check = 0;
        for (unsigned d : data) check += d;   // use the data so the loop is kept
        double ns = std::chrono::duration<double, std::nano>(stop - start).count() / accesses;
        std::printf("%8zu KB  %6.2f ns/access  (check %u)\n", kb, ns, check);
    }
}
```

On the test machine for this chapter (x86-64, GCC 16, `-O2`) the time stayed near 7 ns per access up to 4 MB, then rose to 13 ns at 8 MB and about 23 ns at 32-64 MB. That is one large plateau while the data fits in the caches, and a climb once it no longer does.

## 3. CPU caches and the cache hierarchy

*Talk segment: [6:02](https://www.youtube.com/watch?v=g_X5g3xw43Q&t=362s)*

### A naive model of memory

Start with the simplest picture of a computer. On the left is the CPU: registers, an arithmetic logic unit (ALU) that computes, and a control unit (CU) that decodes instructions. On the right is RAM, a long row of numbered cells. The slide's program is three loads, `load R1, [2]`, then `load R2, [5]`, then `load R3, [2]`. In this model it would travel all the way to RAM three times: once for address 2, once for address 5, and again for address 2, even though it read that value a moment ago.

The speaker's comment on this model: if computers really worked like that, "we wouldn't be very happy, because main memory access is slow. It's like really, really slow."

### How slow is "slow"?

The talk puts numbers on it using the Apple M1 (2020):

- **RAM bandwidth:** about 68.3 GB/s.
- **Compute:** about 2.6 TFLOPS. If each operation consumed fresh data from memory, that would correspond to roughly 10,650 GB/s.

> Main memory is about a hundred times slower than compute. If every operation had to go to RAM, the CPU would spend 99% of its time waiting.

The gap has two parts. *Bandwidth* is how many bytes per second can flow. *Latency* is how long one request takes to come back, on the order of 100 ns for DRAM, which is hundreds of CPU cycles. Both are much worse than what the cores could consume.

### The solution: a cache

The fix is to put a small amount of much faster memory *physically close* to the CPU. Close matters: a signal crossing a few millimetres of silicon arrives much sooner than one travelling across the motherboard to a DRAM module, and the cache is built from faster (and much more expensive) SRAM cells. This memory serves as a **cache**: it keeps copies of data from RAM.

Replay the three loads with a cache in the picture:

1. `load R1, [2]`: address 2 is not in the cache. This is a **cache miss**. The value is fetched from RAM, *a copy is kept in the cache*, and it goes on to the register.
2. `load R2, [5]`: another miss, another fetch, another copy kept.
3. `load R3, [2]`: address 2 *is* in the cache. This is a **cache hit**: the value comes straight from the cache and RAM is not touched.

Programs reuse data all the time (loop counters, the top of the stack, the object you are working on), so even a small cache removes a large share of RAM traffic.

### Unfortunately, caches are small

The catch is in the word *small*. Suppose the cache is already full and the program asks for address 1, which is not cached. Something must be **evicted** (thrown out) to make room. If the evicted entry happens to be the one you need next, address 5 for example, the next load is a miss again, which evicts something else, which might be needed after that.

Which entry to evict is decided by a *replacement policy*. Real CPUs use cheap approximations of "least recently used". No policy can save a program whose working data simply does not fit.

### A hierarchy of caches

Making one cache bigger makes it slower: more cells, longer wires, more complicated lookup. So CPUs use a **hierarchy** of caches, each level larger and slower than the one before it. The talk quotes these figures for the Apple M1 (from 7-cpu.com):

| Level | Size | Shared? | Access cost |
|---|---|---|---|
| L1 | 64-128 KB | per core | ~3 cycles |
| L2 | 8-12 MB | shared between cores | ~18 cycles |
| L3 (system level cache) | 8 MB | shared with the GPU | ~18 cycles + 10-15 ns |
| RAM | 16 GB (on the speaker's machine) | everything | ~18 cycles + 100 ns |

On an L1 miss, the CPU checks L2. On an L2 miss, it checks the next level. Only if every level misses does it pay for a trip to RAM.

### Explaining the benchmark

This model explains the three regions of the random-access chart from the previous section:

- **Blue region (up to ~64 KB): the data fits entirely in L1.** Every random access hits L1, so the size does not matter and the curve is flat.
- **Green region (~80 KB to ~2 MB): the data fits in L2 but not in L1.** Now only a fraction of the data is in L1 at any moment. The bigger the buffer, the smaller that fraction, so the probability that a random index hits L1 shrinks and more and more accesses pay the L2 price. The curve slopes down gently.
- **Orange region (beyond ~2-3 MB): the data exceeds the caches.** The same reasoning now applies to L2: the chance of an L2 hit drops as the buffer grows, and more accesses go to RAM. The curve falls steeply, then flattens.

If you kept growing the buffer, the curve would approach the speed of main memory: almost every random access would miss in every level.

> The cost of an access is not a property of the instruction. It depends on where the data currently lives, and that depends on how much other data you have been touching.

### Watching hit rates fall in a simulation

Real caches are too fast to watch directly, but a simulation shows the same mechanism. The example models a tiny fully associative cache of 512 entries with least-recently-used (LRU) eviction and feeds it random accesses over growing "memory" sizes. While the data fits, every access after warm-up hits. Once it no longer fits, the hit rate drops towards *cache size / data size*, which is exactly the gradual decline in the green and orange regions.

**Listing 4** — Simulating an LRU cache under random access (C++20) *(illustrative example, not from the talk)*

```cpp
#include <cstdio>
#include <list>
#include <random>
#include <unordered_map>

class LruCache {
    std::size_t capacity_;
    std::list<std::size_t> order_;   // front = most recently used
    std::unordered_map<std::size_t, std::list<std::size_t>::iterator> where_;

public:
    explicit LruCache(std::size_t capacity) : capacity_(capacity) {}

    bool access(std::size_t address)   // returns true on a hit
    {
        if (auto it = where_.find(address); it != where_.end()) {
            order_.splice(order_.begin(), order_, it->second);   // move to front
            return true;
        }
        if (order_.size() == capacity_) {   // evict least recently used
            where_.erase(order_.back());
            order_.pop_back();
        }
        order_.push_front(address);
        where_[address] = order_.begin();
        return false;
    }
};

int main()
{
    constexpr std::size_t cache_slots = 512;
    std::mt19937 engine(1);
    for (std::size_t memory = 128; memory <= 16384; memory *= 2) {
        LruCache cache(cache_slots);
        std::uniform_int_distribution<std::size_t> dist(0, memory - 1);
        for (int i = 0; i < 100'000; ++i) cache.access(dist(engine));   // warm up
        int hits = 0;
        for (int i = 0; i < 1'000'000; ++i) hits += cache.access(dist(engine));
        std::printf("data %6zu slots: hit rate %5.1f%%\n", memory, hits / 10'000.0);
    }
}
```

Expected output: a hit rate of essentially 100% for 128, 256 and 512 slots, then about 50% at 1024, 25% at 2048, 12.5% at 4096 and so on. The ratio halves each time the data doubles.

## 4. Writing to cached memory

*Talk segment: [10:00](https://www.youtube.com/watch?v=g_X5g3xw43Q&t=600s)*

### What happens when a cached address is written?

So far every access was a read. A write raises a new question: once a value has been copied into the cache, there are two copies of it, one in the cache and one in RAM. When the program writes, which copy is updated? The talk names the two classic answers.

| Policy | On a write | Cost | Consequence |
|---|---|---|---|
| **Write-through** | update the cache *and* main memory | every write is a RAM write | RAM is always up to date |
| **Write-back** | update only the cache and mark the entry *dirty*; write it to RAM when it is evicted | repeated writes to the same address cost one RAM write | RAM can be stale while the cache holds the newest value |

Write-through is simpler to reason about, but it pays for an extra memory access on every store, which defeats much of the purpose of a cache for code that writes a lot, such as the `++data[i]` benchmark. Write-back is smarter: a loop that increments a counter a million times produces at most one write to RAM, when that cache entry finally leaves.

> CPU caches are usually write-back.

The speaker adds that this choice "will have fun consequences once we talk about multiple threads". If each core has its own cache and that cache may hold a newer value than RAM, cores must coordinate to avoid reading stale data. That coordination is the source of the *false sharing* problem at the end of the chapter.

### Takeaway: make your data fit

The first part of the talk ends with three facts and one rule:

- Main memory access is slow (about 100 times slower than compute on the M1).
- Therefore main memory access is avoided using faster caches.
- But caches are small.

> **Ensure as much data as possible fits into the cache!**

Everything in the next few sections applies this rule. If the data your hot loop touches is smaller, a larger share of it stays in L1 and L2, and fewer accesses pay the trip to RAM. The next question is how to make data smaller without changing what the program computes.

## 5. Smaller types: the average-age benchmark

*Talk segment: [11:09](https://www.youtube.com/watch?v=g_X5g3xw43Q&t=669s)*

### Computing the average age

The first example of "efficiently using cache space" is deliberately ordinary: given the ages of a group of people, compute the average.

**Listing 5** — code from the talk (Computing the average age of a group of people), shown at [11:29](https://www.youtube.com/watch?v=g_X5g3xw43Q&t=689s)

```cpp
enum class Age : int {};

Age average_age(const std::vector<Age>& ages)
{
    int total = 0;
    for (Age age : ages) {
        total += static_cast<int>(age);
    }
    return static_cast<Age>(total / ages.size());
}
```

Walking through it:

- `enum class Age : int {};` declares a **strongly typed integer**. A scoped enumeration with no enumerators is a well-known C++ idiom for a "strong typedef": `Age` holds any value its underlying type `int` can hold, but it does not convert implicitly to or from `int`. You cannot accidentally add an `Age` to a byte count or pass a `Weight` where an `Age` is expected. The text after the colon is the enumeration's **underlying type**, the integer type that stores it.
- `int total = 0;` The running sum is a plain `int`, whatever the size of `Age`. That matters later: shrinking `Age` does not risk overflowing the sum.
- `total += static_cast<int>(age);` Because `Age` is an `enum class`, getting at the number needs an explicit `static_cast`.
- `return static_cast<Age>(total / ages.size());` divides by the count and converts back. Note that `ages.size()` is a `std::size_t`, so the division happens in unsigned arithmetic. That is fine for non-negative ages, and the slide's code assumes them.

The loop reads every age exactly once, in order. There is nothing clever to optimize in the algorithm. What can change is how many *bytes* the loop has to pull through the cache hierarchy.

### Make the type smaller

Ages of real people fit comfortably in a byte, yet `int` spends four bytes on each:

| Declaration | Size | Range |
|---|---|---|
| `enum class Age : int` | 4 bytes | about −2 billion to +2 billion |
| `enum class Age : short` | 2 bytes | −32,768 to +32,767 |
| `enum class Age : uint8_t` | 1 byte | 0 to 255 |

Changing the underlying type is a one-word edit. Nothing else in `average_age` has to change. With `short`, twice as many ages fit in the same amount of cache; with `uint8_t`, four times as many.

### The result: smaller is faster, mostly

The benchmark averages random ages between 0 and 100, for 16k to 128k ages. Measured in billions of items per second:

| Underlying type | Throughput |
|---|---|
| `int` | ~8 |
| `short` | ~14.5 |
| `uint8_t` | ~11 |

`short` beats `int` by almost a factor of two, simply because more of the data fits in the cache and less memory traffic is needed. But `uint8_t`, which packs the data even tighter, lands *between* the two. The slide's explanation is that the smaller size of `uint8_t` is offset by slower byte operations. On this CPU, with this compiler, the arithmetic on single bytes (widening each byte before adding it to the `int` total) turned out more expensive than the arithmetic on 16-bit values.

The speaker draws the lesson that recurs through the talk:

> **Always measure before and after optimizations!** Improving one metric (bytes per element) can cost you on another (instructions per element).

### Rules of thumb for primitive types

Even with that caveat, the speaker recommends picking smaller types where the range allows it:

- `signed char` or `short` instead of `int` (the next section explains why he says `signed char` and not `char`);
- `float` instead of `double`, when the precision is enough;
- **specify the underlying type of enumerations.** An enumeration defaults to `int`, but you will not have billions of enumerators. Since you rarely do arithmetic on enumerators, a one-byte underlying type should cost nothing, and you save space, and therefore cache space.

### Measuring it yourself

The example runs the average-age loop for each underlying type over 16 MB of data (more than the L2 cache of most CPUs), repeating a few times and keeping the best time. On a typical x86-64 desktop with `-O2`, the smaller types are faster. Whether `uint8_t` or `short` wins depends on how well your compiler vectorizes the widening additions, which is exactly the speaker's point about measuring. On the x86-64 machine used to test this chapter, the result was the reverse of the speaker's for the last two: `int` 3.9, `short` 7.3 and `uint8_t` 8.7 billion items per second. There, the byte version was the fastest.

**Listing 6** — Average age with three underlying types (C++20) *(illustrative example, not from the talk)*

```cpp
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <random>
#include <vector>

template <typename Age>
Age average_age(const std::vector<Age>& ages)
{
    int total = 0;
    for (Age age : ages) {
        total += static_cast<int>(age);
    }
    return static_cast<Age>(total / ages.size());
}

template <typename Underlying>
void run(const char* name)
{
    enum class Age : Underlying {};
    constexpr std::size_t bytes = 16 << 20;
    std::vector<Age> ages(bytes / sizeof(Age));
    std::mt19937 engine(7);
    std::uniform_int_distribution<int> dist(0, 100);
    for (Age& a : ages) a = static_cast<Age>(dist(engine));

    double best = 1e9;
    int result = 0;
    for (int rep = 0; rep < 5; ++rep) {
        auto start = std::chrono::steady_clock::now();
        result = static_cast<int>(average_age(ages));
        auto stop = std::chrono::steady_clock::now();
        best = std::min(best, std::chrono::duration<double>(stop - start).count());
    }
    std::printf("%-8s sizeof=%zu  %6.2f billion items/s  (avg %d)\n",
                name, sizeof(Age), ages.size() / best / 1e9, result);
}

int main()
{
    run<int>("int");
    run<short>("short");
    run<std::uint8_t>("uint8_t");
}
```

One caution if you change the sizes: the total is an `int`, as on the slide. With 16 million one-byte ages averaging 50, the sum is about 800 million, comfortably below `INT_MAX` (about 2.1 billion). Four times as much data would overflow it, which is undefined behaviour for a signed integer. For real code that may see large inputs, accumulate in a `long long`.

## 6. Five one-byte types and strict aliasing

*Talk segment: [13:00](https://www.youtube.com/watch?v=g_X5g3xw43Q&t=780s)*

### Three, no, five one-byte types

The previous section recommended `signed char`, not `char`. The difference is deliberate. C++ has **three distinct** "plain" one-byte integer types:

- `signed char`: a signed 8-bit integer (what `int8_t` is on every mainstream platform);
- `unsigned char`: an unsigned 8-bit integer (`uint8_t`);
- `char`: the "character" type. Whether it is signed or unsigned is implementation-defined (signed on x86-64 Linux, unsigned on ARM Linux), but *either way it is a type of its own*.

**Listing 7** — code from the talk (Interlude: char vs. signed char), shown at [13:37](https://www.youtube.com/watch?v=g_X5g3xw43Q&t=817s)

```cpp
// both assertions hold!
static_assert(!std::is_same_v<char, signed char>);
static_assert(!std::is_same_v<char, unsigned char>);
```

Both assertions hold on every compiler. This is special to `char`. For the other integer types, `signed` is just a redundant spelling: `signed int` *is* `int`, and `signed short` *is* `short`, so nobody writes them. `signed char` is the only case where adding `signed` produces a different type from the unadorned name. The speaker admits he has "no idea why this is the case". It is a holdover from C, where `char` had to be allowed to be either, depending on the hardware.

"And when I said three, I obviously meant five." Modern C++ adds two more one-byte types:

- `char8_t` (C++20): a distinct unsigned 8-bit type meant for UTF-8 text, the element type of `u8"..."` literals and `std::u8string`;
- `std::byte` (C++17): an `enum class byte : unsigned char {}` for raw memory. It supports bitwise operators but deliberately no arithmetic.

All five are one byte. Surely they perform the same?

### Benchmarking the five types

The benchmark increments every element of a vector, generically:

**Listing 8** — code from the talk (Benchmarking one byte types), shown at [14:17](https://www.youtube.com/watch?v=g_X5g3xw43Q&t=857s)

```cpp
template <typename T> requires (sizeof(T) == 1)
void increment(std::vector<T>& data)
{
    for (std::size_t i = 0; i < data.size(); ++i)
        data[i] = static_cast<T>(int(data[i]) + 1);
}
```

- `template <typename T> requires (sizeof(T) == 1)` is a C++20 *requires-clause*: the template only accepts one-byte types.
- The loop walks the vector by index and calls `data.size()` in the loop condition, the way many people write such loops.
- `data[i] = static_cast<T>(int(data[i]) + 1);` looks clumsy, and the speaker says so ("I have to write the increment a bit ugly"). `std::byte` has no `operator++` and no `+`, so the value is converted to `int`, incremented, and converted back. For the four character types this compiles to exactly the same instruction as `++data[i]`.

Every type should produce the same machine code, so the speaker expected identical results. The measured throughput, in billions of items per second:

| Type | Throughput |
|---|---|
| `signed char`, `unsigned char`, `char`, `std::byte` | ~89 |
| `char8_t` | ~98 |

One type is about 10% faster than the others. "Does that mean that you should use the UTF-8 type to do one-byte arithmetic?" Not exactly. The reason has nothing to do with caches or with the instructions that do the arithmetic. It is the optimizer, and a rule called **strict aliasing**.

### Strict aliasing

Two pointers or references **alias** when they refer to the same memory. If the compiler cannot rule that out, every write through one of them might have changed what the other sees, so it must reload values from memory instead of keeping them in registers.

C++ restricts which types may be used to read or write an object. The slide quotes the current wording ([basic.lval]/11 in the C++26 draft):

> An object of dynamic type T_obj is *type-accessible* through a glvalue of type T_ref if T_ref is similar to: T_obj, a type that is the signed or unsigned type corresponding to T_obj, or a `char`, `unsigned char`, or `std::byte` type. If a program attempts to access the stored value of an object through a glvalue through which it is not type-accessible, the behavior is undefined.

In plain words: an `int` may be accessed as an `int` or `unsigned int`, and as raw bytes through `char`, `unsigned char` or `std::byte`, but not as a `float` or a `short`. The flip side is what makes the rule useful for optimization. **The compiler may assume that a write through a `float*` does not change any `int`.** Only the three byte types are allowed to alias *everything*.

C's rule (C11 6.5/7) is similar but less precise about bytes: it allows access through "a character type", which includes `signed char`. Compilers share their optimizers between C and C++, so in practice they treat `signed char` as able to alias anything too. That gives four aliasing types. `char8_t` is a separate type that is not on the list, so it aliases only `char8_t`.

### Why aliasing slows down this loop

The slide spells out what this means for `increment`:

- If `T` is **non-aliasing** (`char8_t`), writing `data[i]` can only modify other `T` objects.
- If `T` is **aliasing** (`char`, `signed char`, `unsigned char`, `std::byte`), writing `data[i]` may modify *arbitrary other objects*, "including `std::vector<T> data` (!)".

A `std::vector` is typically three pointers (begin, end, end of capacity) stored somewhere in memory, and `data.size()` computes end − begin from them. For `char8_t`, the optimizer can see that nothing in the loop body could change those pointers. It reads the size once, before the loop, keeps it in a register and vectorizes freely. This is called *hoisting* a loop-invariant load. For `unsigned char`, the store `data[i] = ...` might, as far as the compiler can prove, overwrite the vector's own end pointer. So after every store it must reload the vector's members and recompute the size. That reload is the 10%. You can confirm it in the assembly. Compiled at `-O2` with GCC 16, the `unsigned char` instantiation of the talk's `increment` from Listing 8 reloads the vector's begin and end pointers after every one-byte store and handles one byte per iteration. The `char8_t` instantiation loads them once and processes 16 bytes per SIMD instruction. Clang 20 shows the same split.

In the Q&A someone asked whether the compiler should know that writing past the array is undefined behaviour. The speaker's answer is that the optimizer does not know the *semantics* of `std::vector`. It only sees memory and pointers. Any write through a pointer that may alias anything forces it to assume anything in memory may have changed.

### The fix: hoist it yourself

You can still use the byte types; you just have to do the optimization by hand. The first fixed version copies the size into a local variable:

**Listing 9** — code from the talk (Benchmarking one byte types: Fixed), shown at [16:43](https://www.youtube.com/watch?v=g_X5g3xw43Q&t=1003s)

```cpp
template <typename T> requires (sizeof(T) == 1)
void increment(std::vector<T>& data)
{
    auto size = data.size();
    for (std::size_t i = 0; i < size; ++i)
        data[i] = static_cast<T>(int(value) + 1);
}
```

Because nobody takes the address of the local `size`, no store through any pointer can change it, and the optimizer is free to keep it in a register. Note a slip on the slide: the loop body reads `int(value)`, but no `value` is declared in this version (it comes from the next one). As shown, the code does not compile. It should read `int(data[i])`.

The second fix is simpler and idiomatic:

**Listing 10** — code from the talk (Benchmarking one byte types: Fixed), shown at [16:47](https://www.youtube.com/watch?v=g_X5g3xw43Q&t=1007s)

```cpp
template <typename T> requires (sizeof(T) == 1)
void increment(std::vector<T>& data)
{
    for (auto& value : data)
        value = static_cast<T>(int(value) + 1);
}
```

A range-based `for` loop is defined to call `begin()` and `end()` *once*, before the first iteration, and to compare iterators after that. The end of the range is fetched once by definition, so there is nothing for aliasing to spoil. With either fix, all five types run at the same ~98 billion items per second.

The speaker calls the situation "really unfortunate": C++ has no one-byte arithmetic type you can use "without having the optimizer mess with us". So be particularly careful with optimizations involving the one-byte types, and benchmark.

### Seeing the five types at compile time

The first example checks the type relationships discussed above. Every assertion holds on GCC and Clang.

**Listing 11** — The five one-byte types and their relationships (C++20) *(illustrative example, not from the talk)*

```cpp
#include <cstddef>
#include <cstdint>
#include <type_traits>

// three distinct character types...
static_assert(!std::is_same_v<char, signed char>);
static_assert(!std::is_same_v<char, unsigned char>);
// ...while 'signed' is redundant for every other integer type
static_assert(std::is_same_v<signed int, int>);
static_assert(std::is_same_v<signed short, short>);
// plus two more distinct one-byte types
static_assert(!std::is_same_v<char8_t, unsigned char>);
static_assert(std::is_unsigned_v<char8_t>);
static_assert(std::is_enum_v<std::byte>);
// the fixed-width aliases are the signed/unsigned char types
static_assert(std::is_same_v<std::int8_t, signed char>);
static_assert(std::is_same_v<std::uint8_t, unsigned char>);
static_assert(sizeof(char) == 1 && sizeof(char8_t) == 1 && sizeof(std::byte) == 1);

int main() {}
```

### Seeing the aliasing difference

The second example isolates the effect. `decrement_all` subtracts one from `n` elements and reads the count through a pointer each time round. With `T = char8_t`, a store to `p[i]` cannot change `*count`, so the compiler loads it once. With `T = unsigned char`, it must reload `*count` after every store. Compile with `g++ -O2 -std=c++20 -S` and compare the two instantiations: the `unsigned char` version reloads `*count` from memory inside the loop, while the `char8_t` version keeps it in a register.

The program also shows that the reload is *needed for correctness* when the pointers really do overlap. In the second call the bytes being decremented are the bytes of `count` itself, which is legal because `unsigned char` may alias an `int`. On a little-endian machine the first store turns `count` from 2 into 1, so the loop stops after one iteration. A compiler that had hoisted `*count` would wrongly run twice.

**Listing 12** — A store through an aliasing type forces a reload (C++20) *(illustrative example, not from the talk)*

```cpp
#include <cstdio>

template <typename T>
[[gnu::noinline]] int decrement_all(T* p, const int* count)
{
    int done = 0;
    for (int i = 0; i < *count; ++i) {   // is *count reloaded every iteration?
        p[i] = static_cast<T>(p[i] - 1);
        ++done;
    }
    return done;
}

int main()
{
    // Separate storage: an ordinary call.
    char8_t text[8] = {};
    int n = 8;
    int a = decrement_all(text, &n);

    // Overlapping storage, legal because unsigned char may alias any object:
    // the bytes being decremented are the bytes of count itself.
    int count = 2;
    int b = decrement_all(reinterpret_cast<unsigned char*>(&count), &count);

    std::printf("char8_t: %d iterations, unsigned char aliasing its own count: %d iteration\n", a, b);
}
```

On x86-64 and ARM64 the output is `char8_t: 8 iterations, unsigned char aliasing its own count: 1 iteration`. Calling the `char8_t` version with overlapping storage would be undefined behaviour, which is why the overlap is only demonstrated with `unsigned char`.

## 7. Bitfields

*Talk segment: [17:00](https://www.youtube.com/watch?v=g_X5g3xw43Q&t=1020s)*

### Going below one byte

After choosing the smallest suitable primitive type, the next step is to stop using whole bytes for things that need only a few bits. The talk's example is a GUI-style `Widget` that can be enabled or not, visible or not, and is in one of three states.

Written the obvious way, with `bool is_enabled; bool is_visible; State state;`, the struct is **3 bytes**: one for each `bool`, and one for the enum because its underlying type is `uint8_t`. But the information content is tiny:

- `is_enabled`: 1 bit;
- `is_visible`: 1 bit;
- `state`: three values, so 2 bits.

Four bits of information take 24 bits of storage. **Bitfields** let you tell the compiler how many bits each member needs:

**Listing 13** — code from the talk (Optimize type size: Bitfields), shown at [17:47](https://www.youtube.com/watch?v=g_X5g3xw43Q&t=1067s)

```cpp
enum class State : uint8_t
{
    A, B, C
};

struct Widget
{
    bool is_enabled : 1;
    bool is_visible : 1;
    State state : 2;
};
```

- `enum class State : uint8_t { A, B, C };` is a scoped enum with an explicit one-byte underlying type, as recommended in the previous sections.
- `bool is_enabled : 1;` The number after the colon is the **bit width**. The member behaves like a `bool` but occupies a single bit.
- `State state : 2;` Two bits are enough for the values 0, 1 and 2. This works because the underlying type is *unsigned*. With a signed underlying type, a 2-bit field could hold only −2 to 1, and `C` (= 2) would not fit. (GCC warns about this when the field is too small for the enumeration's values.)

The compiler packs adjacent bitfields into the same storage unit, so `sizeof(Widget) == 1` "with bits to spare!", room for four more flags at no extra cost.

### The cost: slower access

The slide's warning is "Accessing bitfields may be slower → benchmark!". The CPU cannot load or store a single bit. Reading `w.state` means loading the byte, shifting and masking off the other fields. Writing it is a read-modify-write: load the byte, clear the two bits, OR in the new value, store the byte. The speaker's point is that "any effects we might get from having more widgets in our cache can be completely negated by the actual overhead for the access."

Two other properties of bitfields are worth knowing before you use them widely:

- **Layout is implementation-defined.** The order of the bits within the byte, and whether a field may straddle storage units, are up to the compiler and ABI. Bitfields are fine for compact in-memory data, but they are not a portable way to describe a wire format or a hardware register.
- **You cannot take the address of a bitfield** or bind a non-const reference to it. A bitfield is not an object with its own address.

Bitfield members can have default member initializers since C++20, which removes one old annoyance.

### Comparing the two layouts

The example declares both versions of `Widget`, checks their sizes, and shows that the packed one behaves like ordinary members when you read and write it.

**Listing 14** — Plain and bit-packed Widget side by side (C++20) *(illustrative example, not from the talk)*

```cpp
#include <cstdint>
#include <cstdio>

enum class State : std::uint8_t { A, B, C };

struct PlainWidget
{
    bool is_enabled;
    bool is_visible;
    State state;
};

struct Widget
{
    bool is_enabled : 1 = false;   // default member initializers: C++20
    bool is_visible : 1 = false;
    State state : 2 = State::A;
};

static_assert(sizeof(PlainWidget) == 3);
static_assert(sizeof(Widget) == 1);

int main()
{
    Widget w;
    w.is_enabled = true;
    w.state = State::C;   // read-modify-write of one byte
    std::printf("plain: %zu bytes, packed: %zu byte\n", sizeof(PlainWidget), sizeof(Widget));
    std::printf("enabled=%d visible=%d state=%d\n",
                w.is_enabled, w.is_visible, static_cast<int>(w.state));
}
```

It prints `plain: 3 bytes, packed: 1 byte`, then `enabled=1 visible=0 state=2`.

## 8. Alignment, padding and reordering members

*Talk segment: [18:00](https://www.youtube.com/watch?v=g_X5g3xw43Q&t=1080s)*

### A message with 9 wasted bytes

Bitfields cost access speed. The next optimization is one the speaker calls "usually free": **reordering the members of a struct**. The example is a network-style message:

**Listing 15** — code from the talk (Optimize type size: Re-ordering members), shown at [18:19](https://www.youtube.com/watch?v=g_X5g3xw43Q&t=1099s)

```cpp
struct Message
{
    uint8_t type;
    uint32_t length;
    unsigned char* data;
    uint16_t checksum;
};
```

On a 64-bit system the members need 1 (`uint8_t type`) + 4 (`uint32_t length`) + 8 (`unsigned char* data`) + 2 (`uint16_t checksum`) = **15 bytes**. Yet `sizeof(Message) == 24`. Nine bytes, more than a third of the struct, carry no information. The reason is **alignment**.

### Alignment

The standard ([basic.align]/1) says:

> Object types have alignment requirements which place restrictions on the addresses at which an object of that type may be allocated. An alignment is an implementation-defined integer value representing the number of bytes between successive addresses at which a given object can be allocated.

The address of an object must be a multiple of its type's alignment, which you can query with `alignof`. On mainstream 64-bit platforms:

| Type | `alignof` | Allowed addresses |
|---|---|---|
| `uint8_t` | 1 | anywhere |
| `uint16_t` | 2 | 0, 2, 4, 6, ... |
| `uint32_t` | 4 | 0, 4, 8, 12, ... |
| `uint64_t`, pointers | 8 | 0, 8, 16, 24, ... |

Hardware likes aligned data: an aligned 4-byte load never straddles two cache lines or two pages, so it can be done in one step. On some architectures a misaligned access traps. In C++ an object at a misaligned address is undefined behaviour on every architecture.

### Padding

Inside a struct, every member must be aligned too. The slide lists three rules:

- **Struct alignment** is the maximum alignment of any member.
- **Internal padding** is padding bytes inserted between members so that the next member starts at a properly aligned offset.
- **Tail padding** is padding bytes at the end of the struct, so that in an *array* of structs every element, and therefore every member, is aligned. Equivalently, `sizeof` is always a multiple of `alignof`.

Apply them to `Message` in declaration order. The slide shows this as a byte map:

**Listing 16** — code from the talk (Optimize type size: Re-ordering members), shown at [20:01](https://www.youtube.com/watch?v=g_X5g3xw43Q&t=1201s)

```cpp
struct Message { uint8_t; uint32_t; unsigned char*; uint16_t; };
```

| Offset | Content |
|---|---|
| 0 | `uint8_t type` |
| 1-3 | padding: `length` needs a multiple of 4 |
| 4-7 | `uint32_t length` |
| 8-15 | `unsigned char* data` (offset 8 is already a multiple of 8) |
| 16-17 | `uint16_t checksum` |
| 18-23 | tail padding: the struct's alignment is 8, so its size must be a multiple of 8 |

That accounts for 3 + 6 = 9 padding bytes and the size of 24.

### Reorder from largest to smallest alignment

Now sort the members by decreasing alignment:

**Listing 17** — code from the talk (Optimize type size: Re-ordering members), shown at [20:27](https://www.youtube.com/watch?v=g_X5g3xw43Q&t=1227s)

```cpp
struct Message { unsigned char*; uint32_t; uint16_t; uint8_t; };
```

The pointer occupies bytes 0-7, `length` 8-11, `checksum` 12-13, `type` 14, and a single tail padding byte (15) rounds the size up to **16**. Sorting by decreasing alignment never needs internal padding: every size is a multiple of its alignment and alignments are powers of two, so after the larger members the offset is already a multiple of every smaller alignment.

The message shrinks from 24 to 16 bytes, a third less memory and a third more messages per cache line, without changing a single access. That is why the speaker calls it "essentially a free optimization in many situations". The cases where it is not free: the member order is part of an external format (a file layout, a network protocol, an ABI shared with other code), or members that are used together end up far apart. Also remember that members are *initialized in declaration order*, which matters if one member's initializer depends on another.

Compilers can point out padding for you. GCC and Clang accept `-Wpadded`, which warns whenever padding is inserted. It is too noisy to leave on globally, but useful for checking a hot struct.

### Checking a layout with offsetof

You never have to work layouts out by hand: `offsetof`, `sizeof` and `alignof` report them. The example prints the layout of both orderings, and the `static_assert`s document the 64-bit expectations.

**Listing 18** — Printing the layout of both Message orderings (C++20) *(illustrative example, not from the talk)*

```cpp
#include <cstddef>
#include <cstdint>
#include <cstdio>

struct Message
{
    std::uint8_t type;
    std::uint32_t length;
    unsigned char* data;
    std::uint16_t checksum;
};

struct MessageReordered
{
    unsigned char* data;
    std::uint32_t length;
    std::uint16_t checksum;
    std::uint8_t type;
};

static_assert(sizeof(void*) != 8 || sizeof(Message) == 24);
static_assert(sizeof(void*) != 8 || sizeof(MessageReordered) == 16);

template <typename M>
void print(const char* name)
{
    std::printf("%-15s type@%zu length@%zu data@%zu checksum@%zu  sizeof=%zu alignof=%zu\n",
                name, offsetof(M, type), offsetof(M, length), offsetof(M, data),
                offsetof(M, checksum), sizeof(M), alignof(M));
}

int main()
{
    print<Message>("declared order");
    print<MessageReordered>("reordered");
}
```

Output on x86-64 or ARM64 Linux:

| | type | length | data | checksum | sizeof |
|---|---|---|---|---|---|
| declared order | 0 | 4 | 8 | 16 | 24 |
| reordered | 14 | 8 | 0 | 12 | 16 |

## 9. Spare bits in pointers

*Talk segment: [20:30](https://www.youtube.com/watch?v=g_X5g3xw43Q&t=1230s)*

### Free bits at the bottom of every pointer

Alignment wastes bytes inside structs, but it also *gives* something back. The slide makes the observation in two steps:

- **Observation:** a `T*` always points to a multiple of `alignof(T)`.
- **Therefore:** the lower log2(`alignof(T)`) bits of a `T*` are always zero.

Being a multiple of 2 means the last binary digit is 0, being a multiple of 4 means the last two are 0, and so on. A pointer to an 8-byte-aligned object always ends in `000` in binary. Those bits carry no information, so a program can borrow them to store some of its own, as long as it clears them again before using the pointer. This technique is called **pointer tagging**.

### The problem: one bit that costs eight bytes

Imagine a document tree, such as a DOM, where a slot can point either to a `Container` or to a `Text` node. The straightforward representation stores a flag and an untyped pointer:

**Listing 19** — code from the talk (Optimize type size: Pointer bits), shown at [21:39](https://www.youtube.com/watch?v=g_X5g3xw43Q&t=1299s)

```cpp
class ContainerOrText
{
    bool _is_container;
    void* _ptr;

public:
    bool is_container() const { return _is_container; }
    Container* as_container() const { return (Container*)_ptr; }
    Text* as_text() const { return (Text*)_ptr; }
};
```

- `bool _is_container;` records which kind of node `_ptr` points to.
- `void* _ptr;` points to the node.
- The accessors cast `_ptr` back to the right type with C-style casts. A `void*` converts to any object pointer type, so `(Container*)_ptr` is a `static_cast` here.

The size is **16 bytes**: the `bool` takes 1, and 7 bytes of padding follow so that the pointer is 8-aligned. The useful content is an 8-byte pointer plus one bit. The speaker admits this "makes me really sad every time": we "blow up like eight bytes just to store one additional bit".

### The solution: put the bit in the pointer

If both `Container` and `Text` have an alignment of at least 2, the lowest bit of a pointer to either is always 0, and can hold the flag instead:

**Listing 20** — code from the talk (Optimize type size: Pointer bits), shown at [22:09](https://www.youtube.com/watch?v=g_X5g3xw43Q&t=1329s)

```cpp
class ContainerOrText
{
    std::uintptr_t _impl;

public:
    bool is_container() const { return (_impl & 0b0) == 0; }
    Container* as_container() const { return (Container*)(_impl); }
    Text* as_text() const { return (Text*)(_impl & ~uintptr_t(0b0)); }
};
```

- `std::uintptr_t _impl;` is an unsigned integer type wide enough to hold a pointer's value. Converting a pointer to `uintptr_t` and back gives the original pointer. Storing an integer instead of a pointer makes the bit manipulation legal.
- As explained in the talk, the encoding is: `_impl` is the address of the object, with the lowest bit **set to 1 if it is a `Text`** and **0 if it is a `Container`**.
- `is_container()` tests whether that bit is zero.
- `as_container()` can cast `_impl` straight back, because for a container the bit is already 0.
- `as_text()` must first clear the bit, with `_impl & ~mask`, because a real `Text` lives at an even address.

The result is `sizeof(ContainerOrText) == 8`: half the size, so twice as many slots per cache line.

**A slip on the slide.** The mask is written `0b0` in both places. `0b0` is just zero, so `_impl & 0b0` is always 0 and `is_container()` would always return `true`. And `~uintptr_t(0b0)` is all ones, so `as_text()` would not clear anything. The mask the talk describes, the lowest bit, is `0b1`. With `0b1` the code does what the speaker explains.

### Rules for doing this safely

Pointer tagging is common in performance-critical code: LLVM's `PointerIntPair`, tagged pointers in language runtimes, and lock-free algorithms that keep a counter in spare bits all use it. A few rules keep it correct:

- **Check the alignment you rely on.** A `static_assert(alignof(Text) >= 2)` turns a silent assumption into a compile error if someone later changes `Text`.
- **Only use the low bits that alignment guarantees.** The *high* bits of a 64-bit pointer are often unused in practice too, but using them depends on the platform's address-space layout and hardware features, not on anything C++ guarantees.
- **Hide it in a class.** Every access goes through the masking code, so keep that code in one place, exactly as `ContainerOrText` does.

### A corrected, runnable version

The example uses the `0b1` mask, adds factory functions that set the tag, and asserts the alignment assumption.

**Listing 21** — ContainerOrText with a working tag bit (C++20) *(illustrative example, not from the talk)*

```cpp
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

struct Container { std::vector<int> children; };
struct Text { std::string content; };

static_assert(alignof(Container) >= 2 && alignof(Text) >= 2, "need a free low bit");

class ContainerOrText
{
    static constexpr std::uintptr_t text_tag = 0b1;
    std::uintptr_t _impl;

    explicit ContainerOrText(std::uintptr_t impl) : _impl(impl) {}

public:
    static ContainerOrText container(Container* c)
    {
        return ContainerOrText(reinterpret_cast<std::uintptr_t>(c));
    }
    static ContainerOrText text(Text* t)
    {
        return ContainerOrText(reinterpret_cast<std::uintptr_t>(t) | text_tag);
    }

    bool is_container() const { return (_impl & text_tag) == 0; }
    Container* as_container() const { return reinterpret_cast<Container*>(_impl); }
    Text* as_text() const { return reinterpret_cast<Text*>(_impl & ~text_tag); }
};

static_assert(sizeof(ContainerOrText) == sizeof(void*));

int main()
{
    Container box{{1, 2, 3}};
    Text greeting{"hello"};
    ContainerOrText nodes[] = {ContainerOrText::container(&box), ContainerOrText::text(&greeting)};

    std::printf("sizeof = %zu\n", sizeof(ContainerOrText));
    for (const auto& node : nodes) {
        if (node.is_container())
            std::printf("container: %zu children\n", node.as_container()->children.size());
        else
            std::printf("text: \"%s\"\n", node.as_text()->content.c_str());
    }
}
```

Output: `sizeof = 8`, then `container: 3 children` and `text: "hello"`.

### Takeaways

The slide that closes this part of the talk lists the size techniques so far. If types are smaller, more fit into the cache:

- `short` instead of `int`, `float` instead of `double`;
- specify the underlying type of enumerations, consider bitfields;
- reorder struct members to minimize padding;
- consider tricks such as using pointer alignment bits.

> Beware: access to the data might be slower. Benchmark!

## 10. Sequential access and the prefetcher

*Talk segment: [23:00](https://www.youtube.com/watch?v=g_X5g3xw43Q&t=1380s)*

### A puzzle about the average-age benchmark

The speaker now goes back to the average-age loop from earlier and asks the question an attentive listener should have asked:

> Why did a smaller type make it faster? We only access everything once.

`average_age` reads each element exactly once and never comes back to it. With only one access per element, every access should be a cache miss, whatever the element size, because the data has never been in the cache before. Fitting "more data into the cache" should not matter for data that is used once. And the cache model so far still does not explain the opening puzzle: why a sorted `std::vector` beats `std::unordered_set`.

### Random versus sequential access

To find out, the speaker extends the random-access benchmark from the start of the talk with two more access patterns over the same buffer. The random version is the familiar `benchmark::DoNotOptimize(++data[dist(engine)]);`. The two new loops walk the buffer in order, forwards and backwards:

**Listing 22** — code from the talk (Benchmarking memory access patterns), shown at [23:45](https://www.youtube.com/watch?v=g_X5g3xw43Q&t=1425s)

```cpp
for (std::size_t idx = 0; idx < data.size(); ++idx)
{
    benchmark::DoNotOptimize(dist(engine));
    benchmark::DoNotOptimize(++data[idx]);
}
```

**Listing 23** — code from the talk (Benchmarking memory access patterns), shown at [23:45](https://www.youtube.com/watch?v=g_X5g3xw43Q&t=1425s)

```cpp
for (std::size_t idx = data.size(); idx > 0; --idx)
{
    benchmark::DoNotOptimize(dist(engine));
    benchmark::DoNotOptimize(++data[idx - 1]);
}
```

In both loops, `benchmark::DoNotOptimize(dist(engine));` generates a random number and *throws it away*. That keeps the comparison fair: the random version pays for one call to the random number generator per access, so the sequential versions pay for it too, and the only difference left is the *order* of the memory accesses. The backward loop counts `idx` down from `data.size()` to 1 and accesses `data[idx - 1]`. That is the usual way to iterate backwards with an unsigned index, since `idx >= 0` would always be true for a `std::size_t`.

### Sequential access is consistently fast

The result is striking:

| Data size | Random | Forward | Backward |
|---|---|---|---|
| 16 KB | ~94 MB/s | ~100 MB/s | ~100 MB/s |
| 2 MB | ~77 MB/s | ~100 MB/s | ~100 MB/s |
| 10 MB | ~33 MB/s | ~100 MB/s | ~100 MB/s |

Both sequential patterns run at the same speed **whatever the size of the data**: as fast at 10 MB, far beyond the caches, as at 16 KB, deep inside L1. Only truly random access degrades. "Structured memory access is consistently fast!"

### The CPU cache prefetcher

The explanation is a piece of hardware designed for exactly this. The insight, in the slide's words: "when accessing address x, usually you also access x+1, x+2, ...". Programs walk arrays, copy buffers and scan strings. CPU designers know this, so every modern CPU has a **prefetcher**: logic that watches the stream of memory accesses, recognises simple patterns, and loads the addresses it predicts you will need *before you ask for them*.

When the prediction is right, the data is already in the cache when the load instruction arrives, and the access is a hit even though the program has never touched that address. That is why the sequential loops never feel the size of the data: the prefetcher keeps fetching ahead, and the RAM latency overlaps with useful work.

Real prefetchers recognise more than "next address":

- **ascending and descending** streams (hence forward and backward perform the same);
- **strided** access, such as every 16th element or one field of each struct in an array;
- several independent streams at once, for example a loop that reads two arrays and writes a third.

What they cannot predict is an address that is not a simple function of the previous ones, such as a random index or the target of a pointer that has not been loaded yet. That limitation explains the slow containers in a later section.

This answers half the puzzle. `average_age` reads its data sequentially, so the prefetcher streams it in, and the speed depends on how fast bytes can flow from memory. Smaller elements mean fewer bytes for the same number of ages. The other half, why the *amount* fetched matters, comes from the cache line, the topic of the next section.

### Trying the three patterns

The example times one pass over a 64 MB array in three orders: forwards, backwards, and through a random permutation of the indices. Visiting each element exactly once makes the comparison fair. On typical hardware the two sequential orders run at nearly the same speed, and the random order is many times slower, even though every version performs the same additions.

**Listing 24** — Forward, backward and random traversal of the same array (C++20) *(illustrative example, not from the talk)*

```cpp
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <numeric>
#include <random>
#include <vector>

template <typename F>
void measure(const char* name, F f)
{
    auto start = std::chrono::steady_clock::now();
    std::uint64_t sum = f();
    auto stop = std::chrono::steady_clock::now();
    std::printf("%-9s %8.1f ms  (sum %llu)\n", name,
                std::chrono::duration<double, std::milli>(stop - start).count(),
                static_cast<unsigned long long>(sum));
}

int main()
{
    std::vector<std::uint32_t> data(16 << 20);   // 64 MB
    std::iota(data.begin(), data.end(), 0u);

    std::vector<std::uint32_t> order(data.size());
    std::iota(order.begin(), order.end(), 0u);
    std::ranges::shuffle(order, std::mt19937(3));

    measure("forward", [&] {
        std::uint64_t s = 0;
        for (std::size_t i = 0; i < data.size(); ++i) s += data[i];
        return s;
    });
    measure("backward", [&] {
        std::uint64_t s = 0;
        for (std::size_t i = data.size(); i > 0; --i) s += data[i - 1];
        return s;
    });
    measure("random", [&] {
        std::uint64_t s = 0;
        for (std::uint32_t i : order) s += data[i];   // order itself is read sequentially
        return s;
    });
}
```

## 11. Cache lines

*Talk segment: [24:20](https://www.youtube.com/watch?v=g_X5g3xw43Q&t=1460s)*

### Random chunks, random order

The prefetcher explains why sequential access is fast. A second experiment shows that something else is going on as well, even when the access pattern is unpredictable at small scale. The benchmark still reads random data at random locations, but in *chunks*: it picks a random starting point, then visits every element of a chunk of `chunk_size` elements starting there, **in a random order**.

**Listing 25** — code from the talk (Chunked memory access), shown at [24:55](https://www.youtube.com/watch?v=g_X5g3xw43Q&t=1495s)

```cpp
std::vector<std::size_t> indices(chunk_size);
std::iota(indices.begin(), indices.end(), 0);

std::uniform_int_distribution<std::size_t> dist(0, N - chunk_size - 1);
for (auto _ : state)
{
    state.PauseTiming();
    std::shuffle(indices.begin(), indices.end(), engine);
    state.ResumeTiming();

    auto start = dist(engine);
    for (auto i : indices)
        sum += data[start + i];
    benchmark::DoNotOptimize(sum);
}
```

Line by line:

- `std::vector<std::size_t> indices(chunk_size);` and `std::iota(indices.begin(), indices.end(), 0);` create the offsets 0, 1, ..., `chunk_size - 1`.
- `dist(0, N - chunk_size - 1)` chooses a chunk start such that the whole chunk lies inside the `N`-element buffer.
- Inside the timed loop, `state.PauseTiming()` ... `state.ResumeTiming()` shuffles the offsets *without* timing the shuffle. Each iteration therefore visits the chunk in a fresh random order, which the prefetcher cannot predict.
- `auto start = dist(engine);` picks a random chunk, and `for (auto i : indices) sum += data[start + i];` visits it in shuffled order.
- `benchmark::DoNotOptimize(sum);` keeps the additions alive.

At the scale of the whole buffer the accesses are random (a random chunk), and within the chunk they are random too (a shuffled order). The only structure is that a chunk's elements are *close together*.

### Bigger chunks are faster, with diminishing returns

The speaker varies `chunk_size` from 32 to 512 bytes. Performance rises as the chunks get bigger. The chart plots the **forward difference**: how much throughput each step of 32 bytes *adds* (ΔMB/s). That gain is about 41 at 64 bytes and falls steadily to about 23 at 512 bytes. Growing a small chunk helps a lot; growing an already large chunk helps less.

Why would accessing more data *per chunk*, in random order, make each access cheaper?

### Cache lines

Because, in the talk's words, "while we're accessing main memory, it doesn't really make sense to only access one byte". A trip to RAM pays a large fixed latency, so the memory system makes each trip count. It never transfers a single byte or a single `int`. It transfers a whole **cache line**: a contiguous, aligned block of bytes, typically **64 bytes** on x86 and most ARM cores and **128 bytes** on Apple's M-series performance cores.

The slide's animation uses a toy cache-line size of two addresses:

1. `load R1, [2]` misses. The cache fetches the *whole line* containing address 2, that is addresses 2 and 3, and delivers address 2 to the register.
2. `load R2, [5]` misses. The cache fetches the line with addresses 4 and 5.
3. `load R3, [3]` **hits**, although the program has never accessed address 3, because it arrived together with address 2.

This is **spatial locality**: data near something you just used is likely to be used soon, and the cache line makes it nearly free. (The reuse of the *same* address, as in the earlier examples, is called **temporal locality**.)

The chunk benchmark now makes sense. With a tiny chunk, each random chunk costs one or two line fetches, but only a few bytes of each 64-byte line are used, and the rest of what was transferred is thrown away. As the chunk grows, more of every line fetched is put to use, and the cost per element falls. Once a chunk spans many lines, the fixed cost is already well amortised, and making it bigger helps less and less. Hence the diminishing returns.

It also completes the explanation of the average-age benchmark. Memory is delivered in whole lines, so the cost of streaming through an array depends on the number of *lines*, not the number of elements. With `short` instead of `int`, a 64-byte line holds 32 ages instead of 16, so half as many lines have to come from memory.

### Finding the cache line size

C++17 has `std::hardware_constructive_interference_size` in `<new>`: a compile-time constant meant as the "maximum size of contiguous memory to promote true sharing", in practice the line size, typically 64. Being a compile-time constant, it reflects what the compiler *assumed* for the target, not the machine the program runs on. A later section returns to this. On Linux you can ask the running system with `getconf LEVEL1_DCACHE_LINESIZE`, or from code with `sysconf(_SC_LEVEL1_DCACHE_LINESIZE)` (a glibc extension).

### Takeaways

The slide sums up the hardware behaviour:

- The CPU wants to minimize RAM accesses.
- Therefore the CPU fetches the entire cache line around an access.
- In addition, the CPU detects simple access patterns and prefetches relevant memory in the background.

> **Ensure you have a predictable memory access pattern with high locality!**

### Measuring the line size from software

The example reads one `int` out of every `stride` ints over a 64 MB array, and reports the time *per element read*. While the stride is below one line (strides 1 to 16 with 64-byte lines), several elements share each fetched line, and the cost per element grows as fewer elements share a line. Below the line size, doubling the stride roughly doubles the time per read, because each fetched line is shared by half as many reads. Past the line size every read pays for a whole line of its own, and the growth slows down.

On the test machine (64-byte lines) the times per read were 0.48, 0.66, 1.12, 2.04 and 4.59 ns for strides of 4 to 64 bytes, close to doubling each time, and then 6.91 and 8.69 ns for 128 and 256 bytes. The growth does not stop completely beyond 64 bytes, because the prefetchers also fetch neighbouring lines, and that help disappears at large strides.

**Listing 26** — Finding the cache line size with strided reads (C++20) *(illustrative example, not from the talk)*

```cpp
#include <chrono>
#include <cstdio>
#include <new>
#include <vector>

int main()
{
    std::vector<int> data(16 << 20, 1);   // 64 MB, far bigger than the caches
    std::printf("hardware_constructive_interference_size = %zu\n",
                std::hardware_constructive_interference_size);

    for (std::size_t stride = 1; stride <= 64; stride *= 2) {
        long long sum = 0;
        std::size_t reads = 0;
        auto start = std::chrono::steady_clock::now();
        for (int rep = 0; rep < 4; ++rep)
            for (std::size_t i = 0; i < data.size(); i += stride, ++reads)
                sum += data[i];
        auto stop = std::chrono::steady_clock::now();
        double ns = std::chrono::duration<double, std::nano>(stop - start).count() / reads;
        std::printf("stride %2zu ints (%3zu bytes): %6.2f ns per read  (sum %lld)\n",
                    stride, stride * sizeof(int), ns, sum);
    }
}
```

## 12. Cache-friendly containers

*Talk segment: [26:40](https://www.youtube.com/watch?v=g_X5g3xw43Q&t=1600s)*

### Turning the hardware facts into container advice

With cache lines and prefetching understood, the speaker turns to the practical question: which data structures work *with* the hardware? To use prefetching and cache lines well, a container needs two properties:

- **Predictable access**: *avoid pointer chasing* and prefer sequential memory access. To follow a pointer, the CPU must first load the pointer itself, and only then does it know the next address. The prefetcher cannot guess an address that is stored in memory it has not read yet. Every hop is a potential full miss, and the hops cannot overlap.
- **High locality**: *minimize the size of types*, so more elements fit in each cache line, and *move uninteresting data away*, so the lines you fetch are not wasted on bytes the loop does not need.

The talk then goes through the standard containers, using a diagram of each one's memory layout.

### std::vector: the reference point

**Listing 27** — code from the talk (std::vector<T> is cache-friendly), shown at [28:03](https://www.youtube.com/watch?v=g_X5g3xw43Q&t=1683s)

```cpp
T* ptr;
std::size_t size;
std::size_t capacity;
```

A `std::vector<T>` is three words: a pointer to a heap buffer, a size and a capacity. (Real implementations usually store three pointers, begin, end and end-of-capacity, but the idea is the same.) The elements live **contiguously** in the buffer, one after another, with nothing in between:

- no per-element metadata, so every byte of every fetched line is payload;
- iteration is a linear walk through memory, exactly what the prefetcher predicts best.

"So the prefetcher is on our side and everybody's happy." The same is true of `std::array<T, N>`, `std::inplace_vector<T, N>` (new in C++26: a vector with fixed capacity whose storage is inside the object) and any other container built on one contiguous block.

### std::list: the opposite end

**Listing 28** — code from the talk (std::list<T> is not cache-friendly), shown at [28:29](https://www.youtube.com/watch?v=g_X5g3xw43Q&t=1709s)

```cpp
struct Node {
    T value;
    Node* next;
    Node* prev;
};

Node* head;
Node* tail;
```

A `std::list<T>` allocates every element in its own **node** with `next` and `prev` pointers, and holds `head` and `tail` pointers. It fails both tests:

- On a 64-bit system the two pointers add 16 bytes to every element. For a `std::list<int>`, that is 4 bytes of payload in a node of at least 24 bytes, before counting the allocator's own bookkeeping. Most of each cache line is spent on pointers that the *user* of the list does not care about.
- Nodes can be anywhere in memory: the diagram shows element 0 in the middle, 1 at the left and 2 to the right. Iterating means pointer chasing, so the prefetcher cannot help.

### std::deque: it depends on the block size

**Listing 29** — code from the talk (std::deque<T> can be cache-friendly), shown at [29:23](https://www.youtube.com/watch?v=g_X5g3xw43Q&t=1763s)

```cpp
using Block = std::array<T, BLOCK_SIZE>;
std::vector<Block*> blocks;
```

A `std::deque<T>` is logically a vector of pointers to fixed-size **blocks**. Within a block the elements are contiguous without metadata, so the prefetcher is happy. Only when iteration moves from one block to the next is there a pointer to follow. "Note: BLOCK_SIZE has to be large enough!" With big blocks, the occasional jump costs nothing noticeable. With tiny blocks, a deque degenerates into something close to a linked list.

The speaker warns that in "at least one standard library implementation the block size is too small to get any meaningful cache benefits". He does not name it. The usual culprit is Microsoft's STL, whose blocks hold only 16 bytes (a `std::deque<double>` has two elements per block, and any element larger than 8 bytes gets a block to itself). libstdc++ uses 512-byte blocks, and libc++ 4096-byte blocks for small types. A deque is a useful container in principle because it offers **pointer stability**: adding elements at either end never moves existing ones. But the speaker's advice is that if you need that, "you should probably roll your own", because you cannot rely on the standard library's block size.

### std::map and std::set: trees of nodes

**Listing 30** — code from the talk (std::map<K, V>/std::set<T> are not cache-friendly), shown at [29:49](https://www.youtube.com/watch?v=g_X5g3xw43Q&t=1789s)

```cpp
struct Node {
    T key_value;
    Node* left;
    Node* right;
};

Node* root;
```

`std::map` and `std::set` are balanced binary search trees (red-black trees in all three major implementations). Each node holds the key/value plus `left` and `right` pointers (and in practice a parent pointer and a colour). "From a memory layout perspective it is exactly like a linked list." The pointers mean something different, but the prefetcher cannot help either way, and the metadata wastes valuable cache-line space. A lookup in a tree of a thousand elements follows about ten pointers, each to a different line in an unpredictable place.

### std::unordered_map and std::unordered_set: buckets of lists

**Listing 31** — code from the talk (std::unordered_{map<K, V>,set<T>} are not cache-friendly), shown at [30:35](https://www.youtube.com/watch?v=g_X5g3xw43Q&t=1835s)

```cpp
struct Node {
    T key_value;
    Node* next;
};

std::vector<Node*> buckets;
```

"Really unfortunately" the standard hash tables are not cache-friendly either. They use **closed addressing**, also called *separate chaining*: the table is an array of buckets, and each bucket is a singly linked list of nodes holding the key/value and a `next` pointer. A lookup computes the hash, selects a bucket, follows the pointer to the first node, and keeps following `next` pointers until the key is found.

The standard's interface practically forces this design. Bucket iteration (`begin(n)`/`end(n)`) and the guarantee that references to elements stay valid across a rehash both mean every element needs a node of its own.

This finally explains the opening benchmark. The O(1) lookup of `std::unordered_set` costs at least one, and usually two, dependent pointer hops to unpredictable addresses: the bucket array, then the node. A binary search in a sorted vector does more comparisons, but on one contiguous array. For small sets the whole array sits in a handful of lines that stay in cache, and it can be faster than the hash table. For 64 elements, even a plain linear search through 256 contiguous bytes, four cache lines that the prefetcher streams in, beats everything else.

### Hash tables with open addressing

The slide for the cache-friendly kind of hash table shows exactly the same three members as the `std::vector` listing above, `T* ptr; std::size_t size; std::size_t capacity;`. That is the point: a hash table with **open addressing** is, in memory, just an array. All elements are stored in one flat array. To look up a key, hash it to a slot. If that slot holds a different key, look at the *neighbouring* slots ("linear probing") until you find the key or an empty slot. No pointers, no nodes. Probing touches adjacent addresses, usually in the same cache line. "That is now really cache friendly ... so they can be a lot faster." But the standard library does not provide one. Widely used open-addressing tables include Abseil's `absl::flat_hash_map`, Boost's `boost::unordered_flat_map` and Martin Ankerl's `ankerl::unordered_dense`. (These are examples for further exploration; the talk does not name any.)

### Takeaway

> Prefer containers that use **big contiguous chunks of memory**: `std::vector<T>`, some implementations of `std::deque<T>`, and hash tables with open addressing.

| Container | Layout | Cache-friendly? |
|---|---|---|
| `std::vector`, `std::array`, `std::inplace_vector` | one contiguous block | yes |
| `std::deque` | vector of pointers to blocks | only with big blocks |
| `std::list`, `std::forward_list` | one node per element | no |
| `std::map`, `std::set` | tree of nodes | no |
| `std::unordered_map`, `std::unordered_set` | buckets of node lists | no |
| open-addressing hash tables (not in std) | one flat array | yes |

### A minimal open-addressing hash set

To see how little an open-addressing table needs, the example implements a fixed-capacity hash set of non-negative `int`s with linear probing. It reserves −1 to mark empty slots and keeps the table at most half full, so probe sequences stay short. It then times lookups against `std::unordered_set` on the same keys. On typical hardware the flat table is clearly faster once the data exceeds the L1 cache. A production table also needs deletion, growth, a better hash function and support for arbitrary keys, which is why you would normally use one of the libraries above.

**Listing 32** — A linear-probing hash set compared with std::unordered_set (C++20) *(illustrative example, not from the talk)*

```cpp
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <random>
#include <unordered_set>
#include <vector>

class FlatIntSet
{
    std::vector<int> slots_;   // -1 = empty
    std::size_t mask_;

    std::size_t slot_for(int key) const
    {
        auto h = static_cast<std::uint64_t>(key) * 0x9E3779B97F4A7C15ull;   // Fibonacci hashing
        return static_cast<std::size_t>(h >> 32) & mask_;
    }

public:
    explicit FlatIntSet(std::size_t max_elements)
    {
        std::size_t capacity = 16;
        while (capacity < 2 * max_elements) capacity *= 2;   // keep load factor <= 0.5
        slots_.assign(capacity, -1);
        mask_ = capacity - 1;
    }

    void insert(int key)   // key must be >= 0
    {
        for (std::size_t i = slot_for(key);; i = (i + 1) & mask_) {
            if (slots_[i] == key) return;
            if (slots_[i] == -1) { slots_[i] = key; return; }
        }
    }

    bool contains(int key) const
    {
        for (std::size_t i = slot_for(key);; i = (i + 1) & mask_) {   // neighbours: same cache line
            if (slots_[i] == key) return true;
            if (slots_[i] == -1) return false;
        }
    }
};

template <typename Set>
double ns_per_lookup(const Set& set, const std::vector<int>& queries, std::size_t& hits)
{
    auto start = std::chrono::steady_clock::now();
    for (int q : queries) hits += set.contains(q);
    auto stop = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::nano>(stop - start).count() / queries.size();
}

int main()
{
    std::mt19937 engine(5);
    std::uniform_int_distribution<int> dist(0, 1 << 30);
    for (std::size_t n : {1'000, 100'000, 2'000'000}) {
        FlatIntSet flat(n);
        std::unordered_set<int> chained;
        std::vector<int> keys(n);
        for (int& k : keys) { k = dist(engine); flat.insert(k); chained.insert(k); }

        std::vector<int> queries(4'000'000);
        for (std::size_t i = 0; i < queries.size(); ++i)
            queries[i] = i % 2 ? keys[engine() % n] : dist(engine);

        std::size_t hits_flat = 0, hits_std = 0;
        double t_flat = ns_per_lookup(flat, queries, hits_flat);
        double t_std = ns_per_lookup(chained, queries, hits_std);
        std::printf("n=%8zu  open addressing %5.1f ns  unordered_set %5.1f ns  (hits %zu/%zu)\n",
                    n, t_flat, t_std, hits_flat, hits_std);
    }
}
```

## 13. Instruction caches: code is data too

*Talk segment: [31:30](https://www.youtube.com/watch?v=g_X5g3xw43Q&t=1890s)*

### The L2 cache that was too small

After taking his random-access measurements, the speaker checked the CPU's data sheet and found a mismatch. He had drawn the boundary of the "fits in L2" region at about 2 MB, but the CPU used for that chart reports an L1 cache of 128 KB and an **L2 of 4096 KB**. One could argue that the next bar also belongs to the green region, but the throughput clearly drops *before* the data reaches 4 MB. Where did the rest of the L2 go?

### Code is data too

The slide answers with a chain of facts that mirrors the beginning of the talk:

- CPU instructions are stored in memory.
- Main memory access is slow.
- Therefore main memory access is avoided using faster caches, **instruction caches**.
- The L1 cache is usually separate for instructions and data (an *L1i* and an *L1d* per core).
- The L2 cache is usually unified: one cache for both.
- Therefore the measured L2 size was smaller.

The benchmark's own machine code, the Google Benchmark library, the operating system's code running on the same core: all of it competes with the benchmark's data for L2 space. So the effective data capacity of L2 is less than the number on the data sheet.

If code lives in memory and goes through caches, then code has caching effects of its own, and those can be surprising.

### A contrived experiment

To demonstrate, the speaker builds a "silly example". It sums a vector of random `unsigned` values, but whenever an element is 0, which is rare for random 32-bit data, it also does 64 separate increments. `REPEAT64(x)` is a macro that pastes the expression `x` 64 times, which produces a large block of machine code that almost never runs. The two versions differ only in whether the rare block comes before or after the addition.

**Listing 33** — code from the talk (Do sum then increments or increments then sum?), shown at [33:29](https://www.youtube.com/watch?v=g_X5g3xw43Q&t=2009s)

```cpp
void sum_then_increments(const std::vector<unsigned>& data)
{
    auto count = 0;
    for (auto d : data)
    {
        count += d;
        if (d == 0) // rarely true
        {
            REPEAT64(benchmark::DoNotOptimize(++count));
        }
    }
    benchmark::DoNotOptimize(count);
}
```

**Listing 34** — code from the talk (Do sum then increments or increments then sum?), shown at [33:39](https://www.youtube.com/watch?v=g_X5g3xw43Q&t=2019s)

```cpp
void increments_then_sum(const std::vector<unsigned>& data)
{
    auto count = 0;
    for (auto d : data)
    {
        if (d == 0) // rarely true
        {
            REPEAT64(benchmark::DoNotOptimize(++count));
        }
        count += d;
    }
    benchmark::DoNotOptimize(count);
}
```

Both functions compute the same `count`: addition commutes, and the rare increments are the same either way. It is purely a matter of writing the `if` first or second. Asked to vote, the audience barely raised their hands.

The result, on an Apple M1 performance core:

| Version | Throughput |
|---|---|
| sum then increments | ~1.75 GB/s |
| increments then sum | ~9 GB/s |

Moving one addition across an `if` that almost never fires made the loop five times faster. The speaker's explanation is that it is "just because of the way your actual CPU instructions ended up living in memory": how the compiler laid out the basic blocks, where the cache-line boundaries fall, how the code is aligned. He candidly adds that he looked into it and "couldn't really see anything". In the Q&A he says the layout of the basic blocks was essentially the same, the extra addition shifted the code over a little, and the effect appeared on the M1 but not on an Intel CPU, and differed between the M1's performance and efficiency cores. Audience members suggested branch-prediction structures and the small L0/L1 instruction caches as likely suspects. The exact cause is microarchitectural and hard to pin down without a profiler. The lesson is that **code layout can matter a lot, and you cannot reliably predict it from the source**.

### Cache-friendly code patterns

The speaker translates each data rule into a rule for code:

| Cache-friendly data access | Cache-friendly code |
|---|---|
| Access data in linear order | **Avoid long branches**: instructions that execute one after the other are prefetched like an array; a jump to a distant function may miss in the instruction cache the first time |
| Avoid pointer chasing | **Avoid indirect jumps**: a virtual call or a call through a function pointer goes to an address that is only known at run time, so the front end cannot prefetch it in advance |
| Minimize the size of data | **Minimize the size of hot code**: do as little as possible in the body of a hot loop, so it fits comfortably in the instruction cache |

> Design your program by keeping caches in mind, not only your data layout, but your code too.

In the Q&A someone asked how much control a programmer has over where code ends up. The speaker's practical advice: in a hot loop where something rare happens, **call a non-inlined function** for the rare case, so its code lives elsewhere and does not pollute the cache. Conversely, what you do want in the loop should ideally be inlined into it. Beyond that, compilers offer section attributes for "weird cases", but mostly you have to measure.

### Keeping the cold path out of the hot loop

The example applies that advice to the talk's loop. The rare 64-increment block moves into a separate function marked `[[gnu::noinline, gnu::cold]]` (GCC and Clang attributes: never inline it, and place it with other rarely executed code). The branch is marked with C++20's `[[unlikely]]`. The hot loop that remains is a few instructions long, whichever order you write it in. The example also shows how a `REPEAT64` macro can be built from smaller repetitions, and uses an empty inline `asm` statement as a portable stand-in for `benchmark::DoNotOptimize`.

**Listing 35** — Moving a rare, large block out of a hot loop (C++20, GCC/Clang) *(illustrative example, not from the talk)*

```cpp
#include <cstdio>
#include <random>
#include <vector>

// A stand-in for benchmark::DoNotOptimize: the compiler must assume x is read.
#define KEEP(x) asm volatile("" : : "r"(x) : "memory")

#define REPEAT4(x) x; x; x; x
#define REPEAT16(x) REPEAT4(x); REPEAT4(x); REPEAT4(x); REPEAT4(x)
#define REPEAT64(x) REPEAT16(x); REPEAT16(x); REPEAT16(x); REPEAT16(x)

[[gnu::noinline, gnu::cold]] void rare_work(unsigned& count)
{
    REPEAT64(KEEP(++count));
}

unsigned sum_with_cold_path(const std::vector<unsigned>& data)
{
    unsigned count = 0;
    for (auto d : data) {
        count += d;
        if (d == 0) [[unlikely]]   // rarely true
            rare_work(count);
    }
    return count;
}

int main()
{
    std::vector<unsigned> data(1 << 20);
    std::mt19937 engine(9);
    for (auto& d : data) d = engine();
    data[123] = 0;   // make sure the rare path runs once
    std::printf("%u\n", sum_with_cold_path(data));
}
```

## 14. Data-oriented design: records of people

*Talk segment: [36:00](https://www.youtube.com/watch?v=g_X5g3xw43Q&t=2160s)*

### Object-oriented versus data-oriented

The speaker suggests a design philosophy that takes caches seriously from the start: **data-oriented design (DOD)**. He remarks that a surprising number of talks at this CppCon covered it, "which I didn't know when I submitted this talk". He introduces it by contrast with object-oriented programming (OOP):

| Object-oriented programming | Data-oriented design |
|---|---|
| Focused on **objects**: domain-specific abstractions | Focused on **algorithms**: data transformations |
| Smart objects encapsulate data and behavior | Algorithms operate on dumb data without behavior |
| Heterogeneous collections of base classes | Multiple homogeneous collections of different records |

In DOD, a program is a pipeline of transformations: some data comes in, the program's job is to turn it into other data, and that job is what you design around. The data itself is plain, like an `int` that has "no member functions, so to speak".

### The OOP way: design the objects first

The first example is a list of people. Following OOP, you start with the object:

**Listing 36** — code from the talk (OOP: Records of people), shown at [37:19](https://www.youtube.com/watch?v=g_X5g3xw43Q&t=2239s)

```cpp
class Person
{
public:
    std::string_view name() const;
    Age age() const;
};

using People = std::vector<Person>;

Age average_age(const People& people);
std::string_view name_of_oldest_person(const People& people);
```

A `Person` has a name and an age, exposed through getters (the slide omits the constructor and the private members). A collection of people is a `std::vector<Person>`, and the operations come afterwards: `average_age(const People&)` and `name_of_oldest_person(const People&)`. Every function receives *whole people*, whatever parts of a person it needs.

### The DOD way: design the algorithms first

Data-oriented design starts from the other end and asks, for each algorithm, *what data does it actually need?* The slides build this up step by step:

**Listing 37** — code from the talk (DOD: Records of people), shown at [37:53](https://www.youtube.com/watch?v=g_X5g3xw43Q&t=2273s)

```cpp
Age average_age(const std::vector<Age>& ages);

std::string_view name_of_oldest_person(???);
```

The average age needs only the ages, so `average_age` takes a `std::vector<Age>`, the very function from the start of the talk. "We don't need their names in order to compute the average age." Then the slide asks what `name_of_oldest_person` needs. The `???` placeholder marks the question. The final version splits that work in two:

**Listing 38** — code from the talk (DOD: Records of people), shown at [38:35](https://www.youtube.com/watch?v=g_X5g3xw43Q&t=2315s)

```cpp
Age average_age(const std::vector<Age>& ages);

std::size_t index_of_oldest_person(const std::vector<Age>& ages);
std::string_view name_of_oldest_person(
    const std::vector<std::string_view>& names,
    const std::vector<Age>& ages
)
{
    auto index = index_of_oldest_person(ages);
    return names[index];
}
```

- `index_of_oldest_person` needs only the ages, and returns the *position* of the oldest person.
- `name_of_oldest_person` needs the names and the ages. It calls `index_of_oldest_person(ages)` and returns `names[index]`.

The two vectors are *parallel arrays*. They have the same size, and element *i* of each describes the same person. The person exists only as the shared index.

### Two data layouts: AoS and SoA

The two designs correspond to two classic memory layouts.

**Array of Structures (AoS):** a struct holds one record's fields, and you keep an array of structs. This is the natural OOP layout, "because your structs are your objects".

**Listing 39** — code from the talk (Two different data layout approaches), shown at [38:49](https://www.youtube.com/watch?v=g_X5g3xw43Q&t=2329s)

```cpp
struct Person
{
    std::string name;
    Age age;
};

using People = std::vector<Person>;
```

**Structure of Arrays (SoA):** one struct holds one array *per field*. `Person` stops being a type of record and becomes a strongly typed index, the same `enum class` idiom as `Age`:

**Listing 40** — code from the talk (Two different data layout approaches), shown at [39:09](https://www.youtube.com/watch?v=g_X5g3xw43Q&t=2349s)

```cpp
enum class Person : std::size_t {};

struct People
{
    std::vector<std::string> names;
    std::vector<Age> ages;
};
```

The slide summarises who uses which: OOP uses array of structures, while DOD uses "whatever works for the algorithm!". If you usually need name and age together, keep them together. If some algorithms need only one field, split them apart.

In memory:

- **AoS** is `name age name age name age ...`. All fields of one person are adjacent, which gives *efficient access to individual rows*, in database terms.
- **SoA** is `name name name ... age age age ...`. All values of one field are adjacent, which gives *efficient access to individual columns*.

### Why it matters for the cache

`average_age` over an AoS `std::vector<Person>` must stream through the names to reach the ages. A `std::string` is typically 32 bytes on 64-bit platforms, so each 64-byte cache line holds fewer than two people. That means about 4-8 bytes of ages per line, and the rest is names the algorithm never reads. The SoA version streams through a dense array of ages, all of it useful.

The benchmark adds a fourth bar to the earlier average-age chart (billions of items per second):

| Layout | Throughput |
|---|---|
| `enum class Age : int` | ~8 |
| `enum class Age : short` | ~14.5 |
| `enum class Age : uint8_t` | ~11 |
| `Person` (AoS with a name) | ~2.4 |

"Whatever size for the age we pick, `Person` is significantly slower than all of them." The AoS layout wastes "precious cache-line space on storing the names, even though computing the average age never actually cares about the name".

### Writing it both ways

The example implements both layouts for a million people and times the average age. It also implements `index_of_oldest_person` with `std::ranges::max_element` and the slide's `name_of_oldest_person` on the SoA layout.

**Listing 41** — Average age over AoS and SoA layouts (C++20) *(illustrative example, not from the talk)*

```cpp
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

enum class Age : short {};

namespace aos {
struct Person { std::string name; Age age; };
using People = std::vector<Person>;

Age average_age(const People& people)
{
    long long total = 0;
    for (const Person& p : people) total += static_cast<int>(p.age);
    return static_cast<Age>(total / static_cast<long long>(people.size()));
}
}

namespace soa {
enum class Person : std::size_t {};
struct People { std::vector<std::string> names; std::vector<Age> ages; };

Age average_age(const std::vector<Age>& ages)
{
    long long total = 0;
    for (Age age : ages) total += static_cast<int>(age);
    return static_cast<Age>(total / static_cast<long long>(ages.size()));
}

Person index_of_oldest_person(const std::vector<Age>& ages)
{
    auto it = std::ranges::max_element(ages);
    return static_cast<Person>(it - ages.begin());
}

const std::string& name_of_oldest_person(const std::vector<std::string>& names,
                                         const std::vector<Age>& ages)
{
    auto index = index_of_oldest_person(ages);
    return names[static_cast<std::size_t>(index)];
}
}

template <typename F>
void measure(const char* name, F f)
{
    auto start = std::chrono::steady_clock::now();
    int result = static_cast<int>(f());
    auto stop = std::chrono::steady_clock::now();
    std::printf("%s: average %d in %.2f ms\n", name, result,
                std::chrono::duration<double, std::milli>(stop - start).count());
}

int main()
{
    constexpr std::size_t n = 1'000'000;
    std::mt19937 engine(11);
    std::uniform_int_distribution<int> dist(0, 100);

    aos::People rows;
    soa::People columns;
    for (std::size_t i = 0; i < n; ++i) {
        std::string name = "person #" + std::to_string(i);
        Age age = static_cast<Age>(dist(engine));
        rows.push_back({name, age});
        columns.names.push_back(name);
        columns.ages.push_back(age);
    }

    measure("AoS", [&] { return aos::average_age(rows); });
    measure("SoA", [&] { return soa::average_age(columns.ages); });
    std::printf("oldest: %s\n", soa::name_of_oldest_person(columns.names, columns.ages).c_str());
}
```

On typical hardware the SoA average is several times faster. The exact ratio depends on `sizeof(std::string)` and on your memory bandwidth.

## 15. Data-oriented design: heterogeneous shapes

*Talk segment: [40:40](https://www.youtube.com/watch?v=g_X5g3xw43Q&t=2440s)*

### OOP: a class hierarchy of shapes

The second data-oriented example is the classic OOP textbook case: shapes with an area. The OOP approach is to "design a class hierarchy":

**Listing 42** — code from the talk (OOP: Shapes), shown at [40:57](https://www.youtube.com/watch?v=g_X5g3xw43Q&t=2457s)

```cpp
struct Shape
{
    virtual double area() const = 0;
};

struct Circle : Shape
{
    double area() const override;
};
struct Square : Shape
{
    double area() const override;
};
```

- `Shape` is an abstract base class with a *pure virtual* function, `virtual double area() const = 0;`.
- `Circle` and `Square` derive from it and `override` `area()` with their own formulas. In a full program they would also store a radius or a side length.

To process "all the shapes", OOP code needs a collection of objects of *different* types. A `std::vector<Shape>` cannot hold them (the objects would be sliced to `Shape`), so each object is allocated on the heap and the vector holds owning pointers:

**Listing 43** — code from the talk (OOP: Shapes), shown at [41:13](https://www.youtube.com/watch?v=g_X5g3xw43Q&t=2473s)

```cpp
double total_area(const std::vector<std::unique_ptr<Shape>>& shapes)
{
    double total = 0.0;
    for (const auto& shape : shapes)
        total += shape->area();
    return total;
}
```

The loop asks each shape for its area through a virtual call and sums the results. It is short, readable and extensible: a `Triangle` would slot in without touching `total_area`.

### DOD: start from the algorithm

Data-oriented design again starts with the algorithm: a function `total_area` that returns a `double`. The open question is what it takes, which the slides show as a placeholder:

**Listing 44** — code from the talk (DOD: Shapes), shown at [41:25](https://www.youtube.com/watch?v=g_X5g3xw43Q&t=2485s)

```cpp
double total_area(???);
```

What exists are circles and squares, and a shape is one or the other. The first answer is a **sum type**: `Circle` and `Square` become plain structs that just store their data, no base class, and the function takes a vector of `std::variant`:

**Listing 45** — code from the talk (DOD: Shapes), shown at [41:55](https://www.youtube.com/watch?v=g_X5g3xw43Q&t=2515s)

```cpp
double total_area(
    const std::vector<std::variant<Circle, Square>>& shapes
);
```

The speaker anticipates the objection: what if we need a triangle later? "Then we get to it when we get to it." Unless you already know the design must be extensible, he argues, "don't worry about it. Just focus on the problem that you're actually having at the moment."

But if the function only ever deals with circles and squares, why make it sort through a mixed collection? Give it all the circles and all the squares, separately:

**Listing 46** — code from the talk (DOD: Shapes), shown at [42:07](https://www.youtube.com/watch?v=g_X5g3xw43Q&t=2527s)

```cpp
double total_area(
    const std::vector<Circle>& circles,
    const std::vector<Square>& squares
);
```

This `total_area` sums the area of all circles, then the area of all squares, and adds the two.

### Three ways to represent heterogeneous data

The slide compares the three representations:

| Representation | Type | Memory |
|---|---|---|
| **Boxed** | `std::vector<std::unique_ptr<Shape>>` | an array of pointers; each points to a separately allocated circle or square somewhere on the heap |
| **Tagged union** | `std::vector<std::variant<Circle, Square>>` | objects stored inline, each with a *tag* recording which alternative it holds |
| **Multiple homogeneous arrays** | `std::vector<Circle>`, `std::vector<Square>` | two dense arrays, no pointers, no tags |

Measured with the analysis so far:

- **Boxed** is bad for the prefetcher, because every element is a pointer to follow, to an unpredictable address. It also wastes cache-line space on the pointers themselves (and on each object's vtable pointer).
- **Tagged union** is much better: the objects are stored inline and visited sequentially. But every element carries a tag. Only one bit of information is needed, but "it probably gets blown up to eight bytes to account for proper alignment". Each element also needs a branch on the tag.
- **Multiple homogeneous arrays** need no tag at all, because the array an element is in tells you its type. They need no branch either: the loop over circles uses the circle formula, "statically known, no branch needed", and likewise for squares.

The result, in millions of items per second, is about the same at every size from 16k to 128k shapes:

| Representation | Throughput |
|---|---|
| `std::vector<std::unique_ptr<Shape>>` | ~180 |
| `std::vector<std::variant<Circle, Square>>` | ~290 |
| `std::vector<Circle>`, `std::vector<Square>` | ~1080 |

Separate arrays are about six times faster than the classic OOP design.

### Not only about data caches

The Q&A put this result in perspective, and the speaker agreed. In the benchmark the circles and squares were *randomly mixed* in the single-vector versions. An audience member argued that much of the gap comes from the **branch predictor** and the branch target buffer: with random types, the CPU cannot predict which `area` implementation, or which `variant` branch, comes next, and every misprediction costs a pipeline flush. That is a front-end stall, not a data-cache miss, and a profiler separating front-end from back-end stalls would show it. He predicted that just grouping the boxed shapes by type would give a big speed-up. The speaker added that the separate arrays also *vectorize* much better: with no branch in the loop, the compiler can compute several areas per SIMD instruction.

So, in the speaker's words, "this isn't entirely about caches in this case". The broader point stands: the data-oriented layout is faster, through several hardware mechanisms at once.

### Takeaways for data-oriented design

The speaker's summary of DOD:

- **Design around algorithms that transform data.** Start with the algorithm and ask what data it needs.
- **Write functions that operate on N elements, not single ones.** Don't write a function that returns the area of one shape. Write one that takes all the shapes, because you have more than one. Pushing the loop *into* the function gives the optimizer the whole loop to work with.
- **Use structure of arrays to group relevant data.** Keep in each array only what the hot algorithm needs, and let the rest live elsewhere.
- **Use multiple homogeneous arrays for heterogeneous data.** A record with a `bool` member and a loop that branches on it "is usually a red flag": split it into two arrays, one for each value. You no longer store the `bool`, and you no longer branch on it.

### Comparing the three representations

The example implements `total_area` all three ways over the same million randomly mixed shapes and checks that they agree. It uses `std::visit` with an overload set for the variant. As discussed above, part of what you measure is branch prediction rather than caching. Try sorting the boxed vector by type and see how much of the gap closes.

**Listing 47** — Boxed, tagged-union and homogeneous-array shapes (C++20) *(illustrative example, not from the talk)*

```cpp
#include <chrono>
#include <cstdio>
#include <memory>
#include <numbers>
#include <random>
#include <variant>
#include <vector>

namespace oop {
struct Shape { virtual ~Shape() = default; virtual double area() const = 0; };
struct Circle : Shape { double r; explicit Circle(double r) : r(r) {} double area() const override { return std::numbers::pi * r * r; } };
struct Square : Shape { double s; explicit Square(double s) : s(s) {} double area() const override { return s * s; } };

double total_area(const std::vector<std::unique_ptr<Shape>>& shapes)
{
    double total = 0.0;
    for (const auto& shape : shapes) total += shape->area();
    return total;
}
}

namespace dod {
struct Circle { double r; };
struct Square { double s; };

template <typename... F> struct overloaded : F... { using F::operator()...; };

double total_area(const std::vector<std::variant<Circle, Square>>& shapes)
{
    double total = 0.0;
    for (const auto& shape : shapes)
        total += std::visit(overloaded{
            [](const Circle& c) { return std::numbers::pi * c.r * c.r; },
            [](const Square& q) { return q.s * q.s; }}, shape);
    return total;
}

double total_area(const std::vector<Circle>& circles, const std::vector<Square>& squares)
{
    double total = 0.0;
    for (const Circle& c : circles) total += std::numbers::pi * c.r * c.r;   // no branch
    for (const Square& q : squares) total += q.s * q.s;                      // no branch
    return total;
}
}

template <typename F>
void measure(const char* name, F f)
{
    auto start = std::chrono::steady_clock::now();
    double area = f();
    auto stop = std::chrono::steady_clock::now();
    std::printf("%-22s %.6e  %7.2f ms\n", name, area,
                std::chrono::duration<double, std::milli>(stop - start).count());
}

int main()
{
    std::mt19937 engine(13);
    std::uniform_real_distribution<double> size(0.5, 2.0);
    std::bernoulli_distribution is_circle(0.5);

    std::vector<std::unique_ptr<oop::Shape>> boxed;
    std::vector<std::variant<dod::Circle, dod::Square>> tagged;
    std::vector<dod::Circle> circles;
    std::vector<dod::Square> squares;
    for (int i = 0; i < 1'000'000; ++i) {
        double x = size(engine);
        if (is_circle(engine)) {
            boxed.push_back(std::make_unique<oop::Circle>(x));
            tagged.emplace_back(dod::Circle{x});
            circles.push_back({x});
        } else {
            boxed.push_back(std::make_unique<oop::Square>(x));
            tagged.emplace_back(dod::Square{x});
            squares.push_back({x});
        }
    }

    measure("unique_ptr<Shape>", [&] { return oop::total_area(boxed); });
    measure("variant<Circle,Square>", [&] { return dod::total_area(tagged); });
    measure("two vectors", [&] { return dod::total_area(circles, squares); });
    std::printf("sizeof(variant) = %zu, sizeof(Circle) = %zu\n",
                sizeof(std::variant<dod::Circle, dod::Square>), sizeof(dod::Circle));
}
```

The last line shows the tag overhead the speaker mentioned: on 64-bit platforms a `std::variant<Circle, Square>` of two 8-byte structs is 16 bytes, twice the payload.

The three totals agree only approximately, because the two-vector version adds the terms in a different order, and floating-point addition is not associative.

## 16. False sharing

*Talk segment: [44:50](https://www.youtube.com/watch?v=g_X5g3xw43Q&t=2690s)*

### The parallel fold pattern

Everything so far ran on one core. The last technical part of the talk looks at caches in multi-core processors, using one of the most common patterns in parallel code: a **parallel fold**. A *fold* (also *reduce* or *accumulate*) combines many values into one: a sum, a maximum, a count. The standard way to parallelise it:

**Listing 48** — code from the talk (Parallel fold pattern), shown at [46:15](https://www.youtube.com/watch?v=g_X5g3xw43Q&t=2775s)

```cpp
std::vector<T> local_results(pool.thread_count());
pool.run([&](std::size_t thread_idx) {
    local_results[thread_idx] = fold(work.slice(thread_idx));
});

T global_result = fold(local_results);
```

- `pool` is a thread pool with `pool.thread_count()` threads, and `pool.run(f)` calls `f(thread_idx)` once on each thread and waits for all of them. (The pool is not standard C++; any pool with this shape works.)
- `local_results` has one slot per thread.
- Each thread folds *its own slice* of the work, `work.slice(thread_idx)`, and writes the result into *its own slot*.
- When all threads are done, a final sequential `fold(local_results)` combines the per-thread results.

No synchronization is needed: no thread reads or writes another thread's slot, and the input is only read. In the abstract machine of the C++ standard, there is no data race.

### A silly benchmark that does not scale

To measure the pattern, the speaker uses a "silly" workload: recursively computing Fibonacci numbers.

**Listing 49** — code from the talk (Silly parallel fibonacci example), shown at [46:43](https://www.youtube.com/watch?v=g_X5g3xw43Q&t=2803s)

```cpp
void accumulate_fib(unsigned& result, unsigned n)
{
    if (n < 2)
    {
        result += n;
    }
    else
    {
        accumulate_fib(result, n - 1);
        accumulate_fib(result, n - 2);
    }
}
```

`accumulate_fib(result, n)` adds the n-th Fibonacci number to `result` through the famously inefficient recursion. It is a few lines of code that create plenty of work, exponentially many calls, and it **writes to `result` at every leaf**. Through the reference, `result` is the thread's slot in the shared vector.

**Listing 50** — code from the talk (Silly parallel fibonacci example), shown at [47:17](https://www.youtube.com/watch?v=g_X5g3xw43Q&t=2837s)

```cpp
std::vector<unsigned> results(thread_count);
for (auto _ : state)
{
    pool.run([&](std::size_t thread_id) {
        results[thread_id] = 0;
        for (auto i = 0; i != 1024 / thread_count; ++i)
            accumulate_fib(results[thread_id], i % 20);
    });
    benchmark::DoNotOptimize(results);
}
```

Each thread resets its slot `results[thread_id]`, then accumulates Fibonacci numbers of 0 to 19 (`i % 20`), `1024 / thread_count` times. The total work is the same for every thread count, so with *k* threads each thread does 1/*k* of it, and the benchmark should get faster as threads are added.

It does not. The measured throughput is about 2 MB/s with one thread and stays flat, or even slightly worse, with two, three and four threads. "If we use four threads, we are less fast than if you just had a single thread."

### Caches in multi-core processors

The slide builds the explanation in three steps:

- **Each core has its own L1 cache.** Caches are fast because they are close to the core, so every core has a private one.
- **Therefore, modifications invalidate the caches of all other cores.** Recall that caches are write-back. If core 0 writes to its cached copy of an address, the copies in other cores' caches are stale. To keep memory consistent, the hardware runs a *cache coherence protocol* (MESI and its variants): a core that wants to write a line first takes exclusive ownership of it, which invalidates every other core's copy. The next time another core reads that line, it misses and must fetch the current data from the owning core.
- **However, caches operate at the granularity of cache lines.** Coherence does not track individual bytes or `unsigned`s. It invalidates whole lines.

### False sharing

Put the three facts together with the benchmark's `results` vector. Its four `unsigned` slots occupy 16 consecutive bytes, all in **one** cache line.

**Listing 51** — code from the talk (False sharing), shown at [48:29](https://www.youtube.com/watch?v=g_X5g3xw43Q&t=2909s)

```cpp
// invalidates cache line, ...
results[0] = 0;
```

**Listing 52** — code from the talk (False sharing), shown at [48:51](https://www.youtube.com/watch?v=g_X5g3xw43Q&t=2931s)

```cpp
// ... therefore, cache miss!
auto value = results[1];
```

Thread 0 writes `results[0]`, which invalidates the line in every other core. Thread 1 then accesses `results[1]`, a *different* variable that thread 0 never touched, and misses, because the line containing it was invalidated. Then thread 1 writes, invalidating thread 0's copy, and so on. Every write at every leaf of every recursion bounces the line between cores. The threads never share data logically, but they share a cache line physically.

> **False sharing**: cache invalidation due to modifications of unrelated data in the same cache line.

It is called *false* sharing because the program does not share anything. The hardware's line granularity makes it look as if it did.

### Fix 1: give each thread its own cache line

The first fix spreads the slots out so that each thread's slot is on its own line:

**Listing 53** — code from the talk (Preventing false sharing by aligning to cache line size), shown at [49:47](https://www.youtube.com/watch?v=g_X5g3xw43Q&t=2987s)

```cpp
constexpr auto alignment
    = std::hardware_destructive_interference_size / sizeof(unsigned);
std::vector<unsigned> results(thread_count * alignment);
for (auto _ : state)
{
    pool.run([&](std::size_t thread_id) {
        results[thread_id * alignment] = 0;
        for (auto i = 0; i != 1024 / thread_count; ++i)
            accumulate_fib(results[thread_id * alignment], i % 20);
    });
    benchmark::DoNotOptimize(results);
}
```

- `std::hardware_destructive_interference_size` (C++17, `<new>`) is the minimum distance two objects should have to avoid false sharing, in practice the cache line size. The speaker is "not sure why they didn't call it cache line".
- `alignment` converts that byte distance into a number of `unsigned` elements (64 / 4 = 16 with 64-byte lines).
- The vector is `thread_count * alignment` elements long, so it holds `thread_count` cache lines' worth of space, and thread *t* uses element `t * alignment`, the first element of "its" line.

Strictly, this guarantees one slot per line only if the vector's buffer starts on a line boundary. `std::vector<unsigned>` only promises `alignof(unsigned)`, so the slots may straddle lines in pairs. In practice large heap allocations are usually well aligned, and a padded struct with `alignas`, as in the example below, removes the doubt.

The speaker has a caveat about the constant itself: it is a *compile-time* value set by the compiler for the target it assumes, "and it might not always be set properly for your target. So you probably need to roll your own." For example, it is 64 for generic x86-64, which matches; but Apple's M-series performance cores use 128-byte lines. GCC goes as far as warning (`-Winterference-size`) when the constant is used in a header, because its value can differ between compilations with different `-mtune` settings, which would be an ABI break.

With the slots spread out, the benchmark scales: about 2.4, 4.6, 6.4 and 8.9 MB/s for one to four threads.

### Fix 2: accumulate locally

"Of course in this particular example there would be a far easier solution":

**Listing 54** — code from the talk (Preventing false sharing using local accumulation), shown at [50:17](https://www.youtube.com/watch?v=g_X5g3xw43Q&t=3017s)

```cpp
std::vector<unsigned> results(thread_count);
for (auto _ : state)
{
    pool.run([&](std::size_t thread_id) {
        auto local = 0u;
        for (auto i = 0; i != 1024 / thread_count; ++i)
            accumulate_fib(local, i % 20);
        results[thread_id] = local;
    });
    benchmark::DoNotOptimize(results);
}
```

Each thread accumulates into a local variable `local`, which lives in a register or on the thread's own stack, and writes to the shared vector **once**, at the end. The final writes still falsely share a line, but only once per thread per run instead of millions of times, so the cost is negligible. Both fixes scale equally well: about 2.3, 4.5, 6.3 and 8.8 MB/s.

This is the better default. It needs no knowledge of the line size and no padding. It is also the idea behind well-written parallel reductions: combine privately, publish once.

### Takeaways

> **Keep data used by different threads in different cache lines!** (And data used by the same thread together in the same cache line.)

The speaker stresses how devious this is. False sharing is not a race condition, the program is correct and the results are right, but "you should view it as if it was a race condition, because it tanks your performance."

In the Q&A someone asked whether code then needs a different design for single-threaded and multi-threaded use. The speaker's answer: false sharing only happens when threads *modify* data near each other. The typical parallelisation, splitting an array into one large chunk per thread, does share lines at the chunk boundaries, but those are a handful of lines among megabytes, so you still get the speed-up.

### Reproducing the effect with std::jthread

The example runs the talk's workload with plain `std::jthread`s instead of a pool, in three variants: adjacent slots, slots padded to a cache line with an `alignas` struct, and local accumulation. On a multi-core machine the adjacent version is the slowest by a wide margin. The other two should be close to each other. Results vary between CPUs, and a coarse `std::chrono` timer is enough to see the difference.

**Listing 55** — False sharing and two fixes, with std::jthread (C++20) *(illustrative example, not from the talk)*

```cpp
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <new>
#include <thread>
#include <vector>

void accumulate_fib(unsigned& result, unsigned n)
{
    if (n < 2) {
        result += n;
    } else {
        accumulate_fib(result, n - 1);
        accumulate_fib(result, n - 2);
    }
}

struct alignas(std::hardware_destructive_interference_size) PaddedSlot
{
    unsigned value = 0;
};

constexpr unsigned work = 4096;

template <typename Body>
double run(unsigned threads, Body body)
{
    auto start = std::chrono::steady_clock::now();
    {
        std::vector<std::jthread> pool;
        for (unsigned t = 0; t < threads; ++t)
            pool.emplace_back(body, t, threads);
    }   // jthreads join here
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}

int main()
{
    unsigned threads = std::max(2u, std::min(4u, std::thread::hardware_concurrency()));
    std::vector<unsigned> adjacent(threads);
    std::vector<PaddedSlot> padded(threads);
    std::vector<unsigned> results(threads);

    double t1 = run(threads, [&](unsigned id, unsigned n) {
        adjacent[id] = 0;
        for (unsigned i = 0; i != work / n; ++i) accumulate_fib(adjacent[id], i % 20);
    });
    double t2 = run(threads, [&](unsigned id, unsigned n) {
        padded[id].value = 0;
        for (unsigned i = 0; i != work / n; ++i) accumulate_fib(padded[id].value, i % 20);
    });
    double t3 = run(threads, [&](unsigned id, unsigned n) {
        unsigned local = 0;
        for (unsigned i = 0; i != work / n; ++i) accumulate_fib(local, i % 20);
        results[id] = local;
    });

    std::printf("%u threads, sizeof(PaddedSlot) = %zu\n", threads, sizeof(PaddedSlot));
    std::printf("adjacent slots: %7.1f ms (result %u)\n", t1, adjacent[0]);
    std::printf("padded slots:   %7.1f ms (result %u)\n", t2, padded[0].value);
    std::printf("local variable: %7.1f ms (result %u)\n", t3, results[0]);
}
```

GCC and Clang compile it without warnings. GCC's `-Winterference-size` warning only fires when the constant is used where it could affect an ABI, such as in a header or an inline function shared between translation units.

## 17. Conclusion and audience questions

*Talk segment: [51:00](https://www.youtube.com/watch?v=g_X5g3xw43Q&t=3060s)*

### The vocabulary of CPU caches

The speaker closes with the four definitions that the whole talk has built up:

| Term | Definition |
|---|---|
| **Cache** | smaller, faster memory used to avoid accessing slower main memory |
| **Cache line** | smallest unit of data transferred between main memory and caches |
| **Prefetcher** | predicts memory access and pre-emptively loads data into caches |
| **False sharing** | cache invalidation due to modifications of unrelated data in the same cache line |

And one reminder: **code lives in memory too.** Instructions go through caches and have cache effects of their own.

### Leverage CPU caches

The final checklist puts the data rules and the code rules side by side:

| Cache-friendly data access patterns | Cache-friendly code patterns |
|---|---|
| Access data in linear order, so the prefetcher can help | Avoid long branches |
| Avoid pointer chasing, so the prefetcher can do its job | Avoid indirect jumps |
| Minimize the size of data: small types, no padding, no unneeded fields in the hot array | Minimize the size of hot code |

> Also: keep data used by different threads in different cache lines!

These map onto the techniques of the chapter:

| Goal | Techniques from the talk |
|---|---|
| Fit more data into each line | smaller primitive types, enum underlying types, bitfields, member reordering, pointer tag bits |
| Make access predictable | `std::vector` and other contiguous containers, open-addressing hash tables, big `deque` blocks |
| Fetch only what the algorithm needs | data-oriented design: structure of arrays, homogeneous arrays instead of tags or virtual dispatch |
| Avoid optimizer pessimizations | beware of `char`/`std::byte` aliasing; use range-based `for` |
| Scale across cores | separate lines per thread, or accumulate locally and write once |

### Write cache-friendly C++, but benchmark

The talk ends where it began: "Write cache-friendly C++! But benchmark to make sure you're actually optimizing." Several results in the talk went against the obvious expectation. `uint8_t` was slower than `short`, `char8_t` beat `char`, and moving an addition across an `if` changed speed five-fold. So every technique here is a hypothesis to test on your own hardware, not a rule to apply blindly.

### From the Q&A

A few audience questions not covered in earlier sections:

- **Further material.** Asked about talks that go deeper into cache internals (associativity, direct-mapped vs n-way caches and so on), the speaker said he did not know of one; his talk was meant as an introduction, "caches exist, you should be aware of them". The slides recommend Ulrich Drepper's paper *What Every Programmer Should Know About Memory* for more depth.
- **NUMA.** Asked about non-uniform memory access on multi-socket machines, where each socket has its own memory, the speaker said he had no experience with it.
- **The `vector::size` reload.** Asked whether the reload in the byte-type benchmark was caused by the short-buffer optimization, the speaker said no. The optimizer simply does not know `std::vector`'s semantics. It sees a write through a pointer that may alias anything, so it must assume anything in memory, including the vector's own members, may have changed.

## Key takeaways

- Main memory is about 100x slower than computation; caches hide that only if your working data fits in them
- Memory moves in cache lines (64-128 bytes) and prefetchers predict sequential access: contiguous, linear access is fast regardless of data size
- Smaller data means more of it per cache line: use smaller types, enum underlying types, reorder members, consider bitfields and pointer tag bits - and benchmark, because access may get slower
- char, signed char, unsigned char and std::byte may alias anything, which can stop the optimizer from hoisting loads; range-for loops avoid the trap
- Prefer std::vector and other contiguous containers; node-based containers chase pointers and waste line space
- Code is cached too: keep hot loops small, avoid indirect jumps and long branches
- Data-oriented design - structure of arrays and homogeneous arrays - keeps only the data an algorithm needs in the cache
- Data written by different threads must live on different cache lines, or false sharing destroys scaling
- Always measure on your target hardware

## Glossary

| Term | Meaning |
|---|---|
| Cache | Smaller, faster memory close to the CPU that keeps copies of recently used data to avoid slower main memory accesses. |
| Cache hit / miss | A hit finds the requested address in the cache; a miss must fetch it from a lower level or main memory. |
| Eviction | Removing an entry from a full cache to make room for a new one. |
| Cache hierarchy | Several levels of cache (L1, L2, L3) with increasing size and latency. |
| Write-back cache | A cache that updates main memory only when a modified line is evicted; typical for CPUs. |
| Write-through cache | A cache that updates main memory on every write. |
| Cache line | The smallest unit of data transferred between main memory and the caches, typically 64 or 128 bytes. |
| Prefetcher | Hardware that predicts upcoming memory accesses and loads the data into the cache ahead of time. |
| Alignment | The requirement that an object of a type lives at an address that is a multiple of alignof(T). |
| Padding | Unused bytes the compiler inserts inside or at the end of a struct to keep members and array elements aligned. |
| Strict aliasing | The rule that an object may only be accessed through glvalues of similar types, their signed/unsigned counterparts, or char, unsigned char and std::byte. |
| Pointer chasing | Following a chain of pointers, where each address is known only after the previous load completes; defeats prefetching. |
| Open addressing | A hash table design that stores elements directly in one array and probes neighbouring slots on collision. |
| Data-oriented design (DOD) | Designing programs around the algorithms that transform data and the data layout those algorithms need. |
| Array of Structures (AoS) | A layout with one array whose elements are records containing all fields. |
| Structure of Arrays (SoA) | A layout with one array per field; a record is an index into all of them. |
| False sharing | Cache invalidation due to modifications of unrelated data in the same cache line. |
| Instruction cache | A cache holding program instructions; at L1 usually separate from the data cache. |

## Exercises

1. In the motivation benchmark, the unsorted vector was the fastest set at 64 elements even though it does a linear search. Explain why using cache lines and prefetching.

   <details><summary>Hint</summary>How many cache lines do 64 ints occupy, and how are they accessed?</details>

2. Lay out struct S { char a; double b; short c; int d; char e; }; byte by byte on a typical 64-bit platform, give its sizeof, and reorder it to minimise size.

   <details><summary>Hint</summary>Struct alignment is the maximum member alignment; add tail padding.</details>

3. The pointer-tagging slide uses the mask 0b0. What do is_container() and as_text() do with that mask, and what should it be?

   <details><summary>Hint</summary>x & 0 is always 0, and ~0 is all ones.</details>

4. Why can a loop over std::vector<unsigned char> be slower than the same loop over std::vector<char8_t>, and give two ways to fix it.

   <details><summary>Hint</summary>Which types may alias the vector's own members?</details>

5. You need a container with stable element addresses and fast iteration. Why is std::deque not automatically the answer, and what would you build instead?

   <details><summary>Hint</summary>Block size.</details>

6. Rewrite a std::vector<std::unique_ptr<Shape>> holding circles, squares and triangles in data-oriented style and sketch total_area.

   <details><summary>Hint</summary>One vector per concrete type, no virtual call.</details>

7. A parallel histogram uses std::vector<int> counts(thread_count) and each thread increments counts[thread_id] for each item. Predict how it scales and give two fixes.

   <details><summary>Hint</summary>Where do neighbouring counts live?</details>

8. Why should a benchmark result from the talk not be applied blindly to your code? List three things that could differ.

   <details><summary>Hint</summary>The speaker's disclaimer.</details>

## Further reading

- Ulrich Drepper, "What Every Programmer Should Know About Memory" (2007)
- Google Benchmark library (used for all the talk's benchmarks)
- Apple M1 cache figures from https://www.7-cpu.com/cpu/Apple_M1.html and https://en.wikipedia.org/wiki/Apple_M1 (cited on slides)
- Slides: https://github.com/CppCon/CppCon2025/blob/main/Presentations/Cache_Friendly_Cpp.pdf
