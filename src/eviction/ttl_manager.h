#pragma once
/**
 * @file ttl_manager.h
 * @brief Manages TTL-based expiration workflow in KVMemo.
 * 
 * Responsibility :
 *  TTLManager acts as the orchestration layer for time-based expiration.
 * 
 *  IT : 
 *  - Pulls expired keys from TTLIndex.
 *  - Delegates deletion to ShardManager.
 *  - Runs expiration cycle (sync or background driven).
 * 
 *  IT DOES NOT : 
 *  - Track memory usage.
 *  - Handle LRU eviction.
 *  - Store actual key-value entries.
 *  
 *  Thread Safety : 
 *  > Thread-Safe assuming underlying components are thread-safe.
 * 
 *  Copyright © 2026 Gagan Bansal
 *  ALL RIGHT RESERVED
 */

#include <vector>
#include <string>
#include <atomic>

namespace kvmemo::eviction {
    class TTLIndex;
    class ShardManager;

    class TTLManager {
        public:
            /**
             * @brief Constructs TTLManager.
             * 
             * @param ttl_index References to global TTL index.
             * @param shard_manager References to shard manager.
             */
        TTLManager(TTLIndex& ttl_index, ShardManager& shard_manager) noexcept;

        /**
         * @brief Runs one expiration cycle.
         * 
         * - fetches expired keys from TTLIndex
         * - Deletes them via ShardManager.
         * 
         * @return Number of keys expired.
         */
        size_t run_expiration_cycle();

        /**
         * @brief Enables or disables TTL processing.
         */
        void set_enabled(bool enabled) noexcept;

        /**
         * @brief Returns whether TTL expiration is enabled.
         */
        bool is_enabled() const noexcept;

        private:
        TTLIndex& ttl_index_;
        ShardManager& shard_manager_;

        std::atomic<bool> enabled_{true};
    };
} // namespace kvmemo::eviction

/**
 * This source code may not be copied, modified, or
 * distributed without explicit permission from the author.
 */