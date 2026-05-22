#!/usr/bin/env python3

import argparse
import asyncio
import struct
import time


MAGIC_BYTE = 0x48
COUNTER_TYPE = 0x01
DEFAULT_CONNECTIONS = 64
DEFAULT_METRIC_NAME = "load_counter"


def build_packet(metric_name: str) -> bytes:
    name_bytes = metric_name.encode("utf-8")
    if len(name_bytes) > 255:
        raise ValueError("metric name must be no longer than 255 bytes")

    timestamp = time.time_ns()
    header = struct.pack(
        "!BBQB",
        MAGIC_BYTE,
        COUNTER_TYPE,
        timestamp,
        len(name_bytes),
    )
    value = struct.pack("!d", 1.0)

    return header + name_bytes + value


async def open_connections(host: str, port: int, count: int):
    connections = []
    for _ in range(count):
        reader, writer = await asyncio.open_connection(host, port)
        connections.append((reader, writer))

    return connections


async def close_connections(connections) -> None:
    for _, writer in connections:
        writer.close()

    await asyncio.gather(
        *(writer.wait_closed() for _, writer in connections),
        return_exceptions=True,
    )


async def send_packets(host: str, port: int, packet_count: int, connection_count: int) -> float:
    connections = await open_connections(host, port, connection_count)
    writers = [writer for _, writer in connections]
    started_at = time.perf_counter()

    try:
        for index in range(packet_count):
            packet = build_packet(DEFAULT_METRIC_NAME)
            writer = writers[index % len(writers)]
            writer.write(packet)

            if index % 1024 == 0:
                await writer.drain()

        await asyncio.gather(*(writer.drain() for writer in writers))
    finally:
        await close_connections(connections)

    return time.perf_counter() - started_at


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="HydraMetrics async TCP load generator")
    parser.add_argument("positional_host", nargs="?", help="HydraMetrics server host")
    parser.add_argument("positional_port", nargs="?", type=int, help="HydraMetrics server port")
    parser.add_argument("positional_packets", nargs="?", type=int, help="Number of packets to generate")
    parser.add_argument("--host", default="127.0.0.1", help="HydraMetrics server host")
    parser.add_argument("--port", type=int, default=8080, help="HydraMetrics server port")
    parser.add_argument(
        "--packets",
        type=int,
        default=100000,
        help="Number of packets to generate",
    )
    parser.add_argument(
        "--connections",
        type=int,
        default=DEFAULT_CONNECTIONS,
        help="Number of async TCP connections",
    )

    args = parser.parse_args()
    if args.positional_host is not None:
        args.host = args.positional_host
    if args.positional_port is not None:
        args.port = args.positional_port
    if args.positional_packets is not None:
        args.packets = args.positional_packets

    return args


async def main() -> None:
    args = parse_args()
    if args.packets < 0:
        raise ValueError("--packets must be non-negative")
    if args.connections <= 0:
        raise ValueError("--connections must be positive")

    elapsed = await send_packets(args.host, args.port, args.packets, args.connections)
    rate = args.packets / elapsed if elapsed > 0 else 0.0

    print(f"sent_packets={args.packets}")
    print(f"connections={args.connections}")
    print(f"elapsed_seconds={elapsed:.6f}")
    print(f"throughput_pps={rate:.2f}")


if __name__ == "__main__":
    asyncio.run(main())
