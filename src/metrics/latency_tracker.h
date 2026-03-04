#pragma once
/**
 * @file latency_tracker.h
 * @brief High-resolution latency measurement utility for KVMemo.
 *
 *  LatencyTracker provides precise measurement of execution time for
 *  operations such as :
 *  - Command Execution
 *  - Network processing
 *  - Storage operations
 *  - Eviction cycles
 *
 *  Thread Safety :
 *  > Lock-free fast path for recording.
 *  > Mutex-protected snapshot aggregation.
 *
 *  Copyright © 2026 Gagan Bansal
 *  ALL RIGHT RESERVED
 */

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>

namespace kvmemo::metrics
{
    /**
     * @brief Aggregated latency statistics.
     */
    struct LatencyStats
    {
        uint64_t total_operations{0};
        uint64_t total_latency_ns{0};
        uint64_t min_latency_ns{0};
        uint64_t max_latency_ns{0};

        /**
         * @brief Returns average latency in nanoseconds
         */
        uint64_t average_latency_ns() const noexcept
        {
            if (total_operations == 0)
            {
                return 0;
            }
            return total_latency_ns / total_operations;
        }
    };

    /**
     * @brief High-resolution latency tracker.
     */
    class LatencyTracker
    {
    public:
        using Clock = std::chrono::steady_clock;
        using TimePoint = Clock::time_point;

        LatencyTracker() = default;
        LatencyTracker(const LatencyTracker &) = delete;
        LatencyTracker &operator=(const LatencyTracker &) = delete;
        ~LatencyTracker() = default;

        /**
         * @brief Returns current high-resolution timestamp.
         */
        TimePoint start() const noexcept
        {
            return Clock::now();
        }

        /**
         * @brief Records latency using start time.
         */
        void stop(TimePoint start_time) noexcept
        {
            const auto end_time = Clock::now();
            const auto duration =
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    end_time - start_time)
                    .count();

            record(duration);
        }

        /**
         * @brief Returns consistent snapshot of latency statistics.
         */
        LatencyStats snapshot() const
        {
            std::lock_guard<std::mutex> lock(mutex_);

            LatencyStats stats;
            stats.total_operations = total_operations_.load(std::memory_order_relaxed);
            stats.total_latency_ns = total_latency_ns_.load(std::memory_order_relaxed);
            stats.min_latency_ns = min_latency_ns_;
            stats.max_latency_ns = max_latency_ns_;

            return stats;
        }

    private:
        void record(uint64_t latency_ns) noexcept
        {
            total_operations_.fetch_add(1, std::memory_order_relaxed);
            total_latency_ns_.fetch_add(latency_ns, std::memory_order_relaxed);

            std::lock_guard<std::mutex> lock(mutex_);
            if (min_latency_ns_ == 0 || latency_ns < min_latency_ns_)
            {
                min_latency_ns_ = latency_ns;
            }

            if (latency_ns > max_latency_ns_)
            {
                max_latency_ns_ = latency_ns;
            }
        }

    private:
        std::atomic<uint64_t> total_operations_{0};
        std::atomic<uint64_t> total_latency_ns_{0};

        mutable std::mutex mutex_;
        uint64_t min_latency_ns_{0};
        uint64_t max_latency_ns_{0};
    };
} // namespace kvmemo::metrics

/**
 * This source code may not be copied, modified, or
 * distributed without explicit permission from the author.
 */