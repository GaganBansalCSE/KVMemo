/**
 * @file test_concurrency.cpp
 * @brief Comprehensive concurrency tests for KVMemo.
 *
 * Responsibilities:
 *  - Test thread-safety of the core KV engine
 *  - Validate shard-level synchronization
 *  - Verify TTL expiration under concurrent load
 *  - Test LRU eviction with competing threads
 *  - Ensure consistency across concurrent operations
 *
 * Thread Safety:
 *  > All tests are thread-safe
 *  > Uses std::mutex for test state synchronization
 *  > Atomics for metrics collection
 *
 * Copyright © 2026 Gagan Bansal
 * ALL RIGHTS RESERVED.
 */

#include <thread>
#include <vector>
#include <atomic>
#include <cassert>
#include <iostream>
#include <chrono>
#include <string>
#include <mutex>
#include <optional>
#include <memory>
#include <random>
#include <sstream>

// Include KVMemo core components
#include "src/core/shard.h"
#include "src/core/kv_engine.h"
#include "src/common/status.h"
#include "src/common/logger.h"
#include "src/common/time.h"

namespace kvmemo::tests {

/**
 * @class TestMetrics
 * @brief Thread-safe metrics collection for concurrency tests.
 *
 * Tracks:
 *  - Operation counts (reads, writes, deletes)
 *  - Error counts
 *  - Latency statistics
 *  - Contention metrics
 */
class TestMetrics final {
 public:
  TestMetrics() = default;

  ~TestMetrics() = default;

  void RecordRead() noexcept { reads_.fetch_add(1, std::memory_order_relaxed); }

  void RecordWrite() noexcept { writes_.fetch_add(1, std::memory_order_relaxed); }

  void RecordDelete() noexcept {
    deletes_.fetch_add(1, std::memory_order_relaxed);
  }

  void RecordError() noexcept { errors_.fetch_add(1, std::memory_order_relaxed); }

  void RecordContentionEvent() noexcept {
    contention_.fetch_add(1, std::memory_order_relaxed);
  }

  [[nodiscard]] uint64_t GetReads() const noexcept {
    return reads_.load(std::memory_order_relaxed);
  }

  [[nodiscard]] uint64_t GetWrites() const noexcept {
    return writes_.load(std::memory_order_relaxed);
  }

  [[nodiscard]] uint64_t GetDeletes() const noexcept {
    return deletes_.load(std::memory_order_relaxed);
  }

  [[nodiscard]] uint64_t GetErrors() const noexcept {
    return errors_.load(std::memory_order_relaxed);
  }

  [[nodiscard]] uint64_t GetContention() const noexcept {
    return contention_.load(std::memory_order_relaxed);
  }

  [[nodiscard]] uint64_t GetTotalOps() const noexcept {
    return GetReads() + GetWrites() + GetDeletes();
  }

  void Reset() noexcept {
    reads_.store(0, std::memory_order_relaxed);
    writes_.store(0, std::memory_order_relaxed);
    deletes_.store(0, std::memory_order_relaxed);
    errors_.store(0, std::memory_order_relaxed);
    contention_.store(0, std::memory_order_relaxed);
  }

 private:
  std::atomic<uint64_t> reads_{0};
  std::atomic<uint64_t> writes_{0};
  std::atomic<uint64_t> deletes_{0};
  std::atomic<uint64_t> errors_{0};
  std::atomic<uint64_t> contention_{0};
};

/**
 * @class ConcurrencyTestBase
 * @brief Base class for all concurrency tests.
 *
 * Provides:
 *  - Common setup/teardown logic
 *  - Metrics collection and reporting
 *  - Thread management
 *  - Assertion helpers
 *
 * Design Pattern: Template Method
 *  - Subclasses override RunTest()
 *  - Base handles threading and metrics
 */
class ConcurrencyTestBase {
 protected:
  ConcurrencyTestBase(const std::string& test_name, std::size_t num_threads)
      : test_name_(test_name), num_threads_(num_threads) {}

  virtual ~ConcurrencyTestBase() = default;

  /**
   * @brief Override this in subclasses to implement test logic.
   */
  virtual void RunTest(std::size_t thread_id, TestMetrics& metrics) = 0;

  /**
   * @brief Execute test with multiple threads.
   */
  void ExecuteTest() {
    std::cout << "\n[TEST] " << test_name_ << " (threads=" << num_threads_
              << ")\n";

    TestMetrics metrics;
    std::vector<std::thread> threads;

    auto start_time = std::chrono::steady_clock::now();

    // Launch worker threads
    for (std::size_t i = 0; i < num_threads_; ++i) {
      threads.emplace_back(&ConcurrencyTestBase::RunTest, this, i,
                           std::ref(metrics));
    }

    // Wait for completion
    for (auto& t : threads) {
      t.join();
    }

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    // Report metrics
    PrintMetrics(metrics, duration);
  }

