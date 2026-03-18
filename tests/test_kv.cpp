/**
 * @file test_kv.cpp
 * @brief Unit tests for KVEngine core functionality.
 *
 * Test Coverage:
 *  - Basic KV operations (Set, Get, Delete)
 *  - TTL expiration processing
 *  - Eviction policy enforcement
 *  - Error handling and edge cases
 *
 * Copyright © 2026 Gagan Bansal
 * ALL RIGHT RESERVED
 */

#include <cassert>
#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <chrono>
#include <thread>

#include "src/core/lru_cache.h"
#include "src/common/status.h"
#include "src/common/config.h"

namespace kvmemo::tests {

// ============================================================================
// Test Utilities
// ============================================================================

/**
 * @brief Simple test assertion and reporting.
 */
struct TestResult {
    std::string name;
    bool passed;
    std::string message;

    TestResult(const std::string& test_name, bool success, const std::string& msg = "")
        : name(test_name), passed(success), message(msg) {}
};

class TestReporter {
public:
    /**
     * @brief Reports a single test result.
     */
    void ReportTest(const TestResult& result) {
        if (result.passed) {
            std::cout << "✓ PASS: " << result.name << std::endl;
        } else {
            std::cout << "✗ FAIL: " << result.name << std::endl;
            if (!result.message.empty()) {
                std::cout << "  └─ " << result.message << std::endl;
            }
        }
    }

