---
title: "Finding What's Slow: Instrumented Performance Tracking with CTRACK"
date: 2026-11-15
tags: ["C++", "performance", "profiling", "benchmarking", "instrumentation", "RAII", "multithreading", "open source", "CppCon"]
source: "https://www.youtube.com/watch?v=en4OQvZePqg"
video_title: "CTRACK: C++ Performance Tracking and Bottleneck Discovery - Grischa Hauser - CppCon 2025"
channel: "CppCon"
---

> This chapter is based on the talk [CTRACK: C++ Performance Tracking and Bottleneck Discovery - Grischa Hauser - CppCon 2025](https://www.youtube.com/watch?v=en4OQvZePqg) (CppCon). Code listings marked *from the talk* are transcribed from the slides; listings marked *illustrative* were written for this chapter to explain the ideas.

To make a program faster you first have to find out what is slow, and that is surprisingly hard in a large, real code base. Sampling profilers are easy to attach but statistical, and they miss short or irregular functions. Micro-benchmarks are precise but need the code to be lifted out of its context. Hand-written timers are what most of us fall back to, and they don't scale.

In this CppCon 2025 talk, Grischa Hauser (CEO of COMPAILE Solutions) presents **CTRACK**, a small open-source, header-only C++17 library his team built for this gap. You add one macro, `CTRACK;`, at the top of any function or scope you care about, and at the end you get tables showing where the time went: inclusive and exclusive time, call counts, thread counts, outlier-free percentiles, standard deviation and more. It is designed to cost nanoseconds per event, so it can stay in production builds.

This chapter follows the talk: the profiling landscape and its trade-offs, the requirements that shaped CTRACK, how to read its output, how its accuracy compares with a sampling profiler, how to customise it, and how it works inside (RAII timers, lock-free per-thread buffers, implicit call graphs, interval merging and versioned namespaces). Along the way it explains the underlying techniques so you can apply them with any tool. The talk's code was checked against the real library.

## What you will learn

- Compare sampling profilers, instrumentation profilers and micro-benchmarks, and know when each one fits
- Explain the pitfalls of hand-written timing code and why RAII timers are safer
- List the requirements for a tracker that works in legacy code and in production
- Instrument functions and scopes with CTRACK and print or retrieve its results
- Explain time active (inclusive) versus time active exclusive, and why the exclusive view finds bottlenecks
- Interpret percentile ranges, standard deviation and coefficient of variation in a timing table
- Explain why sampling profilers misattribute short or high-variance functions
- Describe how a low-overhead tracker records events: per-thread buffers, a one-time lock, implicit parent/child links
- Explain interval merging for wall-clock time with multiple threads, and inline versioned namespaces for ABI safety
- Configure CTRACK's dev/prod switches and result settings

## Before you start

- Comfortable with modern C++ (RAII, lambdas, std::chrono, threads)
- No profiling experience required
- CTRACK requires C++17; with GCC's standard library its parallel statistics may need TBB (see the text)

## Contents

1. [To make things fast, find what's slow](#1-to-make-things-fast-find-whats-slow) — [0:00](https://www.youtube.com/watch?v=en4OQvZePqg&t=0s)
2. [The profiling landscape, and the timer we all write](#2-the-profiling-landscape-and-the-timer-we-all-write) — [1:15](https://www.youtube.com/watch?v=en4OQvZePqg&t=75s)
3. [Why existing tools didn't fit: legacy code](#3-why-existing-tools-didnt-fit-legacy-code) — [2:32](https://www.youtube.com/watch?v=en4OQvZePqg&t=152s)
4. [Requirements: integration, dev and production, customisation, insights](#4-requirements-integration-dev-and-production-customisation-insights) — [4:14](https://www.youtube.com/watch?v=en4OQvZePqg&t=254s)
5. [CTRACK and its design philosophy](#5-ctrack-and-its-design-philosophy) — [7:16](https://www.youtube.com/watch?v=en4OQvZePqg&t=436s)
6. [One line: CTRACK and result_print](#6-one-line-ctrack-and-resultprint) — [7:44](https://www.youtube.com/watch?v=en4OQvZePqg&t=464s)
7. [Time active and time active exclusive](#7-time-active-and-time-active-exclusive) — [8:57](https://www.youtube.com/watch?v=en4OQvZePqg&t=537s)
8. [Multiple threads](#8-multiple-threads) — [10:06](https://www.youtube.com/watch?v=en4OQvZePqg&t=606s)
9. [Outliers and the detail view](#9-outliers-and-the-detail-view) — [10:28](https://www.youtube.com/watch?v=en4OQvZePqg&t=628s)
10. [Accuracy compared with a sampling profiler](#10-accuracy-compared-with-a-sampling-profiler) — [12:05](https://www.youtube.com/watch?v=en4OQvZePqg&t=725s)
11. [Customisation](#11-customisation) — [13:37](https://www.youtube.com/watch?v=en4OQvZePqg&t=817s)
12. [A real-world example: measure where it runs](#12-a-real-world-example-measure-where-it-runs) — [14:55](https://www.youtube.com/watch?v=en4OQvZePqg&t=895s)
13. [Inside CTRACK: events and RAII](#13-inside-ctrack-events-and-raii) — [15:33](https://www.youtube.com/watch?v=en4OQvZePqg&t=933s)
14. [Lock-free recording and implicit call graphs](#14-lock-free-recording-and-implicit-call-graphs) — [16:35](https://www.youtube.com/watch?v=en4OQvZePqg&t=995s)
15. [Statistics, interval merging and versioned namespaces](#15-statistics-interval-merging-and-versioned-namespaces) — [18:39](https://www.youtube.com/watch?v=en4OQvZePqg&t=1119s)
16. [Open source: adoption, testing and roadmap](#16-open-source-adoption-testing-and-roadmap) — [19:50](https://www.youtube.com/watch?v=en4OQvZePqg&t=1190s)
17. [The ultimate bottleneck: us](#17-the-ultimate-bottleneck-us) — [22:49](https://www.youtube.com/watch?v=en4OQvZePqg&t=1369s)
18. [Questions and answers](#18-questions-and-answers) — [24:46](https://www.youtube.com/watch?v=en4OQvZePqg&t=1486s)

## 1. To make things fast, find what's slow

*Talk segment: [0:00](https://www.youtube.com/watch?v=en4OQvZePqg&t=0s)*

The story starts about a year before the talk. Grischa Hauser, CEO of COMPAILE Solutions in Germany, was working on a huge C++ project, and "like always, the question was: well, can you make this run faster?" He reached for the tools and techniques he had used for years, "and I always kept thinking to myself, well, there has to be something a bit easier."

The opening slide quotes Donald Knuth:

> "Programmers waste enormous amounts of time thinking about, or worrying about, the speed of noncritical parts of their programs."

Knuth wrote that in a discussion of premature optimisation. The speaker rephrases it into the theme of the talk:

> **To make things go fast, you need to figure out what's slow.**

### Why measuring comes first

Intuition about performance is notoriously unreliable. Code that looks expensive is often cheap, and the real cost hides in a call nobody suspected: an allocation in a loop, a lock, a network round trip, a cache miss pattern. Optimising the wrong function wastes effort and adds complexity for nothing.

**Amdahl's law** makes this precise. If a part of the program accounts for a fraction *p* of the running time and you make that part *s* times faster, the whole program gets faster by

  speed-up = 1 / ((1 − p) + p / s)

Making a function that takes 5% of the time infinitely fast saves at most 5%. Making a function that takes 60% of the time twice as fast saves 30%. The first job of performance work is therefore to find the large fractions, and that requires measurement.

**Listing 1** — Amdahl's law: what is an optimisation worth? *(illustrative example, not from the talk)*

```cpp
#include <iostream>

// Overall speed-up when a fraction p of the runtime becomes s times faster.
double amdahl(double p, double s) { return 1.0 / ((1.0 - p) + p / s); }

int main()
{
    std::cout << "5% of runtime, 10x faster:  " << amdahl(0.05, 10) << "x overall\n";   // ~1.05x
    std::cout << "5% of runtime, infinitely:  " << amdahl(0.05, 1e12) << "x overall\n"; // ~1.05x
    std::cout << "60% of runtime, 2x faster:  " << amdahl(0.60, 2) << "x overall\n";    // ~1.43x
}
```

The rest of the talk is about how to get that measurement with as little effort and disruption as possible, in real code and even in production.

## 2. The profiling landscape, and the timer we all write

*Talk segment: [1:15](https://www.youtube.com/watch?v=en4OQvZePqg&t=75s)*

### The current landscape

There are many tools for finding slow code. The slide lists some of the best known, noting that "this list is by far not complete":

| Benchmarking libraries | CPU profilers | Hardware / GPU profilers |
|---|---|---|
| Google Benchmark, nanobench, CppBenchmark, … | perf, Tracy Profiler, Visual Studio Performance Profiler, Very Sleepy | Intel VTune Profiler, AMD µProf, NVIDIA Nsight |

"All those tools are great. That's really important for me to say. … It's not a competition." Each solves a set of problems, but "it wasn't quite the set" his team had. They work in three broad ways.

### Sampling-based profiling

A sampling profiler is *a low-overhead statistical analysis of a running program*. It periodically interrupts the program from outside and records which function is executing, perhaps thousands of times per second. Functions that appear in many samples are where the time goes.

- ✅ **Overhead:** low, and you don't change your program.
- ❌ **Accuracy:** it's a statistical estimate. "The more often you check per second, … you increase the overhead, and the less often you check, then you have lower accuracy."

### Instrumentation-based profiling

An instrumentation profiler produces *a highly detailed trace of program execution* by injecting measurement code (probes) at the entry and exit of functions. You get the exact duration of every call.

- ✅ **Accuracy:** every call is measured.
- ❌ **Overhead:** the probes themselves cost time, and you have to add them.

### Micro-benchmarking

A micro-benchmark is *a controlled experiment to measure a single function or algorithm*. The benchmark framework runs the function in a loop many times to get a stable average. It is excellent for comparing two implementations of one small piece of code. Its limits come up in the next section.

### The timer everyone writes

"Before we dive deeper into the problem, I want to have a really quick look at this function":

**Listing 2** — code from the talk ((code slide)), shown at [4:27](https://www.youtube.com/watch?v=en4OQvZePqg&t=267s)

```cpp
void important_function() {
    auto start = high_resolution_clock::now();
     // Simulate some work
    this_thread::sleep_for(milliseconds(100));
    auto end = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(end - start);
    cout << "took: " << duration.count() << " ms" << endl;
}
```

Record the time, do the work (here a 100 ms sleep standing in for "most important work"), record the time again, print the difference. "Who of you guys wrote something like this?" About 95% of the room raises a hand. "And I did too. I did so many times, and also when I was using those tools, I went back to manual timing more often than I would like to admit."

This code has real problems:

- **Every exit path needs its own timing code.** An early `return` or an exception skips the end of the measurement.
- **`high_resolution_clock` may not be steady.** On some platforms it is an alias for `system_clock`, which can jump when the wall clock is adjusted. `std::chrono::steady_clock` is the right clock for intervals.
- **One measurement, printed immediately.** There are no statistics and no call counts, and printing inside the measured code distorts the timing.
- **It doesn't scale.** Timing fifty functions this way means fifty copies of the boilerplate.

The first problem has a classic C++ solution, **RAII**: start the clock in a constructor, stop it in the destructor. The destructor runs however the scope is left. That idea is at the heart of the tool presented in this talk.

**Listing 3** — An RAII scope timer that survives early returns and exceptions *(illustrative example, not from the talk)*

```cpp
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <thread>

class scope_timer {
public:
    explicit scope_timer(std::string_view name) : name_(name), start_(std::chrono::steady_clock::now()) {}
    ~scope_timer()
    {
        auto us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start_);
        std::cout << name_ << " took " << us.count() << " us\n";
    }
private:
    std::string_view name_;
    std::chrono::steady_clock::time_point start_;
};

int work(bool fail, bool early)
{
    scope_timer t("work");                         // one line, every exit path is measured
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    if (early) return 1;                           // still timed
    if (fail) throw std::runtime_error("oops");    // still timed (during stack unwinding)
    return 0;
}

int main()
{
    work(false, true);
    try { work(true, false); } catch (const std::exception&) {}
}
```

## 3. Why existing tools didn't fit: legacy code

*Talk segment: [2:32](https://www.youtube.com/watch?v=en4OQvZePqg&t=152s)*

### What we need in practice, and why it's hard

The speaker imagines a whiteboard of properties an ideal tool would have: fast, accurate, free, easy to use. He then turns the wish list around: "If we invert this question, … this is also why this is hard, because each of the tools you saw on the previous slide solves some of those problems, but not all of them."

### Problem 1: legacy code

Micro-benchmark code looks much the same in every library. Here it is in nanobench:

**Listing 4** — code from the talk (Simple Integration – Legacy Code), shown at [5:59](https://www.youtube.com/watch?v=en4OQvZePqg&t=359s)

```cpp
nanobench::Bench().minEpochIterations(1000000).run("add",
[&] {
        result = simple_add(x, y);
        nanobench::doNotOptimizeAway(result);
    });
```

You configure the run (here at least a million iterations per epoch), wrap the function to measure in a lambda, and use `doNotOptimizeAway` so the compiler can't delete the work, since the result is otherwise unused. The library runs the lambda many times and reports careful statistics.

"Of course it has its upsides … but it also has its downsides. Often our legacy code bases look a bit more like this picture" (a tangled ball of cables).

- **You can't always isolate the code.** In a real system, the interesting function depends on global state, other subsystems, configuration and I/O. "Stuff interferes."
- **Micro-benchmarks have pitfalls.** The speaker points to an earlier CppCon 2025 talk devoted to them.
- **Behaviour changes when the code is not isolated.** "You optimise and you're very happy, … and later you put it in your big production code, it might behave completely different." In isolation the code's data and instructions sit hot in the caches, branches become perfectly predictable, and the compiler can inline across the benchmark. Embedded in a large program, none of that holds.
- **You may want to measure only part of a function.** "We have a function which is like 200 lines, … oh God why, but it is what it is." The setup at the top and the logging at the bottom may be irrelevant, and only the middle matters.

The last point is easy with the RAII idea from the previous section. A scope timer measures exactly the block it lives in, so an extra pair of braces carves out the region of interest:

**Listing 5** — Measuring just the middle of a long function with a nested scope *(illustrative example, not from the talk)*

```cpp
#include <chrono>
#include <iostream>
#include <numeric>
#include <thread>
#include <vector>

struct scope_timer {
    const char* name;
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    ~scope_timer()
    {
        auto us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start);
        std::cout << name << ": " << us.count() << " us\n";
    }
};

long long long_legacy_function()
{
    std::vector<int> data(1'000'000);                      // setup we don't care about
    std::iota(data.begin(), data.end(), 0);

    long long sum = 0;
    {
        scope_timer t{"core loop"};                        // measure only this block
        for (int x : data) sum += static_cast<long long>(x) * x % 7;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(5));   // "logging" we don't care about
    return sum;
}

int main() { std::cout << long_legacy_function() << '\n'; }
```

That is still hand-written, single-shot timing, though. The next sections collect the rest of the wish list.

## 4. Requirements: integration, dev and production, customisation, insights

*Talk segment: [4:14](https://www.youtube.com/watch?v=en4OQvZePqg&t=254s)*

The rest of the wish list falls into four groups. Each line is something the team needed and couldn't get from one existing tool.

### Simple integration

- **Minimal build-system impact.** Whether the project uses CMake or a package manager, adding the tool should be trivial.
- **Zero or minimal dependencies.** "More or less by definition, … the more dependencies you add, you add complexity to your code, and if you want to make things go fast, adding complexity is like the last thing you want."
- **Broad compiler compatibility.** "I want to use it on every project," on Windows and Linux, with MSVC, GCC and Clang.
- **Compatibility with older C++ standards.** This is a compromise, because newer standards allow "cooler stuff", but support fewer code bases.
- **A non-invasive API.** Sampling profilers change nothing. If a tool must change code, the change should be tiny.

### Development and production

- **The same tool in both.**
- **Resilient to compiler flags.** It should work in debug and release builds, and the measurements must match the optimised code you ship.
- **Precise control over what is tracked.** In production you might track a few major functions "to see if the system is generally behaving well"; in development, everything.

Production adds its own demands:

- **Export the results** to a log, a database or anything else, so they can be collected over time.
- **Very low overhead**, because it slows down the production system.
- **Handle millions of events.** The speaker recalls collecting around 100 million events with the Visual Studio profiler, after which loading the session "takes ages, sometimes it crashes".
- **Can be easily disabled**, with a *guarantee* that the disabled program behaves exactly as if the tool weren't there. "There are scenarios where every milli- or nanosecond matters."

That guarantee is usually achieved with a macro that expands to nothing when a compile-time switch is set. Then the disabled build contains no trace of the instrumentation at all:

**Listing 6** — A tracking macro that disappears completely when disabled *(illustrative example, not from the talk)*

```cpp
#include <chrono>
#include <cstdio>

#ifndef MYTRACK_DISABLE
struct track_scope {
    const char* fn;
    std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
    ~track_scope()
    {
        auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - t0).count();
        std::printf("%s: %lld ns\n", fn, static_cast<long long>(ns));
    }
};
#define MYTRACK_CONCAT2(a, b) a##b
#define MYTRACK_CONCAT(a, b) MYTRACK_CONCAT2(a, b)
#define MYTRACK track_scope MYTRACK_CONCAT(mytrack_, __COUNTER__){__func__}
#else
#define MYTRACK               // expands to nothing: zero cost, nothing in the binary
#endif

int compute(int n)
{
    MYTRACK;
    int s = 0;
    for (int i = 0; i < n; ++i) s += i;
    return s;
}

int main() { return compute(1000) > 0 ? 0 : 1; }
// Build normally to track; build with -DMYTRACK_DISABLE for a binary identical to untracked code.
```

### Customisation

- **Easy to add new metrics**, for domain- or hardware-specific needs.
- **Flexible result storage**, including access to the results **in code**: "we're all coders after all, and if I get the result not just in some file, I get it directly in code, I can do more with it."
- **Custom workflows**, such as skipping the first minute of a run, or dumping results every hour.

### Insights

"Probably the most important": what the tool tells you.

- **Find the performance bottlenecks**, even in complex multithreaded programs. The dream is a tool that says "this is the function you need to look at".
- **Outlier detection**, for functions that are usually fast but sometimes take ages.
- **Source-level precision.** Not huge stack traces that end in the kernel or in an inlined allocator, but where the time actually goes in your code.
- **Call frequency, thread counts, and the impact on overall performance.**
- **Coverage validation.** How much of the total runtime did the tracked code cover? Without this, you can spend a week optimising code that turns out to be 20% of the runtime.
- **Statistics for data-driven decisions**, so an optimisation measured on one machine carries over to others.
- **A thread performance overview.**

"So yeah, those were exactly the pain points we had. So we built CTRACK basically to solve that subset for ourselves."

## 5. CTRACK and its design philosophy

*Talk segment: [7:16](https://www.youtube.com/watch?v=en4OQvZePqg&t=436s)*

### The library

The result is **CTRACK** (github.com/Compaile/ctrack):

- A **single-header** library for **C++17 and newer**.
- It supports all major compilers and compiles **without warnings**.
- It is **open source** under the MIT licence, with **zero dependencies**.
- It works via **CMake**, and the community has packaged it for several package managers.

"Basically, yeah, you can use it as a single-header library." Most of the wish list's integration points are met simply by being one header with no dependencies.

### Design philosophy

The slide sets out the priorities explicitly:

1. **Minimise runtime impact.** The profiled application should run "as close to native speed as possible".
2. **Prioritise recording speed.** The **hot path**, capturing an event when a tracked scope ends, must be "extremely lightweight and efficient".
3. **Analysis speed is secondary.** The **cold path**, calculating the statistics, "can be more computationally intensive, as it occurs after data collection is complete".

"It's always a trade-off. If you calculate more during the recording, well, you save some time at the end, but it slows down your code more."

This *record now, analyse later* split is a pattern worth recognising, because it appears in many high-performance systems: loggers, tracers, metrics libraries. The hot path does the bare minimum: take a timestamp and append a small record to memory that only this thread touches. Everything that needs sorting, grouping, merging or allocating is postponed until someone asks for the results. The inside-CTRACK sections show how far this goes.

**Listing 7** — Record now, analyse later: a minimal hot-path / cold-path split *(illustrative example, not from the talk)*

```cpp
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <numeric>
#include <vector>

struct record { std::int64_t ns; };
std::vector<record> g_records;            // single-threaded sketch

inline void hot_path_record(std::int64_t ns) { g_records.push_back({ns}); }   // cheap: append only

void cold_path_report()                   // expensive work happens later, once
{
    std::vector<std::int64_t> v;
    for (auto r : g_records) v.push_back(r.ns);
    std::sort(v.begin(), v.end());
    double mean = std::accumulate(v.begin(), v.end(), 0.0) / v.size();
    std::cout << "calls " << v.size() << ", mean " << mean << " ns, median " << v[v.size() / 2] << " ns\n";
}

int main()
{
    g_records.reserve(100'000);
    for (int i = 0; i < 100'000; ++i) {
        auto t0 = std::chrono::steady_clock::now();
        volatile int x = i * i; (void)x;
        auto t1 = std::chrono::steady_clock::now();
        hot_path_record(std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
    }
    cold_path_report();
}
```

## 6. One line: CTRACK and result_print

*Talk segment: [7:44](https://www.youtube.com/watch?v=en4OQvZePqg&t=464s)*

### Just add one line

Back to "our very, very important function". Instead of the manual timer, you write one macro at the start of the scope:

**Listing 8** — code from the talk (CTRACK - Sample), shown at [15:43](https://www.youtube.com/watch?v=en4OQvZePqg&t=943s)

```cpp
void important_function() {
    CTRACK; // Just add this one line!
    // Simulate some work
    this_thread::sleep_for(milliseconds(100));
}
// Print CTRACK
ctrack::result_print()
```

The `CTRACK;` line tracks the whole function, and somewhere at the end of the program, `ctrack::result_print()` prints the results. (The slide omits the semicolon after `result_print()`.)

"Nowadays it's even faster to just include CTRACK, do this macro, and I have the results, … instead of starting to manually time stuff." The team noticed developers reaching for it even on small projects and scripts, "a good validation that it's easy to use".

To try it yourself, add the header to your include path and compile as C++17. For this chapter, the talk's examples were compiled against the real `ctrack.hpp` with both GCC 16 and Clang 20 in C++17 mode. One practical note from that check: CTRACK computes its statistics with C++17 parallel algorithms (`std::execution::par_unseq`, described later). With GCC's standard library, those are implemented on top of Intel TBB when TBB's headers are installed. On such a system the programs compiled but **failed to link** with undefined `tbb::` symbols. Either link with `-ltbb`, or define `CTRACK_DISABLE_EXECUTION_POLICY`, the library's own switch for sequential statistics. Both worked when tried for this chapter, and both builds produced the same tables. "Zero dependencies" holds for the header, but on Linux the parallel algorithms can pull in a run-time library.

### Reading the output

`result_print` writes two kinds of table to the terminal (with or without colour). `ctrack::result_as_string()` returns the same text, and `ctrack::result_get_tables()` returns the data as structures you can use in code. The slides convert the tables into readable form.

The **summary** says what was measured overall:

| Start | End | Time Total | Time Tracked | Time Tracked % |
|---|---|---|---|---|
| 2025-09-16 00:16:07 | 2025-09-16 00:16:10 | 100.16 ms | 100.09 ms | 99.92% |

**Time Tracked %** is the coverage metric from the wish list: how much of the program's lifetime fell inside tracked scopes. Here, almost all of it.

The **overview** table lists every tracked location:

| Filename | Function | Line | Calls | AE[1-99]% | AE[0-100]% | Time AE[0-100] | Time A[0-100] |
|---|---|---|---|---|---|---|---|
| main.cpp | important_function | 8 | 1 | 99.92% | 99.92% | 100.16 ms | 100.16 ms |

- **Filename, function, line** identify where the `CTRACK` macro is.
- **Calls** is how many times the scope ran.
- **A** stands for *time active* and **AE** for *time active exclusive*, explained in the next section. The bracketed ranges say which calls are included: [0-100] means all of them, and [1-99] means all except the fastest and slowest 1 percent.

By default the overview is sorted by time active exclusive, so the most expensive functions are at the top.

Running the talk's sample for this chapter (CTRACK 1.1.0, GCC 16, Linux) printed `important_function`, 1 call, 99.97%, 100.17 ms, essentially the slide's numbers. The real terminal output differs from the slide's prettified tables in small ways: it labels the coverage columns `time ctracked` and `time ctracked %`, and writes microseconds as `mcs`.

Getting the results in code makes it easy to log them, compare runs or fail a test when something regresses:

**Listing 9** — Getting CTRACK results as a string and as tables (requires ctrack.hpp, C++17) *(illustrative example, not from the talk)*

```cpp
// Compile: g++ -std=c++17 -I<ctrack>/include  (add -ltbb, or -DCTRACK_DISABLE_EXECUTION_POLICY, with libstdc++ + TBB headers)
#include "ctrack.hpp"
#include <chrono>
#include <iostream>
#include <thread>

void important_function()
{
    CTRACK;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

int main()
{
    important_function();
    std::string report = ctrack::result_as_string();    // same text as result_print
    std::cout << report.substr(0, 200) << "...\n";
    // ctrack::result_get_tables() returns ctrack_result_tables with summary and detail rows
}
```

## 7. Time active and time active exclusive

*Talk segment: [8:57](https://www.youtube.com/watch?v=en4OQvZePqg&t=537s)*

### Two functions, two views

The two metrics in the overview table are easiest to understand with an example:

**Listing 10** — code from the talk (Time Active), shown at [17:55](https://www.youtube.com/watch?v=en4OQvZePqg&t=1075s)

```cpp
void slow_function() {
    CTRACK;
    this_thread::sleep_for(milliseconds(900));
}
void fast_function() {
    CTRACK;
    this_thread::sleep_for(milliseconds(100));
    slow_function();
}
```

`slow_function` sleeps for 900 ms ("eternity, super, super slow"). `fast_function` itself only sleeps for 100 ms, but then calls `slow_function`. Suppose the program runs `fast_function` once, for one second in total.

### Time active (inclusive)

**Time active** answers the top-down question: *while was this function running?* It counts the whole time between entering and leaving the function, including everything it called.

| | slow_function | fast_function |
|---|---|---|
| Time Active (ms) | 900 | 1000 |
| Time Active (%) | 90% | 100% |

`fast_function` was on the stack for the whole second, so its time active is 100%. That's useful for a top-down tour ("where should I start?"), but it doesn't point at the culprit. The 100% belongs mostly to its child.

### Time active exclusive

**Time active exclusive** subtracts the time spent in *tracked* children:

| | slow_function | fast_function |
|---|---|---|
| Time Active-Exclusive (ms) | 900 | 100 |
| Time Active-Exclusive (%) | 90% | 10% |

Running the example under CTRACK for this chapter gave almost exactly these numbers: `slow_function` 900.11 ms exclusive (89.96%), and `fast_function` 100.43 ms exclusive (10.04%) with 1.00 s inclusive.

Now the bottleneck is obvious: `slow_function` owns 90% of the runtime, and `fast_function`'s own work is just 10%. "This is the one I'm using to find out what's slow."

The slides draw both as timelines. *Time active* spans the whole bar of each function. *Time active exclusive* covers only the parts of a function's bar not covered by a tracked child's bar.

Other profilers call these **inclusive** (or *total*) and **exclusive** (or *self*) time. Two details are specific to an instrumentation tool like CTRACK:

- **"Tracked" matters.** Only children that have their own `CTRACK` are subtracted. Untracked callees count as the parent's own time, which is often what you want, because you can add `CTRACK` lower down to drill in.
- **These are wall-clock times**, so a function blocked on I/O or a lock is "active" while it waits. That is deliberate: waiting time is often exactly the bottleneck.

How exclusive time is computed from recorded events is a simple interval calculation: take the function's interval and subtract the union of its children's intervals.

**Listing 11** — Inclusive and exclusive time from nested intervals *(illustrative example, not from the talk)*

```cpp
#include <algorithm>
#include <iostream>
#include <vector>

struct interval { double start, end; };

// Length of the union of intervals (they may overlap).
double union_length(std::vector<interval> v)
{
    std::sort(v.begin(), v.end(), [](auto a, auto b) { return a.start < b.start; });
    double total = 0, cur_s = 0, cur_e = -1;
    for (auto [s, e] : v) {
        if (s > cur_e) { total += std::max(0.0, cur_e - cur_s); cur_s = s; cur_e = e; }
        else cur_e = std::max(cur_e, e);
    }
    return total + std::max(0.0, cur_e - cur_s);
}

int main()
{
    interval fast{0, 1000};                       // fast_function: 0..1000 ms
    std::vector<interval> children{{100, 1000}};  // slow_function called after 100 ms of own work

    double inclusive = fast.end - fast.start;
    double exclusive = inclusive - union_length(children);
    std::cout << "fast_function inclusive " << inclusive << " ms, exclusive " << exclusive << " ms\n";  // 1000, 100
}
```

(Recursion needs care, since a function is its own child. CTRACK's test suite has a category for it, as the open-source section describes.)

## 8. Multiple threads

*Talk segment: [10:06](https://www.youtube.com/watch?v=en4OQvZePqg&t=606s)*

### A small threaded program

"It also needs to work with multiple threads":

**Listing 12** — code from the talk ((code slide)), shown at [20:13](https://www.youtube.com/watch?v=en4OQvZePqg&t=1213s)

```cpp
atomic<bool> ready_flag{false};
void child_function() {
    CTRACK;
    this_thread::sleep_for(milliseconds(900));
}
//Thread 2
void worker_thread() {
    CTRACK;
    this_thread::sleep_for(milliseconds(100));
    child_function();
    ready_flag.store(true);
}
//Thread 1
void waiting_thread() {
    CTRACK;
    cout << "[Thread 1] Waiting for ready signal..." << endl;
    while (!ready_flag.load()) {
        this_thread::sleep_for(milliseconds(10));
    }
    cout << "[Thread 1] Got signal! Exiting." << endl;
}
```

The worker thread runs `worker_thread`: 100 ms of its own work, then `child_function` for 900 ms, then it sets an atomic flag. The other thread runs `waiting_thread`, which busy-polls the flag every 10 ms ("let's generate some CPU load") until it is set, then exits. All three functions are tracked.

### The result

| Filename | Function | Line | Calls | AE[1-99]% | AE[0-100]% | Time AE[0-100] | Time A[0-100] |
|---|---|---|---|---|---|---|---|
| main.cpp | waiting_thread | 34 | 1 | 99.94% | 99.94% | 1.01 s | 1.01 s |
| main.cpp | child_function | 12 | 1 | 89.30% | 89.30% | 900.16 ms | 900.16 ms |
| main.cpp | worker_thread | 20 | 1 | 9.96% | 9.96% | 100.41 ms | 1.00 s |

"It's precisely what we would expect":

- `waiting_thread` was active for essentially the whole run, about 1 s, since it waits for everything else to finish.
- `child_function` accounts for about 900 ms (roughly 90%).
- `worker_thread`'s *exclusive* time is about 100 ms (about 10%), while its *inclusive* time is the full second.

"It's not perfect. … I've been running this on Windows, after all. So we have some variance." On Linux, the same program measured for this chapter gave 99.95%, 89.76% and 10.03%. Sleeps and timer resolution vary by operating system.

### Percentages with threads

The percentages are relative to the program's **total wall-clock time**, not the sum of all threads' time. That is why the percentages in the table add up to about 200%: two threads were busy at the same time. Each row answers *for what fraction of the run was this function active?* That fraction can be large for several functions at once.

The slide's waiting thread polls a flag in a sleep loop, which is fine for a demonstration. In real code, C++20 offers blocking primitives that wait without polling and wake immediately:

**Listing 13** — The same hand-off without polling (C++20 latch) *(illustrative example, not from the talk)*

```cpp
#include <chrono>
#include <iostream>
#include <latch>
#include <thread>

int main()
{
    using namespace std::chrono_literals;
    std::latch ready{1};

    std::jthread worker([&] {
        std::this_thread::sleep_for(100ms);   // worker_thread's own work
        std::this_thread::sleep_for(900ms);   // child_function
        ready.count_down();                   // instead of ready_flag.store(true)
    });

    std::jthread waiter([&] {
        std::cout << "[Thread 1] Waiting for ready signal..." << std::endl;
        ready.wait();                         // blocks without burning CPU
        std::cout << "[Thread 1] Got signal! Exiting." << std::endl;
    });
}
```

## 9. Outliers and the detail view

*Talk segment: [10:28](https://www.youtube.com/watch?v=en4OQvZePqg&t=628s)*

### Why exclude outliers: the [1-99]% range

The overview table reports AE[1-99]%: exclusive time with the fastest 1% and slowest 1% of calls left out. "This is an idea we stole from game benchmarks or system benchmarks." You don't care about the single fastest or slowest event; "you want to know, generally, a normal event, how fast is it."

The slide's reasons:

- **Mitigates system noise.** Other processes, interrupts and rare cache misses produce occasional slow calls.
- **Isolates algorithmic cost.** It leaves out the first call of a function that does one-time start-up, or a call that returns immediately on null input. Neither is typical.
- **Establishes a stable baseline** that is comparable from run to run.
- **Quantifies performance jitter**, by comparing the trimmed value with the full range.
- **Reveals "hot path" performance**, what the function costs when doing its normal job.

"Of course, this range can be custom." 1% on each side is the default; the customisation section shows how to change it.

### The detail table

For each tracked location, the details view has two parts. The first summarises the distribution:

| Field | Meaning |
|---|---|
| Source code location | file, function, line |
| Calls | number of executions |
| Threads | how many different threads executed it |
| Time Acc | **accumulated** time: the sum of all call durations |
| SD | standard deviation of the call durations |
| CV | coefficient of variation = SD / mean |

**Time Acc differs from time active.** "Let's say we have one function which gets called from four threads at the same time. It takes 1 second. It would in total take four seconds" accumulated, but from a top-down view only one second of wall time passed. Both numbers are useful: accumulated time measures total work (CPU or thread time spent), and active time measures latency.

**CV makes variability comparable across scales.** A function whose calls range from 1 ms to 100 ms varies far more than one that takes 9 to 10 minutes, even though the second has a much larger SD in absolute terms. Dividing by the mean removes the unit. As a rule of thumb from the talk, values above 1 mean very high variability, and well-behaved functions are far below that.

The second part splits the calls into three groups, **Fastest [0-1]%**, **Center [1-99]%** and **Slowest [99-100]%**, and gives each group's minimum, mean, median and maximum as applicable. The centre group also shows its time active and time active exclusive. For the single-call `important_function` example, every call falls into the centre group, so the fastest and slowest columns show 0. The accuracy example next fills them in.

The statistics themselves are standard. Computing them for a sample of call durations makes the meaning of each column concrete:

**Listing 14** — Mean, median, SD, CV and trimmed groups for a set of call durations *(illustrative example, not from the talk)*

```cpp
#include <algorithm>
#include <cmath>
#include <iostream>
#include <numeric>
#include <vector>

int main()
{
    std::vector<double> ms;                                   // 90 fast calls, 10 slow ones
    for (int i = 0; i < 100; ++i) ms.push_back(i % 10 == 0 ? 100.0 : 1.0);
    std::sort(ms.begin(), ms.end());

    auto mean = [](auto b, auto e) { return std::accumulate(b, e, 0.0) / (e - b); };
    double m = mean(ms.begin(), ms.end());
    double sd = std::sqrt(std::accumulate(ms.begin(), ms.end(), 0.0,
                          [m](double acc, double x) { return acc + (x - m) * (x - m); }) / ms.size());

    std::size_t cut = ms.size() * 1 / 100;                    // 1% on each side
    auto cb = ms.begin() + cut, ce = ms.end() - cut;
    std::cout << "mean " << m << " ms, median " << ms[ms.size() / 2] << " ms, SD " << sd
              << " ms, CV " << sd / m << '\n';
    std::cout << "fastest[0-1]% " << ms.front() << " ms, center[1-99]% mean " << mean(cb, ce)
              << " ms, slowest[99-100]% " << ms.back() << " ms\n";
}
```

## 10. Accuracy compared with a sampling profiler

*Talk segment: [12:05](https://www.youtube.com/watch?v=en4OQvZePqg&t=725s)*

### An experiment designed to trip up sampling

To compare with a sampling profiler, "we designed a small example". It has two functions:

**Listing 15** — code from the talk ((code slide)), shown at [24:35](https://www.youtube.com/watch?v=en4OQvZePqg&t=1475s)

```cpp
// Fast function called many times - often missed by sampling
void micro_operation()
{
    CTRACK;
    this_thread::sleep_for(milliseconds(1));
}
// Function with high variance - sampling misses the pattern
void variable_timing_function(bool slow_path)
{
    CTRACK;
    if (slow_path)
    {
        this_thread::sleep_for(milliseconds(100));
    }
    else
    {
        this_thread::sleep_for(milliseconds(1));
    }
}
```

- `micro_operation` is short (1 ms) and called very often, the kind of function "often missed by sampling".
- `variable_timing_function` usually takes 1 ms, but 100 ms on its slow path. It has high variance, "sampling misses the pattern".

**Listing 16** — code from the talk ((code slide)), shown at [24:47](https://www.youtube.com/watch?v=en4OQvZePqg&t=1487s)

```cpp
int main()
{
    const int micro_op_count = 1000;
    for (int i = 0; i < micro_op_count; ++i)
    {
        micro_operation();
    }
    for (int i = 0; i < 100; ++i)
    {
        bool slow = i % 10 == 0; // Every 10th call is slow
        variable_timing_function(slow);
    }
    return 0;
}
```

The micro-operation runs 1000 times (about 1 s). The variable function runs 100 times, and every tenth call is slow: 10 × 100 ms + 90 × 1 ms, also about 1 s. The slide annotates both loops with "~ 1 second expected". A good tool should report roughly a 50/50 split.

### The sampling profiler

In the Visual Studio sampling profiler, the first run attributed about 53% of samples to `micro_operation`, "which is sort of fine", but only around 10% to `variable_timing_function`. Much of the remaining time was attributed to a kernel function. Running it again "with standard Visual Studio settings" gave the variable function about 4%. "So I have quite a high variance between run to run, and this is a problem." In more complex code with even shorter functions, sampling may miss functions entirely. "That's a known sampling limitation."

Two effects are at work here:

- **Sampling is statistical.** With a finite number of samples, short or rare events are estimated with large relative error, and two runs disagree.
- **Many profilers sample CPU time.** A thread blocked in `sleep_for` is not on a CPU, so it produces few samples in user code, and its time appears under the kernel's wait functions or not at all. An instrumentation tool that measures wall-clock time between scope entry and exit has no such blind spot.

The first effect is easy to demonstrate. Simulate a profiler that takes 100 random samples of a timeline in which the two functions really take 50% each, and look at the spread of its estimates:

**Listing 17** — Why sampling estimates vary from run to run *(illustrative example, not from the talk)*

```cpp
#include <algorithm>
#include <iostream>
#include <random>

// A 2-second timeline: [0,1) is function A, [1,2) is function B (truly 50/50).
// A profiler takes `samples` random snapshots and estimates A's share.
int main()
{
    std::mt19937 rng(7);
    std::uniform_real_distribution<double> when(0.0, 2.0);
    for (int samples : {20, 100, 1000}) {
        double lo = 1, hi = 0;
        for (int run = 0; run < 50; ++run) {
            int a = 0;
            for (int i = 0; i < samples; ++i) a += when(rng) < 1.0;
            double share = double(a) / samples;
            lo = std::min(lo, share); hi = std::max(hi, share);
        }
        std::cout << samples << " samples/run: A's estimated share ranged " << lo * 100 << "% .. " << hi * 100 << "%\n";
    }
}
```

When run for this chapter, 50 simulated runs of 20 samples estimated function A's share anywhere from 35% to 75%; with 100 samples, 38% to 58%; with 1000 samples, 46% to 53%. The truth is 50% in every case.

### CTRACK on the same program

| Function | Calls | AE[1-99]% | AE[0-100]% | Time AE[0-100] |
|---|---|---|---|---|
| variable_timing_function | 100 | 46.39% | 51.07% | 1.11 s |
| micro_operation | 1000 | 47.48% | 48.90% | 1.06 s |

Total time is 2.17 s, of which 99.97% was tracked, and the split is the expected half and half. "That's exactly what we expect." (Real sleeps last a little longer than requested, so 1000 × 1 ms is about 1.06 s.)

The details explain the two functions' characters:

| Function | Time Acc | SD | CV | Fastest min | Center mean | Center median | Slowest mean |
|---|---|---|---|---|---|---|---|
| variable_timing_function | 1.11 s | 29.71 ms | 2.68 | 1.03 ms | 10.27 ms | 1.11 ms | 100.42 ms |
| micro_operation | 1.06 s | 153.01 µs | 0.14 | 1.02 ms | 1.05 ms | 1.04 ms | 2.05 ms |

Measured for this chapter on Linux, CTRACK reported 49.44% for `variable_timing_function` and 50.50% for `micro_operation` (1.10 s and 1.13 s), with CVs of 2.69 and 0.03: the same picture. Linux's 1 ms sleeps were even more regular than on the speaker's Windows machine.

- The variable function's **CV of 2.68** flags high variability, while the micro-operation's 0.14 shows it is stable.
- The **fastest** calls of the variable function take about 1 ms, the **slowest** about 100 ms, matching the two paths in the code.
- Its centre group's **mean (10.27 ms) is far above its median (1.11 ms)**. Most calls are fast, but the slow ones pull the mean up. Whenever mean and median diverge like this, look for a slow path.

## 11. Customisation

*Talk segment: [13:37](https://www.youtube.com/watch?v=en4OQvZePqg&t=817s)*

### Custom names

"Sometimes I want to name my function a specific way":

**Listing 18** — code from the talk (Customization), shown at [27:27](https://www.youtube.com/watch?v=en4OQvZePqg&t=1647s)

```cpp
void example_function() {
    CTRACK_NAME("my custom name");
}
```

`CTRACK_NAME("...")` tracks the scope like `CTRACK`, but reports it under the given name instead of the function name.

### Development and production tracks

**Listing 19** — code from the talk (Customization), shown at [27:53](https://www.youtube.com/watch?v=en4OQvZePqg&t=1673s)

```cpp
void example_function() {
    CTRACK_DEV;
}
void important_function() {
    CTRACK_PROD;
}
```

Mark scopes with `CTRACK_DEV` or `CTRACK_PROD`, "and you can compile in such a way that on your development machine you get everything, … and on the production machine the development tracks are disabled." In the real header, plain `CTRACK` is an alias for `CTRACK_PROD`, and the switches are preprocessor macros:

| Define | Effect |
|---|---|
| *(none)* | `CTRACK_DEV` and `CTRACK_PROD` both track |
| `CTRACK_DISABLE_DEV` | `CTRACK_DEV` expands to nothing |
| `CTRACK_DISABLE_PROD` | `CTRACK_PROD` (and `CTRACK`) expand to nothing |
| `CTRACK_DISABLE` | everything expands to nothing |

From the Q&A: since the tracking type is just a macro, it's easy to define your own groups (for A/B tests, say) the same way.

### Sub-scopes

"For long functions we might also add subscopes":

**Listing 20** — code from the talk (Customization), shown at [28:27](https://www.youtube.com/watch?v=en4OQvZePqg&t=1707s)

```cpp
void complex_algorithm() {
    CTRACK; // Track the whole function
    {
        CTRACK_NAME("algorithm_init");
        this_thread::sleep_for(milliseconds(10));
    }
    for (int i = 0; i < 3; ++i) {
        CTRACK_NAME("algorithm_iteration");
        this_thread::sleep_for(milliseconds(15));
    }
}
```

The outer `CTRACK` tracks the whole function. The initialisation block and each loop iteration get their own named entries, because a `CTRACK` lives exactly as long as its enclosing `{ }`. This is how you measure "the middle part" of the 200-line legacy function from the wish list. Thanks to exclusive time, the outer function's own share then shows only the time not covered by the sub-scopes.

How can a function contain several `CTRACK`s without name clashes? The real macro declares an object whose name is built with `__COUNTER__`, a compiler extension (supported by GCC, Clang and MSVC) that yields a new integer at each use. Every expansion therefore gets a unique variable name.

### Result settings

**Listing 21** — code from the talk (Customization), shown at [29:07](https://www.youtube.com/watch?v=en4OQvZePqg&t=1747s)

```cpp
ctrack::ctrack_result_settings custom_settings;
custom_settings.non_center_percent = 2; // Use [2-98]
custom_settings.min_percent_active_exclusive = 1.0;
custom_settings.percent_exclude_fastest_active_exclusive = 0.0;
```

- `non_center_percent = 2` makes the "centre" range [2-98] instead of [1-99].
- `min_percent_active_exclusive = 1.0` hides entries whose exclusive time is under 1% of the total. "If you track everything you probably get a table like, woof, I don't know, 200 entries", and this filter keeps it readable.
- `percent_exclude_fastest_active_exclusive` drops the fastest N% of entries. Setting it to 90 keeps only the slowest 10%.

Pass the settings to `result_print`, `result_as_string` or `result_get_tables`.

The filter has a consequence worth knowing. When the slide's `complex_algorithm` was run with `min_percent_active_exclusive = 1.0`, the output listed `algorithm_iteration` (3 calls, 81.70%) and `algorithm_init` (18.18%), but **no row for `complex_algorithm` itself**. Its sub-scopes cover practically all of its time, so its exclusive time is nearly zero and falls below the 1% threshold. That is exactly what exclusive time is for, but don't be surprised when a tracked function seems to vanish.

### Outputs

The results can go to the terminal (with or without colour), to a string, or to files such as CSV, and you can access them directly in code via `ctrack_result_tables`.

**Listing 22** — Sub-scopes, custom names and filtered results (requires ctrack.hpp, C++17) *(illustrative example, not from the talk)*

```cpp
#include "ctrack.hpp"
#include <chrono>
#include <thread>

using namespace std::chrono;

void complex_algorithm()
{
    CTRACK;                                        // whole function
    {
        CTRACK_NAME("algorithm_init");
        std::this_thread::sleep_for(milliseconds(10));
    }
    for (int i = 0; i < 3; ++i) {
        CTRACK_NAME("algorithm_iteration");        // one entry, 3 calls
        std::this_thread::sleep_for(milliseconds(15));
    }
    CTRACK_DEV;                                    // second tracker in the same scope: unique name via __COUNTER__
}

int main()
{
    for (int i = 0; i < 10; ++i) complex_algorithm();

    ctrack::ctrack_result_settings s;
    s.non_center_percent = 5;                      // centre = [5-95]
    s.min_percent_active_exclusive = 1.0;          // hide entries below 1% exclusive time
    ctrack::result_print(s);
}
```

## 12. A real-world example: measure where it runs

*Talk segment: [14:55](https://www.youtube.com/watch?v=en4OQvZePqg&t=895s)*

### Six cameras and a surprise

"Before we do the deep dive into how this all works internally, I want to show a quick real-world example which I just had four weeks ago." The project is an embedded measurement system with **six industrial cameras**, three 2D and three 3D, so "a lot of data going in and out".

The speaker was developing on a high-end machine, running Linux through WSL on Windows. CTRACK's table there (columns: file, function, line, calls, AE[1-99]%) showed:

| Function | AE[1-99]% |
|---|---|
| `capture_manager::grab_frames` | **42.80%** |
| `resize_bgrx_to_rgb` | 11.30% |
| `dma_copy_to_userspace` | 9.00% |
| `depth_temporal_sync_and_fuse` | 7.70% |
| `segment_net::infer` | **6.40%** |
| `kcf_tracker::update` | 4.70% |
| `mjpeg_writer::write_frame` | 3.20% |

Grabbing frames dominated. "I was very sad to see, well, grabbing frames takes most of our time. … It's basically the camera, right? There's nothing I can do." The segmentation network looked "super fast. It's no problem."

"But since I had CTRACK installed, I could just run it on the production PC with one click and see how the output differs":

| Function | AE[1-99]% |
|---|---|
| `segment_net::infer` | **38.00%** |
| `resize_bgrx_to_rgb` | 12.70% |
| `capture_manager::grab_frames` | **9.30%** |
| `depth_temporal_sync_and_fuse` | 6.90% |
| `mask_morphological_ops` | 2.70% |
| `deepsort_tracker::update` | 1.80% |
| `dma_copy_to_userspace` | 1.10% |

On the production machine, frame grabbing was "way, way faster than on my machine". The networking path through WSL and the firewall had been slowing the camera packets on the development box. The real bottleneck was the segmentation network. With that knowledge the team made the network faster and more efficient, saving "a lot of the time on the actual production machine".

(The second table also lists different post-processing functions, such as a DeepSORT tracker instead of a KCF tracker, so the pipeline had evolved between the two runs. The headline shift is the same either way.)

### The lesson

Profiles depend on the environment as much as on the code: hardware, operating system, virtualisation, drivers, network, data. "If you run your code on a different machine, you might get much different results. So it's really important to measure as often as you can", and above all **on the machine where the code actually runs**. Low overhead and a production-safe build are what make that cheap enough to do routinely.

## 13. Inside CTRACK: events and RAII

*Talk segment: [15:33](https://www.youtube.com/watch?v=en4OQvZePqg&t=933s)*

### What gets recorded

"Starting very simple, this is basically what we record":

**Listing 23** — code from the talk ((code slide)), shown at [31:25](https://www.youtube.com/watch?v=en4OQvZePqg&t=1885s)

```cpp
struct Event
{
    time_point start_time;
    time_point end_time;
    std::string_view filename;
    std::string_view function;
    int line;
    int thread_id;
    unsigned int event_id;
}
```

Each event is one execution of a tracked scope: start and end times, where it happened (file, function, line), on which thread, and an event ID unique within that thread. (The slide leaves out the closing semicolon.)

Note that `filename` and `function` are `std::string_view`s, not `std::string`s. They point at the string literals the compiler generates for the file and function names, which live for the whole program, so recording an event copies two pointer-and-length pairs and never allocates. (An audience member raised this in the Q&A; it is one of the small decisions that keep the hot path cheap.)

### RAII timing

"CTRACK does use resource acquisition is initialisation." Writing `CTRACK;` declares an object:

**Listing 24** — code from the talk (How does CTRACK work), shown at [32:31](https://www.youtube.com/watch?v=en4OQvZePqg&t=1951s)

```cpp
void my_function() {
    CTRACK;
// Expands to:
EventHandler _ctrack_instance{__LINE__,
__FILE__, __FUNCTION__};

    // Your code here
    if (error) return;
// Destructor called here

    // More code
}  // Destructor called here
```

- The **constructor** starts the timer when the object is created at the start of the scope.
- The **destructor** stops it and records the event when the object goes out of scope.
- **Zero manual intervention.** The `return` in the middle and the closing brace at the end are both exits, and the destructor runs on both. Nobody has to remember to stop the timer.
- **Exception-safe.** Destructors run during stack unwinding, so the event is recorded even if an exception escapes.
- **Minimal overhead**, as the next section shows.

"I'm sure most of you implemented, for example, a lock guard … instead of manually locking and unlocking a mutex. That's basically the same."

The slide's expansion is simplified. In the real header, `CTRACK` expands to

```
ctrack::EventHandler ctrack_instance_N { __builtin_LINE(), __builtin_FILE(), __builtin_FUNCTION() }
```

with `N` from `__COUNTER__`, so that several trackers can share a scope. GCC, Clang and MSVC all provide the `__builtin_*` functions, which return the location of the call site (C++20's `std::source_location` is the standard equivalent). The slide writes the more familiar `__LINE__`, `__FILE__` and `__FUNCTION__`.

### Advantages over manual timing

The slide sets the macro against the manual version from the start of the talk:

**Listing 25** — code from the talk (Key Advantages Over Manual Timing), shown at [32:55](https://www.youtube.com/watch?v=en4OQvZePqg&t=1975s)

```cpp
void important_function() {
    auto start = high_resolution_clock::now();
    // Simulate some work
    this_thread::sleep_for(milliseconds(100));
    auto end = high_resolution_clock::now();
    auto duration =
duration_cast<milliseconds>(end - start);
    cout << "took: " << duration.count() << "
ms" << endl;
}
```

1. **Automatic.** There is no need to stop timers manually.
2. **Complete.** It captures all exit paths.
3. **Clean.** One line instead of several timing statements.
4. **Safe.** Exception-safe by design.
5. **Fast.** Optimised for production use.

And, most importantly, "you get more insights": statistics across all calls and threads, not a single printed number.

Here is a self-contained miniature of the same design: an event struct, an RAII handler and a store, without any of CTRACK's optimisations. It is useful for seeing the moving parts in a few dozen lines:

**Listing 26** — A miniature RAII event recorder *(illustrative example, not from the talk)*

```cpp
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <vector>

using clock_type = std::chrono::steady_clock;

struct Event {
    clock_type::time_point start_time, end_time;
    std::string_view function;   // points at a string literal: no allocation
    int line;
};

std::vector<Event> g_events;

class EventHandler {
public:
    EventHandler(int line, std::string_view fn) : line_(line), fn_(fn), start_(clock_type::now()) {}
    ~EventHandler() { g_events.push_back({start_, clock_type::now(), fn_, line_}); }   // runs on every exit
private:
    int line_;
    std::string_view fn_;
    clock_type::time_point start_;
};

#define MINI_TRACK EventHandler mini_track_instance{__LINE__, __func__}

int risky(int x)
{
    MINI_TRACK;
    if (x == 0) return -1;                       // early return: recorded
    if (x < 0) throw std::invalid_argument("x"); // exception: recorded during unwinding
    return x * 2;
}

int main()
{
    risky(0); risky(5);
    try { risky(-1); } catch (...) {}
    for (const auto& e : g_events)
        std::cout << e.function << ':' << e.line << " took "
                  << std::chrono::duration_cast<std::chrono::nanoseconds>(e.end_time - e.start_time).count() << " ns\n";
}
```

## 14. Lock-free recording and implicit call graphs

*Talk segment: [16:35](https://www.youtube.com/watch?v=en4OQvZePqg&t=995s)*

"We record all sorts of stuff and can produce all those metrics, but it should still be fast. So how do we do that?" The speaker picks two of the techniques.

### Per-thread data buffers: zero contention

Events from all threads must end up somewhere that the analysis can reach later, even after the threads that recorded them have exited. The obvious solution, one shared vector protected by a mutex, would make every recorded event a potential point of contention between threads. Instead:

**Listing 27** — code from the talk ((code slide)), shown at [34:59](https://www.youtube.com/watch?v=en4OQvZePqg&t=2099s)

```cpp
// Global store with per-thread vectors
struct store {
    inline static std::deque<t_events> a_events{};   // One vector per thread
    inline static std::mutex event_mutex;            // Used ONLY during
setup
};
// Thread-local pointers to this thread's buffer
inline thread_local t_events* event_ptr = nullptr;
// First-time thread setup (happens once on thread spawn)
int fetch_event_t_id() {
    if (thread_id == nullptr) {
        std::scoped_lock lock(store::event_mutex); // ONLY lock
        store::a_events.emplace_back(t_events{});   // Create buffer
        event_ptr = &store::a_events[*thread_id];   // Point to it
        event_ptr->reserve(100);                    // Pre-allocate
    }
    return *thread_id;
}
// Recording (no locks!)
event_ptr->emplace_back(Event{...}); // Just append to vector
```

- **One buffer per thread.** `store::a_events` holds one event vector (`t_events`) per thread.
- **The only lock is taken once per thread.** The first time a thread records an event, it locks `event_mutex`, appends a fresh vector for itself, and stores a pointer to it in the `thread_local` variable `event_ptr`. "I'm very sorry, but spawning a thread, we need to wait a tiny bit if we spawn them at the same time."
- **Pre-allocation.** `reserve` gives each buffer room for its first events, avoiding a burst of early reallocations.
- **The hot path is lock-free.** Recording is `event_ptr->emplace_back(Event{...})`: an append to a vector that only this thread touches. "No blocking, no checking. All the vectors can basically just fire as they want."

Why a `std::deque` for the outer container? Each thread keeps a raw pointer to *its* vector inside the store. A `std::vector` of vectors would move every inner vector whenever it grew, leaving the threads' pointers dangling. A deque never moves its existing elements when you add to the end, so the pointers stay valid. (The slide is simplified: in the real header, thread IDs are also handed out from a second deque under the same one-time lock.)

The result, from the slide: thread-local pointers mean "each thread owns its data structures", there is a single mutex lock per thread lifetime, the hot path has no synchronisation, and the design scales: "N threads = N× throughput". The speaker measured "around 75, 80 million events per second" on his laptop.

**Growth strategy is a trade-off.** The team experimented with custom vector growth. Growing more aggressively costs memory but reallocates less often; growing more conservatively saves memory but adds overhead. In the Q&A he notes they settled on "a bit more aggressive growth strategy than std's default".

### Implicit call graph construction

Exclusive time needs to know which events are children of which. Recording an explicit call stack would cost time on every event. Instead, each thread keeps just a "current event ID", and RAII order does the rest:

**Listing 28** — code from the talk ((code slide)), shown at [36:39](https://www.youtube.com/watch?v=en4OQvZePqg&t=2199s)

```cpp
// Constructor - Entering a scope
EventHandler(...) {
    previous_event_id = *current_event_id;             // Save parent
    event_id = ++(*current_event_cnt);                 // Get new ID
    *current_event_id = event_id;                      // Become current
}
// Destructor - Leaving a scope
~EventHandler() {
    event_ptr->emplace_back(Event{...});               // Record self
    *current_event_id = previous_event_id;             // Restore parent
    if (previous_event_id > 0) {
        // Register as child of parent
        (*sub_events_ptr)[previous_event_id].push_back(event_id);
    }
}
```

- On **entry**, the handler remembers the current event as its parent, takes a new ID, and becomes the current event.
- On **exit**, it records itself, restores the parent as current, and appends its own ID to the parent's list of children.

"Even if you have many of those events chained, the constructor[s and destructors are] always guaranteed to resolve in the correct order." Scopes nest, so objects are destroyed in exactly the reverse order of construction, and the "current" pointer always walks back up correctly. That gives root → branch → leaf relationships "with zero explicit tracking", and **time active exclusive** follows: an event's time minus its children's.

Here is a minimal, thread-safe recorder that combines both ideas, per-thread buffers registered once and parent links maintained by RAII:

**Listing 29** — Per-thread buffers plus implicit parent links, in miniature *(illustrative example, not from the talk)*

```cpp
#include <chrono>
#include <deque>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

struct Event { unsigned id, parent; long long ns; const char* name; };

struct store {
    inline static std::deque<std::vector<Event>> buffers;   // deque: existing buffers never move
    inline static std::mutex mutex;                          // used only when a thread registers
};
inline thread_local std::vector<Event>* buf = nullptr;
inline thread_local unsigned current_id = 0, id_counter = 0;

std::vector<Event>& my_buffer()
{
    if (!buf) {
        std::scoped_lock lock(store::mutex);                 // once per thread
        buf = &store::buffers.emplace_back();
        buf->reserve(1024);
    }
    return *buf;
}

class Track {
    unsigned id_, parent_;
    const char* name_;
    std::chrono::steady_clock::time_point t0_ = std::chrono::steady_clock::now();
public:
    explicit Track(const char* name) : id_(++id_counter), parent_(current_id), name_(name) { current_id = id_; }
    ~Track()
    {
        auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - t0_).count();
        my_buffer().push_back({id_, parent_, ns, name_});    // lock-free: only this thread uses it
        current_id = parent_;                                // restore parent
    }
};

void leaf()  { Track t("leaf");  std::this_thread::sleep_for(std::chrono::milliseconds(5)); }
void outer() { Track t("outer"); leaf(); leaf(); }

int main()
{
    { std::jthread a(outer), b(outer); }
    for (auto& v : store::buffers)
        for (auto& e : v) std::cout << e.name << " id " << e.id << " parent " << e.parent << '\n';
}
```

## 15. Statistics, interval merging and versioned namespaces

*Talk segment: [18:39](https://www.youtube.com/watch?v=en4OQvZePqg&t=1119s)*

The final slides of the implementation part list three more techniques.

### Parallel analysis

The analysis runs only when results are requested, but "at the end I'm a developer and don't want to wait for my results." So the statistics use C++17's **parallel algorithms**:

**Listing 30** — code from the talk (Parallel Analysis), shown at [37:39](https://www.youtube.com/watch?v=en4OQvZePqg&t=2259s)

```cpp
constexpr auto execution_policy =
std::execution::par_unseq;
return std::transform_reduce(
    execution_policy, // Parallel execution
    vec.begin(), vec.end(),
    FieldType{}, std::plus<>(),
    [field](const auto &item) { return item.*field;
});
```

`std::transform_reduce` maps each element to a value (here, a member selected by a pointer-to-member `field`) and combines the results with `std::plus<>`. Passing an **execution policy** as the first argument lets the library run it in parallel: `std::execution::par_unseq` means "parallel and vectorised, in any order". That is safe because addition is associative and the transformation has no side effects.

"That's completely optional. You don't have to use it if your system or compiler doesn't support it." Some C++17 toolchains don't ship parallel algorithms. As the section on reading output noted, GCC's libstdc++ implements them with Intel TBB when TBB is installed, which then has to be linked; defining `CTRACK_DISABLE_EXECUTION_POLICY` switches them off.

### Interval merging

"Let's imagine we have four events, … 10 ms, and they all start at the same time." There are two different questions you can ask about them:

- **Accumulated time:** add them up, giving 40 ms of work.
- **Active (wall-clock) time:** "from a top-down view, … only 10 ms" passed.

To compute wall-clock time, overlapping intervals must be **merged** into their union:

**Listing 31** — code from the talk (Interval Merging), shown at [38:13](https://www.youtube.com/watch?v=en4OQvZePqg&t=2293s)

```cpp
// Problem: 4 threads × 10ms ≠ 40ms wall time!
sorted_create_grouped_simple_events(events) {
    for (size_t i = 1; i < events.size(); i++) {
        if (result[current].end_time >= events[i].start_time) {
        // Merge overlapping intervals
        result[current].end_time=
        max(result[current].end_time,events[i].end_time);
        } else {
            result.push_back(events[i]); // New interval
        }
    }
}
```

The events are sorted by start time first. Then each event either overlaps the current merged interval, in which case the end is extended to the later of the two ends, or it starts a new interval. Summing the merged intervals gives the wall-clock time covered. (The slide's comment uses ≠: 4 threads × 10 ms is *not* 40 ms of wall time.) The same algorithm also gives CTRACK's *time tracked* coverage and the subtraction of children for exclusive time.

**Listing 32** — Accumulated versus wall-clock time with interval merging *(illustrative example, not from the talk)*

```cpp
#include <algorithm>
#include <iostream>
#include <vector>

struct interval { double start, end; };

std::vector<interval> merge(std::vector<interval> v)
{
    std::sort(v.begin(), v.end(), [](const interval& a, const interval& b) { return a.start < b.start; });
    std::vector<interval> out;
    for (const auto& iv : v) {
        if (!out.empty() && out.back().end >= iv.start)
            out.back().end = std::max(out.back().end, iv.end);   // overlap: extend
        else
            out.push_back(iv);                                    // gap: new interval
    }
    return out;
}

int main()
{
    // Four threads, 10 ms each, starting 2 ms apart.
    std::vector<interval> events{{0, 10}, {2, 12}, {4, 14}, {6, 16}};
    double accumulated = 0, wall = 0;
    for (auto e : events) accumulated += e.end - e.start;
    for (auto m : merge(events)) wall += m.end - m.start;
    std::cout << "accumulated " << accumulated << " ms, wall-clock " << wall << " ms\n";   // 40, 16
}
```

### Versioned namespaces

Because CTRACK is open source and header-only, two parts of one program might be built against **different versions** of it, for example a library compiled with v1.0 linked into an application using v1.1. If the internal types differ but have the same names, the program violates the One Definition Rule, and mismatched layouts can crash in subtle ways. The fix:

**Listing 33** — code from the talk (Versioned Namespaces), shown at [38:39](https://www.youtube.com/watch?v=en4OQvZePqg&t=2319s)

```cpp
#define CTRACK_VERSION_NAMESPACE v1_1_0
namespace ctrack {
    inline namespace CTRACK_VERSION_NAMESPACE {
        // All API here
    }
}
// Users write: ctrack::EventHandler
// Actually get: ctrack::v1_1_0::EventHandler
```

An **inline namespace** makes its members visible as if they were declared in the enclosing namespace, so users write `ctrack::EventHandler`. The mangled name, which the linker sees, includes the version: `ctrack::v1_1_0::EventHandler`. Code built against different versions therefore uses different symbols and can't be silently mixed. In the real header, the namespace name is assembled from the version macros with token pasting: `v##MAJOR##_##MINOR##_##PATCH`. The standard library uses the same technique (libc++'s `std::__1`, libstdc++'s `__cxx11` for the C++11 string ABI).

## 16. Open source: adoption, testing and roadmap

*Talk segment: [19:50](https://www.youtube.com/watch?v=en4OQvZePqg&t=1190s)*

### From internal tool to open source

"First, we basically built it for ourselves. … Then we thought, well, why not open-source it? We're using so many open-source libraries anyway." Being a dependency-free single header made it easy to share. The response:

- "A couple hundred GitHub stars."
- It was used for performance contributions to **ClickHouse**, the high-performance C++ database: "one, if not the biggest, C++ project on GitHub".
- It is used across the company's own projects.
- They are integrating it into the **Hank.ai Darknet** flow, a C++ neural-network framework used for YOLO object detection. "This is sort of the worst scenario", because you want to track every operation in the network, and that "really puts it to the limit."

### The obligations

The slide lists what open-sourcing brought with it: **commitment to maintenance**, **responsibility**, **performance** and **accuracy**. "If I want to improve CTRACK, make it faster, I have to make sure I don't break the stuff for any of my users."

So the team added two things (on a branch at the time of the talk, due for release shortly after).

**A benchmark suite** covering:

- **Accuracy**: timing precision validated against known values.
- **Overhead**: the performance impact on the measured program.
- **Memory efficiency**: bytes per event, during recording and during analysis.
- **Scalability**: one core, many cores, many machines.
- **Baseline comparison**: regression detection across releases. Before releasing a version, they run the suite and compare every metric with the previous version, "so we can really make sure a new CTRACK version is not slower than the last one."

**A test suite** using **doctest**, "our only dependency, but you only need that during development". They chose it because it matches CTRACK's minimalist philosophy better than a bigger framework. It has six categories (basic, multithreaded, nested, statistics, results, edge cases) and over 100 test cases. Recursion gets special attention: "if you exclude child graphs, you need to think about, well, how am I going to solve that if I call myself and still want to have meaningful metrics."

### The roadmap

- **Flame-graph export**, for use with other tools.
- **JSON export.**
- **CI/CD runners** on GitHub instead of the team's own infrastructure.
- **RDTSC**: reading the CPU's time-stamp counter directly instead of `std::chrono`. CTRACK computes time in "just two places", so the switch is easy, but the analysis must still hold.
- **A light version.** Recording 200 million events in the Darknet experiment worked fine, but the full analysis "on small machines took some minutes", so they may offer fewer metrics for more speed.
- **Your ideas.** Community suggestions include a per-thread view.

### Contributing

Pull requests (with CI passing), bug reports ("there might still be some systems which behave a bit differently"), feature requests, and roadmap items. "Many of those features are rather trivial, so I think it's also a good way to maybe get started in open source."

### How cheap is a timestamp?

The floor for any instrumentation tool is the cost of reading the clock twice per event. That cost varies a lot between platforms and clock sources, which is why RDTSC is on the roadmap. You can measure it on your own machine:

**Listing 34** — Measuring the cost of reading the clock *(illustrative example, not from the talk)*

```cpp
#include <chrono>
#include <iostream>

int main()
{
    using clk = std::chrono::steady_clock;
    constexpr int N = 10'000'000;
    long long sink = 0;

    auto t0 = clk::now();
    for (int i = 0; i < N; ++i) sink += clk::now().time_since_epoch().count();   // the probe's floor cost
    auto t1 = clk::now();

    double ns = std::chrono::duration<double, std::nano>(t1 - t0).count() / N;
    std::cout << "steady_clock::now(): ~" << ns << " ns per call (sink " << (sink & 1) << ")\n";
}
```

On the x86-64 Linux machine used for this chapter, one `steady_clock::now()` took about 18 ns. That is the order of magnitude behind "nanoseconds per event".

## 17. The ultimate bottleneck: us

*Talk segment: [22:49](https://www.youtube.com/watch?v=en4OQvZePqg&t=1369s)*

"Usually this is the time where we would do the summary." Instead, the speaker ends with what he calls the most important slides of the talk, "maybe not the most professional".

### Thanks

He thanks the attendees, speakers and organisers, and "especially also the volunteers and the technicians" who make the conference possible. He thanks his colleagues and team, who challenge his ideas: "just with collaboration such ideas and projects can exist". And he thanks "the people at home": his family, and his wife, who "has my back. She always supports me, and … enables me to focus on stuff like that, to follow my passion."

### The ultimate bottleneck

> **The Ultimate Bottleneck**
> - → It's us
> - We can always replace code.
> - We can never replace people.

"At the end of the day, we spend our careers optimising everything else: our compilers, our algorithms, our pipelines, even the language itself. And often we forget that the most complex system in the entire chain is not a constant. That's the human developer."

Human performance varies even more than software performance. "Some days we are this 10x engineer we all want to be. … But other days we get stuck on a simple bug for hours and hours." People get tired, burn out, and have lives outside work. "That's why I think it's fundamentally important that we look out for each other. … Maybe just ask, hey, are you okay?"

There is a hopeful difference from a code bottleneck. A slow function drags the whole program down every time, and it can't help itself. A colleague who is struggling today can be helped: "you have the chance to … offer help, share some of that load, support him, and that bottleneck might be gone faster than you would think."

"We can basically replace any code. … But we can't replace the human sitting next to you." It's an unusual ending for a profiling talk, and a fitting one. Performance work is a team sport, and the measurements in this chapter are only as useful as the people acting on them.

## 18. Questions and answers

*Talk segment: [24:46](https://www.youtube.com/watch?v=en4OQvZePqg&t=1486s)*

The Q&A covered practical questions about using CTRACK in large systems.

### Instrumenting everything

*"I have to annotate every function where I want the metric. Have you something like automatic annotation?"* The Tracy profiler does this with compiler support, and the team considered it, but supporting every compiler is a lot of effort. "In these LLM times it became rather trivial to just say to your code agent: well, for every function, please enter CTRACK." Another audience member had used a Python script for the same job. A global "track everything" switch would be nice, "but we didn't come around to do it yet". For leaving no trace in the binary, `CTRACK_DISABLE` maps every macro to nothing, although the macros stay in the source.

### Overhead and control

- **Overhead per entry?** "It's in the nanosecond range", with detailed numbers in the repository's benchmarks.
- **Enable or disable at run time?** No. "Right now, during compilation you need to decide." An audience member pointed to a Morgan Stanley open-source project that patches trace points into an executable at run time; the speaker will look at it.
- **Tail recursion?** Ordinary recursion is supported; the effect of tail-call optimisation is something "I have to check".
- **Different flags for A/B testing?** Easy: the dev/prod split is "basically just one line of code", so you can define your own groups.

### CTRACK versus telemetry

Telemetry frameworks aim at system health and general metrics in large systems. CTRACK is focused on the high-performance part, with almost no overhead. The speaker has no benchmark comparison with telemetry tools, so he doesn't claim one.

### Memory in long-running programs

*"If you just keep pushing events, … in our code it would be a few gigabytes per second."* Events accumulate until results are calculated, which also flushes them. A ring buffer with a cap is on the roadmap. Meanwhile:

- Print or log results **periodically**, then **clear** them (clearing without calculating is also possible).
- Serialise the raw data without computing statistics.
- **Track less in production.** Nested trackers in tiny, hot functions produce most of the events. "We can find the fastest and smallest functions by using CTRACK itself" and remove those trackers, which in their experience cuts around 70% of the calls.

The questioner noted that the calculation itself also costs CPU time. It does, and that is the price of the record-fast, analyse-later design.

### Data structures

- Names are stored as `std::string_view`, with no allocation per event.
- The global store is a deque of per-thread vectors, so a thread's buffer never moves. The per-thread vectors use a more aggressive growth strategy than the standard default, chosen after extensive benchmarking.
- *Why not an ordered map, which would keep things sorted?* "We prioritise the recording speed." Sorting happens later, during analysis, where it doesn't slow the program.

### Threads and processes

- **Work submitted to thread pools.** The work isn't attributed to the parent on the submitting thread. Parent/child links (and so exclusive time) currently work only within one thread. The questioner knew how to do it by passing context through every call; neither knew a non-invasive way.
- **Offloading analysis to another process.** The event records are simple, so shipping them elsewhere "should not be too hard". The tricky part is draining buffers while new events keep arriving, without adding locks.
- **Linux signal handlers.** A handler that runs on a thread counts as work on that thread, and nesting is still resolved by the same parent links.
- **Sorting the output.** By default the overview is sorted by time active exclusive, with the slowest at the top, and the sort is customisable.

### A periodic-report pattern

For long-running services, the pattern from the memory discussion looks like this. Run for this chapter, it printed three reports, one per second, each covering only the calls since the previous one:

**Listing 35** — Periodic reporting for a long-running service (requires ctrack.hpp, C++17) *(illustrative example, not from the talk)*

```cpp
#include "ctrack.hpp"
#include <chrono>
#include <thread>

void handle_request()
{
    CTRACK;
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
}

int main()
{
    using clock = std::chrono::steady_clock;
    auto next_report = clock::now() + std::chrono::seconds(1);

    for (int i = 0; i < 3000; ++i) {
        handle_request();
        if (clock::now() >= next_report) {
            ctrack::ctrack_result_settings s;
            s.min_percent_active_exclusive = 1.0;  // keep the report short
            ctrack::result_print(s);               // computing results also flushes the recorded events
            next_report += std::chrono::seconds(1);
        }
    }
}
```

## Key takeaways

- Measure before optimising: find where the time actually goes, on the machine where the code actually runs.
- Sampling is cheap but statistical and blind to short or blocked work; instrumentation is exact but must be cheap per event.
- An RAII timer at the top of a scope records every exit path, including early returns and exceptions.
- Inclusive time shows where to start reading; exclusive time (children subtracted) points straight at bottlenecks.
- Trimmed percentiles, standard deviation and the coefficient of variation separate typical cost from noise and outliers.
- Recording can be made almost free with per-thread buffers, a one-time registration lock and stack-like parent links; expensive statistics are deferred.
- Overlapping intervals from several threads must be merged to report wall-clock time.
- Inline versioned namespaces let header-only libraries change ABI safely.

## Glossary

| Term | Meaning |
|---|---|
| Sampling profiler | A profiler that periodically interrupts the program and records where it is executing; results are statistical. |
| Instrumentation | Adding measurement code (probes) at the start and end of functions or scopes. |
| Micro-benchmark | A controlled experiment that runs one piece of code many times in isolation. |
| Time active (inclusive) | Wall-clock time during which a tracked function was running, including the functions it called. |
| Time active exclusive | Time active minus the time of tracked child functions: the function's own cost. |
| Time accumulated | The sum of all individual call durations; can exceed wall-clock time when threads overlap. |
| Coefficient of variation (CV) | Standard deviation divided by the mean: a unit-free measure of variability. |
| Percentile range [1-99]% | The calls left after dropping the fastest and slowest 1 percent, used to describe typical performance. |
| RAII | Resource Acquisition Is Initialization: tying an action to an object's constructor and destructor. |
| Hot path / cold path | The frequently executed, latency-critical code versus code that runs rarely or after the fact. |
| Interval merging | Combining overlapping time intervals into a union to compute covered wall-clock time. |
| Inline namespace | A namespace whose members are visible in the enclosing namespace; used to version ABIs. |
| Execution policy | C++17 tag (seq, par, par_unseq) that lets standard algorithms run in parallel or vectorised. |

## Exercises

1. Write an RAII timer that prints the elapsed time of a scope, and show that it reports correctly when the scope exits via return and via an exception.

   <details><summary>Hint</summary>Start the clock in the constructor, report in the destructor; use std::chrono::steady_clock.</details>

2. For the slow_function/fast_function example, compute time active and time active exclusive by hand, then predict both values if fast_function also called slow_function a second time.

   <details><summary>Hint</summary>Exclusive time subtracts tracked children only.</details>

3. Given call times of 1 ms (90 calls) and 100 ms (10 calls), compute the mean, median, standard deviation and coefficient of variation. Which number describes 'typical' cost?

   <details><summary>Hint</summary>The mean is pulled up by the slow calls; the median is not.</details>

4. Why can a sampling profiler that samples on CPU time report almost nothing for a function that spends its time in sleep_for?

   <details><summary>Hint</summary>A sleeping thread is not on a CPU.</details>

5. Implement interval merging for a list of (start, end) pairs and use it to compute the wall-clock time covered by four threads that each run 10 ms starting 2 ms apart.

   <details><summary>Hint</summary>Sort by start, then extend the current interval while the next one overlaps.</details>

6. Explain why the event store is a std::deque of per-thread vectors rather than a std::vector, and what would break if it were a vector.

   <details><summary>Hint</summary>Threads keep raw pointers to their buffers.</details>

7. Design a periodic reporting loop for a long-running service that prints CTRACK results every hour without letting memory grow without bound.

   <details><summary>Hint</summary>Calculate/print, then clear; or exclude tiny functions.</details>

## Further reading

- CTRACK: github.com/Compaile/ctrack
- Google Benchmark, nanobench, CppBenchmark (benchmarking libraries listed in the talk)
- perf, Tracy Profiler, Visual Studio Performance Profiler, Very Sleepy (CPU profilers listed)
- Intel VTune Profiler, AMD uProf, NVIDIA Nsight (hardware/GPU profilers listed)
- ClickHouse (project that used CTRACK for performance work)
- doctest (the test framework CTRACK uses)
- Slides: CppCon2025 repository, Presentations/CTRACK.pdf
