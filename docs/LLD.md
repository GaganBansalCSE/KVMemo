# KVMemo — Low-Level Design Document

**Version:** 1.0  
**Author:** Gagan Bansal  
**Date:** 2026-03-22  
**Copyright © 2026 KVMemo — All Rights Reserved**

---

## Table of Contents

1. [Overview](#1-overview)
2. [Architecture](#2-architecture)
3. [Module Breakdown](#3-module-breakdown)
4. [Data Structures](#4-data-structures)
5. [Threading & Concurrency](#5-threading--concurrency)
6. [API Contracts](#6-api-contracts)
7. [Dependency Injection](#7-dependency-injection)
8. [Interaction Flows](#8-interaction-flows)

---

## 1. Overview

**KVMemo** is a production-grade, in-memory key-value store inspired by Redis. It is built with modern C++17 and follows SOLID design principles throughout.

### Key Features

| Feature | Description |
|---|---|
| **KV Operations** | `SET`, `GET`, `DEL` with optional TTL |
| **TTL Support** | Per-key time-to-live with lazy + proactive expiration |
| **Eviction Policies** | Pluggable eviction; LRU is the default policy |
| **Metrics & Monitoring** | Latency tracking, counters, and point-in-time snapshots |
| **Network Layer** | `select()`-based single-threaded TCP server; RESP-like framing |
| **Client** | Simple single-threaded TCP CLI client (`kv_cli`) |
| **Thread Safety** | Shard-level mutex isolation; no global storage lock |

### Design Philosophy

- **Layered separation:** Network → Protocol → Server → Engine → Storage
- **Constructor injection:** All dependencies passed at construction; no singletons
- **Stateless utilities:** Parser, Serializer, Framing, and Clock are all static/delete-constructible
- **Minimal allocations:** Buffer uses a contiguous `vector<char>` with a read cursor

---

## 2. Architecture

### 2.1 High-Level Component Hierarchy

```
main.cpp
  └── ServerApp                        (server layer orchestrator)
        ├── TcpServer                  (accept TCP connections)
        │    └── ConnectionManager     (map fd → Connection)
        │         └── Connection       (per-client socket + buffers)
        ├── KVEngine                   (core KV API surface)
        │    ├── ShardManager          (hash-partition keys across shards)
        │    │    └── Shard[N]         (mutex-protected KV map)
        │    │         ├── Entry       (value + TTL metadata)
        │    │         ├── LRUCache    (per-shard recency tracking)
        │    │         └── TTLIndex    (per-shard expiry map)
        │    ├── TTLIndex              (engine-level global expiry index)
        │    └── EvictionManager       (memory limit + LRU victim selection)
        │         ├── MemoryTracker    (atomic memory counter)
        │         └── LRUPolicy        (wraps LRUCache for policy interface)
        └── Dispatcher                 (route Request → KVEngine method)
              └── Protocol layer
                    ├── Framing        (extract \r\n-delimited frames)
                    ├── Parser         (tokenize frame → Request)
                    └── Serializer     (Response → RESP wire bytes)
```

### 2.2 Request Lifecycle

```
Client TCP Socket
      │
      ▼
Connection::ReadFromSocket()           ← raw bytes into input Buffer
      │
      ▼
Framing::NextFrame()                   ← extract \r\n delimited frame
      │
      ▼
Parser::Parse()                        ← tokenize frame → Request
      │
      ▼
Dispatcher::Dispatch()                 ← map command → KVEngine call
      │
  ┌───┴────────────────────────┐
  │  SET   │  GET   │   DEL    │
  ▼         ▼         ▼
KVEngine operations
      │
      ▼
Serializer::Serialize()               ← Response → RESP wire string
      │
      ▼
Connection::WriteToSocket()           ← flush output Buffer to socket
```

---

## 3. Module Breakdown

### 3.1 Server Module (`src/server/`)

#### **ServerApp** — `server_app.h`

| Attribute | Detail |
|---|---|
| **Purpose** | Top-level application orchestrator |
| **Namespace** | `kvmemo::server` |
| **Lifecycle** | Constructed in `main()`; owns all subsystems |

**Responsibilities:**
- Initializes `TcpServer`, `KVEngine`, and `Dispatcher` via constructor
- Runs the main event loop using `select()` for I/O multiplexing
- Accepts new connections on the listening FD
- Reads and processes requests for all active connections

**Key Methods:**

```cpp
void Run()                                          // Starts the server loop (blocking)
void ProcessConnections()                           // select() loop body
void ConnectionSafeProcess(ConnectionManager&, fd)  // Handle one client FD safely
```

**Implementation Notes:**
- `select()` timeout is `50 ms` (`kSelectTimeoutUs = 50000`)
- `active_fds_` is rebuilt each iteration to avoid iterator invalidation
- Exceptions during request handling close the connection silently

**Dependencies:** `TcpServer`, `KVEngine`, `Dispatcher`, `Framing`, `Parser`, `Serializer`  
**Thread Safety:** Single-threaded; the event loop runs entirely on the main thread

---

#### **Dispatcher** — `dispatcher.h`

| Attribute | Detail |
|---|---|
| **Purpose** | Routes parsed requests to KVEngine operations |
| **Pattern** | Stateless command dispatcher |

**Responsibilities:**
- Validates request structure (argument count)
- Maps `SET`, `GET`, `DEL` commands to `KVEngine` methods
- Constructs `Response` objects from engine results

**Key Method:**

```cpp
protocol::Response Dispatch(const protocol::Request& request)
```

**Command Routing Table:**

| Command | Minimum Args | Engine Call |
|---|---|---|
| `SET` | 2 (key, value) | `engine_.Set(key, value)` |
| `GET` | 1 (key) | `engine_.Get(key)` |
| `DEL` | 1 (key) | `engine_.Delete(key)` |

**Dependencies:** `KVEngine`, `Request`, `Response`  
**Thread Safety:** Stateless — thread-safe if `KVEngine` is thread-safe

---

#### **CommandRegistry** — `command_registry.h`

| Attribute | Detail |
|---|---|
| **Purpose** | Registry pattern for extensible command handlers |
| **Pattern** | Registry / Strategy |

**Key Methods:**

```cpp
void Register(const std::string& command, std::unique_ptr<CommandHandler> handler)
CommandHandler* Get(const std::string& command)      // nullptr if not found
bool Exists(const std::string& command) const
```

**Internal Storage:** `std::unordered_map<std::string, std::unique_ptr<CommandHandler>>`  
**Thread Safety:** Not thread-safe; intended for read-only use after startup registration

---

#### **CommandHandler** — `command_handler.h`

| Attribute | Detail |
|---|---|
| **Purpose** | Abstract interface for command execution |
| **Pattern** | Template Method / Command |

```cpp
virtual protocol::Response Execute(
    const protocol::Request& request,
    core::KVEngine& engine) = 0;
```

**Thread Safety:** Stateless implementations are inherently thread-safe

---

#### **RequestContext** — `request_context.h`

| Attribute | Detail |
|---|---|
| **Purpose** | Bundles per-request runtime dependencies |

**Fields:**

```cpp
net::Connection&         connection_   // owning TCP connection
const protocol::Request& request_      // parsed request
core::KVEngine&          engine_       // engine reference
```

**Thread Safety:** Not thread-safe; one context per request, not shared

---

#### **ThreadPool** — `thread_pool.h`

| Attribute | Detail |
|---|---|
| **Purpose** | Fixed-size worker thread pool for future async work |

**Key Methods:**

```cpp
explicit ThreadPool(std::size_t thread_count)
void Submit(std::function<void()> task)
~ThreadPool()   // graceful shutdown: drains queue, joins threads
```

**Internal Synchronization:** `std::mutex` + `std::condition_variable`  
**Thread Safety:** Fully thread-safe; multiple producers can call `Submit()` concurrently

---

### 3.2 Core Module (`src/core/`)

#### **KVEngine** — `kv_engine.h`

| Attribute | Detail |
|---|---|
| **Purpose** | Central public API boundary for KV operations |
| **Namespace** | `kvmemo::core` |

**Responsibilities:**
- Exposes `Set`, `Get`, `Delete` as the sole public KV interface
- Coordinates `ShardManager`, `TTLIndex`, and `EvictionManager`
- Called by `TTLManager` background thread via `ProcessExpired()`

**Key Methods:**

```cpp
void Set(const std::string& key, const std::string& value,
         std::optional<uint64_t> ttl_ms = std::nullopt)

std::optional<std::string> Get(const std::string& key)

void Delete(const std::string& key)

void ProcessExpired()    // collect expired keys from TTLIndex → delete from shards
void ProcessEvictions()  // collect victims from EvictionManager → delete from shards
```

**Set Logic:**
- With TTL → `shard_manager_->SetWithTTL(...)`, then `ttl_index_->Upsert(key, expire_at)`
- Without TTL → `shard_manager_->Set(...)`, then `ttl_index_->Remove(key)`
- Always calls `eviction_manager_->OnWrite(key)`

**Dependencies:** `ShardManager`, `TTLIndex`, `EvictionManager`  
**Thread Safety:** Thread-safe by delegation to shard-level mutexes

---

#### **ShardManager** — `shard_manager.h`

| Attribute | Detail |
|---|---|
| **Purpose** | Distributes keys across N independently-locked shards |

**Key Methods:**

```cpp
void Set(const Key& key, std::string value)
void SetWithTTL(const Key& key, std::string value, uint64_t ttl_ms)
std::optional<std::string> Get(const Key& key)
void Delete(const Key& key)
void CleanupExpired(uint64_t now)       // sweeps all shards
std::size_t ShardCount() const noexcept
```

**Shard Routing:**

```cpp
// Shard index for a key
std::size_t index = std::hash<std::string>{}(key) % shard_count_;
```

**Construction:**

```cpp
ShardManager(std::size_t shard_count, std::size_t shard_capacity)
// Example: ShardManager(16, 10000) → 16 shards, 10000 keys each
```

**Thread Safety:** Thread-safe by delegation; each `Shard` has its own `std::mutex`

---

#### **Shard** — `shard.h`

| Attribute | Detail |
|---|---|
| **Purpose** | Single mutex-protected KV storage partition |

**Private Members:**

```cpp
const std::size_t capacity_;
mutable std::mutex mutex_;

std::unordered_map<Key, Entry> store_;   // key → value + metadata
LRUCache lru_;                           // per-shard recency order
TTLIndex ttl_index_;                     // per-shard expiry tracking
```

**Key Methods:**

```cpp
void Set(const Key& key, std::string value)
void SetWithTTL(const Key& key, std::string value, uint64_t ttl_ms)
std::optional<std::string> Get(const Key& key)   // lazy expiry on read
void Delete(const Key& key)
std::size_t Size() const
void CleanupExpired(uint64_t now)
```

**Overflow Handling:** When `lru_.Touch(key)` returns `true` (capacity exceeded), `EvictOne()` is called immediately, removing the LRU key from both `store_` and `ttl_index_`.

**Thread Safety:** All public methods lock `mutex_`; not a `shared_mutex` (writes dominate)

---

#### **Entry** — `entry.h`

| Attribute | Detail |
|---|---|
| **Purpose** | Value record with TTL metadata |

**Fields:**

```cpp
std::string value_         // stored payload
Timestamp   created_at_    // epoch ms at construction
Timestamp   expire_at_     // epoch ms at expiry; 0 = no TTL
```

**Key Methods:**

```cpp
const std::string& Value() const noexcept
bool HasTTL() const noexcept               // expire_at_ != 0
bool IsExpired() const noexcept            // Clock::NowEpochMillis() >= expire_at_
Timestamp ExpireAt() const noexcept
Timestamp CreatedAt() const noexcept
uint64_t RemainingTTL() const noexcept     // ms remaining; 0 if no TTL or expired
void Update(std::string new_value, uint64_t ttl_ms = 0)
```

**Thread Safety:** Not thread-safe; protected by the owning `Shard`'s mutex

---

#### **LRUCache** — `lru_cache.h`

| Attribute | Detail |
|---|---|
| **Purpose** | O(1) LRU recency tracking (keys only, no values) |

**Data Structure:** Doubly-linked list (`std::list<Key>`) + hash map (`std::unordered_map<Key, list::iterator>`)

**Key Methods:**

```cpp
bool Touch(const Key& key)           // insert or move-to-front; returns true on overflow
void Remove(const Key& key)          // remove from tracking
const Key& EvictionCandidate() const // peek LRU key (back of list)
Key PopEvictionCandidate()           // remove and return LRU key
std::size_t Size() const noexcept
std::size_t Capacity() const noexcept
void Clear() noexcept
```

**Time Complexity:**

| Operation | Complexity |
|---|---|
| `Touch` | O(1) |
| `Remove` | O(1) |
| `PopEvictionCandidate` | O(1) |

**Thread Safety:** Not thread-safe; caller (`Shard`, `LRUPolicy`) must synchronize

---

#### **TTLIndex** — `ttl_index.h`

| Attribute | Detail |
|---|---|
| **Purpose** | Time-ordered expiration tracking for keys with TTL |

**Data Structures:**

```cpp
std::map<Timestamp, std::vector<Key>> expiry_map_  // expire_at → [keys]
std::unordered_map<Key, Timestamp>    key_index_   // key → expire_at
```

**Key Methods:**

```cpp
void Upsert(const Key& key, Timestamp expire_at)      // add or update TTL
void Remove(const Key& key)                           // cancel TTL tracking
std::vector<Key> CollectExpired(Timestamp now)        // all keys with expire_at <= now
std::size_t Size() const noexcept
void Clear() noexcept
```

**Time Complexity:**

| Operation | Complexity |
|---|---|
| `Upsert` | O(log N) — `std::map` insertion |
| `Remove` | O(log N) |
| `CollectExpired` | O(K log N) — K = expired keys |

**Thread Safety:** Not thread-safe; caller must synchronize

---

### 3.3 Eviction Module (`src/eviction/`)

#### **EvictionPolicy** — `eviction_manager.h` (abstract base)

```cpp
class EvictionPolicy {
public:
    virtual void OnRead(const std::string& key) = 0;
    virtual void OnWrite(const std::string& key) = 0;
    virtual void OnDelete(const std::string& key) = 0;
    virtual std::optional<std::string> SelectVictim() = 0;
};
```

---

#### **LRUPolicy** — `eviction_manager.h`

| Attribute | Detail |
|---|---|
| **Purpose** | LRU eviction policy; wraps `LRUCache` |

**Methods delegate to `LRUCache`:**

```cpp
void OnRead(const std::string& key)    → lru_->Touch(key)
void OnWrite(const std::string& key)   → lru_->Touch(key)
void OnDelete(const std::string& key)  → lru_->Remove(key)
std::optional<std::string> SelectVictim() → lru_->PopEvictionCandidate()
```

---

#### **EvictionManager** — `eviction_manager.h`

| Attribute | Detail |
|---|---|
| **Purpose** | Coordinates memory tracking and eviction policy |

**Key Methods:**

```cpp
void OnRead(const std::string& key)
void OnWrite(const std::string& key)   // reserves 100 bytes; triggers eviction if over limit
void OnDelete(const std::string& key)  // releases 100 bytes
std::vector<std::string> CollectEvictionCandidates()
```

**Eviction Flow in `OnWrite`:**
1. `memory_tracker_->Reserve(100)` — increment byte counter
2. `policy_->OnWrite(key)` — update LRU position
3. `EnforceMemoryLimit()` — no-op if within limit; future hook for proactive eviction

**`CollectEvictionCandidates` Flow:**
1. While `memory_tracker_->IsOverLimit()`:
   - `policy_->SelectVictim()` → candidate key
   - Add to result, `memory_tracker_->Release(100)`

**Internal Synchronization:** `std::mutex mutex_` protects all public methods  
**Thread Safety:** Fully thread-safe

---

#### **MemoryTracker** — `memory_tracker.h`

| Attribute | Detail |
|---|---|
| **Purpose** | Lock-free memory usage accounting |

**Key Methods:**

```cpp
bool Reserve(std::size_t bytes) noexcept     // fetch_add; returns !IsOverLimit()
void Release(std::size_t bytes) noexcept     // fetch_sub
std::size_t CurrentUsage() const noexcept
std::size_t MaxLimit() const noexcept
bool IsOverLimit() const noexcept            // CurrentUsage() > max_memory_bytes_
```

**Internal State:**

```cpp
const std::size_t        max_memory_bytes_
std::atomic<std::size_t> current_memory_bytes_
```

**Thread Safety:** Fully thread-safe via `std::atomic` with `memory_order_relaxed`

---

#### **TTLManager** — `ttl_manager.h`

| Attribute | Detail |
|---|---|
| **Purpose** | Orchestrates proactive TTL expiration cycles |

**Key Methods:**

```cpp
TTLManager(TTLIndex& ttl_index, ShardManager& shard_manager) noexcept
size_t run_expiration_cycle()   // fetch expired keys → delete via ShardManager
void set_enabled(bool enabled) noexcept
bool is_enabled() const noexcept
```

**Thread Safety:** Thread-safe assuming underlying components are thread-safe; `enabled_` uses `std::atomic<bool>`

---

### 3.4 Protocol Module (`src/protocol/`)

#### **Request** — `request.h`

| Attribute | Detail |
|---|---|
| **Purpose** | Immutable parsed client command |

```cpp
Request(std::string command, std::vector<std::string> args)

const std::string& Command() const noexcept
std::size_t ArgCount() const noexcept
const std::string& Arg(std::size_t index) const   // throws std::out_of_range
const std::vector<std::string>& Args() const noexcept
bool Empty() const noexcept
```

---

#### **Response** — `response.h`

| Attribute | Detail |
|---|---|
| **Purpose** | Server response value object |

```cpp
enum class ResponseStatus { Ok, Error };

static Response Ok(std::string message = {})
static Response Error(std::string message)

ResponseStatus Status() const noexcept
const std::string& Message() const noexcept
bool IsOk() const noexcept
bool IsError() const noexcept
```

---

#### **Parser** — `parser.h`

| Attribute | Detail |
|---|---|
| **Purpose** | Stateless tokenizer for wire frames |
| **Pattern** | Static utility (non-instantiable) |

```cpp
static Request Parse(const std::string& input)
// Splits on whitespace; first token = command, rest = args
// Throws std::invalid_argument on empty input
```

---

#### **Serializer** — `serializer.h`

| Attribute | Detail |
|---|---|
| **Purpose** | Converts `Response` to RESP-like wire bytes |
| **Pattern** | Static utility (non-instantiable) |

```cpp
static std::string Serialize(const Response& response)
```

**Wire Format:**

| Response Type | Format | Example |
|---|---|---|
| OK (no data) | `+OK\r\n` | `+OK\r\n` |
| Bulk string | `$<len>\r\n<data>\r\n` | `$5\r\nAlice\r\n` |
| Error | `-ERR<message>\r\n` | `-ERRKey not found\r\n` |

---

#### **Framing** — `framing.h`

| Attribute | Detail |
|---|---|
| **Purpose** | Extracts complete frames from the TCP byte stream |
| **Pattern** | Static utility (non-instantiable) |

```cpp
static bool NextFrame(Buffer& buffer, std::string& frame)
// Searches for \r\n delimiter
// Returns true + populated frame on success
// Returns false if more data is needed (partial read)
// Consumes frame_len + 2 bytes from buffer on success
```

---

#### **Buffer** — `buffer.h`

| Attribute | Detail |
|---|---|
| **Purpose** | Dynamic byte buffer for connection I/O |

**Key Methods:**

```cpp
void Append(const char* data, std::size_t len)
void Append(const std::string& data)
const char* Data() const noexcept          // pointer to readable data
std::size_t ReadableBytes() const noexcept
void Consume(std::size_t len)              // advance read cursor; clears if fully consumed
void Clear() noexcept
```

**Internal State:**

```cpp
std::vector<char> storage_
std::size_t       read_pos_ = 0
```

**Memory Optimization:** `Consume()` resets `storage_` and `read_pos_` to zero when all bytes are consumed, reclaiming memory without reallocation.

**Thread Safety:** Not thread-safe; each `Connection` owns its own buffers

---

### 3.5 Network Module (`src/net/`)

#### **TcpServer** — `tcp_server.h`

| Attribute | Detail |
|---|---|
| **Purpose** | Create and manage the listening TCP socket |

**Key Methods:**

```cpp
explicit TcpServer(int port)
void Start()    // CreateSocket() → Bind() → Listen()
void Accept()   // accept() → new Connection → ConnectionManager::Add()
void Stop()     // close(listen_fd_)
int ListenFD() const noexcept
ConnectionManager& Connection() noexcept
```

**Socket Options:** `INADDR_ANY`, backlog = 128  
**Thread Safety:** Not thread-safe; runs on the single event-loop thread

---

#### **Connection** — `connection.h`

| Attribute | Detail |
|---|---|
| **Purpose** | Owns a single client socket and its I/O buffers |

**Key Methods:**

```cpp
explicit Connection(int fd)
int FD() const noexcept
protocol::Buffer& InputBuffer() noexcept
protocol::Buffer& OutputBuffer() noexcept
ssize_t ReadFromSocket()     // read() up to 4096 bytes → InputBuffer
size_t WriteToSocket()       // write() from OutputBuffer → socket
void Close()                 // close(fd_)
~Connection()                // calls Close()
```

**Read Buffer Size:** 4096 bytes per `ReadFromSocket()` call  
**Thread Safety:** Not thread-safe; owned by a single event-loop

---

#### **ConnectionManager** — `connection_manager.h`

| Attribute | Detail |
|---|---|
| **Purpose** | Stores and manages the set of active connections |

**Key Methods:**

```cpp
void Add(std::unique_ptr<Connection> connection)  // keyed by fd
void Remove(int fd)                               // destructs Connection (closes socket)
Connection* Get(int fd)                           // throws std::runtime_error if not found
std::size_t Size() const noexcept
template<typename Callback>
void ForEachConnection(Callback&& callback)       // callback(fd, Connection*)
```

**Internal Storage:** `std::unordered_map<int, std::unique_ptr<Connection>>`  
**Thread Safety:** Not thread-safe; used within single event-loop thread

---

### 3.6 Metrics Module (`src/metrics/`)

#### **Counter** — `metrics_registry.h`

```cpp
void increment(uint64_t value = 1) noexcept   // std::atomic fetch_add
uint64_t value() const noexcept
```

---

#### **MetricsRegistry** — `metrics_registry.h`

| Attribute | Detail |
|---|---|
| **Purpose** | Thread-safe named counter registry |

**Key Methods:**

```cpp
void register_counter(const std::string& name)
void increment(const std::string& name, uint64_t value = 1)  // auto-registers if absent
MetricSnapshot snapshot() const
```

**Internal State:** `std::unordered_map<std::string, Counter>` protected by `std::mutex`  
**Thread Safety:** Fully thread-safe; mutex protects registry map

---

#### **LatencyTracker** — `latency_tracker.h`

| Attribute | Detail |
|---|---|
| **Purpose** | High-resolution operation latency measurement |

**Key Methods:**

```cpp
TimePoint start() const noexcept           // Clock::now()
void stop(TimePoint start_time) noexcept   // records ns duration
LatencyStats snapshot() const              // consistent stats copy
```

**Internal State:**

```cpp
std::atomic<uint64_t> total_operations_
std::atomic<uint64_t> total_latency_ns_
mutable std::mutex    mutex_               // protects min/max
uint64_t min_latency_ns_
uint64_t max_latency_ns_
```

**`LatencyStats` Fields:** `total_operations`, `total_latency_ns`, `min_latency_ns`, `max_latency_ns`, `average_latency_ns()`  
**Thread Safety:** Lock-free recording path; mutex only for min/max

---

#### **MetricsSnapshot** — `metrics_snapshot.h`

| Attribute | Detail |
|---|---|
| **Purpose** | Immutable point-in-time view of all subsystem metrics |

**Sub-Snapshots:**

```cpp
struct CommandLatencySnapshot {
    LatencyStats get_latency;
    LatencyStats set_latency;
    LatencyStats delete_latency;
};

struct EngineMetricsSnapshot {
    uint64_t total_keys;
    uint64_t total_requests;
    uint64_t total_evictions;
    uint64_t total_expirations;
};

struct NetworkMetricsSnapshot {
    uint64_t active_connections;
    uint64_t total_connections;
    uint64_t bytes_received;
    uint64_t bytes_sent;
};
```

**Thread Safety:** Immutable after construction; inherently thread-safe for concurrent reads

---

### 3.7 Common Utilities (`src/common/`)

#### **Config** — `config.h`

| Field | Type | Default | Description |
|---|---|---|---|
| `shard_count` | `size_t` | `64` | Must be a power of two |
| `max_memory_bytes` | `uint64_t` | `256 MB` | Global memory limit |
| `max_value_bytes` | `uint64_t` | `8 MB` | Max single value size |
| `listen_port` | `uint16_t` | `8080` | TCP listen port |
| `max_connections` | `size_t` | `4096` | Soft connection limit |
| `worker_threads` | `size_t` | `0` (auto) | 0 = auto-detect |
| `enable_ttl` | `bool` | `true` | Enables TTL expiration |
| `ttl_sweep_interval_ms` | `uint32_t` | `250` | Background sweep period |
| `enable_metrics` | `bool` | `true` | Enables metrics collection |
| `eviction_policy` | `EvictionPolicy` | `kLRU` | `kNone` or `kLRU` |

**Validation:**

```cpp
[[nodiscard]] Status Validate() const noexcept
```

Validates: `shard_count > 0` and power-of-two, `max_memory_bytes > 0`, `max_value_bytes <= max_memory_bytes`, valid `listen_port`, `max_connections > 0`, `worker_threads <= 1024`, TTL sweep > 0 if TTL enabled.

> **Note:** `main.cpp` currently constructs `ServerApp` directly with hardcoded values (default port `6379`, 16 shards, 10000 capacity per shard, 256 MB memory limit) rather than using the `Config` struct. The `Config` struct defines its own independent defaults (port `8080`, 64 shards) and is available for future integration to replace the hardcoded construction.

---

#### **Logger** — `logger.h`

| Attribute | Detail |
|---|---|
| **Purpose** | Thread-safe structured logging utility |
| **Pattern** | Static-only class (non-instantiable) |

**Log Levels:** `kTrace`, `kDebug`, `kInfo`, `kWarn`, `kError`, `kFatal`, `kOff`

**Log Format:**
```
[EPOCH_MS] [LEVEL] [tid=THREAD_ID] file:line | message
```

**Macros:**
```cpp
KV_LOG_TRACE(msg)
KV_LOG_DEBUG(msg)
KV_LOG_INFO(msg)
KV_LOG_WARN(msg)
KV_LOG_ERROR(msg)
KV_LOG_FATAL(msg)   // calls std::abort() after logging
```

**Thread Safety:** `std::mutex` protects `std::cerr` writes; `std::atomic<LogLevel>` for level

---

#### **Status** — `status.h`

| Attribute | Detail |
|---|---|
| **Purpose** | Lightweight error-carrying return type |

**Status Codes:**

| Code | Value | Use Case |
|---|---|---|
| `kOk` | 0 | Success |
| `kInvalidArgument` | 2 | Bad parameters |
| `kNotFound` | 3 | Missing key |
| `kAlreadyExists` | 4 | Duplicate key |
| `kProtocolError` | 100 | Malformed protocol |
| `kNetworkError` | 101 | Socket errors |
| `kTimeout` | 102 | Operation timeout |
| `kResourceExhausted` | 200 | Memory/connection limit |
| `kInternalError` | 201 | Unexpected failures |

**Factory Methods:**
```cpp
static Status Ok()
static Status InvalidArgument(std::string message)
static Status NotFound(std::string message)
static Status ProtocolError(std::string message)
// ... and more
```

---

#### **Clock** — `time.h`

| Attribute | Detail |
|---|---|
| **Purpose** | Centralized time provider |
| **Pattern** | Static-only class (non-instantiable) |

**Type Aliases:**
```cpp
using EpochMillis      = std::uint64_t
using SteadyTimePoints = std::chrono::steady_clock::time_point
using DurationMillis   = std::chrono::milliseconds
```

**Methods:**
```cpp
static EpochMillis NowEpochMillis() noexcept     // wall-clock for TTL expiry
static SteadyTimePoints NowSteady() noexcept     // monotonic for latency
static EpochMillis ElapsedMillis(start, end) noexcept
```

> **Design Rationale:** Wall-clock (`system_clock`) for TTL expiry timestamps; monotonic (`steady_clock`) for latency measurement to guard against NTP adjustments.

---

### 3.8 Client Module (`src/client/`)

#### **KVClient** — `kv_client.h`

| Attribute | Detail |
|---|---|
| **Purpose** | Single-threaded TCP client for CLI and integration testing |

**Key Methods:**

```cpp
KVClient(std::string host, int port)
void Connect()                                // creates socket, connects
std::string SendCommand(const std::string& command)  // sends cmd\r\n, reads response
~KVClient()                                   // close(socket_fd_)
```

**Wire Protocol:** Commands sent as `<cmd> [args...]\r\n`; responses read into a 4096-byte buffer.

**Thread Safety:** Not thread-safe; intended for single-threaded CLI use

---

## 4. Data Structures

### 4.1 KV Storage Hierarchy

```
KVEngine
  └── ShardManager (N shards)
        └── Shard[i]
              ├── std::unordered_map<Key, Entry>   ← primary store
              ├── LRUCache                          ← recency order
              │     ├── std::list<Key>              ← MRU front, LRU back
              │     └── std::unordered_map<Key, list::iterator>
              └── TTLIndex                          ← per-shard expiry
                    ├── std::map<Timestamp, vector<Key>>  ← sorted by expire time
                    └── std::unordered_map<Key, Timestamp>
```

### 4.2 Time Complexity Summary

| Operation | Complexity | Notes |
|---|---|---|
| `Set(key, value)` | O(1) avg | Hash map insert + LRU O(1) touch |
| `Get(key)` | O(1) avg | Hash map lookup + LRU O(1) touch |
| `Delete(key)` | O(1) avg | Hash map erase + LRU O(1) remove |
| `LRU Touch` | O(1) | Doubly-linked list splice + map update |
| `LRU Evict` | O(1) | Pop back of list |
| `TTL Upsert` | O(log N) | `std::map` insert |
| `TTL CollectExpired` | O(K log N) | K = expired keys; erases prefix of sorted map |
| `Shard Lookup` | O(1) | `std::hash` + modulo |
| `MemoryTracker Reserve` | O(1) | Atomic fetch_add |

### 4.3 Entry Memory Layout

```
Entry {
    std::string value_       // heap-allocated payload
    uint64_t    created_at_  // 8 bytes
    uint64_t    expire_at_   // 8 bytes (0 = no TTL)
}
```

Key is stored as the `unordered_map` key (not inside `Entry`), keeping `Entry` minimal.

---

## 5. Threading & Concurrency

### 5.1 Thread Model

| Thread | Responsibility | Count |
|---|---|---|
| **Main / Event Loop** | `select()`, accept, read, dispatch, write | 1 |
| **TTLManager** | Periodic expiration sweep | 1 (optional) |
| **ThreadPool Workers** | Future async command handling | Configurable |

### 5.2 Synchronization Strategy

```
┌──────────────────────────────────────────────────────┐
│                  Main Event Loop                     │
│   (single thread: select + I/O + dispatch)           │
│   No lock required for network/protocol layer        │
└──────────────────┬───────────────────────────────────┘
                   │ calls KVEngine (thread-safe)
┌──────────────────▼───────────────────────────────────┐
│              KVEngine                                │
│   (stateless orchestration; delegates to shards)     │
└──────────────────┬───────────────────────────────────┘
                   │ hash → Shard[i]
┌──────────────────▼───────────────────────────────────┐
│                Shard[i]                              │
│   std::mutex mutex_  (exclusive per shard)           │
│   Protects: store_, lru_, ttl_index_                 │
└──────────────────────────────────────────────────────┘
                   ▲
         TTLManager thread also calls
         ShardManager → Shard[i].CleanupExpired()
         (acquires same per-shard mutex)
```

### 5.3 Mutex Inventory

| Class | Mutex Type | Guards | Scope |
|---|---|---|---|
| `Shard` | `std::mutex` | `store_`, `lru_`, `ttl_index_` | Per-shard |
| `EvictionManager` | `std::mutex` | `policy_`, `memory_tracker_` | Single instance |
| `MetricsRegistry` | `std::mutex` | `counters_` map | Single instance |
| `LatencyTracker` | `std::mutex` | `min_latency_ns_`, `max_latency_ns_` | Per-tracker |
| `ThreadPool` | `std::mutex` | `tasks_` queue | Single instance |
| `Logger` | `std::mutex` | `std::cerr` output | Static |

### 5.4 Lock-Free Components

| Class | Mechanism |
|---|---|
| `MemoryTracker` | `std::atomic<size_t>` with `memory_order_relaxed` |
| `LatencyTracker` (hot path) | `std::atomic<uint64_t>` for `total_ops`, `total_ns` |
| `ThreadPool::stop_` | `std::atomic<bool>` |
| `Logger::level_` | `std::atomic<LogLevel>` |
| `TTLManager::enabled_` | `std::atomic<bool>` |

### 5.5 Deadlock Prevention

- No two mutexes are ever held simultaneously (no lock ordering issue)
- Each `Shard` acquires only its own mutex per operation
- `EvictionManager` acquires its own mutex; never calls back into shards while locked

---

## 6. API Contracts

### 6.1 Protocol Format

```
Request (client → server):
    <COMMAND> [ARG1] [ARG2] ...\r\n

Response (server → client):
    +OK\r\n                         (success, no data)
    $<len>\r\n<data>\r\n            (success, bulk string)
    -ERR<message>\r\n               (error)
```

### 6.2 Supported Commands

| Command | Syntax | Success Response | Error Response |
|---|---|---|---|
| `SET` | `SET <key> <value>` | `+OK\r\n` | `-ERR SET requires key and value\r\n` |
| `GET` | `GET <key>` | `$<len>\r\n<value>\r\n` | `-ERRKey not found\r\n` |
| `DEL` | `DEL <key>` | `+OK\r\n` | `-ERR DEL requires key\r\n` |
| *(unknown)* | any other | — | `-ERRUnknown command\r\n` |
| *(empty)* | empty frame | — | `-ERREmpty Command\r\n` |

### 6.3 Example Session

```
Client → Server:  SET username Alice\r\n
Server → Client:  +OK\r\n

Client → Server:  GET username\r\n
Server → Client:  $5\r\nAlice\r\n

Client → Server:  DEL username\r\n
Server → Client:  +OK\r\n

Client → Server:  GET username\r\n
Server → Client:  -ERRKey not found\r\n
```

### 6.4 KVEngine C++ API

```cpp
// Store key with optional TTL (milliseconds)
engine.Set("key", "value");
engine.Set("session:123", "data", 30000);  // expires in 30 seconds

// Retrieve value; nullopt if missing or expired
std::optional<std::string> v = engine.Get("key");

// Delete key (no error if missing)
engine.Delete("key");

// Called by TTLManager thread
engine.ProcessExpired();

// Called when memory limit is exceeded
engine.ProcessEvictions();
```

---

## 7. Dependency Injection

All components use **constructor injection**. No singletons, no service locators, no global state.

### 7.1 Injection Graph

```
main()
  └── ServerApp(port)
        ├── TcpServer(port)                          ← value
        ├── KVEngine(
        │     ShardManager(16, 10000),               ← unique_ptr
        │     TTLIndex(),                            ← unique_ptr
        │     EvictionManager(
        │       MemoryTracker(256 MB),               ← unique_ptr
        │       LRUPolicy(
        │         LRUCache(10000)                    ← unique_ptr
        │       )                                    ← unique_ptr
        │     )                                      ← unique_ptr
        │   )                                        ← by value (owns ptrs)
        └── Dispatcher(engine_)                      ← reference
```

### 7.2 Ownership Rules

| Owner | Owned Type | Mechanism |
|---|---|---|
| `KVEngine` | `ShardManager` | `std::unique_ptr` |
| `KVEngine` | `TTLIndex` | `std::unique_ptr` |
| `KVEngine` | `EvictionManager` | `std::unique_ptr` |
| `EvictionManager` | `MemoryTracker` | `std::unique_ptr` |
| `EvictionManager` | `EvictionPolicy` | `std::unique_ptr` |
| `LRUPolicy` | `LRUCache` | `std::unique_ptr` |
| `ShardManager` | `Shard[]` | `vector<unique_ptr<Shard>>` |
| `ConnectionManager` | `Connection[]` | `unordered_map<fd, unique_ptr<Connection>>` |
| `Dispatcher` | `KVEngine` | non-owning reference |

### 7.3 Extensibility via Interfaces

- **New eviction policy:** Extend `EvictionPolicy`, inject into `EvictionManager`
- **New command:** Extend `CommandHandler`, register via `CommandRegistry`
- **New transport:** Replace `TcpServer` with any class exposing `ListenFD()` + `Accept()` + `Connection()`

---

## 8. Interaction Flows

### 8.1 SET Request Flow

```
Client
  │
  │  "SET key value\r\n"
  ▼
Connection::ReadFromSocket()
  → raw bytes into input_buffer_

Framing::NextFrame(input_buffer_, frame)
  → frame = "SET key value"

Parser::Parse(frame)
  → Request{command="SET", args=["key","value"]}

Dispatcher::Dispatch(request)
  → HandleSet()
      → engine_.Set("key", "value")

KVEngine::Set(key, value, nullopt)
  ├── ShardManager::Set(key, value)
  │     └── Shard[hash(key) % N]::Set(key, value)
  │           ├── store_["key"] = Entry("value")
  │           ├── lru_.Touch("key")
  │           │     overflow? → EvictOne() → remove LRU key
  │           └── ttl_index_.Remove("key")
  ├── ttl_index_.Remove("key")   (engine-level)
  └── eviction_manager_.OnWrite("key")
        ├── memory_tracker_.Reserve(100)
        └── policy_.OnWrite("key") → lru_.Touch("key")

Response::Ok() → "+OK\r\n"

Connection::WriteToSocket()
  → flush "+OK\r\n" to socket
```

---

### 8.2 GET Request Flow

```
Client
  │
  │  "GET key\r\n"
  ▼
Framing::NextFrame() → "GET key"
Parser::Parse()       → Request{command="GET", args=["key"]}

Dispatcher::Dispatch()
  → HandleGet()
      → engine_.Get("key")

KVEngine::Get(key)
  └── ShardManager::Get(key)
        └── Shard[hash(key) % N]::Get(key)
              ├── store_.find("key")
              │     not found? → return nullopt
              ├── entry.IsExpired()?
              │     yes → RemoveInternal("key")  (lazy expiry)
              │           → return nullopt
              └── lru_.Touch("key")
                    → return entry.Value()

value found?
  ├── yes → eviction_manager_.OnRead("key")
  │           └── policy_.OnRead("key") → lru_.Touch("key")
  │         → Response::Ok(value) → "$5\r\nAlice\r\n"
  └── no  → Response::Error("Key not found") → "-ERRKey not found\r\n"
```

---

### 8.3 TTL Expiration Flow

```
TTLManager thread
  │
  │  run_expiration_cycle() [called periodically]
  ▼
TTLIndex::CollectExpired(Clock::NowEpochMillis())
  → scan expiry_map_ prefix where expire_at <= now
  → return expired_keys[]

for each expired_key:
  ShardManager::Delete(expired_key)
    └── Shard[hash(key) % N]::Delete(key)
          → store_.erase(key)
          → lru_.Remove(key)
          → ttl_index_.Remove(key)  (per-shard index)

[Parallel path — lazy expiry on read]
Shard::Get(key)
  → entry.IsExpired()?
      yes → RemoveInternal(key)
              → store_.erase(key)
              → lru_.Remove(key)
              → ttl_index_.Remove(key)
            → return nullopt
```

---

### 8.4 Memory Eviction Flow

```
KVEngine::Set(key, value) [or any write path]
  │
  ▼
EvictionManager::OnWrite(key)
  ├── memory_tracker_.Reserve(100)
  │     current_memory_bytes_ += 100
  ├── policy_.OnWrite(key)  → lru_.Touch(key)
  └── EnforceMemoryLimit()
        memory_tracker_.IsOverLimit()?
          no  → return
          yes → (proactive eviction hook for future use)

[Explicit eviction — KVEngine::ProcessEvictions()]
EvictionManager::CollectEvictionCandidates()
  loop while IsOverLimit():
    victim = policy_.SelectVictim()
              → lru_.PopEvictionCandidate()
                    → remove back of list (LRU key)
    victims.push_back(victim)
    memory_tracker_.Release(100)

for each victim in victims:
  ShardManager::Delete(victim)
    └── Shard[hash(victim) % N]::Delete(victim)
  TTLIndex::Remove(victim)  (engine-level)
```

---

*Copyright © 2026 KVMemo — All Rights Reserved*  
*This document may not be copied, modified, or distributed without explicit permission from the author.*
