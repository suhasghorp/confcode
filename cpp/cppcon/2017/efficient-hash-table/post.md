---
title: "Designing a Fast, Cache-Friendly Hash Table: From std::unordered_set to SwissTable"
date: 2026-11-14
tags: ["C++", "hash tables", "SwissTable", "Abseil", "performance", "SIMD", "SSE", "cache", "data structures", "CppCon"]
source: "https://www.youtube.com/watch?v=ncHmEUmJZf4"
video_title: "CppCon 2017: Matt Kulukundis “Designing a Fast, Efficient, Cache-friendly Hash Table, Step by Step”"
channel: "CppCon"
---

> This chapter is based on the talk [CppCon 2017: Matt Kulukundis “Designing a Fast, Efficient, Cache-friendly Hash Table, Step by Step”](https://www.youtube.com/watch?v=ncHmEUmJZf4) (CppCon). Code listings marked *from the talk* are transcribed from the slides; listings marked *illustrative* were written for this chapter to explain the ideas.

Hash tables are everywhere. At Google in 2017, about one percent of all the CPU in the fleet was busy inside a hash table, and more than four percent of all RAM was owned by one, counting C++ alone. At that scale, small constant factors turn into data centres.

In this CppCon 2017 talk, Matt Kulukundis walks through how a team at Google redesigned the hash table step by step, starting from `std::unordered_set` and ending with **SwissTable**, the design later open-sourced in Abseil as `absl::flat_hash_map` and `absl::node_hash_map`. Each step removes a pointer, a stored value or a memory access, and each one is judged by the same two questions: *what do we gain, and what do we lose?* The destination is an open-addressing table with a separate array of one-byte **control bytes** that hold seven bits of each element's hash, searched sixteen at a time with three SSE instructions.

The talk is also about the realities of changing code at enormous scale: benchmarks that lie, users who depend on every observable behaviour (Hyrum's Law), API "bear traps" in the standard containers, and deliberate randomisation to stop people relying on iteration order. This chapter follows the talk's order and adds the background a student needs: chaining versus open addressing, load factors, tombstones, what a cache line is, and how SIMD comparisons work. The talk's code was reassembled into a small working table and tested.

## What you will learn

- Describe how std::unordered_set is laid out in memory (buckets, nodes, the singly linked list) and count the memory accesses a lookup costs
- Explain separate chaining versus open addressing, and the role of load factor and tombstones
- Evaluate each design step with the question 'what do we gain, what do we lose' in memory, speed and API guarantees
- Explain why storing values inline (dense_hash_set) is fast but needs sentinel values and degrades with large payloads
- Describe SwissTable's control bytes and the H1/H2 split of the hash code
- Explain how _mm_set1_epi8, _mm_cmpeq_epi8 and _mm_movemask_epi8 test 16 control bytes at once
- Implement find and erase over groups of 16 control bytes, and explain when erase can avoid a tombstone
- Recognise performance bear traps in std::unordered_map's API (temporary keys, insert overloads, emplace) and how heterogeneous lookup avoids them
- Explain why hash quality matters for power-of-two tables and how a good default hash prevents pathological slowdowns
- Explain how randomising iteration order defends against Hyrum's Law

## Before you start

- Comfortable with C++ templates, iterators and the standard containers
- Basic knowledge of what a hash function and a hash table are
- Helpful but not required: a rough idea of CPU caches and what SIMD instructions are

## Contents

1. [A team effort, and a talk that is 90% true](#1-a-team-effort-and-a-talk-that-is-90-true) — [0:00](https://www.youtube.com/watch?v=ncHmEUmJZf4&t=0s)
2. [Ground rules: hot and cold tables, lying benchmarks, and Hyrum's Law](#2-ground-rules-hot-and-cold-tables-lying-benchmarks-and-hyrums-law) — [1:40](https://www.youtube.com/watch?v=ncHmEUmJZf4&t=100s)
3. [Anatomy of std::unordered_set](#3-anatomy-of-stdunorderedset) — [6:55](https://www.youtube.com/watch?v=ncHmEUmJZf4&t=415s)
4. [Trimming the standard design: per-bucket chains and no stored hash](#4-trimming-the-standard-design-per-bucket-chains-and-no-stored-hash) — [11:20](https://www.youtube.com/watch?v=ncHmEUmJZf4&t=680s)
5. [Removing the pointers: probing with nodes](#5-removing-the-pointers-probing-with-nodes) — [15:35](https://www.youtube.com/watch?v=ncHmEUmJZf4&t=935s)
6. [Tombstones, load factor and breaking the standard: node_hash_set](#6-tombstones-load-factor-and-breaking-the-standard-nodehashset) — [17:00](https://www.youtube.com/watch?v=ncHmEUmJZf4&t=1020s)
7. [Benchmarking a hash table](#7-benchmarking-a-hash-table) — [19:00](https://www.youtube.com/watch?v=ncHmEUmJZf4&t=1140s)
8. [Values inline: dense_hash_set and its sentinels](#8-values-inline-densehashset-and-its-sentinels) — [22:00](https://www.youtube.com/watch?v=ncHmEUmJZf4&t=1320s)
9. [Metadata: control bytes and the H1/H2 split](#9-metadata-control-bytes-and-the-h1h2-split) — [24:00](https://www.youtube.com/watch?v=ncHmEUmJZf4&t=1440s)
10. [Finding with control bytes](#10-finding-with-control-bytes) — [27:05](https://www.youtube.com/watch?v=ncHmEUmJZf4&t=1625s)
11. [SIMD: matching sixteen control bytes in three instructions](#11-simd-matching-sixteen-control-bytes-in-three-instructions) — [29:45](https://www.youtube.com/watch?v=ncHmEUmJZf4&t=1785s)
12. [Groups: find and erase sixteen at a time](#12-groups-find-and-erase-sixteen-at-a-time) — [31:55](https://www.youtube.com/watch?v=ncHmEUmJZf4&t=1915s)
13. [Ergonomics: bear traps in std::unordered_map](#13-ergonomics-bear-traps-in-stdunorderedmap) — [36:35](https://www.youtube.com/watch?v=ncHmEUmJZf4&t=2195s)
14. [Defaults matter: hash quality](#14-defaults-matter-hash-quality) — [40:25](https://www.youtube.com/watch?v=ncHmEUmJZf4&t=2425s)
15. [Rolling out at Google, and defending against Hyrum](#15-rolling-out-at-google-and-defending-against-hyrum) — [42:15](https://www.youtube.com/watch?v=ncHmEUmJZf4&t=2535s)
16. [Why 'Swiss', and the experiments in progress](#16-why-swiss-and-the-experiments-in-progress) — [45:15](https://www.youtube.com/watch?v=ncHmEUmJZf4&t=2715s)
17. [Questions and answers](#17-questions-and-answers) — [49:20](https://www.youtube.com/watch?v=ncHmEUmJZf4&t=2960s)

## 1. A team effort, and a talk that is 90% true

*Talk segment: [0:00](https://www.youtube.com/watch?v=ncHmEUmJZf4&t=0s)*

Matt Kulukundis opens the talk with credits, because "this is the work of a lot of people. I am not the primary author of much of this; I'm more the primary evangelist." He names the team, in alphabetical order. (Names here are as given on stage; the auto-captions garble several of them.)

- **Alkis Evlogimenos**, "probably the most primary author", who did "the lion's share of the coding and much of the deep thought".
- **Jeff Dean** and **Sanjay Ghemawat**, who arrived "with some brilliant insights at a very opportune moment". "They're sort of like a single hive mind that walks around in two bodies." Their ideas appear twice in this chapter, as cartoon cameos on the slides.
- **Jeffrey Lim**, for assembly-level micro-optimisation, which is "way too low level" to cover.
- **Roman Perepelitsa**, "responsible for all of the crazy template hackery".
- **Samuel Benzaquen**, for code reviews, templates and optimisation.
- **Vitaly Goldshteyn**, for work on the hash functions.

### "Step by step"

"You've probably seen this sort of talk before. I start with something simple, and then I'm just gonna blow past it to something complicated much faster than you can follow." The slide makes the joke with the famous *How to draw an owl* meme: step 1, draw two circles; step 2, draw the rest of the owl.

In fact the talk keeps its promise better than the joke suggests. It starts from `std::unordered_set` and changes one thing at a time. After each change it asks two questions:

> **What do we gain? What do we lose?**

"This is how you need to think when you are optimizing a data structure." This chapter keeps that discipline. Every design step gets a small gain/loss table.

### What a hash table does

As a reminder before we take one apart: a hash table stores elements so that insert, find and erase take expected constant time. A **hash function** turns a key into a large integer, the *hash code*. The table reduces that number to a position in an array, typically with a modulo or a bit mask. Two keys that land on the same position **collide**, and the table needs a strategy for that. *Where* the elements live, and *how* collisions are resolved, are exactly the design decisions this talk revisits.

The standard library's hash containers, `std::unordered_set` and `std::unordered_map`, are the starting point:

**Listing 1** — The standard hash containers and their bucket interface *(illustrative example, not from the talk)*

```cpp
#include <iostream>
#include <string>
#include <unordered_map>
#include <unordered_set>

int main()
{
    std::unordered_set<int> s{3, 1, 4, 1, 5, 9, 2, 6};
    s.insert(7);
    std::cout << "contains 4? " << s.contains(4) << '\n';   // C++20
    s.erase(4);

    std::unordered_map<std::string, int> ages{{"ada", 36}, {"alan", 41}};
    ages["grace"] = 85;
    if (auto it = ages.find("alan"); it != ages.end()) std::cout << it->second << '\n';

    // The bucket interface: rarely used, but it constrains the implementation.
    std::cout << "buckets: " << s.bucket_count()
              << ", load factor: " << s.load_factor()
              << ", max load factor: " << s.max_load_factor()
              << ", bucket of 9: " << s.bucket(9) << '\n';
}
```

The last four calls in that example (`bucket_count`, `load_factor`, `max_load_factor` and `bucket`) belong to the **bucket interface**. Remember them, because the talk claims that nobody uses them, and that turns out to matter a great deal.

## 2. Ground rules: hot and cold tables, lying benchmarks, and Hyrum's Law

*Talk segment: [1:40](https://www.youtube.com/watch?v=ncHmEUmJZf4&t=100s)*

Before any design work, the speaker sets some ground rules and gives the context that drove the project.

### 90% true

"For the purposes of this talk, table, map and set are interchangeable words." More importantly: "What I say, I am aiming for 90% true. Perfect fidelity interferes with understanding when you are dealing with a topic this complicated." Every diagram carries a little footnote, *90% true*, *98% true*, *2% true*, as a running joke about how simplified it is. Where this chapter adds precision, it tries not to lose the simplicity that makes the talk work.

### Assume a good hash function

"A huge amount of the performance of a hash table depends on the quality of your hash functions. There are doctoral theses about this, and I'm gonna gloss over all of it." The interaction between hash function and table is "very subtle and often depends on the exact implementation of the table". The talk assumes a good hash and flags the places where that matters most. One of them, a 25x slowdown, gets its own section later.

### Why it matters: the scale

"Hash tables are incredibly performance sensitive. At Google, right now, as I am speaking, in our fleet, one percent of the CPUs are computing something inside a hash table." And "more than four percent of Google's RAM is owned by a hash table, and that's just in C++." At that scale, "there is no trade-off … that is correct for all users". A design must be good across a huge range of uses, not perfect for one.

### Hot tables and cold tables

"When we talk about hash tables, you have to talk about whether a table is cold or hot."

- A **hot** table fits in the L1 cache, the CPU's smallest and fastest cache (tens of kilobytes, a few cycles away). Then "you should be counting instructions".
- A **cold** table is far out in main memory. Then "all you really need to talk about are … cache misses. Everything else comes for free." A trip to RAM costs on the order of a hundred nanoseconds, enough time for hundreds of instructions.

Keep this in mind for the rest of the talk. Most of the design steps reduce **memory accesses to different places**, because for large tables each one is potentially a cache miss. The example below shows the effect on any machine: the same number of lookups gets much slower once the table no longer fits in cache.

**Listing 2** — Hot versus cold: the same lookups at different table sizes *(illustrative example, not from the talk)*

```cpp
#include <chrono>
#include <cstdint>
#include <iostream>
#include <random>
#include <unordered_set>
#include <vector>

// Average nanoseconds per successful find, probing keys in random order.
double ns_per_find(std::size_t n)
{
    std::unordered_set<std::uint64_t> s;
    std::vector<std::uint64_t> keys(n);
    std::mt19937_64 rng(42);
    for (auto& k : keys) { k = rng(); s.insert(k); }
    std::shuffle(keys.begin(), keys.end(), rng);

    std::size_t hits = 0;
    auto t0 = std::chrono::steady_clock::now();
    for (int rep = 0; rep < 4; ++rep)
        for (auto k : keys) hits += s.count(k);
    auto t1 = std::chrono::steady_clock::now();
    if (hits == 0) std::cout << "";                       // keep the loop alive
    return std::chrono::duration<double, std::nano>(t1 - t0).count() / (4.0 * n);
}

int main()
{
    for (std::size_t n : {1'000u, 100'000u, 4'000'000u})   // L1/L2-sized ... RAM-sized
        std::cout << n << " elements: " << ns_per_find(n) << " ns/find\n";
}
```

### Benchmarks lie

"Benchmarks are the only source of truth you will ever get, and they are lies. Every last one of them." There are plenty of blog posts claiming the fastest hash table ever, backed by a single benchmark, and "anytime someone comes to you with one benchmark, they have gamed that benchmark."

### Hyrum's Law

"Titus has told me this slide is obligatory":

> With a sufficient number of users of an API, it does not matter what you promise in the contract: all observable behaviors of your system will be depended on by somebody. — Hyrum Wright

For a hash table, iteration order is the classic example. The standard does not specify it, but with millions of lines of code, some code will depend on it. Hyrum's Law "is the bane of the janitor's existence", and the talk returns to it near the end.

### What users actually do

Google can study its own codebase, so the speaker reports what the code shows:

- "Nobody erases, except for the people who do."
- "Only two operations really matter, insert and find", except for people who need efficient **table scans** (iterating over everything).
- "Nobody uses the bucket interface." "There was one use of it in Google3 and it's a debug API."
- "Nobody uses load factor correctly. I audited every user of load factor in Google3. They were all wrong. All of them."

These observations become design licences. Features nobody uses can be sacrificed for speed.

The slide after this one introduces "the new hash table at Google called SwissTable. It is the fastest hash table in the world, with no caveats, ifs, ands or buts." A pause. "Okay, a couple of people got that it was a joke." The real story starts from the standard container.

## 3. Anatomy of std::unordered_set

*Talk segment: [6:55](https://www.youtube.com/watch?v=ncHmEUmJZf4&t=415s)*

### The diagram everyone draws

The starting point is `std::unordered_set`. The first diagram shows the classic picture of a **separately chained** hash table:

- An array of **buckets** (`s⁰`, `s¹`, `s²`).
- Each bucket points to a singly linked list of **nodes**.
- Each node holds three things: **H**, the element's 64-bit hash code ("technically this is optional, but it's usually there"); **V**, the value itself; and a **next** pointer (or the null symbol, ⌀, at the end of a chain).

"Because the value is in a separate allocation, we say that it has **pointer stability**. No matter what happens to the internals of your hash table, the value doesn't move. This is a property guaranteed by the standard." Pointers and references to elements stay valid across inserts and rehashes. Only erasing that element invalidates them.

The **bucket** is found by taking the hash modulo the number of buckets. All values in the same bucket hang on its chain. The **load factor** is the number of elements divided by the number of buckets; the example table is at 60%. "Tables use the load factor to decide when they want to grow." When it exceeds `max_load_factor()`, which defaults to 1.0, the table allocates more buckets and **rehashes** every element into its new bucket.

### What libstdc++ actually does

The diagram is "90% true". The real layout in libstdc++, the GNU standard library ("4.8, 4.9, something around there"), is stranger:

- All nodes form **one** singly linked list that runs through the whole table, starting at a `head` node.
- Each bucket points not to its first node, but to the node **before** its first node.

"I know you're sitting there going, 'Whoa'. … You slow down, take a deep breath, and just lie back and think, 'Oh, now it makes sense.'"

It does make sense once you see the constraints. One list through all elements makes **iteration** a simple list walk: O(size), regardless of how many empty buckets there are. Pointing at the *previous* node is what lets you insert into or erase from a singly linked list in O(1), because you need the predecessor to relink.

### The costs

The speaker reads the picture for costs:

- **Memory.** Each element carries a stored hash and a next pointer: 16 bytes on top of the value and the allocation overhead. "16 bytes? That's like practically free. Who cares?" At Google it is "somewhere between 0.1 and 0.2% of fleet-wide RAM".
- **Lookups.** Finding the second element of a chain involves following the bucket to the *previous* node, then the first real node (compare its stored hash, then compare the value), then the next node. **Four memory accesses**, each to a different heap allocation, so for a cold table each can be a cache miss. "It's kinda stinky."
- **Iteration order** is reproducible for a given insertion order, and Hyrum's Law will make someone depend on it. The speaker puts "a pin in that".

You can observe the chained structure through the standard bucket API, which lists each bucket's chain:

**Listing 3** — Looking at the buckets of a std::unordered_set *(illustrative example, not from the talk)*

```cpp
#include <iostream>
#include <unordered_set>

int main()
{
    std::unordered_set<int> s;
    s.max_load_factor(1.0f);
    for (int v : {10, 3, 17, 24, 31, 5, 12}) s.insert(v);

    std::cout << "size " << s.size() << ", buckets " << s.bucket_count()
              << ", load factor " << s.load_factor() << '\n';

    for (std::size_t b = 0; b < s.bucket_count(); ++b) {
        if (s.bucket_size(b) == 0) continue;
        std::cout << "bucket " << b << ":";
        for (auto it = s.begin(b); it != s.end(b); ++it) std::cout << ' ' << *it;   // the chain
        std::cout << '\n';
    }
}
```

### Why the dummy node?

"In order to insert into a singly linked list, you need to have the element before, so that you can splice it in appropriately." The alternative, appending at the end of the chain, means walking the chain on every insert, which the library designers considered unacceptable. A **doubly** linked list would remove the need for the predecessor trick, "but that adds another eight bytes to everything. That's another 0.1% of fleet-wide RAM, and most of the reason I started this was to save RAM." It would also "fluff out our L1 caches". So the talk takes a different turn.

## 4. Trimming the standard design: per-bucket chains and no stored hash

*Talk segment: [11:20](https://www.youtube.com/watch?v=ncHmEUmJZf4&t=680s)*

### Back to the simple picture

"What if we just try and return it to our mental model?" Give each bucket its own singly linked chain, with no global list and no predecessor trick. "Coincidentally, this is what GNU CXX `hash_map` actually looks like": the pre-standard SGI hash table that `std::unordered_map` descends from.

| | |
|---|---|
| **Gain** | One fewer memory access on find and insert: the bucket points straight at the first node. |
| **Lose** | Iteration must scan the **whole bucket array**, not just the elements: O(capacity + size). |

"I can hear the theoreticians" object that capacity is bounded by a constant multiple of size, so iteration is still O(size). True, "except for the people who reuse their tables. They grow it out to a million elements because they have some request that needs a million things, then they call `clear`, and then they insert seven elements, and then they iterate over it. Ask me how I know about these people." Hyrum's Law again. But "these people are the minority. We're kinda willing to lose them for now." (The final design addresses them differently; see the Q&A.)

### Dropping the stored hash

"Why are we paying for that hash?" Remove H from the nodes. "Felt good, I feel like I lost some weight."

| | |
|---|---|
| **Gain** | About 0.1% of fleet-wide RAM (8 bytes per element). |
| **Lose (1)** | On every **rehash**, the hash of every element must be recomputed by calling the hash function again. |
| **Lose (2)** | **find** can't take a cheap fast path by comparing stored hashes before calling the (possibly expensive) equality operator. |

How much each loss costs "depends on the type". For an `int` or a `double`, computing the hash is "basically free" and equality is a single comparison. For a long string, both are real work. "And we got rid of a branch. Trade-offs."

The key insight is that **users can get it back**: "They can put in a type that caches its own hash value. We didn't technically lose any power. We may have lost some convenience." The speaker adds a joke: "also the vague feeling of superiority when we force a difficult decision onto the user. And that is the C++ way."

Here is what that looks like: a key type that computes its hash once and stores it, plus a hasher and equality that use it. The equality check compares the cached hashes first, recreating the fast path the table gave up.

**Listing 4** — A key that caches its own hash *(illustrative example, not from the talk)*

```cpp
#include <cstddef>
#include <functional>
#include <iostream>
#include <string>
#include <unordered_set>

class hashed_string {
public:
    explicit hashed_string(std::string s) : s_(std::move(s)), h_(std::hash<std::string>{}(s_)) {}
    const std::string& str() const noexcept { return s_; }
    std::size_t hash() const noexcept { return h_; }

    friend bool operator==(const hashed_string& a, const hashed_string& b)
    {
        return a.h_ == b.h_ && a.s_ == b.s_;     // cheap fast path, then the real comparison
    }

private:
    std::string s_;
    std::size_t h_;                              // computed once, reused on every rehash
};

struct cached_hash {
    std::size_t operator()(const hashed_string& k) const noexcept { return k.hash(); }
};

int main()
{
    std::unordered_set<hashed_string, cached_hash> s;
    for (int i = 0; i < 1000; ++i) s.emplace("a fairly long key number " + std::to_string(i));
    std::cout << s.count(hashed_string("a fairly long key number 42")) << '\n';   // 1
}
```

## 5. Removing the pointers: probing with nodes

*Talk segment: [15:35](https://www.youtube.com/watch?v=ncHmEUmJZf4&t=935s)*

### No more chains

"So maybe I can do it again, right? What if I remove the pointers?" The next diagram has no `next` pointers at all. The bucket array now holds a **pointer to each value's node**, one per slot. When two values want the same slot, the second goes into a nearby free slot.

"What did I just do? This isn't even a chaining hash table anymore. This is a **probing** table. This is madness, or is it?"

This is **open addressing**. Every element lives in a slot of the main array (here, a pointer to its node). On a collision, the table **probes** other slots in a fixed order until it finds the key or a free slot. The simplest order is **linear probing**: try slot `h`, then `h+1`, `h+2`, … wrapping around at the end. "Probing has an entire rich field of academic depth to it that we're not gonna go into." The best-known complication is *primary clustering*: runs of occupied slots that grow and lengthen later probes.

| | |
|---|---|
| **Gain** | Another ~0.1% of fleet-wide RAM: no next pointers. |
| **Lose** | Iterating over one bucket's elements becomes complicated, but "nobody uses the bucket interface. Literally nobody. Not even Hyrum." |

Values still live in separately allocated nodes, so **pointer stability is preserved**.

### find in a probing table

Finding the same element as before: compute the hash, go to the slot, follow its pointer, compare. It's not a match, so **probe to the next slot**, follow that pointer, and it's found. "It's worth noting that this probe is free, because it's right next to the first piece in memory. Our cache fetch pulled it in."

That point drives everything that follows. Memory moves between RAM and the CPU in **cache lines** of 64 bytes. Reading one 8-byte pointer brings its seven neighbours along for free. Linear probing exploits that, while a linked list scatters its nodes across the heap. The count drops to **three memory accesses instead of four**: the slot array, the first node and the second node.

Here is the core of a linear-probing lookup over an array of pointers, to make the mechanics concrete. It is deliberately minimal: no growth and no erase.

**Listing 5** — Linear probing over an array of node pointers *(illustrative example, not from the talk)*

```cpp
#include <cstddef>
#include <functional>
#include <iostream>
#include <memory>
#include <vector>

template <typename T, typename Hash = std::hash<T>>
class probing_node_set {
public:
    explicit probing_node_set(std::size_t capacity) : slots_(capacity) {}

    bool insert(const T& v)
    {
        for (std::size_t i = Hash{}(v) % slots_.size();; i = (i + 1) % slots_.size()) {
            if (!slots_[i]) { slots_[i] = std::make_unique<T>(v); return true; }   // free slot
            if (*slots_[i] == v) return false;                                     // already there
        }
    }

    const T* find(const T& v) const
    {
        for (std::size_t i = Hash{}(v) % slots_.size();; i = (i + 1) % slots_.size()) {
            if (!slots_[i]) return nullptr;          // an empty slot ends the probe sequence
            if (*slots_[i] == v) return slots_[i].get();
        }
    }

private:
    std::vector<std::unique_ptr<T>> slots_;          // nodes keep pointer stability
};

int main()
{
    probing_node_set<int> s(16);
    for (int v : {3, 19, 35, 7}) s.insert(v);       // 3, 19, 35 collide modulo 16
    const int* p = s.find(35);
    std::cout << (p ? *p : -1) << ' ' << (s.find(51) ? "found" : "missing") << '\n';
}
```

## 6. Tombstones, load factor and breaking the standard: node_hash_set

*Talk segment: [17:00](https://www.youtube.com/watch?v=ncHmEUmJZf4&t=1020s)*

### Erasing from a probing table

"But I hear sort of susurrations of concern. What about erasing elements?" In a probing table you **cannot simply empty a slot**. Suppose `a` and `b` both hash to slot 5. `a` was inserted first, so `b` probed on to slot 6. If we erase `a` by clearing slot 5, a later search for `b` starts at slot 5, finds it empty, and wrongly concludes that `b` isn't there.

"We just add sentinels. You have a little dead marker that says: this is a **tombstone**." The rules become:

- When probing for find, **skip** tombstones and stop only at a truly *empty* slot.
- When inserting, a tombstone can be reused (after making sure the key isn't present further along).

The cost shows up in the load factor. Load factor "is really just trying to tell you how long your probe sequences are", and tombstones lengthen probe sequences just like live elements do. So in this design "**tombstones add to your load factor**, so erase doesn't actually free up your load factor. It's kinda annoying, but nobody uses load factor correctly anyway." Tables of this kind count tombstones toward the growth threshold and clean them up on the next rehash.

**Listing 6** — Why erase needs tombstones *(illustrative example, not from the talk)*

```cpp
#include <array>
#include <iostream>
#include <optional>

// Slots hold a value, are Empty, or are a Tombstone (Deleted).
enum class state { empty, full, deleted };
struct slot { state st = state::empty; int value = 0; };

std::array<slot, 8> table;
std::size_t home(int v) { return static_cast<std::size_t>(v) % table.size(); }

void insert(int v)
{
    for (std::size_t i = home(v);; i = (i + 1) % table.size())
        if (table[i].st != state::full) { table[i] = {state::full, v}; return; }
}

bool find(int v)
{
    for (std::size_t i = home(v);; i = (i + 1) % table.size()) {
        if (table[i].st == state::empty) return false;                 // only EMPTY stops the search
        if (table[i].st == state::full && table[i].value == v) return true;
    }
}

void erase_wrong(int v) { for (auto& s : table) if (s.st == state::full && s.value == v) s.st = state::empty; }
void erase_right(int v) { for (auto& s : table) if (s.st == state::full && s.value == v) s.st = state::deleted; }

int main()
{
    insert(5); insert(13);             // 13 % 8 == 5: collides, probes to slot 6
    erase_right(5);
    std::cout << "with tombstone, find(13): " << find(13) << '\n';   // 1

    table = {};
    insert(5); insert(13);
    erase_wrong(5);
    std::cout << "with plain empty, find(13): " << find(13) << '\n'; // 0: lost!
}
```

### Breaking the standard, on purpose

"You might be asking, did we just break standards compatibility? Yes. Multiple times." For example:

**Listing 7** — code from the talk ((code slide)), shown at [18:35](https://www.youtube.com/watch?v=ncHmEUmJZf4&t=1115s)

```cpp
s.reserve(1000);
s.insert(0);
for (int i = 0; i < 100000; ++i) {
  s.erase(i);

  // may rehash, `std::unordered_set` will not
  s.insert(i+1);
}
```

The standard guarantees that this loop **never rehashes** a `std::unordered_set`. After `reserve(1000)`, inserting while the size stays below the reserved capacity cannot trigger a rehash, and the loop never holds more than one element. In the probing design, every `erase` leaves a tombstone. After enough iterations, tombstones push the effective load past the threshold and the table rehashes to clean them up, even though the size never exceeded 1. The comment on the slide says it plainly: "may rehash, `std::unordered_set` will not."

"I just don't feel bad about it. I've looked in Google. Nobody relies on this, nobody. It turns out Hyrum isn't quite as right as he thinks, but boy is he nearly as right as he thinks." (The speaker sips his Diet Mountain Dew while you read the code, and calls that "a joke for my friends".)

### node_hash_set

"Let's give this guy a name: `node_hash_set`." It still stores values in **nodes**, so it keeps pointer stability. And "it has this sort of weighty, future portent. Why would I call it `node_hash_set` if I didn't have some other thing that could go there?" Abseil later shipped exactly that pair: `absl::node_hash_set` and `absl::flat_hash_set`.

## 7. Benchmarking a hash table

*Talk segment: [19:00](https://www.youtube.com/watch?v=ncHmEUmJZf4&t=1140s)*

### The space of benchmarks

"I've been told that all talks need graphs." But first: how do you benchmark a hash table? The speaker lists the dimensions that matter:

| Dimension | Why it matters |
|---|---|
| **How full is the table?** Just grew, about to grow, or in between? | Probe lengths and cache footprint change across a growth cycle. |
| **Where is the memory?** All in L1, or "so large that it's way out to RAM"? | Hot means count instructions; cold means count cache misses. |
| **Which operation, which outcome?** Find hit or miss; insert of a new or an existing key. | Misses usually probe further than hits; inserts may allocate. |
| **How big is the element?** "Are you keeping ints … or massive structs that are a meg each?" | Large elements change what fits in a cache line, and what a rehash costs. |

"When we first wrote our benchmarks, the very first run took three days. And then we looked at that data and aggressively cut dimensionality. But without running the data once, how do you know what dimensions are inconsequential?" This is a valuable lesson. Measure broadly first, then prune with evidence.

(As an aside on giant elements: "Please don't keep massive structs that are a meg each. Or use a node one that has an indirection." A node-based table moves pointers, not the elements, when it rehashes.)

### The first graph

The slide before the graph is an xkcd-style self-describing chart. Then comes the real one: **std::unordered_set vs node_hash_set**, time per successful find, for 4-byte elements, against table size from 10² to 10⁶ elements. `std::unordered_set` sits around 260–300 ns; `node_hash_set` around 115–170 ns.

"What's the biggest takeaway? Two times faster." "Less RAM. That's also made of awesome." The other graphs for this comparison "are basically the same", and he moves on.

### Writing a fair micro-benchmark

Most of the bugs in hash-table benchmarks come from the measuring code, not from the table. A few rules of thumb are worth following:

- **Separate the setup from the measurement**, and build the table before the timer starts.
- **Look up keys in a random order**, so the hardware prefetcher can't hide misses.
- **Use the results**, so the optimiser cannot delete the loop.
- **Sweep the size** across the cache levels and report each size separately.
- **Measure hits and misses separately.**

**Listing 8** — A small parameterised find benchmark (hits and misses, several sizes) *(illustrative example, not from the talk)*

```cpp
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <random>
#include <unordered_set>
#include <vector>

template <typename Set>
void run(const char* name, std::size_t n)
{
    std::mt19937_64 rng(n);
    std::vector<std::uint64_t> present(n), absent(n);
    for (auto& k : present) k = rng() | 1;          // odd keys are stored
    for (auto& k : absent) k = rng() & ~1ull;       // even keys never are

    Set s(present.begin(), present.end());          // setup: not timed
    std::shuffle(present.begin(), present.end(), rng);

    auto time = [&](const std::vector<std::uint64_t>& keys) {
        std::size_t found = 0;
        auto t0 = std::chrono::steady_clock::now();
        for (auto k : keys) found += s.count(k);
        auto t1 = std::chrono::steady_clock::now();
        volatile std::size_t sink = found; (void)sink;            // use the result
        return std::chrono::duration<double, std::nano>(t1 - t0).count() / keys.size();
    };

    std::cout << name << " n=" << n << "  hit: " << time(present)
              << " ns  miss: " << time(absent) << " ns\n";
}

int main()
{
    for (std::size_t n : {100u, 10'000u, 1'000'000u})
        run<std::unordered_set<std::uint64_t>>("unordered_set", n);
}
```

For serious work, use a benchmarking library such as Google Benchmark, which handles repetitions, statistics and "do not optimise" barriers for you.

## 8. Values inline: dense_hash_set and its sentinels

*Talk segment: [22:00](https://www.youtube.com/watch?v=ncHmEUmJZf4&t=1320s)*

### Red pill or blue pill

"We got this far by removing data and indirections. Now we have a choice." The slide shows Morpheus from *The Matrix*:

- **Red pill**: "leave the standard further behind", eliminate more indirections, and "find all the performance we can".
- **Blue pill**: "play games hiding hash codes in inappropriate places in our pointers", which is speculative territory with no data.

"But before you decide, let me ask you a trick question. What's the first thing you reach for, a `std::vector` or a `std::list`? Yes, a vector. Why is an unordered map any different?" Contiguous storage beats linked storage, for the same cache-line reasons as before. So: red pill. **Put the values directly into the array.**

### dense_hash_set

The new diagram is one flat array of slots containing values, with a black slot for a deleted element. Every slot needs a way to say "empty" or "deleted" without extra metadata. The solution is **sentinels**: two key values that the user promises never to store, one meaning *empty* and one meaning *deleted*. "I'm sure you have two distinct entries for your value that you're never gonna need to represent in your map." (That line is sarcasm.)

The result: "**Four to five times better**" than `std::unordered_set`. The graph shows `std::unordered_set` at roughly 170–230 ns per find-hit, against `dense_hash_set` at roughly 35–75 ns.

"Also, I cheated. This hash table exists in the wild. It is called `dense_hash_set`. You can find multiple copies of it running around on the internet. Just search for it using Google." It is part of Google's open-source *sparsehash* library.

### Three problems

"It only has a couple of small problems":

1. **Old and crufty.** The API predates modern C++ and would need updating.
2. **Sentinels.** The user must choose two impossible keys and call `set_empty_key` (and `set_deleted_key` before erasing). For many key types there is no natural candidate. This is "the biggest usability issue".
3. **Performance degrades with element size.** The graph compares 4-byte with 64-byte payloads: roughly 35–75 ns against 105–120 ns, "almost 2x". With values stored inline, big values mean fewer slots per cache line, so every probe step can touch a new line. A rehash also has to move every value. "You have your `dense_hash_set` somewhere, and it's some value you don't own, and somebody over there adds some entries into that struct, and all of a sudden your performance falls off a cliff."

The speaker "tables the size question" for now and attacks the sentinels first.

Here is what the sentinel requirement looks like in practice, reduced to its essentials:

**Listing 9** — Sentinel keys: the price of a flat table without metadata *(illustrative example, not from the talk)*

```cpp
#include <cassert>
#include <cstddef>
#include <functional>
#include <limits>
#include <vector>

// A flat open-addressing set that needs two reserved key values.
template <typename T>
class sentinel_set {
public:
    sentinel_set(std::size_t capacity, T empty_key, T deleted_key)
        : slots_(capacity, empty_key), empty_(empty_key), deleted_(deleted_key) {}

    void insert(const T& v)
    {
        assert(v != empty_ && v != deleted_);        // users must never store these values!
        for (std::size_t i = std::hash<T>{}(v) % slots_.size();; i = (i + 1) % slots_.size()) {
            if (slots_[i] == v) return;
            if (slots_[i] == empty_ || slots_[i] == deleted_) { slots_[i] = v; return; }
        }
    }

    bool contains(const T& v) const
    {
        for (std::size_t i = std::hash<T>{}(v) % slots_.size();; i = (i + 1) % slots_.size()) {
            if (slots_[i] == empty_) return false;
            if (slots_[i] == v) return true;
        }
    }

private:
    std::vector<T> slots_;          // values stored inline: no nodes, no pointers
    T empty_, deleted_;
};

int main()
{
    constexpr int kEmpty = std::numeric_limits<int>::min();       // "impossible" values,
    constexpr int kDeleted = std::numeric_limits<int>::min() + 1; // chosen by the user
    sentinel_set<int> s(64, kEmpty, kDeleted);
    s.insert(42);
    return s.contains(42) ? 0 : 1;
}
```

## 9. Metadata: control bytes and the H1/H2 split

*Talk segment: [24:00](https://www.youtube.com/watch?v=ncHmEUmJZf4&t=1440s)*

### Metadata instead of sentinels

"Maybe if I add just a little bit of metadata instead of having sentinels. Everybody loves metadata." The slot array gets a **parallel array of metadata**, one entry per slot. ("Dark secret: it's in the same allocation, but for purposes of our talk we can think of them as separate.")

What must the metadata say? Each slot is **empty**, **full** or **deleted**. "One and a half bits. That is slightly awkward." You could pack three states into fractional bits ("every two elements gets three bits and something"), "but the bookkeeping gets really painful", and "I've never seen the code for it".

### A wild Sanjay and Jeff appear

"I kinda wish someone way smarter than me would just show up…" and the slide shows photos of Sanjay Ghemawat and Jeff Dean with a thought bubble:

> *Use a whole byte and store 7 bits of hash code to reduce comparisons!*

"By the way, this totally happened in real life. We were struggling with problems, and Jeff Dean and Sanjay sent me an email out of nowhere."

One byte per slot. One bit says whether the slot is **special** (empty, deleted or the end sentinel) or **full**. If it is full, the other **seven bits hold part of the element's hash**. Now the metadata does double duty: it records occupancy, and it filters out almost every non-matching slot without touching the slot itself.

### H1 and H2

The 64-bit hash code is split in two:

- **H1**, the upper **57 bits**, chooses *where* to look ("the position in the array"). H1 is "what we take the modulus of".
- **H2**, the lower **7 bits**, is the **metadata** stored in the control byte of a full slot.

"This is where your hash function starts to become important." If the hash "hides all its entropy in the first seven bits", H1 collides a lot. If it "puts none of its entropy in the first seven bits", H2 collides a lot. "And any type of collision is bad." The hash needs to spread information over all 64 bits. "There's even ways to take a weak hash function and turn it into a strong hash function."

### Names

"Things are gonna get complicated really quickly now. We're right at the moment where … the machete is heading towards your arm, and we're hoping it hits the coconut." So the speaker names the parts. Position 3 consists of a **control byte** (in the metadata) and a **slot** (which stores the value).

### The control byte encoding

**Listing 10** — code from the talk (flat_hash_set), shown at [27:41](https://www.youtube.com/watch?v=ncHmEUmJZf4&t=1661s)

```cpp
enum Ctrl : ctrl_t {
  kEmpty = -128,   // 0b10000000
  kDeleted = -2,   // 0b11111110
  kSentinel = -1,  // 0b11111111
  // kFull =          0b0xxxxxxx
};

size_t H1(size_t hash) { return hash >> 7; }
ctrl_t H2(size_t hash) { return hash & 0x7F; }
```

A `ctrl_t` is a signed byte. The special values all have the **high bit set** (they are negative); a full slot has the **high bit clear** and its seven low bits hold H2:

| Name | Value | Bits | Meaning |
|---|---|---|---|
| `kEmpty` | −128 | `1000 0000` | never used; ends a probe |
| `kDeleted` | −2 | `1111 1110` | tombstone |
| `kSentinel` | −1 | `1111 1111` | end marker for iteration |
| full | 0…127 | `0hhh hhhh` | occupied, low 7 bits = H2 |

"You may ask what that extra `kSentinel` is." It marks the end of the control bytes, so that a table scan (iteration) knows where to stop. "I'm gonna mention it now, and we're never gonna talk about it again. This never happened."

The choice of encodings is not arbitrary. "Is this slot full?" is a single sign test, and an exact match against an H2 value can never match a special byte, because H2 is always between 0 and 127. The two helper functions do exactly what the names say: `H1` shifts away the low seven bits, and `H2` keeps only them.

**Listing 11** — Classifying control bytes with simple bit tests *(illustrative example, not from the talk)*

```cpp
#include <cstdint>
#include <iostream>

using ctrl_t = signed char;
enum Ctrl : ctrl_t { kEmpty = -128, kDeleted = -2, kSentinel = -1 };

bool is_full(ctrl_t c)             { return c >= 0; }            // high bit clear
bool is_empty_or_deleted(ctrl_t c) { return c < kSentinel; }     // -128 or -2

std::size_t H1(std::size_t hash) { return hash >> 7; }
ctrl_t      H2(std::size_t hash) { return hash & 0x7F; }

int main()
{
    std::size_t hash = 0x7FDF9632F1F8EB43;          // the talk's example hash: 7F DF 96 32 F1 F8 EB 43
    std::cout << std::hex << "H1 = " << H1(hash) << ", H2 = " << int(H2(hash)) << '\n';   // H2 = 43
    for (ctrl_t c : {ctrl_t(kEmpty), ctrl_t(kDeleted), ctrl_t(kSentinel), H2(hash)})
        std::cout << std::dec << int(c) << ": full=" << is_full(c)
                  << " empty_or_deleted=" << is_empty_or_deleted(c) << '\n';
}
```

## 10. Finding with control bytes

*Talk segment: [27:05](https://www.youtube.com/watch?v=ncHmEUmJZf4&t=1625s)*

### Find, scalar version

"Quick show of hands: who wants to read all the code on this slide, and who wants to see a picture of how it works? … Sorry, coders, pictures have it." Here is the code, then the picture.

**Listing 12** — code from the talk (flat_hash_set), shown at [27:59](https://www.youtube.com/watch?v=ncHmEUmJZf4&t=1679s)

```cpp
iterator find(const K& key, size_t hash) const {
  size_t pos = H1(hash) % size_;
  while (true) {
    if (H2(hash) == ctrl_[pos] && key == slots_[pos])
      return iterator_at(pos);
    if (ctrl_[pos] == kEmpty) return end();
    pos = (pos + 1) % size_;
  }
}
```

1. Start at position `H1(hash) % size_`.
2. If the control byte equals `H2(hash)` **and** the slot's key equals the search key, we found it.
3. If the control byte is `kEmpty`, the key is not in the table, so stop.
4. Otherwise (a full slot with a different H2, a match whose key differs, or a tombstone) move to the next position, wrapping around at the end.

### The picture

The animation zooms in on the control bytes ("I made sure that eight of them fit in one of the others", meaning eight control bytes fit in the width of one slot in the diagram). For the element being searched:

- The first control byte has a different H2, so skip it without looking at the slot.
- The next is deleted, so keep going.
- "Hey, third position in our control bytes matches. H2, anyway. Let's go check our value." Yes, found.

"**Two memory accesses**. Pretty sweet." One is the control bytes and the other is the single slot whose H2 matched.

### Why it is fast

"It still does probing, but as you saw, most of the probing happens in the control bytes. And that means it all happens really dense in L1 cache. We can handle really large probe chains before we need to fetch another cache line." A 64-byte cache line holds 64 control bytes, while it might hold only a handful of large values.

The 7-bit H2 also makes false positives rare. A non-matching full slot has only a 1 in 128 chance of having the same H2, so we almost never compare keys needlessly. That matters when key comparison is expensive, as with strings.

Here is a complete, runnable toy table built around the slide's scalar `find`. It uses linear probing over separate control-byte and slot arrays, and an insert that is not on the slides.

**Listing 13** — A tiny table using the slide's scalar find *(illustrative example, not from the talk)*

```cpp
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iostream>
#include <optional>
#include <vector>

using ctrl_t = signed char;
enum Ctrl : ctrl_t { kEmpty = -128, kDeleted = -2, kSentinel = -1 };
std::size_t H1(std::size_t hash) { return hash >> 7; }
ctrl_t H2(std::size_t hash) { return hash & 0x7F; }

std::size_t mix(std::size_t h) { return h * 0x9E3779B97F4A7C15ull; }  // spread bits (see "Defaults matter")

template <typename K>
class scalar_table {
public:
    explicit scalar_table(std::size_t n) : size_(n), ctrl_(n, kEmpty), slots_(n) {}

    std::optional<std::size_t> find(const K& key, std::size_t hash) const {
        std::size_t pos = H1(hash) % size_;
        while (true) {
            if (H2(hash) == ctrl_[pos] && key == slots_[pos])
                return pos;
            if (ctrl_[pos] == kEmpty) return std::nullopt;
            pos = (pos + 1) % size_;
        }
    }
    std::optional<std::size_t> find(const K& key) const { return find(key, mix(std::hash<K>{}(key))); }

    void insert(const K& key) {                       // not on the slides
        std::size_t hash = mix(std::hash<K>{}(key));
        if (find(key, hash)) return;
        for (std::size_t pos = H1(hash) % size_;; pos = (pos + 1) % size_)
            if (ctrl_[pos] < 0) { ctrl_[pos] = H2(hash); slots_[pos] = key; return; }
    }

private:
    std::size_t size_;               // number of positions (the slide's size_)
    std::vector<ctrl_t> ctrl_;       // one control byte per position
    std::vector<K> slots_;           // values stored inline
};

int main()
{
    scalar_table<int> t(64);
    for (int i = 0; i < 40; ++i) t.insert(i * 3);
    std::cout << t.find(27).has_value() << t.find(28).has_value() << '\n';   // 10
}
```

## 11. SIMD: matching sixteen control bytes in three instructions

*Talk segment: [29:45](https://www.youtube.com/watch?v=ncHmEUmJZf4&t=1785s)*

### Sanjay and Jeff, again

"I kinda wish someone brilliant would just show up… yeah, I know it's lame to reuse the same joke, but it really did happen." The thought bubble this time says: *Use SSE instructions…*

**SIMD** (Single Instruction, Multiple Data) instructions apply one operation to several values at once. x86-64 processors all support **SSE2**, which works on 128-bit registers, so a single instruction can process sixteen bytes at once. Compilers expose these instructions as **intrinsics**: functions in `<emmintrin.h>` (or `<immintrin.h>`) that map one-to-one onto machine instructions.

Sixteen control bytes fit exactly in one SSE2 register. So instead of testing control bytes one at a time, we can test sixteen with a few instructions:

**Listing 14** — code from the talk (flat_hash_set), shown at [29:57](https://www.youtube.com/watch?v=ncHmEUmJZf4&t=1797s)

```cpp
BitMask<uint32_t> Match(h2_t hash) const {
  auto match = _mm_set1_epi8(hash);
  return BitMask<uint32_t>(
      _mm_movemask_epi8(_mm_cmpeq_epi8(match, ctrl)));
}
```

"A couple of machine instructions, everyone follows that instantly. Totes. I didn't spend three weeks with Alkis trying to understand this; you all got this." Here they are one at a time, as the slides show them.

### Step 1: `_mm_set1_epi8` broadcasts

"Set1 epi8 will initialize a 128-bit vector of sixteen 8-bit things. It's just splatting it out." The H2 value we are looking for (0x96 in the slide's example) is copied into all sixteen bytes of a register.

### Step 2: `_mm_cmpeq_epi8` compares bytewise

"Compare EQ epi8 takes two 128-bit things, and it gives you masks of zero or FF where the bytes line up." Comparing the broadcast value against sixteen control bytes gives a register in which each byte is `0xFF` where they are equal and `0x00` where they are not. In the slide's example, positions 2 and 10 hold `96`, so those two bytes come out `FF`.

### Step 3: `_mm_movemask_epi8` squeezes

"Movemask epi8 takes our big 128-bit thing and squishes it. Takes the high bit of each byte and gives us a 16-bit thing that has zeroes where they didn't match and ones where they do match." That is a **bit mask** with one bit per control byte, here bits 2 and 10.

"You get 16 probe length in three instructions. It's amazing."

### Using the mask

`Match` wraps the result in a `BitMask<uint32_t>`, a small class that can be iterated over its set bits. Each iteration yields the index of the next set bit, the position of a candidate. The standard technique is to take the count of trailing zero bits for the index, then clear the lowest set bit with `m &= m - 1`. Since C++20 the count is portable: `std::countr_zero` in `<bit>`.

**Listing 15** — Match: three SSE2 instructions and a bit-mask iterator *(illustrative example, not from the talk)*

```cpp
#include <emmintrin.h>
#include <bit>
#include <cstdint>
#include <cstdio>

template <typename T>
struct BitMask {
    T mask;
    explicit BitMask(T m) : mask(m) {}
    struct iterator {
        T m;
        int operator*() const { return std::countr_zero(m); }   // index of lowest set bit
        iterator& operator++() { m &= m - 1; return *this; }     // clear lowest set bit
        bool operator!=(const iterator& o) const { return m != o.m; }
    };
    iterator begin() const { return {mask}; }
    iterator end() const { return {0}; }
};

BitMask<std::uint32_t> Match(__m128i ctrl, std::uint8_t h2)
{
    auto match = _mm_set1_epi8(static_cast<char>(h2));                  // 96 96 96 ... 96
    return BitMask<std::uint32_t>(
        _mm_movemask_epi8(_mm_cmpeq_epi8(match, ctrl)));               // FF where equal -> bits
}

int main()
{
    // The control bytes from the slide: 7F DF 96 32 F1 F8 EB 43, twice.
    alignas(16) unsigned char bytes[16] = {0x7F, 0xDF, 0x96, 0x32, 0xF1, 0xF8, 0xEB, 0x43,
                                           0x7F, 0xDF, 0x96, 0x32, 0xF1, 0xF8, 0xEB, 0x43};
    __m128i ctrl = _mm_load_si128(reinterpret_cast<const __m128i*>(bytes));
    for (int i : Match(ctrl, 0x96)) std::printf("candidate at %d\n", i);   // 2 and 10
}
```

The slides also use `g.MatchEmpty()`, which is just `Match(kEmpty)`. The idea is the same with `kEmpty`'s bit pattern broadcast instead of H2.

### Platforms without SSE

From the Q&A: "On non-SSE platforms, we emulate these SSE instructions by using arithmetic on longs. They produce more false positives, but they are still correct. Your group size goes from 16 to 8." Bit tricks on a 64-bit integer test eight bytes at once.

## 12. Groups: find and erase sixteen at a time

*Talk segment: [31:55](https://www.youtube.com/watch?v=ncHmEUmJZf4&t=1915s)*

### Groups

"You can always tell when things are about to get complicated, because I start naming them." **A group is 16 control bytes and the 16 slots they describe**, and the table consists of N groups. ("It does mean our table will have a size of N times 16, but who cares. You never really wanted an exact size for your table. What you wanted was that sweet, sweet performance.")

The picture: "Step one, look at all of them in parallel. Step two, jump to the matching element." The whole group of sixteen control bytes is tested in one go, and the only slot touched is the one whose H2 matched. "It's okay if you're wowed by this. I still am."

### find, group version

**Listing 16** — code from the talk (flat_hash_set), shown at [32:15](https://www.youtube.com/watch?v=ncHmEUmJZf4&t=1935s)

```cpp
iterator find(const K& key, size_t hash) const {
  size_t group = H1(hash) % num_groups_;
  while (true) {
    Group g{ctrl_ + group * 16};
    for (int i : g.Match(H2(hash))) {
      if (key == slots_[group * 16 + i])
        return iterator_at(group * 16 + i);
    }
    if (g.MatchEmpty()) return end();
    group = (group + 1) % num_groups_;
  }
}
```

1. Start at group `H1(hash) % num_groups_`.
2. Load the group's control bytes and `Match` the H2. For each candidate position `i`, compare the key with slot `group * 16 + i`.
3. If the group contains any **empty** byte, the key is not in the table, so stop.
4. Otherwise move to the next group.

"Now is the moment when we should make some brilliant observations about this code":

- **The key comparison is almost always true.** "By the time we've gotten to this equality operator, we know that H1 mod our table size is a match for this hash code. We know that H2 is a match. … So this is always true, and we can just tell our compiler that. Predict true." (The real code uses a branch-prediction hint, which he left off the slide because "it's ugly and it's in all caps. Caps scare people." In C++20 you would write `[[likely]]`.)
- **Probing to the next group almost never happens.** It is needed only if *the entire group of 16 is full*, with no empty slot. "If you have a perfectly distributed hash table … you have to have a load factor greater than 94% before this if can ever be false." That hint goes to the compiler too.

Why does "the group has an empty slot" prove the key is absent? A key is always inserted into the *first* group along its probe sequence that has a free slot. If this group has an empty slot (not a tombstone), it had free space ever since it was created, so the key would have been placed here or in an earlier group. Since it wasn't found here, it isn't anywhere.

### erase, group version

**Listing 17** — code from the talk (flat_hash_set), shown at [34:49](https://www.youtube.com/watch?v=ncHmEUmJZf4&t=2089s)

```cpp
void erase(iterator it) {
  --size_;
  Group g{(it.ctrl_ - ctrl_) / 16 * 16 + ctrl_};
  *it.ctrl_ = g.MatchEmpty() ? kEmpty : kDeleted;
  it.slot_.~K()
}
```

The same reasoning gives erase a gift. "If any other element in the group was empty, you don't have to put a tombstone in. You can just set it back to empty." No search could ever have probed *past* this group, because the group always had an empty slot to stop at. So erasing here cannot break any probe chain. "Which means you don't need to hurt your load factor, and then all your finds are more likely to never need that branch. It's just win-win."

Two small details in the slide code. The first line of the body rounds the iterator's control-byte pointer down to the start of its 16-byte group. And `it.slot_.~K()` calls the element's destructor in place; the slide is missing the semicolon after it.

### Assembling the pieces

Here is a working toy table assembled from the slides' `Ctrl`, `H1`/`H2`, `Match`, `find` and `erase`, with the small amount of glue the slides leave out (`Group`, `BitMask`, an insert without growth). It was run for this chapter. It found all 60 inserted keys, and erase worked as described. It also gave a lesson for free: with `std::hash<int>` (the identity function on this platform) and consecutive multiples of 7, every key landed in the first four groups. Those groups filled up, so every erase there *had* to leave a tombstone. That is the hash-quality problem of the "Defaults matter" section in miniature, so the example applies a mixing step. With it, the same 60 inserts and 30 erases print `found 60, size 30, tombstones 0`: every erase found an empty slot in its group and needed no tombstone. With the identity hash, all 30 erases left tombstones.

**Listing 18** — A toy SwissTable-style set assembled from the slides *(illustrative example, not from the talk)*

```cpp
// Builds on Listing 10, Listing 14, Listing 16 and Listing 17 (Ctrl, H1/H2, Match, find, erase); glue code added.
#include <emmintrin.h>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <vector>

using ctrl_t = signed char;
using h2_t = std::uint8_t;
enum Ctrl : ctrl_t { kEmpty = -128, kDeleted = -2, kSentinel = -1 };
std::size_t H1(std::size_t hash) { return hash >> 7; }
ctrl_t H2(std::size_t hash) { return hash & 0x7F; }

template <typename T> struct BitMask {
  T mask;
  explicit BitMask(T m) : mask(m) {}
  struct iterator { T m; int operator*() const { return std::countr_zero(m); }
                    iterator& operator++() { m &= m - 1; return *this; }
                    bool operator!=(const iterator& o) const { return m != o.m; } };
  iterator begin() const { return {mask}; }
  iterator end() const { return {0}; }
  explicit operator bool() const { return mask != 0; }
};

struct Group {
  __m128i ctrl;
  explicit Group(const ctrl_t* p) : ctrl(_mm_loadu_si128(reinterpret_cast<const __m128i*>(p))) {}
  BitMask<std::uint32_t> Match(h2_t hash) const {
    auto match = _mm_set1_epi8(static_cast<char>(hash));
    return BitMask<std::uint32_t>(_mm_movemask_epi8(_mm_cmpeq_epi8(match, ctrl)));
  }
  BitMask<std::uint32_t> MatchEmpty() const { return Match(static_cast<h2_t>(kEmpty)); }
  BitMask<std::uint32_t> MatchEmptyOrDeleted() const {    // special bytes are < kSentinel
    return BitMask<std::uint32_t>(_mm_movemask_epi8(_mm_cmpgt_epi8(_mm_set1_epi8(kSentinel), ctrl)));
  }
};

template <typename K> class toy_flat_set {
public:
  explicit toy_flat_set(std::size_t groups) : num_groups_(groups), ctrl_(groups * 16, kEmpty), slots_(groups * 16) {}
  struct iterator { ctrl_t* ctrl_; K* slot_; bool operator==(const iterator&) const = default; };
  iterator end() { return {nullptr, nullptr}; }
  iterator iterator_at(std::size_t i) { return {&ctrl_[i], &slots_[i]}; }
  static std::size_t hash_of(const K& k) { return std::hash<K>{}(k) * 0x9E3779B97F4A7C15ull; }

  iterator find(const K& key) { return find(key, hash_of(key)); }
  iterator find(const K& key, std::size_t hash) {
    std::size_t group = H1(hash) % num_groups_;
    while (true) {
      Group g{ctrl_.data() + group * 16};
      for (int i : g.Match(H2(hash))) {
        if (key == slots_[group * 16 + i])
          return iterator_at(group * 16 + i);
      }
      if (g.MatchEmpty()) return end();
      group = (group + 1) % num_groups_;
    }
  }

  void insert(const K& key) {                                 // glue: no growth
    std::size_t hash = hash_of(key);
    if (find(key, hash) != end()) return;
    for (std::size_t group = H1(hash) % num_groups_;; group = (group + 1) % num_groups_) {
      if (auto free = Group{ctrl_.data() + group * 16}.MatchEmptyOrDeleted()) {
        std::size_t pos = group * 16 + *free.begin();
        ctrl_[pos] = H2(hash); slots_[pos] = key; ++size_;
        return;
      }
    }
  }

  void erase(iterator it) {
    --size_;
    Group g{(it.ctrl_ - ctrl_.data()) / 16 * 16 + ctrl_.data()};
    *it.ctrl_ = g.MatchEmpty() ? kEmpty : kDeleted;
    it.slot_->~K();
  }

  std::size_t size() const { return size_; }
  std::size_t tombstones() const { std::size_t n = 0; for (auto c : ctrl_) n += (c == kDeleted); return n; }

private:
  std::size_t num_groups_, size_ = 0;
  std::vector<ctrl_t> ctrl_;
  std::vector<K> slots_;
};

int main()
{
  toy_flat_set<int> s(8);                                     // 8 groups = 128 positions
  for (int i = 0; i < 60; ++i) s.insert(i * 7);
  int found = 0;
  for (int i = 0; i < 420; ++i) found += (s.find(i) != s.end());
  for (int i = 0; i < 30; ++i) s.erase(s.find(i * 7));
  std::printf("found %d, size %zu, tombstones %zu\n", found, s.size(), s.tombstones());
}
```

### How fast is it?

The benchmark slide shows four panels: find-hit and find-miss, for small (4-byte) and large (64-byte) payloads. Red is `std::unordered_set`, blue is `dense_hash_set` and green is the new `flat_hash_set`.

- "We are crushing `std::unordered_set`, just across the board."
- "We actually crush `dense_hash_set` most of the time." The exception is **find-hit with 4-byte values**, where `dense_hash_set` is slightly faster. "When your values are four bytes, the one byte you spend on metadata for probing is kind of expensive as a ratio", and the shorter probe chains don't make up for the extra cache line.
- For **misses**, SwissTable shines. "If your elements aren't in the set, you don't even go to the values. You stay right in the metadata, most of the time."

## 13. Ergonomics: bear traps in std::unordered_map

*Talk segment: [36:35](https://www.youtube.com/watch?v=ncHmEUmJZf4&t=2195s)*

### Something completely different

The next slide is Monty Python's man at a desk on a beach: "And now for something completely different." "Thus far, everything I've explained to you is about **algorithmics**." Now he turns to **ergonomics**: how easy the container is to use correctly. "And don't worry, `std::unordered_map` was built with all the loving care of `std::vector<bool>`."

The next slide is a field of bear traps. "We're gonna play a little game. People just throw up a hand when they realise what bear trap they're standing in."

### Trap 1: a temporary key on every find

**Listing 19** — code from the talk ((code slide)), shown at [37:45](https://www.youtube.com/watch?v=ncHmEUmJZf4&t=2265s)

```cpp
void Benchmark_Slow(int iters) {
  std::unordered_map<string, int> m;
  const char key[] = "abcdefghijklmnopqrst";
  while (iters--) m.find(key);
}
```

"Your `const char*` key is being created as a temporary string in that find call. It is, of course, 2x slower." `find` takes a `const key_type&`, so passing a `const char*` constructs a `std::string` on every call. A 20-character string is too long for the small-string buffer, so that also means a heap allocation. Passing a real `std::string` avoids it:

**Listing 20** — code from the talk ((code slide)), shown at [38:03](https://www.youtube.com/watch?v=ncHmEUmJZf4&t=2283s)

```cpp
void Benchmark_Fast(int iters) {
  std::unordered_map<string, int> m;
  string key = "abcdefghijklmnopqrst";
  while (iters--) m.find(key);
}
```

"**Heterogeneous lookup** would fix this, but `std::unordered_set` and `map` don't get heterogeneous lookup. They do for our containers, because we love the user." Heterogeneous lookup means looking up with a different but compatible type, such as `std::string_view` or `const char*` for `std::string` keys, without creating a key object. The ordered containers gained it in C++14. **C++20 added it to the unordered containers**: the hash and equality types must both declare `is_transparent`.

### Trap 2: the wrong insert overload

**Listing 21** — code from the talk ((code slide)), shown at [38:31](https://www.youtube.com/watch?v=ncHmEUmJZf4&t=2311s)

```cpp
void Benchmark_Slow(int iters) {
  std::unordered_map<string, int> m;
  std::pair<const string, int> p = {};
  while (iters--) m.insert(p);
}
```

"This one is a little bit harder." The fix is a single `const`:

**Listing 22** — code from the talk ((code slide)), shown at [38:41](https://www.youtube.com/watch?v=ncHmEUmJZf4&t=2321s)

```cpp
void Benchmark_Fast(int iters) {
  std::unordered_map<string, int> m;
  const std::pair<const string, int> p = {};
  while (iters--) m.insert(p);
}
```

"Because you don't have a const on your `p`, your insert is going to be the [forwarding] overload and not the const-ref overload, and thus you are making a copy when you thought you were doing all the right things. It's C++. Really?" `unordered_map::insert` has an overload for `const value_type&`, one for `value_type&&`, and a template `insert(P&&)` that accepts anything convertible to `value_type`. For a non-const lvalue `pair`, the template is the better match, and in the library he was using it constructed a node first.

### Trap 3: emplace allocates first

**Listing 23** — code from the talk ((code slide)), shown at [39:09](https://www.youtube.com/watch?v=ncHmEUmJZf4&t=2349s)

```cpp
void Benchmark_Slow(int iters) {
  std::unordered_map<string, int> m;
  const string key = "";
  while (iters--) m.emplace(key, 0);
}
```

"Naturally, calling `emplace` will new a node on the heap, then try and do the emplace, and realise, oh, I already have this in the container, and then free that node from the heap for you." The `Benchmark_Fast` for this trap on the slide is the `const pair` `insert` again: "don't forget the const down here, or you're back in the previous bear trap". Since C++17 there is also `try_emplace`, which takes the key separately and constructs nothing if the key is already present.

### Do the traps still bite?

The talk used libstdc++ from GCC 4.8/4.9. For this chapter, the three traps were re-measured by counting heap allocations with GCC 16's libstdc++, with the key already present and 100 calls each:

| Call | Allocations |
|---|---|
| `find(const char*)` | **100**, one temporary `std::string` per call |
| `find(std::string)` | 0 |
| heterogeneous `find(std::string_view)` (C++20) | 0 |
| `insert(non-const pair)` | 0 |
| `insert(const pair)` | 0 |
| `emplace(key, 0)` | 0 |
| `try_emplace(key, 0)` | 0 |

**Trap 1 is still real**, and C++20 heterogeneous lookup is the fix. **Traps 2 and 3 no longer cost an allocation in current libstdc++**, which now recognises a key it can extract from the arguments and checks for it before building a node. That is the very idea the next slide describes. The standard still doesn't guarantee it, and other standard libraries may differ, so `try_emplace` and a `const` pair remain good habits.

**Listing 24** — Counting allocations to expose the bear traps *(illustrative example, not from the talk)*

```cpp
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <new>
#include <string>
#include <string_view>
#include <unordered_map>

static long g_allocs = 0;
void* operator new(std::size_t n) { ++g_allocs; if (void* p = std::malloc(n)) return p; throw std::bad_alloc(); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

template <typename F> long allocs(F f) { long before = g_allocs; f(); return g_allocs - before; }

// C++20 heterogeneous lookup: transparent hash + transparent equality.
struct string_hash {
    using is_transparent = void;
    std::size_t operator()(std::string_view s) const { return std::hash<std::string_view>{}(s); }
};

int main()
{
    const char key[] = "abcdefghijklmnopqrst";       // longer than the small-string buffer

    std::unordered_map<std::string, int> m{{key, 1}};
    std::printf("find(const char*): %ld\n", allocs([&] { for (int i = 0; i < 100; ++i) (void)m.find(key); }));

    std::unordered_map<std::string, int, string_hash, std::equal_to<>> h{{key, 1}};
    std::printf("transparent find : %ld\n", allocs([&] { for (int i = 0; i < 100; ++i) (void)h.find(std::string_view(key)); }));

    std::pair<const std::string, int> p{key, 0};
    std::printf("insert(pair&)    : %ld\n", allocs([&] { for (int i = 0; i < 100; ++i) m.insert(p); }));
    std::string k = key;
    std::printf("emplace(k, 0)    : %ld\n", allocs([&] { for (int i = 0; i < 100; ++i) m.emplace(k, 0); }));
    std::printf("try_emplace(k, 0): %ld\n", allocs([&] { for (int i = 0; i < 100; ++i) m.try_emplace(k, 0); }));
}
```

### Looking through the arguments

"It turns out we can avoid constructing these temporary objects if we do something reasonably clever with templates. … The answer is: it's simple enough to fit on one slide." The slide is a wall of tiny template code, with names like `DecomposePair`, `PairArgs` and `WithConstructed`. "It doesn't fit well, and it kinda makes my brain hurt." What it does: "it grovels through the arguments you were passed, and it tries really hard to see their true nature. Once it has seen the platonic ideal of the arguments … it uses that to look at the hash code", instead of allocating a node first. "Please don't ask me how. Roman is the person you need to ask how. Fortunately, he's not here."

## 14. Defaults matter: hash quality

*Talk segment: [40:25](https://www.youtube.com/watch?v=ncHmEUmJZf4&t=2425s)*

### A 25x slowdown

"This one is a little bit harder. You're gonna need to know some of the internals of `dense_hash_map`."

**Listing 25** — code from the talk ((code slide)), shown at [41:05](https://www.youtube.com/watch?v=ncHmEUmJZf4&t=2465s)

```cpp
void Benchmark_Slow(int iters) {
  dense_hash_map<int, int> m;
  std::uniform_int_distribution<int> dist;
  while (iters--)
    m.insert(std::make_pair(dist(rng) << 10, 0));
}
```

"First hint: it is a probing table. … Second hint: `std::hash` for an int is an identity." In the major standard libraries, `std::hash<int>` simply returns the integer. "Remember when I talked about low-order bits and entropy? We're shifting left by ten bits, so everything has the same low-order bits in its hash code. And it's **25 times slower**."

`dense_hash_map` sizes its table as a **power of two** and picks the position by masking off the low bits (`hash & (capacity - 1)`), which is the cheap alternative to a modulo. With identity hashing, every key of the form `x << 10` has its ten lowest bits equal to zero. For any table of up to 1024 slots, *every key maps to the same slot*, and lookups degenerate into scanning one long probe chain.

"All you need to do to fix it is to give it a better hash function":

**Listing 26** — code from the talk ((code slide)), shown at [41:27](https://www.youtube.com/watch?v=ncHmEUmJZf4&t=2487s)

```cpp
void Benchmark_Slow(int iters) {
  dense_hash_map<int, int, ScalarHash> m;
  std::uniform_int_distribution<int> dist;
  while (iters--)
    m.insert(std::make_pair(dist(rng) << 10, 0));
}
```

(The slide names the fixed version `Benchmark_Slow` as well. With `ScalarHash`, a hash that mixes the bits, it is the fast one.)

### The power of defaults

"We made that a little better for you. We provide a better hash function by default. You're welcome." (He regrets not having watched *Moana* earlier: "I totally would have put the 'You're Welcome' there.")

"These things are all subtle. They interact really deeply with each other. The power of defaults is massive. We're not gonna give you a button to blow up the world on the first attempt." The new tables pair themselves with a hash that spreads entropy across all bits. Users get good behaviour without having to know about low-order bits, power-of-two masks or H1/H2 splits.

### What a mixing step does

A good mixer makes every input bit influence every output bit. A cheap and effective one for integers is *multiplicative hashing*: multiply by a large odd constant, such as 2⁶⁴ divided by the golden ratio, and use the **high** bits of the product, where all the input bits have had a chance to contribute. The example counts how many distinct positions the keys `i << 10` reach in a 1024-slot power-of-two table, with and without mixing:

**Listing 27** — Identity hash versus a mixed hash in a power-of-two table *(illustrative example, not from the talk)*

```cpp
#include <cstdint>
#include <functional>
#include <iostream>
#include <set>

std::uint64_t mix(std::uint64_t h)
{
    h *= 0x9E3779B97F4A7C15ull;      // ~2^64 / golden ratio, odd
    return h ^ (h >> 32);            // fold the well-mixed high bits down
}

int main()
{
    constexpr std::uint64_t capacity = 1024;               // power of two
    std::set<std::uint64_t> identity_positions, mixed_positions;
    for (std::uint64_t i = 0; i < 1000; ++i) {
        std::uint64_t key = i << 10;
        identity_positions.insert(std::hash<std::uint64_t>{}(key) & (capacity - 1));
        mixed_positions.insert(mix(key) & (capacity - 1));
    }
    std::cout << "distinct positions, identity hash: " << identity_positions.size() << '\n'   // 1
              << "distinct positions, mixed hash:    " << mixed_positions.size() << '\n';     // 801
}
```

From the Q&A: power-of-two sizes are "really nice, because it lets you just do a bit mask to get the lowest bits. It's bad because, if your hash function isn't good … that bit mask comes by and hurts you." The team was experimenting with other sizes and with strengthening weak hash functions inside the table, at the cost of "a few extra instructions. So it's a trade-off. I hear trade-offs are a thing."

## 15. Rolling out at Google, and defending against Hyrum

*Talk segment: [42:15](https://www.youtube.com/watch?v=ncHmEUmJZf4&t=2535s)*

### Rolling it out

"We've even begun migrating … across all of Google. We are sending changes to everybody's code in Google: 'Hey, you got an `unordered_set`. Switch you to this one.'" The next slide shows a baby Groot hovering over a big red button.

Two charts from Google's live sampling profiler in production ("This is God's truth, as near as we can tell it") show the effect between June and August 2017:

- **Percentage of fleet-wide CPU in hash tables**, as a stacked chart with the new tables as a growing band. About **half a percent of fleet-wide CPU** saved so far.
- **Percentage of fleet-wide RAM**: "We are saving a little over one percent. Ballpark one percent of fleet-wide RAM." Note the axis change: 2.4% at the top of the CPU chart, 6% for RAM.

"These numbers are really big when you multiply it by the size of Google."

### How do you change anything under Hyrum's Law?

"Wait, you're doing a rollout, but you've also said that there are experiments in the works. How are you doing this without Hyrum coming and beating down your door? … The answer is careful defense." The slide shows a three-card monte game: something that looks predictable but isn't.

**Listing 28** — code from the talk ((code slide)), shown at [44:03](https://www.youtube.com/watch?v=ncHmEUmJZf4&t=2643s)

```cpp
bool demo() {
  flat_hash_set<int> i1 = { 0, 1 };
  flat_hash_set<int> i2 = { 0, 1 };
  return *i1.begin() == *i2.begin();
}
```

"Anyone guess what this returns?" Two tables with the same contents, inserted in the same order: do they iterate in the same order? With `std::unordered_set` they would, because its order is a deterministic function of the insertion history. That is exactly the kind of observable behaviour people come to depend on. SwissTable makes it **unpredictable on purpose**.

**Per-table seed.** H1 is mixed with a seed:

**Listing 29** — code from the talk ((code slide)), shown at [44:35](https://www.youtube.com/watch?v=ncHmEUmJZf4&t=2675s)

```cpp
size_t HashSeed(const ctrl_t* ctrl) {
#if !defined(NDEBUG)
  static const size_t kSeed = rand();
  return kSeed ^
#else
  return
#endif
      (reinterpret_cast<uintptr_t>(ctrl) >> 12);
}

size_t H1(size_t hash) {
  return (hash >> 7) ^ HashSeed(ctrl_);
}
```

- In **debug** builds (`NDEBUG` not defined), the seed includes `rand()`, fixed once per process, combined with the table's address.
- In **optimised** builds it uses only the address of the table's control bytes. "We use **address space layout randomisation** to give us just a little bit of randomness." The operating system places allocations at randomised addresses, so tables get different seeds from run to run at no cost. "And that shift by 12? That's because we found out it works better." The low twelve bits of an address are largely fixed by page alignment, so they carry little randomness.

"I've done some things I ain't proud of, and the things I am proud of are frankly disgusting."

**Random position within a group.** In debug builds, insertion also randomly chooses the front or the back of the group's free slots:

**Listing 30** — code from the talk ((code slide)), shown at [44:51](https://www.youtube.com/watch?v=ncHmEUmJZf4&t=2691s)

```cpp
size_t find_insert_position(size_t group_pos, size_t hash) {
#if !defined(NDEBUG)
  // To force small tables to have random entries, insert into
  // either the front or back of the group.
  if (H1(hash, ctrl_) % 13 > 6)
    return group_pos + mask.HighestBitSet();
  else
    return group_pos + mask.LowestBitSet();
#else
  return group_pos + mask.LowestBitSet();
#endif
}
```

"It's a kinda cute way to be like, flip a coin for me. I mean, it's almost a coin." The result for the demo is on the next slide: **true 50.3% of the time (in debug mode)**. "Pretty sure somebody is going to start using `flat_hash_set` to generate random bits, and then they're gonna complain when I change the distribution. 'I was relying on that 50.3!'"

The randomisation turns hidden order dependencies into test failures early, in debug builds and in tests, rather than into breakage years later when the implementation changes. A minimal version of the idea follows. Mixing a per-process random seed into the hash makes iteration order differ between runs, so order-dependent code fails fast.

**Listing 31** — Catching order dependence with a per-process seed *(illustrative example, not from the talk)*

```cpp
#include <cstdint>
#include <iostream>
#include <random>
#include <unordered_set>
#include <vector>

// A hasher that mixes in a seed chosen once per process (like the talk's debug-mode kSeed).
struct seeded_hash {
    static std::uint64_t seed()
    {
        static const std::uint64_t s = std::random_device{}();
        return s;
    }
    std::size_t operator()(int v) const
    {
        std::uint64_t h = (static_cast<std::uint64_t>(v) ^ seed()) * 0x9E3779B97F4A7C15ull;
        return static_cast<std::size_t>(h ^ (h >> 32));
    }
};

int main()
{
    std::unordered_set<int, seeded_hash> s{1, 2, 3, 4, 5, 6, 7, 8};
    std::vector<int> order(s.begin(), s.end());

    // Code like this "works" with a deterministic hash and silently depends on it.
    // With a seeded hash its output changes from run to run, exposing the bug in testing.
    std::cout << "first element this run: " << order.front() << '\n';
}
```

## 16. Why 'Swiss', and the experiments in progress

*Talk segment: [45:15](https://www.youtube.com/watch?v=ncHmEUmJZf4&t=2715s)*

### Why "SwissTable"?

"I read the program description and it said Swiss table, but he hasn't said Swiss table more than once. Why is it called Swiss table?" The answer has three parts: "The primary developers, Alkis and Roman, are in the Zurich office." "Closed hashing, abbreviated 'ch', is also the top-level domain for Switzerland." And "Swiss efficiency has just a good spot in the zeitgeist." (*Closed hashing* is another name for open addressing.) The slide shows a gigantic Swiss Army knife. "In the end, we decided to name it something more descriptive, but we still use the codename." The descriptive names are `flat_hash_set`/`flat_hash_map` and `node_hash_set`/`node_hash_map`.

### Experiment 1: groups at arbitrary positions

The slide carries a warning sign: this is work in progress. So far, groups are aligned at multiples of 16. "What if any offset … can start a group?" A probe then loads the 16 control bytes starting *at the hashed position*, wherever that is.

**Gains:**
- "Better probing. You have sort of more different windows you can probe in."
- "Better randomisation", which lets the address-based seed shuffle order in optimised builds too.
- "It lets us raise the default max load factor from **seven-eighths to fifteen-sixteenths**. And that is a lot of RAM. And we can raise the max load factor and be faster."

**Cost:** a 16-byte load that starts near the end of the control array would run off the end. So the table keeps a **copy of its first 16 control bytes after the last one**, and "insert requires us to write two control bytes in some cases": the real one and its mirror.

**Listing 32** — Mirroring the first control bytes so a group can start anywhere *(illustrative example, not from the talk)*

```cpp
#include <cstddef>
#include <cstdio>
#include <vector>

using ctrl_t = signed char;
constexpr ctrl_t kEmpty = -128;
constexpr std::size_t kGroupWidth = 16;

struct control_bytes {
    explicit control_bytes(std::size_t capacity)
        : capacity_(capacity), bytes_(capacity + kGroupWidth, kEmpty) {}

    // Every write to one of the first 16 bytes is mirrored past the end, so that a
    // 16-byte load starting at any position i < capacity reads valid, wrapped-around data.
    void set(std::size_t i, ctrl_t v)
    {
        bytes_[i] = v;
        if (i < kGroupWidth) bytes_[capacity_ + i] = v;     // the second write
    }

    const ctrl_t* group_at(std::size_t i) const { return bytes_.data() + i; }   // any offset

    std::size_t capacity_;
    std::vector<ctrl_t> bytes_;
};

int main()
{
    control_bytes c(64);
    c.set(3, 0x2A);                                   // also appears at index 64 + 3
    const ctrl_t* g = c.group_at(60);                 // a group spanning the wrap-around
    std::printf("byte 7 of the group starting at 60 (= position 3): %d\n", g[7]);   // 42
}
```

"This experiment is probably going to land. I wouldn't be surprised if Sam landed it while I was talking, just to spite me." It did land. Open-source SwissTable probes from arbitrary positions and mirrors its first control bytes, with a different count of cloned bytes than in this sketch.

### Experiment 2: cache-line-aware groups (node only)

"This was a really fun experiment. It's kind of a failure in some way, but I can't resist showing it", with an explosion sound effect. For node-based tables, where slots are 8-byte pointers, lay out each group as exactly one cache line:

| Part | Size |
|---|---|
| "Ever full?" flag | 1 bit |
| Presence bits | 7 × 1 bit |
| Hashes (H2, now **8 bits** each) | 7 × 1 byte |
| Pointers | 7 × 8 bytes |
| **Total** | **64 bytes**, one cache line |

Seven pointers take 56 bytes, which leaves 8 bytes of metadata: seven full-byte hashes (so an 8-bit H2, "which is nice") plus one byte of flags. The single "has this group ever been full?" bit is all that find needs to decide whether to probe on. "The hardware people in the room will think, oh man, that's an L1 cache line. This is really brilliant. And it is."

But "if you have eight-byte values, it is very slightly faster. But most of the time you don't have eight-byte values, and the cost of specialising both of them was a lot of code complexity." It was not worth it for a general library. "But if I were implementing a Python interpreter, or a core hash table in any kind of interpreted language that always forced me to have an indirection, that would be my go-to."

## 17. Questions and answers

*Talk segment: [49:20](https://www.youtube.com/watch?v=ncHmEUmJZf4&t=2960s)*

The talk ends with a slide reading *Questions?* over a picture of an elephant painted to match the wallpaper of its room. "The clever man in the front row has observed that I will not talk about the elephant in the room." Here are the questions and answers, grouped by theme.

### Availability

- **Will it be open-sourced?** "I hear Abseil's a thing. I'd keep my eye on Abseil. We're not gonna commit to a timeline, but if I were a betting man, I would put money on before the end of the year." SwissTable was indeed released in Abseil, as `absl::flat_hash_map`, `absl::flat_hash_set`, `absl::node_hash_map` and `absl::node_hash_set`.
- **Is the existing `dense_hash_map` still supported?** It is reliable ("many production services at Google use it"), but "are we supporting it? No. We can't be bothered. It's old and crufty."

### Sizing and growth

- **How does the table grow?** "The table grows when it goes above its max load factor. Because no one knows how to use max load factor correctly, the set max load factor method is a no-op. You're welcome." The capacity is a **power of two**, so the position can be computed with a mask. That is fast, but sensitive to weak hashes, which is why the default hash matters so much.
- **When is a linear scan better?** For a tiny set, say three 4-byte values, "just use an array and scan them". Google has `flat_set` and `flat_map`, sorted containers backed by a vector. But because SwissTable scans 16 control bytes in a single instruction, "it's very competitive for most things".
- **Could type traits describe where a hash function's entropy is?** "You can", but "the performance effect of getting your hash function wrong is so deleterious, it's often on the order of 30 times", so the team leans towards "we will make your hash good."

### Deletes and reuse

- **Heavy delete loads?** "It is reasonably fast", but the priorities are "find, then insert, and then erase".
- **The grow-then-clear users** from the start of the talk now get special treatment: "On our clear call, we won't actually keep that full million elements. We will drop you down to 128 elements on the first clear, because otherwise your table scan might go to hell." To keep the capacity, call `erase(begin(), end())`: "that's your back door".

### Comparisons

- **Robin Hood hashing?** "We actually did implement a Robin Hood hash on top of the same thing, and it turns out all of the extra logic around it is more expensive. The whole raison d'être of Robin Hood hashing is we can reduce the length of probe chains. But if you can scan a longer probe chain for free, it doesn't matter."
- **Benchmarks and blog posts:** "The difference between how much code you need to implement in order to get a benchmark and how much code you need to implement to have a useful map is way different. We got the benchmarks, and then we spent months filling out the tables and adding enough tests."
- **Assembly tricks?** Only intrinsics. The rest was Jeff Lim "look[ing] at the generated assembly, look[ing] at the code, … tweak[ing] the code until it generated the assembly he wanted".

### Platforms and miscellany

- **Platforms without SSE?** Emulated with 64-bit integer arithmetic, with groups of 8 and more false positives, "but they are still correct". Chrome and Android had expressed interest.
- **What is "Google3"?** The third iteration of Google's source control, after SVN and Perforce: "it is the monorepo".
- **The `const` pair trap, again:** "If you hand it the local that is not const, you will end up in the [forwarding] overload, with a temporary being created. You might call it a bug in the standard. I would call it 'C++ loves you'."

### Using the result today

Abseil's tables are drop-in replacements for most uses of the unordered containers, with heterogeneous lookup and good default hashing built in. The same ideas, metadata bytes and SIMD group probing, have since appeared in other hash tables, including `boost::unordered_flat_map`. Here is what typical use looks like. It needs Abseil installed, so it was not compiled for this chapter.

**Listing 33** — Using absl::flat_hash_map (requires Abseil; not compiled here) *(illustrative example, not from the talk)*

```cpp
#include <string>
#include <string_view>
#include "absl/container/flat_hash_map.h"

int main()
{
    absl::flat_hash_map<std::string, int> ages;
    ages["ada"] = 36;
    ages.try_emplace("alan", 41);

    // Heterogeneous lookup works out of the box for string keys:
    std::string_view who = "ada";
    if (auto it = ages.find(who); it != ages.end()) return it->second == 36 ? 0 : 1;
    return 1;
}
```

## Key takeaways

- Every design step should be judged by what it gains and what it loses, in memory, speed and guaranteed behaviour.
- std::unordered_set pays for pointer stability and its bucket API with extra pointers, stored hashes and up to four memory accesses per lookup.
- Open addressing with values stored inline is far faster but needs a way to mark empty and deleted slots.
- SwissTable keeps one control byte per slot: one bit says 'special or full', and full entries keep seven bits of the hash (H2), while the other 57 bits (H1) choose the position.
- Three SSE instructions compare H2 against sixteen control bytes at once, so most lookups touch only one metadata cache line and one slot.
- Groups make long probe sequences rare, and erase can often clear a slot to empty instead of leaving a tombstone.
- API design matters as much as algorithms: heterogeneous lookup, careful insert overloads and good default hashes avoid 2x-25x slowdowns.
- Randomising iteration order keeps users from depending on it, which keeps the implementation free to improve.

## Glossary

| Term | Meaning |
|---|---|
| Separate chaining | A hash table where each bucket holds a linked list (chain) of the elements that hash to it. |
| Open addressing (probing) | A hash table where elements live in the array itself; on a collision you probe other positions in a fixed sequence. |
| Load factor | Number of elements divided by the number of buckets or slots; tables grow when it exceeds a maximum. |
| Tombstone | A marker left in a probing table where an element was erased, so that searches continue past it. |
| Pointer stability | The guarantee that an element's address does not change while it stays in the container. |
| Sentinel value | A special key value reserved to mean 'empty' or 'deleted' instead of storing separate metadata. |
| Control byte | SwissTable's one-byte metadata per slot: empty, deleted, sentinel, or full plus seven bits of hash. |
| H1 / H2 | The two parts of the hash in SwissTable: H1 (57 bits) chooses the position, H2 (7 bits) is stored in the control byte. |
| Group | Sixteen consecutive control bytes (and their slots) examined together with one SIMD comparison. |
| SIMD / SSE2 | Single Instruction, Multiple Data; x86 SSE2 instructions operate on 128-bit registers, e.g. 16 bytes at once. |
| Cache line | The unit of memory transferred between RAM and CPU caches, typically 64 bytes. |
| Heterogeneous lookup | Looking up a key using a different but comparable type (e.g. string_view for string keys) without constructing a temporary key. |
| Hyrum's Law | With enough users, every observable behaviour of an API will be depended on by somebody. |
| ASLR | Address Space Layout Randomization: the OS places memory at randomised addresses, which SwissTable uses as a cheap seed. |
| Robin Hood hashing | An open-addressing scheme that moves elements to equalise probe distances. |

## Exercises

1. Count the memory accesses needed to find an element in libstdc++'s std::unordered_set, node_hash_set and flat_hash_set as described in the talk. Which ones are likely to be cache misses for a cold table?

   <details><summary>Hint</summary>Bucket array, preceding node, node, value; then pointer array + node; then control bytes + slot.</details>

2. Why can a probing table not stop a search at a deleted slot, and why does that make erase 'not free up load factor'?

   <details><summary>Hint</summary>An element inserted after the erased one may have probed past it.</details>

3. Show the bit patterns of kEmpty, kDeleted and kSentinel as int8_t values, and write a one-line test for 'is this control byte full?'

   <details><summary>Hint</summary>Full bytes have the high bit clear: ctrl >= 0.</details>

4. With 7-bit H2 values, what is the expected number of false-positive candidates when matching a full group of 16 control bytes?

   <details><summary>Hint</summary>Each of the 16 has a 1/128 chance of matching by accident.</details>

5. In the group-based erase, explain why setting a slot back to kEmpty is safe whenever the group still contains an empty byte.

   <details><summary>Hint</summary>find stops at a group containing an empty slot; no probe sequence can have continued past this group.</details>

6. Write a benchmark that shows the cost of std::unordered_map<std::string,int>::find(const char*) and fix it with C++20 heterogeneous lookup.

   <details><summary>Hint</summary>A transparent hash (is_transparent) and std::equal_to<> enable find(std::string_view).</details>

7. Explain why inserting keys of the form i << 10 into a power-of-two table with std::hash<int> collides badly, and design a cheap mixing function that fixes it.

   <details><summary>Hint</summary>Masking keeps only the low bits; multiply by a large odd constant and use the high bits.</details>

8. Why do the arbitrary-group-positions experiment need 16 extra mirrored control bytes, and what does that buy?

   <details><summary>Hint</summary>A 16-byte load starting near the end must read valid bytes; any start position gives more probing windows and a higher max load factor.</details>

## Further reading

- Abseil (abseil.io): absl::flat_hash_map / flat_hash_set / node_hash_map, mentioned in the Q&A as the open-source home
- Google's dense_hash_map / dense_hash_set (sparsehash)
- Hyrum's Law (Hyrum Wright)
- Intel intrinsics: _mm_set1_epi8, _mm_cmpeq_epi8, _mm_movemask_epi8
- Robin Hood hashing (discussed in the Q&A)
- xkcd 688, 'Self-Description' (shown on a slide)
