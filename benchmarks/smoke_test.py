#!/usr/bin/env python3

import asyncio
import subprocess
import sys

from load_generator import build_packet


SERVER_HOST = "127.0.0.1"
SERVER_PORT = 8080
REDIS_HOST = "127.0.0.1"
REDIS_PORT = 6337
REDIS_CONTAINER = "hydrametrics_redis"
METRIC_NAME = "smoke_counter"
PACKET_COUNT = 5
FLUSH_WAIT_SECONDS = 1.5


async def send_smoke_packets() -> None:
    _, writer = await asyncio.open_connection(SERVER_HOST, SERVER_PORT)

    try:
        for _ in range(PACKET_COUNT):
            writer.write(build_packet(METRIC_NAME))

        await writer.drain()
    finally:
        writer.close()
        await writer.wait_closed()


def read_counter_with_redis_package() -> str | None:
    try:
        import redis
    except ImportError:
        return None

    try:
        client = redis.Redis(host=REDIS_HOST, port=REDIS_PORT, decode_responses=True)
        return client.hget("hydra:counters", METRIC_NAME)
    except redis.RedisError:
        return None


def run_redis_cli(*args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        ["docker", "exec", REDIS_CONTAINER, "redis-cli", "-p", str(REDIS_PORT), *args],
        check=False,
        capture_output=True,
        text=True,
    )


def read_counter_with_docker_exec() -> str | None:
    result = run_redis_cli("HGET", "hydra:counters", METRIC_NAME)
    if result.returncode != 0:
        return None

    value = result.stdout.strip()
    return value if value else None


def reset_counter_with_redis_package() -> bool:
    try:
        import redis
    except ImportError:
        return False

    try:
        client = redis.Redis(host=REDIS_HOST, port=REDIS_PORT, decode_responses=True)
        client.hdel("hydra:counters", METRIC_NAME)
        return True
    except redis.RedisError:
        return False


def reset_counter_with_docker_exec() -> bool:
    result = run_redis_cli("HDEL", "hydra:counters", METRIC_NAME)
    return result.returncode == 0


def reset_counter() -> bool:
    return reset_counter_with_redis_package() or reset_counter_with_docker_exec()


def read_counter() -> str | None:
    value = read_counter_with_redis_package()
    if value is not None:
        return value

    return read_counter_with_docker_exec()


def is_expected_value(value: str | None) -> bool:
    if value is None:
        return False

    try:
        return float(value) == float(PACKET_COUNT)
    except ValueError:
        return False


async def main() -> int:
    if not reset_counter():
        print("SMOKE TEST FAILED: could not reset smoke counter")
        return 1

    await send_smoke_packets()
    await asyncio.sleep(FLUSH_WAIT_SECONDS)

    value = read_counter()
    if is_expected_value(value):
        print("SMOKE TEST PASSED")
        return 0

    print(f"SMOKE TEST FAILED: expected {PACKET_COUNT}, got {value}")
    return 1


if __name__ == "__main__":
    sys.exit(asyncio.run(main()))
