# HydraMetrics

HydraMetrics is a high-performance asynchronous telemetry ingestion gateway and metric aggregation engine written in Modern C++20. It accepts streams of binary metrics over raw TCP, aggregates them in memory using a sharded architecture, and flushes data to Redis with write-behind pipelining.

## Key Architectural Highlights

- **C++20 Coroutines:** Uses `boost::asio::awaitable` and `co_await` for non-blocking asynchronous TCP I/O.
- **Sharded Registry:** Accumulates metrics inside a 16-shard partition array, reducing mutex contention on the hot path.
- **Low-Allocation Byte Parsing:** Validates incoming TCP streams directly from Asio buffers via `std::span` and copies metric names only into the final packet structure.
- **Cross-Platform Endianness Safety:** Network data uses Big-Endian encoding. Floating-point values are transported as IEEE-754 double payloads and reconstructed with `std::bit_cast<double>`.
- **Write-Behind Redis Pipelining:** A background `FlushWorker` uses `std::jthread` and `std::condition_variable_any` to batch-upload metrics to Redis every 1000ms.
- **Graceful Shutdown With Final Flush:** Handles `SIGINT` and `SIGTERM`, stops the TCP server and background worker, then performs a final memory-to-Redis flush before exit.
- **Dockerized Runtime:** Docker Compose starts the C++ server and Redis cache, with Redis health checks gating server startup.

## Performance Snapshot

Measured locally with the async Python load generator:

- **Throughput:** about 295,000 packets per second in a Docker-based smoke benchmark.
- **Sample Run:** 500,000 binary telemetry packets over 64 concurrent TCP connections.
- **Data Consistency:** verified end-to-end by checking the final Redis counter value.

Your numbers will vary depending on CPU, Docker runtime, network stack, and connection count.

## Local Quick Start

### Prerequisites

Install Docker and Docker Compose:

```bash
sudo apt update
sudo apt install -y docker.io docker-compose-v2
```

### 1. Run The Stack

Start the HydraMetrics C++ server and Redis:

```bash
docker compose up --build -d
```

The server waits until Redis passes its `service_healthy` health check before starting.

### 2. Run The End-To-End Smoke Test

The smoke test sends 5 binary packets for `smoke_counter`, waits for the background flush, and verifies Redis contains the value `5`.

```bash
pip3 install redis --break-system-packages
python3 benchmarks/smoke_test.py
```

Expected output:

```text
SMOKE TEST PASSED
```

### 3. Run A High-Load Benchmark

Send 5 million telemetry packets across 128 async TCP connections:

```bash
python3 benchmarks/load_generator.py --host 127.0.0.1 --port 8080 --packets 5000000 --connections 128
```

Short positional form is also supported:

```bash
python3 benchmarks/load_generator.py 127.0.0.1 8080 5000000 --connections 128
```

### 4. Inspect Aggregated Data In Redis

```bash
docker exec -it hydrametrics_redis redis-cli -p 6337 HGETALL hydra:counters
```

### 5. Stop The Stack

```bash
docker compose down
```

## Binary Protocol

Each metric packet uses this layout:

```text
[Magic Byte: 0x48 (1 byte)]
[Metric Type: 0x01 Counter or 0x02 Gauge (1 byte)]
[Timestamp: unsigned integer, Big-Endian (8 bytes)]
[Name Length (1 byte)]
[Metric Name (variable bytes)]
[Value: IEEE-754 double payload, Big-Endian (8 bytes)]
```

Expected packet size:

```text
11 + name_length + 8
```

## Development Commands

Configure and build locally with vcpkg:

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake \
  -G Ninja
cmake --build build --config Release
```

Run tests:

```bash
cd build
ctest --output-on-failure
```
