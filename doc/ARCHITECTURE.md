# HydraMetrics Architecture & Project Plan

## 1. Project Goal ("North Star")

HydraMetrics is an ultra-high-performance, asynchronous metric aggregation engine written in Modern C++20. It is designed to act as an ingestion gateway for massive streams of telemetry data (telemetry buffer).

- **Input Data:** Binary metric packets (Counters and Gauges).
- **Protocol:** Custom binary wire protocol over raw TCP.
- **Core Logic:** Real-time lock-free aggregation and bucketing in memory.
- **Storage:** Asynchronous write-behind flushing to Redis via pipelining.
- **Output Access:** Downstream services read aggregated data directly from Redis.
- **MVP Criteria:** Accept 1M+ RPS of binary packets over TCP, aggregate counters/gauges in memory without blocking network threads, and flush chunks to Redis every 1000ms.

## 2. Binary Wire Protocol Specification

Each metric packet follows this layout:

```text
[Magic Byte: 0x48 (1b)]
[Metric Type (1b)]
[Timestamp (8b, unsigned integer, big-endian)]
[Name Length (1b)]
[Metric Name (variable, raw bytes)]
[Value (8b, IEEE-754 double payload, big-endian)]
```

- **Metric Types:** `0x01` (Counter), `0x02` (Gauge).
- **Header Size:** 11 bytes.
- **Minimum Packet Size:** 20 bytes (with a 1-byte metric name).
- **Expected Full Size:** `11 + name_length + 8`.
- **Endianness:** Multi-byte fields are encoded in big-endian network byte order.
- **Value Encoding:** `Value` is encoded as the 8-byte IEEE-754 binary64 representation of `double`, transported as a big-endian `uint64_t` payload and converted back with `std::bit_cast<double>`.

Invalid packets are reported without C++ exceptions. Parser errors use `std::error_code` with `std::errc::bad_message`.

## 3. High-Level Modular Roadmap & Status

- [x] **Phase 1: Infrastructure & Environment Setup** (CMake, vcpkg manifests, strict CI/CD, Multi-stage Docker).
- [x] **Phase 2: Architectural Contracts** (`IProtocolParser`, `IMetricAggregator`, `INetworkServer`).
- [x] **Phase 3: Core Parser Implementation** (`ProtocolParser` implemented, Google Test suite added).
- [ ] **Phase 4: Component Implementation** (`MetricAggregator`, `RedisStorage`, `FlushWorker`, and coroutine TCP server implemented; integration hardening pending).
- [ ] **Phase 5: Validation & Testing** (Google Test suites, Concurrency stress tests).
- [ ] **Phase 6: Verification & Benchmarking** (Resource usage profiling, RPS/Latency percentiles reports).

## 4. Current Implementation Snapshot

### Core

- `src/core/types.hpp` owns the shared protocol constants, `MetricType`, and `MetricPacket`.
- `src/core/protocol_parser.hpp` defines `IProtocolParser` and the concrete `ProtocolParser`.
- `src/core/protocol_parser.cpp` implements header validation and packet deserialization.
- `src/core/metric_aggregator.hpp` defines the concrete `MetricAggregator`.
- `src/core/metric_aggregator.cpp` implements 16-shard counter/gauge aggregation with per-shard mutexes.
- `src/storage/storage_interface.hpp` defines `IStorageBackend`.
- `src/storage/redis_storage.hpp` defines the concrete Redis-backed storage implementation.
- `src/storage/redis_storage.cpp` flushes snapshots to Redis hashes with redis-plus-plus pipelining.
- `src/storage/flush_worker.hpp` defines a C++20 `std::jthread`-based background flush worker.
- `src/storage/flush_worker.cpp` periodically extracts aggregator snapshots and sends them to storage.
- `src/network/tcp_server.hpp` defines the concrete Boost.Asio coroutine TCP server.
- `src/network/tcp_server.cpp` accepts TCP sessions, reads binary packets asynchronously, parses them, and forwards valid metrics to the aggregator.
- `src/main.cpp` wires parser, aggregator, Redis storage, flush worker, TCP server, signal handling, lifecycle logs, and final shutdown flush.
- `Dockerfile` builds the Release server binary in a multi-stage Ubuntu 24.04 image with vcpkg dependencies.
- `docker-compose.yml` runs the server with Redis 7 on an internal Docker network.
- `benchmarks/load_generator.py` sends protocol-correct binary Counter packets over async TCP connections for throughput smoke testing.
- `parse_header(std::span<const uint8_t>) noexcept` validates the magic byte and metric type, reads the timestamp field, and returns the expected full packet size.
- `deserialize(std::span<const uint8_t>, MetricPacket&) noexcept` validates the full packet and fills `MetricPacket`.
- `update_metric(const MetricPacket&) noexcept` updates only the metric's target shard.
- `extract_snapshot()` locks all shard mutexes with `std::scoped_lock`, copies all shard data into `MetricSnapshot`, clears counters, and keeps gauges as the latest known values.
- `RedisStorage::flush_snapshot()` writes counters to `hydra:counters` with `HINCRBYFLOAT` and gauges to `hydra:gauges` with `HSET`.
- `FlushWorker::start()` runs a cancellable background loop; `FlushWorker::stop()` requests stop, wakes the sleeping worker, and joins it.
- `TcpServer::start()` creates an `io_context`, runs it on a Boost.Asio thread pool, and starts a coroutine listener.
- `TcpServer::stop()` closes the acceptor, stops the `io_context`, and joins the thread pool.
- On `SIGINT`/`SIGTERM`, `main.cpp` stops the worker and performs a final `extract_snapshot()` + `flush_snapshot()` before exit.

### Build

- `hydra_core` is a compiled CMake library because core components have `.cpp` implementations.
- `hydra_storage` is a compiled CMake library linked against `hydra_core` and redis-plus-plus.
- `hydra_network` is a compiled CMake library linked against `hydra_core` and Boost.System.
- `hydrametrics_server` is the application executable.
- Tests and benchmarks are still lightweight/stub targets.

## 5. Current Phase

Target: integration hardening, end-to-end smoke tests, and benchmark harness.

Operational smoke test path:

- Start Redis and the server with `docker compose up --build`.
- Run `benchmarks/load_generator.py` against `127.0.0.1:8080`.
- Inspect Redis hash `hydra:counters` for aggregated counter values.

Parser test coverage:

- Valid Counter packet.
- Invalid magic byte.
- Invalid metric type.
- Truncated full packet after a valid header.
- Valid full deserialization with big-endian timestamp and IEEE-754 double payload.

Aggregator test coverage:

- Single-threaded counter accumulation and gauge overwrite.
- Counter reset after `extract_snapshot()`.
- Gauge retention after `extract_snapshot()`.
- Concurrent stress test with 8 threads and 10,000 counter updates per thread.

Remaining parser test candidates:

- Valid Gauge packet.
- Slice smaller than 11-byte header.
- Name length boundary cases.
