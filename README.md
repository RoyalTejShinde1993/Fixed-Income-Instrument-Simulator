# Fixed-Income-Instrument-Simulator

A polished C++20 fixed income trading and analytics platform project module with core pricing, yield curve construction, risk analytics, FIX 4.4 session handling, and asynchronous networking.

## Overview

This repository demonstrates a resume-ready implementation of fixed income trading infrastructure, including:

- Fixed income analytics for coupon-bearing bonds and floating rate notes
- Yield curve interpolation and bootstrapping
- FIX 4.4 session message creation/parsing and sequence management
- Asynchronous TCP transport skeleton using Boost.Asio
- Portfolio risk aggregation, CS01, and scenario stress testing
- Google Test coverage and GitHub Actions CI automation

## Architecture

The project is organized into the following modules:

- `fixed_income::Bond`, `TreasuryBond`, `CorporateBond`, `FloatingRateNote`
- `fixed_income::YieldCurve` and `YieldCurveBootstrapper`
- `fixed_income::FixMessage` and `FixSession`
- `fixed_income::FixConnection`, `FixServer`, `FixClient`
- `fixed_income::Portfolio`, `RiskEngine`, `ScenarioResult`

## Features

- Pricing and cash flow scheduling for fixed rate and floating rate instruments
- Newton-Raphson YTM solver
- Macaulay and modified duration
- DV01, PV01, convexity
- Zero curve interpolation: linear and cubic spline
- FIX 4.4 session messages: Logon, Logout, Heartbeat, TestRequest, ResendRequest, SequenceReset, SecurityDefinition, MarketDataSnapshot
- Asynchronous FIX transport scaffold using Boost.Asio
- Portfolio-level risk calculations and scenario analysis

## Build Instructions

### Local build

```bash
mkdir -p build
cmake -S . -B build
cmake --build build -- -j$(nproc)
```

### Run tests

```bash
ctest --test-dir build --output-on-failure
```

### Build with sanitizers

```bash
cmake -S . -B build-sanitizers -DENABLE_SANITIZERS=ON
cmake --build build-sanitizers -- -j$(nproc)
```

### CMake presets

```bash
cmake --preset default
cmake --preset sanitizers
```

## Demo

Build and run the demo executable:

```bash
cmake --build build --target fixed_income_demo
./build/fixed_income_demo
```

### Sample Output

```text
Bond price: 1000
Yield to maturity: 0.05
Duration: 4.33
Demo initialized. Run a local FIX server on port 15001 to connect.
```

## Project Output

The demo target prints fixed income analytics results and initializes the asynchronous FIX client skeleton. The unit test suite validates pricing, yield curve interpolation, FIX message behavior, networking scaffolding, and risk analytics.

## Testing

This repository uses Google Test with a single executable test target. Tests cover:

- Bond cash flow generation and pricing
- Yield curve interpolation and bootstrapping
- FIX 4.4 message serialization/parsing and session sequence handling
- Boost.Asio based FIX connection skeleton
- Portfolio risk aggregation and scenario P&L

## Code Style

Formatting is enforced with `.clang-format` using Google style and a 100-column limit.

## CI

Continuous integration is configured in `.github/workflows/ci.yml` to:

- install dependencies
- configure and build the project
- run tests with CTest
- execute `clang-tidy`

## Project Structure

- `include/fixed_income/` — public headers
- `src/` — implementation files
- `tests/` — unit test suite
- `.github/workflows/ci.yml` — CI automation
- `CMakeLists.txt` — build configuration
- `CMakePresets.json` — developer presets
- `.clang-format` — formatting rules


