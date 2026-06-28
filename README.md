# TickForge

TickForge is a foundation for a low-latency market data processing and order execution engine.

This repository is intentionally structured for performance-oriented C++20 development and a clean open-source presentation. It is designed to evolve toward a modern trading systems architecture without introducing unnecessary runtime overhead in the core path.

## Goals

- Keep the hot path deterministic and cache-friendly.
- Avoid dynamic allocation in latency-sensitive code.
- Use modern C++20 and modern CMake.
- Keep the repository suitable for a strong GitHub portfolio.
- Leave room for future DPDK integration without forcing it into the first foundation pass.

## Current Scope

The repository currently contains the first-pass architecture skeleton and the early protocol, book, strategy, risk, and engine headers.

- `include/tickforge/wire/frame.hpp` defines the 62-byte market and order wire frames.
- `include/tickforge/book/order_book.hpp` provides a fixed-depth Level-2 book.
- `include/tickforge/strategy/imbalance.hpp` contains the imbalance signal.
- `include/tickforge/risk/guard.hpp` contains the pre-trade risk guard.
- `include/tickforge/engine/trading_engine.hpp` ties the pipeline together.
- `tests/test_book.cpp` exercises the core foundation path.
- `apps/ring_scenarios.cpp` validates the ring-oriented scenarios.
- `apps/ring_benchmark.cpp` provides a DPDK ring benchmark scaffold.
- `apps/afpacket_engine.cpp` and `apps/afpacket_generator.cpp` scaffold AF_PACKET transport work.

## Target Environment

- Windows 11
- WSL2 Ubuntu 24.04
- C++20
- CMake
- Ninja
- GCC or Clang

## Repository Layout

- `include/tickforge/wire/` for wire formats and protocol utilities.
- `include/tickforge/book/` for order book infrastructure.
- `include/tickforge/strategy/` for signal generation.
- `include/tickforge/risk/` for pre-trade checks.
- `include/tickforge/engine/` for orchestration.
- `include/tickforge/dpdk/` for future DPDK-specific integration.
- `apps/` for runnable experiments and benchmarks.
- `tests/` for unit and integration tests.
- `scripts/` for developer tooling.
- `results/` for benchmark output.
- `docs/` for architecture notes and design decisions.

## Apps

- `ring_scenarios.cpp` checks the core engine behaviors across BUY, SELL, NO_SIGNAL, RISK_REJECT, and INVALID_FRAME paths.
- `ring_benchmark.cpp` measures a million ring events and exports benchmark results to `results/ring_benchmark.csv`.
- `afpacket_engine.cpp` and `afpacket_generator.cpp` establish the AF_PACKET transport scaffold for later DPDK integration.

## Build

The current foundation provides an interface library for shared headers and a minimal test target.

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build
```

For DPDK-oriented apps, build and run only on a system with the DPDK headers and runtime available.

## Design Notes

- The project is organized around explicit subsystems rather than one monolithic engine.
- Performance-sensitive code is expected to favor fixed-size data structures and predictable control flow.
- The first pass keeps the repository compilable and ready for later implementation work.

## Status

This repository is still a foundation-first build. The code intentionally avoids trading logic depth so the architecture remains easy to extend and review in small steps.