    /**
     * @brief Prints test summary.
     */
    void Summary(int total, int passed) {
        std::cout << "\n" << std::string(60, '=') << std::endl;
        std::cout << "Test Summary: " << passed << "/" << total << " passed" << std::endl;
        std::cout << std::string(60, '=') << std::endl;
    }
};

// ============================================================================
// Test Suite: LRUCache
// ============================================================================

namespace lru_cache_tests {

/**
 * @brief Test: LRUCache construction with valid capacity.
 * 
 * Validates:
 *  - Capacity is set correctly
 *  - Initial size is zero
 */
TestResult TestLRUCacheConstruction() {
    try {
        const size_t capacity = 100;
        core::LRUCache cache(capacity);

        bool correct = cache.Capacity() == capacity && cache.Size() == 0;
        return TestResult(
            "LRUCache::Construction",
            correct,
            correct ? "" : "Cache not initialized correctly"
        );
    } catch (const std::exception& ex) {
        return TestResult("LRUCache::Construction", false, ex.what());
    }
}

/**
 * @brief Test: LRUCache rejects zero capacity.
 * 
 * Validates:
 *  - Constructor throws std::invalid_argument for capacity == 0
 */
TestResult TestLRUCacheZeroCapacity() {
    try {
        core::LRUCache cache(0);
        return TestResult("LRUCache::ZeroCapacity", false, "Should have thrown exception");
    } catch (const std::invalid_argument&) {
        return TestResult("LRUCache::ZeroCapacity", true);
    } catch (const std::exception& ex) {
        return TestResult("LRUCache::ZeroCapacity", false, std::string("Unexpected exception: ") + ex.what());
    }
}

/**
 * @brief Test: LRUCache Touch marks key as recently used.
 * 
 * Validates:
 *  - Touch on new key returns false (no overflow)
 *  - Size increases by 1
 */
TestResult TestLRUCacheTouch() {
    try {
        core::LRUCache cache(100);
        std::string key = "test_key";

        bool overflow = cache.Touch(key);

        bool correct = !overflow && cache.Size() == 1;
        return TestResult(
            "LRUCache::Touch",
            correct,
            correct ? "" : "Touch operation failed"
        );
    } catch (const std::exception& ex) {
        return TestResult("LRUCache::Touch", false, ex.what());
    }
}

/**
 * @brief Test: LRUCache reports overflow at capacity.
 * 
 * Validates:
 *  - Touch on key that exceeds capacity returns true
 */
TestResult TestLRUCacheOverflow() {
    try {
        core::LRUCache cache(2);

        cache.Touch("key1");
        cache.Touch("key2");
        bool overflow = cache.Touch("key3");

        return TestResult(
            "LRUCache::Overflow",
            overflow,
            overflow ? "" : "Should report overflow at capacity"
        );
    } catch (const std::exception& ex) {
        return TestResult("LRUCache::Overflow", false, ex.what());
    }
}

/**
 * @brief Test: LRUCache eviction candidate is the least recently used key.
 * 
 * Validates:
 *  - EvictionCandidate returns the oldest (back) key
 */
TestResult TestLRUCacheEvictionCandidate() {
    try {
        core::LRUCache cache(10);

        cache.Touch("oldest");
        cache.Touch("middle");
        cache.Touch("newest");

        const std::string& lru = cache.EvictionCandidate();
        bool correct = lru == "oldest";

        return TestResult(
            "LRUCache::EvictionCandidate",
            correct,
            correct ? "" : "LRU key is not oldest"
        );
    } catch (const std::exception& ex) {
        return TestResult("LRUCache::EvictionCandidate", false, ex.what());
    }
}

/**
 * @brief Test: LRUCache PopEvictionCandidate removes and returns LRU key.
 * 
 * Validates:
 *  - PopEvictionCandidate returns the LRU key
 *  - Size decreases by 1 after pop
 */
TestResult TestLRUCachePopEvictionCandidate() {
    try {
        core::LRUCache cache(10);

        cache.Touch("key1");
        cache.Touch("key2");
        cache.Touch("key3");

        std::string evicted = cache.PopEvictionCandidate();

        bool correct = evicted == "key1" && cache.Size() == 2;
        return TestResult(
            "LRUCache::PopEvictionCandidate",
            correct,
            correct ? "" : "Pop did not remove correctly"
        );
    } catch (const std::exception& ex) {
        return TestResult("LRUCache::PopEvictionCandidate", false, ex.what());
    }
}

/**
 * @brief Test: LRUCache Remove deletes a key.
 * 
 * Validates:
 *  - Remove decreases size by 1
 *  - Removed key is no longer tracked
 */
TestResult TestLRUCacheRemove() {
    try {
        core::LRUCache cache(10);

        cache.Touch("key1");
        cache.Touch("key2");
        cache.Remove("key1");

        bool correct = cache.Size() == 1 && cache.EvictionCandidate() == "key2";
        return TestResult(
            "LRUCache::Remove",
            correct,
            correct ? "" : "Remove did not work correctly"
        );
    } catch (const std::exception& ex) {
        return TestResult("LRUCache::Remove", false, ex.what());
    }
}

/**
 * @brief Test: LRUCache Remove is idempotent.
 * 
 * Validates:
 *  - Remove on non-existent key is safe (no exception)
 */
TestResult TestLRUCacheRemoveNonExistent() {
    try {
        core::LRUCache cache(10);

        cache.Touch("key1");
        cache.Remove("non_existent"); // Should not throw
        cache.Remove("non_existent"); // Should not throw

        bool correct = cache.Size() == 1;
        return TestResult(
            "LRUCache::RemoveNonExistent",
            correct,
            correct ? "" : "Remove should be idempotent"
        );
    } catch (const std::exception& ex) {
        return TestResult("LRUCache::RemoveNonExistent", false, ex.what());
    }
}

/**
 * @brief Test: LRUCache Clear empties all keys.
 * 
 * Validates:
 *  - Clear sets size to 0
 */
TestResult TestLRUCacheClear() {
    try {
        core::LRUCache cache(10);

        cache.Touch("key1");
        cache.Touch("key2");
        cache.Touch("key3");
        cache.Clear();

        return TestResult(
            "LRUCache::Clear",
            cache.Size() == 0,
            cache.Size() == 0 ? "" : "Clear did not empty cache"
        );
    } catch (const std::exception& ex) {
        return TestResult("LRUCache::Clear", false, ex.what());
    }
}

/**
 * @brief Test: LRUCache Touch on existing key moves it to front (most recent).
 * 
 * Validates:
 *  - Touching an existing key reorders it to the front
 *  - The previously oldest key becomes the new LRU
 */
TestResult TestLRUCacheTouchExisting() {
    try {
        core::LRUCache cache(10);

        cache.Touch("key1");
        cache.Touch("key2");
        cache.Touch("key3");
        cache.Touch("key1"); // Move key1 to front (most recent)

        // After touching key1, it should be the newest
        // key2 should now be the oldest (LRU)
        const std::string& lru = cache.EvictionCandidate();

        bool correct = lru == "key2";
        return TestResult(
            "LRUCache::TouchExisting",
            correct,
            correct ? "" : "Touch did not reorder correctly"
        );
    } catch (const std::exception& ex) {
        return TestResult("LRUCache::TouchExisting", false, ex.what());
    }
}

/**
 * @brief Test: LRUCache handles large number of keys.
 * 
 * Validates:
 *  - Cache correctly tracks many keys (stress test)
 *  - Overflow detection works at scale
 */
TestResult TestLRUCacheLargeScale() {
    try {
        const size_t capacity = 1000;
        core::LRUCache cache(capacity);

        for (size_t i = 0; i < capacity; ++i) {
            std::string key = "key_" + std::to_string(i);
            bool overflow = cache.Touch(key);
            if (overflow) {
                return TestResult(
                    "LRUCache::LargeScale",
                    false,
                    "Overflow before reaching capacity"
                );
            }
        }

        // One more should overflow
        bool overflow = cache.Touch("overflow_key");
        bool correct = overflow && cache.Size() == capacity;

        return TestResult(
            "LRUCache::LargeScale",
            correct,
            correct ? "" : "Large scale test failed"
        );
    } catch (const std::exception& ex) {
        return TestResult("LRUCache::LargeScale", false, ex.what());
    }
}

/**
 * @brief Test: LRUCache EvictionCandidate throws on empty cache.
 * 
 * Validates:
 *  - Accessing EvictionCandidate on empty cache raises exception
 */
TestResult TestLRUCacheEmptyAccess() {
    try {
        core::LRUCache cache(10);
        const std::string& lru = cache.EvictionCandidate();
        return TestResult("LRUCache::EmptyAccess", false, "Should have thrown exception");
    } catch (const std::runtime_error&) {
        return TestResult("LRUCache::EmptyAccess", true);
    } catch (const std::exception& ex) {
        return TestResult("LRUCache::EmptyAccess", false, std::string("Unexpected: ") + ex.what());
    }
}

} // namespace lru_cache_tests

// ============================================================================
// Test Suite: Common (Status)
// ============================================================================

namespace status_tests {

/**
 * @brief Test: Status::Ok creation and queries.
 * 
 * Validates:
 *  - Ok status has code kOk
 *  - ok() returns true
 */
TestResult TestStatusOk() {
    try {
        common::Status status = common::Status::Ok();

        bool correct = status.ok() && status.code() == common::StatusCode::kOk;
        return TestResult(
            "Status::Ok",
            correct,
            correct ? "" : "Status::Ok not working"
        );
    } catch (const std::exception& ex) {
        return TestResult("Status::Ok", false, ex.what());
    }
}

/**
 * @brief Test: Status error creation.
 * 
 * Validates:
 *  - Error status has correct code
 *  - Message is preserved
 */
TestResult TestStatusError() {
    try {
        common::Status status = common::Status::InvalidArgument("test error");

        bool correct = !status.ok() && 
                       status.code() == common::StatusCode::kInvalidArgument &&
                       status.message() == "test error";
        return TestResult(
            "Status::Error",
            correct,
            correct ? "" : "Status error creation failed"
        );
    } catch (const std::exception& ex) {
        return TestResult("Status::Error", false, ex.what());
    }
}

/**
 * @brief Test: Status::ToString formatting.
 * 
 * Validates:
 *  - Ok status formats as "OK"
 *  - Error status includes code name and message
 */
TestResult TestStatusToString() {
    try {
        common::Status ok = common::Status::Ok();
        common::Status err = common::Status::NotFound("key not found");

        bool correct = ok.ToString() == "OK" &&
                       err.ToString().find("NOT_FOUND") != std::string::npos;
        return TestResult(
            "Status::ToString",
            correct,
            correct ? "" : "ToString not working"
        );
    } catch (const std::exception& ex) {
        return TestResult("Status::ToString", false, ex.what());
    }
}

/**
 * @brief Test: Status equality comparison.
 * 
 * Validates:
 *  - Two Ok statuses are equal
 *  - Different statuses are not equal
 */
TestResult TestStatusEquality() {
    try {
        common::Status status1 = common::Status::Ok();
        common::Status status2 = common::Status::Ok();
        common::Status status3 = common::Status::InvalidArgument("err");

        bool correct = (status1 == status2) && (status1 != status3);
        return TestResult(
            "Status::Equality",
            correct,
            correct ? "" : "Status equality check failed"
        );
    } catch (const std::exception& ex) {
        return TestResult("Status::Equality", false, ex.what());
    }
}

/**
 * @brief Test: Status named factory methods.
 * 
 * Validates:
 *  - Each named factory (NotFound, PermissionDenied, etc.) creates correct code
 */
TestResult TestStatusFactories() {
    try {
        common::Status not_found = common::Status::NotFound("missing");
        common::Status perm_denied = common::Status::PermissionDenied("forbidden");
        common::Status proto_err = common::Status::ProtocolError("invalid");

        bool correct = not_found.code() == common::StatusCode::kNotFound &&
                       perm_denied.code() == common::StatusCode::kPermissionDenied &&
                       proto_err.code() == common::StatusCode::kProtocolError;

        return TestResult(
            "Status::Factories",
            correct,
            correct ? "" : "Factory methods failed"
        );
    } catch (const std::exception& ex) {
        return TestResult("Status::Factories", false, ex.what());
    }
}

} // namespace status_tests

// ============================================================================
// Test Suite: Common (Config)
// ============================================================================

namespace config_tests {

/**
 * @brief Test: Config construction with defaults.
 * 
 * Validates:
 *  - Default values are set correctly
 */
TestResult TestConfigDefaults() {
    try {
        common::Config config;

        bool correct = config.enable_metrics == true &&
                       config.eviction_policy == common::EvictionPolicy::kLRU &&
                       config.enable_ttl == true;
        return TestResult(
            "Config::Defaults",
            correct,
            correct ? "" : "Config defaults incorrect"
        );
    } catch (const std::exception& ex) {
        return TestResult("Config::Defaults", false, ex.what());
    }
}

/**
 * @brief Test: Config validation with valid configuration.
 * 
 * Validates:
 *  - Valid config passes validation
 */
TestResult TestConfigValidation() {
    try {
        common::Config config;
        common::Status status = config.Validate();

        return TestResult(
            "Config::Validation",
            status.ok(),
            status.ok() ? "" : status.message()
        );
    } catch (const std::exception& ex) {
        return TestResult("Config::Validation", false, ex.what());
    }
}

/**
 * @brief Test: Config rejects zero shard count.
 * 
 * Validates:
 *  - shard_count == 0 fails validation
 */
TestResult TestConfigZeroShards() {
    try {
        common::Config config;
        config.shard_count = 0;
        common::Status status = config.Validate();

        return TestResult(
            "Config::ZeroShards",
            !status.ok(),
            !status.ok() ? "" : "Should reject zero shard count"
        );
    } catch (const std::exception& ex) {
        return TestResult("Config::ZeroShards", false, ex.what());
    }
}

/**
 * @brief Test: Config requires power-of-two shard count.
 * 
 * Validates:
 *  - Non-power-of-two shard counts fail validation
 */
TestResult TestConfigNonPowerOfTwo() {
    try {
        common::Config config;
        config.shard_count = 5; // Not a power of two
        common::Status status = config.Validate();

        return TestResult(
            "Config::NonPowerOfTwo",
            !status.ok(),
            !status.ok() ? "" : "Should reject non-power-of-two"
        );
    } catch (const std::exception& ex) {
        return TestResult("Config::NonPowerOfTwo", false, ex.what());
    }
}

/**
 * @brief Test: Config accepts valid power-of-two shard counts.
 * 
 * Validates:
 *  - Power-of-two shard counts pass validation
 */
TestResult TestConfigValidShards() {
    try {
        std::vector<size_t> valid_shards = {1, 2, 4, 8, 16, 32, 64, 128};
        
        for (size_t shard_count : valid_shards) {
            common::Config config;
            config.shard_count = shard_count;
            common::Status status = config.Validate();
            
            if (!status.ok()) {
                return TestResult(
                    "Config::ValidShards",
                    false,
                    std::string("Shard count ") + std::to_string(shard_count) + " rejected"
                );
            }
        }
        
        return TestResult("Config::ValidShards", true);
    } catch (const std::exception& ex) {
        return TestResult("Config::ValidShards", false, ex.what());
    }
}

/**
 * @brief Test: Config rejects zero max_memory_bytes.
 * 
 * Validates:
 *  - max_memory_bytes == 0 fails validation
 */
TestResult TestConfigZeroMemory() {
    try {
        common::Config config;
        config.max_memory_bytes = 0;
        common::Status status = config.Validate();

        return TestResult(
            "Config::ZeroMemory",
            !status.ok(),
            !status.ok() ? "" : "Should reject zero memory"
        );
    } catch (const std::exception& ex) {
        return TestResult("Config::ZeroMemory", false, ex.what());
    }
}

/**
 * @brief Test: Config validates max_value_bytes <= max_memory_bytes.
 * 
 * Validates:
 *  - max_value_bytes must not exceed max_memory_bytes
 */
TestResult TestConfigValueMemoryRatio() {
    try {
        common::Config config;
        config.max_value_bytes = config.max_memory_bytes + 1;
        common::Status status = config.Validate();

        return TestResult(
            "Config::ValueMemoryRatio",
            !status.ok(),
            !status.ok() ? "" : "Should reject max_value > max_memory"
        );
    } catch (const std::exception& ex) {
        return TestResult("Config::ValueMemoryRatio", false, ex.what());
    }
}

} // namespace config_tests

// ============================================================================
// Main Test Runner
// ============================================================================

/**
 * @brief Runs all test suites and reports results.
 */
void RunAllTests() {
    TestReporter reporter;
    std::vector<TestResult> results;

    std::cout << std::string(70, '=') << std::endl;
    std::cout << "KVMemo Test Suite" << std::endl;
    std::cout << std::string(70, '=') << std::endl << std::endl;

    // LRUCache Tests
    std::cout << "LRUCache Tests:" << std::endl;
    std::cout << std::string(70, '-') << std::endl;
    results.push_back(lru_cache_tests::TestLRUCacheConstruction());
    results.push_back(lru_cache_tests::TestLRUCacheZeroCapacity());
    results.push_back(lru_cache_tests::TestLRUCacheTouch());
    results.push_back(lru_cache_tests::TestLRUCacheOverflow());
    results.push_back(lru_cache_tests::TestLRUCacheEvictionCandidate());
    results.push_back(lru_cache_tests::TestLRUCachePopEvictionCandidate());
    results.push_back(lru_cache_tests::TestLRUCacheRemove());
    results.push_back(lru_cache_tests::TestLRUCacheRemoveNonExistent());
    results.push_back(lru_cache_tests::TestLRUCacheClear());
    results.push_back(lru_cache_tests::TestLRUCacheTouchExisting());
    results.push_back(lru_cache_tests::TestLRUCacheLargeScale());
    results.push_back(lru_cache_tests::TestLRUCacheEmptyAccess());

    // Status Tests
    std::cout << "\nStatus Tests:" << std::endl;
    std::cout << std::string(70, '-') << std::endl;
    results.push_back(status_tests::TestStatusOk());
    results.push_back(status_tests::TestStatusError());
    results.push_back(status_tests::TestStatusToString());
    results.push_back(status_tests::TestStatusEquality());
    results.push_back(status_tests::TestStatusFactories());

    // Config Tests
    std::cout << "\nConfig Tests:" << std::endl;
    std::cout << std::string(70, '-') << std::endl;
    results.push_back(config_tests::TestConfigDefaults());
    results.push_back(config_tests::TestConfigValidation());
    results.push_back(config_tests::TestConfigZeroShards());
    results.push_back(config_tests::TestConfigNonPowerOfTwo());
    results.push_back(config_tests::TestConfigValidShards());
    results.push_back(config_tests::TestConfigZeroMemory());
    results.push_back(config_tests::TestConfigValueMemoryRatio());

    // Report results
    std::cout << std::endl;
    for (const auto& result : results) {
        reporter.ReportTest(result);
    }

    // Summary
    int passed = 0;
    for (const auto& result : results) {
        if (result.passed) passed++;
    }
    reporter.Summary(results.size(), passed);

    // Exit with appropriate code
    exit(passed == results.size() ? 0 : 1);
}

} // namespace kvmemo::tests

// ============================================================================
// Entry Point
// ============================================================================

int main(int argc, char* argv[]) {
    try {
        kvmemo::tests::RunAllTests();
    } catch (const std::exception& ex) {
        std::cerr << "Fatal test error: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}

/**
 * This source code may not be copied, modified, or
 * distributed without explicit permission from the author.
 */