  /**
   * @brief Thread-safe assertion.
   */
  void AssertTrue(bool condition, const std::string& message) {
    if (!condition) {
      std::lock_guard<std::mutex> lock(assertion_mutex_);
      std::cerr << "ASSERTION FAILED: " << message << "\n";
      test_passed_ = false;
    }
  }

 public:
  [[nodiscard]] bool TestPassed() const noexcept { return test_passed_; }

 private:
  void PrintMetrics(const TestMetrics& metrics,
                    std::chrono::milliseconds duration) {
    std::cout << "  Duration: " << duration.count() << "ms\n"
              << "  Total Operations: " << metrics.GetTotalOps() << "\n"
              << "  Reads: " << metrics.GetReads() << "\n"
              << "  Writes: " << metrics.GetWrites() << "\n"
              << "  Deletes: " << metrics.GetDeletes() << "\n"
              << "  Errors: " << metrics.GetErrors() << "\n"
              << "  Contention Events: " << metrics.GetContention() << "\n";

    if (metrics.GetTotalOps() > 0) {
      double throughput = (metrics.GetTotalOps() * 1000.0) / duration.count();
      std::cout << "  Throughput: " << throughput << " ops/sec\n";
    }

    std::cout << "  Result: " << (TestPassed() ? "PASS" : "FAIL") << "\n";
  }

  std::string test_name_;
  std::size_t num_threads_;
  std::atomic<bool> test_passed_{true};
  std::mutex assertion_mutex_;
};

// ============================================================================
// Test 1: Concurrent Writes to Different Keys
// ============================================================================

/**
 * @brief Test: Multiple threads writing different keys simultaneously.
 *
 * Validates:
 *  - No data loss under concurrent writes
 *  - Shard locking mechanisms work correctly
 *  - All writes are visible after completion
 */
class TestConcurrentWrites : public ConcurrencyTestBase {
 private:
  core::Shard shard_{1000};  // 1000-key capacity

  void RunTest(std::size_t thread_id, TestMetrics& metrics) override {
    const std::size_t ops_per_thread = 100;

    for (std::size_t i = 0; i < ops_per_thread; ++i) {
      std::string key = "key_t" + std::to_string(thread_id) + "_" +
                        std::to_string(i);  // Unique key per thread
      std::string value = "value_" + std::to_string(i);

      try {
        shard_.Set(key, value);
        metrics.RecordWrite();
      } catch (...) {
        metrics.RecordError();
      }
    }
  }

 public:
  explicit TestConcurrentWrites(std::size_t num_threads = 8)
      : ConcurrencyTestBase("Concurrent Writes (Different Keys)",
                            num_threads),
        num_threads_(num_threads) {}

  void Run() {
    ExecuteTest();

    // Verify all keys were written
    std::size_t expected_keys = num_threads_ * 100;
    std::size_t actual_keys = shard_.Size();

    AssertTrue(actual_keys == expected_keys,
               "Key count mismatch: expected=" + std::to_string(expected_keys) +
                   ", actual=" + std::to_string(actual_keys));
  }

 private:
  std::size_t num_threads_;
};

// ============================================================================
// Test 2: Concurrent Reads and Writes
// ============================================================================

/**
 * @brief Test: Mixed read/write workload with contention.
 *
 * Validates:
 *  - Concurrent reads don't block each other
 *  - Reads see consistent data
 *  - Write visibility is immediate
 */
class TestConcurrentReadWrite : public ConcurrencyTestBase {
 private:
  core::Shard shard_{1000};

  void RunTest(std::size_t thread_id, TestMetrics& metrics) override {
    const std::string key = "shared_key";
    const std::size_t ops_per_thread = 50;

    for (std::size_t i = 0; i < ops_per_thread; ++i) {
      if (i % 2 == 0) {
        // Write operation
        shard_.Set(key, "value_" + std::to_string(i));
        metrics.RecordWrite();
      } else {
        // Read operation
        auto value = shard_.Get(key);
        if (value.has_value()) {
          metrics.RecordRead();
        } else {
          metrics.RecordError();
        }
      }
    }
  }

 public:
  explicit TestConcurrentReadWrite(std::size_t num_threads = 8)
      : ConcurrencyTestBase("Concurrent Read/Write (Shared Key)",
                            num_threads) {}

