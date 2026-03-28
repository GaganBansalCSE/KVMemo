# KVMemo

A high-performance, cross-platform in-memory key-value store written in C++17.

<img src="./docs/assets/kvmemo-logo.png" alt="KVMemo">

**Live:** [kvmemo.dev](https://www.kvmemo.dev)

---

## Overview

KVMemo is a lightweight, TCP-based key-value store designed for low-latency data access. It features a multi-threaded server, a sharded storage engine with LRU eviction, TTL-based key expiration, and an interactive command-line client.

The project is under active development and built incrementally with daily commits.

---

## Features

- Multi-threaded TCP server with a configurable thread pool
- Sharded storage engine for reduced lock contention
- LRU eviction policy with configurable memory limits
- TTL-based key expiration with millisecond precision
- Built-in metrics collection (latency, memory usage)
- Lightweight binary protocol with request framing
- Interactive CLI client (`kv_cli`)
- Cross-platform build via CMake (C++17)

---

## Requirements

- CMake 3.16 or higher
- A C++17-compliant compiler (GCC, Clang, or MSVC)

---

## Building

```bash
mkdir build && cd build
cmake ..
make
```

This produces two executables:

| Executable | Description |
|---|---|
| `kvmemo` | The server |
| `kv_cli` | The interactive command-line client |

---

## Running

### Start the server

```bash
./kvmemo [port]
```

The default port is `6379`.

```bash
# Start on default port
./kvmemo

# Start on a custom port
./kvmemo 8082
```

### Connect with the CLI

```bash
./kv_cli [host] [port]
```

The default host is `127.0.0.1` and default port is `8082`.

```bash
# Connect using defaults
./kv_cli

# Connect to a specific host and port
./kv_cli 127.0.0.1 6379
```

---

## Commands

| Command | Syntax | Description |
|---|---|---|
| `SET` | `SET <key> <value>` | Store a key-value pair |
| `GET` | `GET <key>` | Retrieve the value for a key |
| `DEL` | `DEL <key>` | Delete a key |
| `SETEX` | `SETEX <key> <ttl_ms> <value>` | Store a key with a TTL (milliseconds) |
| `KEYS` | `KEYS` | List all keys and values |
| `PING` | `PING` | Health check; returns `PONG` |

For detailed command documentation and error reference, see [Command.md](./Command.md).

---

## Example Session

```
$ ./kv_cli 127.0.0.1 6379
Connected to KVMemo server at 127.0.0.1:6379
Type 'exit' to quit.

kvmemo> SET username alice
OK
kvmemo> GET username
alice
kvmemo> SETEX session_token 5000 abc123
OK
kvmemo> KEYS
username:alice
session_token:abc123
kvmemo> DEL username
OK
kvmemo> PING
PONG
kvmemo> exit
```

---

## Project Structure

```
src/
  core/        Storage engine, sharding, LRU cache, TTL index
  server/      Request dispatcher, command handler, thread pool
  net/         TCP server and connection management
  protocol/    Request/response framing and parsing
  eviction/    LRU eviction and memory tracking
  metrics/     Latency and metrics collection
  client/      CLI client
```

---

## License

This project is proprietary and not licensed for reuse.

