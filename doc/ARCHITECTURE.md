# HydraMetrics Architecture & Project Plan

## 1. Project Goal

HydraMetrics is a high-performance asynchronous telemetry ingestion gateway written in Modern C++20. It accepts binary metrics over raw TCP, aggregates counters and gauges in memory, and periodically flushes aggregated snapshots to Redis.

- **Input:** Custom binary metric packets over TCP.
- **Protocol:** Fixed binary header plus variable metric name and 8-byte IEEE-754 value.
- **Core:** Sharded in-memory aggregation with per-shard mutexes.
- **Storage:** Redis hashes updated through redis-plus-plus pipelining.
- **Runtime:** Boost.Asio coroutine TCP server, background flush worker, graceful shutdown with final flush.
- **Deployment:** Docker Compose stack with the C++ server and Redis 7.

## 2. Binary Wire Protocol

Each packet follows this layout:

```text
[Magic Byte: 0x48 (1 byte)]
[Metric Type: 0x01 Counter or 0x02 Gauge (1 byte)]
[Timestamp: unsigned integer, Big-Endian (8 bytes)]
[Name Length (1 byte)]
[Metric Name (variable raw bytes)]
[Value: IEEE-754 double payload, Big-Endian (8 bytes)]
```

- **Header Size:** 11 bytes.
- **Expected Full Size:** `11 + name_length + 8`.
- **Integer Endianness:** Big-Endian network byte order.
- **Double Encoding:** The 8-byte payload is read as a Big-Endian `uint64_t` and reconstructed with `std::bit_cast<double>`.
- **Invalid Packets:** Parser returns `std::errc::bad_message` via `std::error_code`; parsing methods do not throw outward.

## 3. Runtime Data Flow

```text
TCP client
  -> Boost.Asio coroutine session
  -> ProtocolParser
  -> MetricAggregator
  -> FlushWorker
  -> RedisStorage
  -> Redis hashes
```

Redis keys:

- `hydra:counters`: updated with `HINCRBYFLOAT`.
- `hydra:gauges`: updated with `HSET`.

Shutdown path:

- `SIGINT` or `SIGTERM` stops the TCP server.
- `FlushWorker` is stopped and joined.
- `main.cpp` performs a final `extract_snapshot()` + `flush_snapshot()` before process exit.

## 4. Components

### Core

- `src/core/types.hpp`
  Defines `MetricType`, `ProtocolConstants`, and `MetricPacket`.
- `src/core/protocol_parser.hpp/.cpp`
  Implements `ProtocolParser`, header validation, Big-Endian timestamp parsing, Big-Endian double reconstruction, and full packet deserialization.
- `src/core/aggregator_interface.hpp`
  Defines `IMetricAggregator` and `MetricSnapshot`.
- `src/core/metric_aggregator.hpp/.cpp`
  Implements `MetricAggregator` with 16 shards. Each shard owns `counters`, `gauges`, and a mutex. `update_metric()` locks only the target shard. `extract_snapshot()` locks all shard mutexes with `std::scoped_lock`, copies data, clears counters, and keeps gauges.

### Storage

- `src/storage/storage_interface.hpp`
  Defines `IStorageBackend`.
- `src/storage/redis_storage.hpp/.cpp`
  Implements `RedisStorage` with redis-plus-plus. `connect()` validates Redis with `PING`. `flush_snapshot()` writes counters and gauges through a Redis pipeline. Redis exceptions are logged to `std::cerr`.
- `src/storage/flush_worker.hpp/.cpp`
  Implements a C++20 `std::jthread` background worker. It sleeps with `std::condition_variable_any` and `std::stop_token`, extracts snapshots, and flushes them every 1000ms.

### Network

- `src/network/server_interface.hpp`
  Defines `INetworkServer`.
- `src/network/tcp_server.hpp/.cpp`
  Implements `TcpServer` using Boost.Asio coroutines. It accepts sockets asynchronously, reads packet headers and bodies with `boost::asio::async_read`, deserializes valid packets, and forwards them to the aggregator.

### Application

- `src/main.cpp`
  Wires all layers through dependency injection, reads runtime configuration from environment variables, starts the flush worker and TCP server, handles `SIGINT`/`SIGTERM`, logs lifecycle events, and performs final flush.

Runtime environment variables:

- `REDIS_HOST`, default `127.0.0.1`.
- `REDIS_PORT`, default `6337`.
- `SERVER_ADDRESS`, default `0.0.0.0`.
- `SERVER_PORT`, default `8080`.

## 5. Build And Deployment

### CMake Targets

- `hydra_core`: parser and aggregator.
- `hydra_storage`: Redis storage and flush worker.
- `hydra_network`: Boost.Asio TCP server.
- `hydrametrics_server`: final executable.
- `hydra_core_tests`: Google Test executable for core tests.

### Docker

- `Dockerfile`
  Multi-stage Ubuntu 24.04 build. The builder stage installs vcpkg dependencies and compiles `hydrametrics_server` in Release mode. The runner stage copies only the final binary and runs it as non-root `appuser`.
- `docker-compose.yml`
  Runs `hydrametrics-server` and `hydrametrics-cache`. Redis runs on port `6337` with disk persistence disabled. Redis has a healthcheck (`redis-cli -p 6337 ping`), and the server starts only after Redis is healthy.

## 6. Validation And Benchmarks

### Unit Tests

Parser coverage:

- Valid Counter header.
- Invalid magic byte.
- Invalid metric type.
- Valid full packet deserialization.
- Corrupted/truncated packet handling.

Aggregator coverage:

- Single-threaded counter accumulation.
- Gauge overwrite behavior.
- Counter reset after snapshot extraction.
- Gauge retention after snapshot extraction.
- Concurrent stress test with 8 threads and 10,000 updates per thread.

### E2E Smoke Test

`benchmarks/smoke_test.py` sends 5 valid packets for `smoke_counter`, waits for the flush interval, reads Redis, and expects the counter value to be `5`.

### Load Generator

`benchmarks/load_generator.py` opens async TCP connections and sends protocol-correct Counter packets at high speed.

Detailed benchmark notes are kept in `doc/BENCHMARK.md`.

Measured local Docker run:

- `500,000` packets.
- `64` async TCP connections.
- About `295,000` packets per second.
- Redis value verified as `500000`.

## 7. Current Status

Completed:

- Binary protocol parser.
- Sharded aggregator.
- Redis storage with pipelining.
- Background flush worker.
- Boost.Asio coroutine TCP server.
- Application entry point with DI and signal handling.
- Final flush on shutdown.
- Dockerfile and Docker Compose stack.
- Redis healthcheck and startup gating.
- Unit tests, stress test, smoke test, and load generator.
- README and architecture documentation.

Remaining hardening candidates:

- Add structured logging instead of raw `std::cout`/`std::cerr`.
- Add explicit server health endpoint or lightweight TCP health probe.
- Add Redis reconnect/backoff strategy after startup.
- Extend the benchmark report with reproducible machine/container details.
- Pin vcpkg to a known commit for fully reproducible Docker builds.
