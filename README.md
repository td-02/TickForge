# DPDKTrade

DPDKTrade is a foundation for a low-latency market data processing and order execution engine built around DPDK-oriented transport paths.

This repository is intentionally structured for performance-oriented C++20 development and a clean open-source presentation. It is designed to evolve toward a modern trading systems architecture without introducing unnecessary runtime overhead in the core path.

## Goals

- Keep the hot path deterministic and cache-friendly.
- Avoid dynamic allocation in latency-sensitive code.
- Use modern C++20 and modern CMake.
- Keep the repository suitable for a strong GitHub portfolio.
- Leave room for future DPDK integration without forcing it into the first foundation pass.

## Current Scope

The repository currently contains the first-pass architecture skeleton and the early protocol, book, strategy, risk, and engine headers.

- `include/dpdktrade/wire/frame.hpp` defines the 62-byte market and order wire frames.
- `include/dpdktrade/book/order_book.hpp` provides a fixed-depth Level-2 book.
- `include/dpdktrade/strategy/imbalance.hpp` contains the imbalance signal.
- `include/dpdktrade/risk/guard.hpp` contains the pre-trade risk guard.
- `include/dpdktrade/engine/dpdktrade_engine.hpp` ties the pipeline together.
- `tests/test_book.cpp` exercises the core foundation path.
- `apps/ring_scenarios.cpp` validates the ring-oriented scenarios.
- `apps/ring_benchmark.cpp` provides a DPDK ring benchmark scaffold.
- `scripts/profile.sh` captures perf-based profiling for the ring benchmark or the AF_PACKET-vs-DPDK benchmark.
- `results/profile_baseline.txt` stores the baseline `perf stat` output for the ring benchmark.
- `apps/afpacket_engine.cpp` and `apps/afpacket_generator.cpp` scaffold AF_PACKET transport work.
- `apps/stress.cpp` runs a long-form engine, ring, and AF_PACKET stress pass.

## Target Environment

- Windows 11
- WSL2 Ubuntu 24.04
- C++20
- CMake
- Ninja
- GCC or Clang

## Repository Layout

- `include/dpdktrade/wire/` for wire formats and protocol utilities.
- `include/dpdktrade/book/` for order book infrastructure.
- `include/dpdktrade/strategy/` for signal generation.
- `include/dpdktrade/risk/` for pre-trade checks.
- `include/dpdktrade/engine/` for orchestration and execution flow.
- `include/dpdktrade/dpdk/` for future DPDK-specific integration.
- `apps/` for runnable experiments and benchmarks.
- `tests/` for unit and integration tests.
- `scripts/` for developer tooling.
- `results/` for benchmark output.
- `docs/` for architecture notes and design decisions.

## Apps

- `ring_scenarios.cpp` checks the core engine behaviors across BUY, SELL, NO_SIGNAL, RISK_REJECT, and INVALID_FRAME paths.
- `ring_benchmark.cpp` measures a million ring events and exports benchmark results to `results/ring_benchmark.csv`.
- `afpacket_engine.cpp` and `afpacket_generator.cpp` establish the AF_PACKET transport scaffold for later DPDK integration.
- `stress.cpp` runs configurable engine, ring, and AF_PACKET stress checks; use `--iterations N` or `-n N` to tune runtime.

## Build

The current foundation provides an interface library for shared headers and a minimal test target.

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build
build/dpdktrade_stress
build/dpdktrade_stress --iterations 100000
```

For DPDK-oriented apps, build and run only on a system with the DPDK headers and runtime available.

## Profiling

Use `scripts/profile.sh` on Linux with `perf` installed to build a profiling-friendly binary and capture baseline counters for the ring benchmark.

```bash
bash scripts/profile.sh ring_benchmark
```

The script writes the captured `perf stat` output to `results/profile_baseline.txt`. If `perf` is unavailable, it records the expected command in that file so the workflow is still documented.

## Design Notes

- The project is organized around explicit subsystems rather than one monolithic engine.
- Performance-sensitive code is expected to favor fixed-size data structures and predictable control flow.
- The first pass keeps the repository compilable and ready for later implementation work.

## Status

This repository is still a foundation-first build. The code intentionally avoids trading logic depth so the architecture remains easy to extend and review in small steps.