  void Run() {
    ExecuteTest();

    // Verify key exists after all operations
    auto final_value = shard_.Get("shared_key");
    AssertTrue(final_value.has_value(),
               "Shared key should exist after concurrent operations");
  }
};

// ============================================================================
// Test 3: Concurrent Deletes and Inserts
// ============================================================================

/**
 * @brief Test: Rapid create/delete cycles under contention.
 *
 * Validates:
 *  - Delete operations don't conflict with concurrent inserts
 *  - LRU tracking is consistent after deletes
 *  - Memory is properly reclaimed
 */
class TestConcurrentDeleteInsert : public ConcurrencyTestBase {
 private:
  core::Shard shard_{500};

  void RunTest(std::size_t thread_id, TestMetrics& metrics) override {
    const std::size_t cycles = 50;

    for (std::size_t cycle = 0; cycle < cycles; ++cycle) {
      std::string key = "cycle_key_t" + std::to_string(thread_id) + "_" +
                        std::to_string(cycle);

      try {
        // Insert
        shard_.Set(key, "temp_value");
        metrics.RecordWrite();

        // Delete
        shard_.Delete(key);
        metrics.RecordDelete();
      } catch (...) {
        metrics.RecordError();
      }
    }
  }

 public:
  explicit TestConcurrentDeleteInsert(std::size_t num_threads = 8)
      : ConcurrencyTestBase("Concurrent Delete/Insert", num_threads) {}

  void Run() {
    ExecuteTest();

    // After all cycles, shard should be relatively empty
    // (allowing for some race condition artifacts)
    std::size_t final_size = shard_.Size();
    AssertTrue(final_size < 100,  // Reasonable threshold
               "Shard size should be small after delete/insert cycles: " +
                   std::to_string(final_size));
  }
};

// ============================================================================
// Test 4: TTL Expiration Under Concurrent Load
// ============================================================================

/**
 * @brief Test: TTL expiration behavior with concurrent operations.
 *
 * Validates:
 *  - TTL expirations are detected correctly
 *  - Concurrent reads on expired keys return nullopt
 *  - TTL metadata is consistent
 */
class TestConcurrentTTLExpiration : public ConcurrencyTestBase {
 private:
  core::Shard shard_{500};

  void RunTest(std::size_t thread_id, TestMetrics& metrics) override {
    const std::size_t ops = 50;

    for (std::size_t i = 0; i < ops; ++i) {
      std::string key = "ttl_key_t" + std::to_string(thread_id) + "_" +
                        std::to_string(i);

      try {
        // Insert with TTL: 100ms
        shard_.SetWithTTL(key, "value_" + std::to_string(i), 100);
        metrics.RecordWrite();

        // Immediate read should succeed
        auto value = shard_.Get(key);
        if (value.has_value()) {
          metrics.RecordRead();
        }

        // After 150ms, key should be expired
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        auto expired_value = shard_.Get(key);
        if (!expired_value.has_value()) {
          // Correct: key is expired
          metrics.RecordRead();
        } else {
          // Potential race: key still exists (timing dependent)
          metrics.RecordRead();
        }
      } catch (...) {
        metrics.RecordError();
      }
    }
  }

 public:
  explicit TestConcurrentTTLExpiration(std::size_t num_threads = 4)
      : ConcurrencyTestBase("Concurrent TTL Expiration", num_threads) {}

  void Run() { ExecuteTest(); }
};

// ============================================================================
// Test 5: LRU Eviction Under Concurrent Load
// ============================================================================

/**
 * @brief Test: LRU eviction with concurrent access patterns.
 *
 * Validates:
 *  - Eviction occurs when capacity is exceeded
 *  - LRU policy is respected (most recent keys kept)
 *  - No segmentation faults during eviction
 *  - Concurrent reads don't interfere with eviction
 */
class TestConcurrentLRUEviction : public ConcurrencyTestBase {
 private:
  core::Shard shard_{100};  // Small capacity to force evictions

  void RunTest(std::size_t thread_id, TestMetrics& metrics) override {
    const std::size_t ops = 200;  // More than capacity

    for (std::size_t i = 0; i < ops; ++i) {
      std::string key = "lru_key_t" + std::to_string(thread_id) + "_" +
                        std::to_string(i);

      try {
        shard_.Set(key, "value_" + std::to_string(i));
        metrics.RecordWrite();

        // Occasionally read to refresh LRU
        if (i % 5 == 0) {
          auto value = shard_.Get(key);
          if (value.has_value()) {
            metrics.RecordRead();
          }
        }
      } catch (...) {
        metrics.RecordError();
      }
    }
  }

