# Memory manager design

Status: proposal. Nothing here is built. Written against the code as of branch
`fixedtimestep-time-scale`.

## Summary

Don't start with an allocator. Start with a counter. This engine is a small maze game, and most of
its containers already reuse their capacity. The design is a short ladder: each rung is added only
if the rung below it shows a problem.

1. **Allocation counter** (debug/test builds only), reported in `FrameStats`, the debug panel and a test.
2. **Frame arena**, first user `CommandBuffer`.
3. **Pool storage change** (stable addresses), only if the hook-reference bug or a benchmark needs it.
4. **Small items**: `reserve`, pool lookup, hook allocation.

## Goals and non-goals

Goals

- Zero heap allocations per frame in steady state, after warm-up. `DrawBatch` already promises this
  for itself ("doesn't allocate once warm"); make it true and tested for the whole frame.
- References into component pools are either stable or documented as invalidated by structural
  changes (IMPROVEMENTS item 2: `OnAdd` hooks get a `T&` into a vector that `emplace_back` can move).
- Diagnostics: allocation count and bytes visible per frame.

Non-goals

- Replacing global `operator new`/`malloc` in release builds.
- Thread-safe allocators. The engine is single-threaded; keep it that way until it isn't.
- A general-purpose heap, size-class allocator or garbage collector.

## What the code does today

| Site | Allocation pattern | Per frame? |
| --- | --- | --- |
| `DrawBatch` (`entries`, `vertices`, `indices`, `glyphs`) | vectors reused, capacity kept | No once warm |
| `EventChannel::pending` (`events.hpp`) | `clear()` keeps capacity | No once warm |
| `CommandBuffer` (`commandbuffer.hpp`) | `make_unique` per Add/Remove, plus the `commands` vector | **Yes**, one per recorded component command |
| `ComponentPool::Add` | `emplace_back` on three vectors; can reallocate | Only while growing |
| `Registry` pool lookup | `unordered_map<type_index, unique_ptr>`; hashing, no allocation after first use | No, but hashed on each call |
| Hooks (`std::function<void(Entity, T&)>`) | allocates if the capture exceeds the small buffer | At registration only |
| `core/text.cpp` (`SplitLines`, `FormatError`, `ReadTextFile`) | strings and vectors | No: load/parse time only |

So the one confirmed per-frame allocator is `CommandBuffer`. Everything else is either already
amortised or off the frame path. The counter in step 1 is how we find out whether that survey is right.

## Step 1: allocation counter

- Replace global `operator new`/`delete` only under a `ENGINE_TRACK_ALLOCATIONS` define, set for
  Debug and the test targets, never Release. The replacement increments two counters (count, bytes)
  and forwards to `malloc`/`free`. Include the aligned and `nothrow` overloads, or the counter
  silently misses over-aligned types.
- Expose `core::AllocationStats { std::uint64_t count; std::uint64_t bytes; }` with
  `Snapshot()`. In builds without tracking, `Snapshot()` returns zeros so callers need no `#if`.
- `FrameStats` gains `allocationsPerFrame`, computed from the difference between snapshots at frame
  start and end. The debug panel shows it beside fps and draw ms.
- Test: run the real update/draw loop for N warm-up frames, then M frames, and assert the per-frame
  count is zero. This test starts out failing (CommandBuffer), which is the point. Mark known
  offenders with an explicit allow-list entry rather than loosening the assertion.
- Cost: one translation unit and a few atomics-free increments. The `/Wall /WX /analyze` and
  `-Wconversion` CI flags apply; keep the replacement functions minimal.

Every later step must cite a number from this counter or from a benchmark.

## Step 2: frame arena

A linear (bump) allocator, reset once per frame at a fixed point in the loop, after command buffers
are applied.

Interface sketch:

```cpp
class FrameArena {
    std::byte* base; std::size_t capacity; std::size_t used;
public:
    void* Allocate(std::size_t size, std::size_t align) noexcept; // nullptr when full
    void Reset() noexcept;  // runs registered destructors, then rewinds
    // stats: Used(), HighWater(), Overflows()
};
```

Decisions

- **Fixed capacity, with fallback.** Allocate one block up front (start at 1 MiB, tune from the
  high-water mark). On exhaustion fall back to the heap and count an overflow, shown in the debug
  panel. It never fails a frame, and the step 1 test catches overflows.
- **Destructors.** Commands hold components that may not be trivially destructible. The arena keeps
  an intrusive list of `{dtor, ptr}` nodes (allocated from the arena itself) and runs them in
  reverse on `Reset`. Trivially destructible types skip the node.
- **Alignment arithmetic.** Compute the aligned offset in `std::uintptr_t`/`std::size_t` with
  explicit `static_cast`s; `-Wconversion` and `/WX` reject the sloppy version. Assert alignment is a
  power of two.
