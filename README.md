# TickForge

TickForge is a foundation for a low-latency market data processing and order execution engine.

This repository is intentionally structured for performance-oriented C++20 development and a clean open-source presentation. It is designed to evolve toward a modern trading systems architecture without introducing unnecessary runtime overhead in the core path.

## Goals

- Keep the hot path deterministic and cache-friendly.
- Avoid dynamic allocation in latency-sensitive code.
- Use modern C++20 and modern CMake.
- Keep the repository suitable for a strong GitHub portfolio.
- Leave room for future DPDK integration without forcing it into the first foundation pass.

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

## Build

The current foundation provides an interface library for shared headers and a minimal test target.

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build
```

## Design Notes

- The project is organized around explicit subsystems rather than one monolithic engine.
- Performance-sensitive code is expected to favor fixed-size data structures and predictable control flow.
- The first pass keeps the repository compilable and ready for later implementation work.

## Status

This repository currently contains the project foundation only. Trading logic, strategy logic, and transport-specific implementations are intentionally deferred until the next confirmed step.