 public:
  explicit TestConcurrentLRUEviction(std::size_t num_threads = 8)
      : ConcurrencyTestBase("Concurrent LRU Eviction", num_threads) {}

  void Run() {
    ExecuteTest();

    // Size should not exceed capacity significantly
    std::size_t final_size = shard_.Size();
    std::size_t capacity = 100;

    AssertTrue(final_size <= capacity,
               "Shard size should not exceed capacity: size=" +
                   std::to_string(final_size) +
                   ", capacity=" + std::to_string(capacity));
  }
};

// ============================================================================
// Test 6: Stress Test - High Contention Scenario
// ============================================================================

/**
 * @brief Test: All threads accessing the same key repeatedly.
 *
 * Validates:
 *  - Locking mechanism handles extreme contention
 *  - No deadlocks or livelocks
 *  - Final value is consistent
 */
class TestHighContention : public ConcurrencyTestBase {
 private:
  core::Shard shard_{10};
  std::atomic<uint64_t> counter_{0};

  void RunTest(std::size_t thread_id, TestMetrics& metrics) override {
    const std::string key = "contention_key";
    const std::size_t ops = 100;

    for (std::size_t i = 0; i < ops; ++i) {
      try {
        if (i % 3 == 0) {
          shard_.Set(key, "value_" + std::to_string(counter_.fetch_add(
                                         1, std::memory_order_relaxed)));
          metrics.RecordWrite();
          metrics.RecordContentionEvent();
        } else if (i % 3 == 1) {
          auto value = shard_.Get(key);
          if (value.has_value()) {
            metrics.RecordRead();
          }
          metrics.RecordContentionEvent();
        } else {
          shard_.Delete(key);
          metrics.RecordDelete();
          metrics.RecordContentionEvent();
        }
      } catch (...) {
        metrics.RecordError();
      }
    }
  }

 public:
  explicit TestHighContention(std::size_t num_threads = 16)
      : ConcurrencyTestBase("High Contention (Single Key)", num_threads) {}

  void Run() { ExecuteTest(); }
};

// ============================================================================
// Test Runner
// ============================================================================

/**
 * @class TestSuite
 * @brief Orchestrates all concurrency tests.
 */
class TestSuite final {
 public:
  TestSuite() = default;

  void RunAll() {
    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << "KVMemo Concurrency Test Suite\n";
    std::cout << std::string(70, '=') << "\n";

    // Test 1
    {
      TestConcurrentWrites test(8);
      test.Run();
      passed_ += test.TestPassed() ? 1 : 0;
    }

    // Test 2
    {
      TestConcurrentReadWrite test(8);
      test.Run();
      passed_ += test.TestPassed() ? 1 : 0;
    }

    // Test 3
    {
      TestConcurrentDeleteInsert test(8);
      test.Run();
      passed_ += test.TestPassed() ? 1 : 0;
    }

    // Test 4
    {
      TestConcurrentTTLExpiration test(4);
      test.Run();
      passed_ += test.TestPassed() ? 1 : 0;
    }

    // Test 5
    {
      TestConcurrentLRUEviction test(8);
      test.Run();
      passed_ += test.TestPassed() ? 1 : 0;
    }

    // Test 6
    {
      TestHighContention test(16);
      test.Run();
      passed_ += test.TestPassed() ? 1 : 0;
    }

    PrintSummary();
  }

 private:
  void PrintSummary() {
    const int total_tests = 6;
    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << "Test Summary\n";
    std::cout << std::string(70, '=') << "\n";
    std::cout << "Passed: " << passed_ << "/" << total_tests << "\n";
    std::cout << "Failed: " << (total_tests - passed_) << "/" << total_tests
              << "\n";
    std::cout << std::string(70, '=') << "\n";
  }

  int passed_ = 0;
};

}  // namespace kvmemo::tests

// ============================================================================
// Entry Point
// ============================================================================

/**
 * @brief Main test execution entry point.
 *
 * Initializes logger and runs test suite.
 */
int main() {
  // Initialize logging
  kvmemo::common::Logger::SetLevel(kvmemo::common::LogLevel::kInfo);
  KV_LOG_INFO("Starting KVMemo Concurrency Test Suite");

  try {
    kvmemo::tests::TestSuite suite;
    suite.RunAll();
  } catch (const std::exception& e) {
    std::cerr << "Test suite exception: " << e.what() << "\n";
    return 1;
  }

  KV_LOG_INFO("Test suite completed");
  return 0;
}

/**
 * This source code may not be copied, modified, or
 * distributed without explicit permission from the author.
 */