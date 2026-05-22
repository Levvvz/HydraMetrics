# HydraMetrics Architecture & Project Plan

## 1. Project Goal ("North Star")
HydraMetrics is an ultra-high-performance, asynchronous metric aggregation engine written in Modern C++20. It is designed to act as an ingestion gateway for massive streams of telemetry data (telemetry buffer).

* **Input Data:** Binary metric packets (Counters and Gauges).
* **Protocol:** Custom binary wire protocol over raw TCP.
* **Core Logic:** Real-time lock-free aggregation and bucketing in memory.
* **Storage:** Asynchronous write-behind flushing to Redis via Pipelining.
* **Output Access:** Downstream services read aggregated data directly from Redis.
* **MVP Criteria:** Accept 1M+ RPS of binary packets over TCP, aggregate counters/gauges in memory without blocking network threads, and flush chunks to Redis every 1000ms.

## 2. Binary Wire Protocol Specification
Every TCP packet follows this strict Big-Endian layout:
`[Magic Byte: 0x48 (1b)][Metric Type (1b)][Timestamp (8b)][Name Length (1b)][Metric Name (Variable)][Value (8b)]`

* **Metric Types:** `0x01` (Counter), `0x02` (Gauge).
* **Minimum Packet Size:** 20 bytes (with 1-character name).

## 3. High-Level Modular Roadmap & Status
- [x] **Phase 1: Infrastructure & Environment Setup** (CMake, vcpkg manifests, strict CI/CD, Multi-stage Docker).
- [x] **Phase 2: Architectural Contracts** (`IProtocolParser`, `IMetricAggregator`, `INetworkServer`).
- [ ] **Phase 4: Component Implementation** (Parser, Sharded Aggregator, Redis Async Client, Asio Coroutines).
- [ ] **Phase 5: Validation & Testing** (Google Test suites, Concurrency stress tests).
- [ ] **Phase 6: Verification & Benchmarking** (Resource usage profiling, RPS/Latency percentiles reports).

## 4. Current Phase: Phase 4 (Component Implementation)
*Target: Implement zero-copy parser and cover it with unit tests.*