- **Lifetime rule.** Anything from the arena is dead after `Reset`. Never store arena pointers in
  components or resources. In debug builds, poison freed memory (0xDD) on `Reset` so a stale read
  fails loudly, and ASan manual poisoning is a follow-up if the sanitizer job (IMPROVEMENTS item 1)
  exists.

First user: `CommandBuffer`

- Replace `std::unique_ptr<IComponentCommand>` with arena-allocated `AddCommand<T>` objects, and the
  `commands` vector with arena-backed storage (a pointer and count that grows by doubling inside the
  arena).
- Keep the public API (`Add`, `Remove`, `Destroy`, `Apply`, `Clear`) unchanged. `Apply` still clears
  the buffer; the arena memory is reclaimed at `Reset`, not at `Apply`.
- A `CommandBuffer` is constructed with a `FrameArena&`. Tests that build one standalone pass a local
  arena; the default constructor can own a small private arena so existing call sites don't change.

`std::pmr::monotonic_buffer_resource` vs a hand-written arena

| | pmr | Hand-written |
| --- | --- | --- |
| Code to write | Almost none | ~100 lines plus tests |
| Destructor handling | None: you must run them yourself | Built in |
| Container types | `std::pmr::vector<T>` is a different type; moving between resources copies | Our own types |
| Overflow fallback and stats | Needs a wrapper resource | Native |
| Portability across MSVC/GCC/Clang | Fine | Fine |

Recommendation: hand-write the arena (the destructor list and stats are the point), and offer a
`std::pmr::memory_resource` adapter over it so `std::pmr::vector` can borrow from it where that is
convenient. Don't adopt pmr as the engine-wide container type.

## Step 3: component pool storage

Problem: `Add` can reallocate `components`, invalidating the `T&` that `Registry::Add` returns and
the one passed to `OnAdd` hooks.

Constraint found in the code: `Registry::Each` takes `first.Data()` as a `std::span<First>` and
indexes it, so it relies on contiguous storage. Only that one site uses `Data()`.

| Option | Stable addresses | Contiguous `Data()` | Cost |
| --- | --- | --- | --- |
| A. Keep vector, document invalidation, add hook tests | No | Yes | Smallest. Matches IMPROVEMENTS item 2 |
| B. Paged storage (fixed-size pages of T) | Yes, until removal | No: iterate page by page | Rewrite `Each` and `Data()`; slower cache behaviour across pages |
| C. Vector with `reserve` per type | Yes up to the cap | Yes | Needs a cap per component; a hard failure past it |

Recommendation: **A now.** Swap-and-pop removal already moves components, so even paged storage
doesn't give stable addresses across a `Remove`; stability would only be partial. Document that a
component reference is valid until the next structural change, add the two hook tests from
IMPROVEMENTS item 2, and let the arena handle deferred changes. Revisit B only if a benchmark shows
a real bug class the documentation can't cover. If pools later need a custom allocator, give
`ComponentPool` an allocator template parameter defaulting to `std::allocator`, so nothing changes
until a pool opts in.

## Step 4: small items, with what to measure

- **`reserve` on hot vectors.** Pools, `commands`, event `pending`. Measure first-frame allocation
  count from the step 1 counter; add `Registry::Reserve<T>(n)` only if warm-up spikes matter.
- **Pool lookup hashing.** `unordered_map<type_index, ...>` per `Each`/`Add`/`Get`/`Has`. This is
  CPU, not memory; IMPROVEMENTS item 3 covers it. Benchmark before changing. It belongs to this
  design only because a per-type id would let pools live in a flat vector.
- **`std::function` hooks.** A capturing lambda over the small-buffer size heap-allocates at
  registration. Not per frame. Measure by counting allocations during `OnAdd`; if it matters, use a
  small-buffer callable with a static size assertion.
- **Strings.** `core/text.cpp` allocates, but only while parsing level and config files. Leave it.

## Rollout

| Step | Ships | Tests it must include |
| --- | --- | --- |
| 1 | Counter, `FrameStats` field, debug panel line | Counter counts known allocations; zero-per-frame test (initially allow-listing `CommandBuffer`); Release build has no tracking |
| 2a | `FrameArena` | Alignment, exhaustion and fallback, `Reset` runs destructors in reverse order, high-water mark, poison on reset |
| 2b | `CommandBuffer` on the arena | Existing command-buffer tests unchanged; allow-list entry removed; non-trivial component (e.g. one holding a `std::string`) is destroyed exactly once |
| 3 | Pool docs and hook tests (option A) | Hook calls `Create()`; hook reads another pool while this one is being added to |
| 4 | Only what the counter or benchmarks justify | A benchmark before and after each |

Steps 2b and 3 can be done in either order. Step 1 gates everything.

## Open questions

- Is the zero-allocation goal for the whole frame, or just update? Rendering through SDL allocates
  inside SDL; the counter will see those. Decide whether the test measures engine code only (hook
  the counter around update and `DrawBatch::Build`, not `SDL_RenderGeometry`).
- Arena size: start at 1 MiB and let the high-water mark decide, or compute from the maze's entity count?
