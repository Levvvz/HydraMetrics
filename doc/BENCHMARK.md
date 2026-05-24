# HydraMetrics Benchmark Report

## Summary

HydraMetrics was validated with an end-to-end Docker-based benchmark using the asynchronous Python load generator. The test exercised the full production path:

```text
Python async TCP generator
  -> HydraMetrics TCP server
  -> ProtocolParser
  -> MetricAggregator
  -> FlushWorker
  -> RedisStorage
  -> Redis hash hydra:counters
```

## Environment

- **Runtime:** Docker Compose.
- **Server:** `hydrametrics_server` C++20 executable.
- **Storage:** Redis 7 Alpine, port `6337`, disk persistence disabled.
- **Protocol:** HydraMetrics binary TCP protocol.
- **Metric:** Counter metric named `load_counter`.
- **Flush Interval:** 1000ms.

## Benchmark Command

```bash
python3 benchmarks/load_generator.py --host 127.0.0.1 --port 8080 --packets 5000000 --connections 128
```

## Observed Results

- **Packets Sent:** 5,000,000.
- **Concurrent TCP Connections:** 128.
- **Observed Throughput Range:** approximately 234,000 to 295,000 packets per second.
- **Redis Verification:** the final Redis counter matched the number of sent packets.
- **Packet Loss:** no packet loss observed in the verified runs.

## Redis Verification

The aggregated counter was checked with:

```bash
docker exec -it hydrametrics_redis redis-cli -p 6337 HGETALL hydra:counters
```

Expected shape:

```text
1) "load_counter"
2) "5000000"
```

## Notes

- Results depend on CPU, Docker runtime, kernel networking, and the number of concurrent connections.
- The benchmark measures end-to-end ingestion and aggregation, not only raw socket write speed.
- Redis receives aggregated batches through pipelined write-behind flushes, so Redis values should be checked after allowing at least one flush interval.

## Follow-Up Benchmark Ideas

- Repeat with 1, 16, 64, 128, and 256 TCP connections.
- Compare Docker bridge networking with host networking.
- Measure p50/p95/p99 client-side latency.
- Add CPU and memory snapshots during sustained 5M+ packet runs.
