#pragma once
/**
 * @file metrics_registry.h
 * @brief Centralized metrics collection and aggregation facility.
 *
 * MetricsRegistry provides a thread-safe mechanism to:
 *  - Register counters and gauges.
 *  - Increment/decrement metrics.
 *  - Capture runtime snapshots .
 *  - Expose aggregated metrics for monitoring/export.
 *
 *   Thread Safety :
 *  > Fully Thread-safe.
 *  > Atomic counters for hot path metrics.
 *  > Mutex-protected registry map.
 *
 *  Copyright © 2026 Gagan Bansal
 *  ALL RIGHT RESERVED
 */

#include <atomic>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace kvmemo::metrics
{
    /**
     * @brief Represents a single counter metric.
     * Thread-safe via atomic operations.
     */
    class Counter
    {
    public:
        Counter() noexcept = default;

        void increment(uint64_t value = 1) noexcept
        {
            value_.fetch_add(value, std::memory_order_relaxed);
        }

        uint64_t value() const noexcept
        {
            return value_.load(std::memory_order_relaxed);
        }

    private:
        std::atomic<uint64_t> value_{0};
    };

    /**
     * @brief Snapshot view of metrics at a point in time.
     */
    struct MetricSnapshot
    {
        std::vector<std::pair<std::string, uint64_t>> counters;
    };

    /**
     * @brief Central metrics registry.
     * Manages named counters and exposes aggregated snapshots.
     */
    class MetricsRegistry
    {
    public:
        MetricsRegistry() = default;
        MetricsRegistry(const MetricsRegistry &) = delete;
        MetricsRegistry &operator=(const MetricsRegistry &) = delete;
        ~MetricsRegistry() = delete;

        /**
         * @brief Registers a counter if not already present.
         */
        void register_counter(const std::string &name)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            counters_.emplace(name, Counter{});
        }

        /**
         * @brief Increments a counter
         * If counter does not exist, it is auto-registered.
         */
        void increment(const std::string &name, uint64_t value = 1)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (counters_.find(name) == counters_.end())
            {
                counters_.emplace(name, Counter{});
            }

            counters_[name].increment(value);
        }

        /**
         * @brief Returns a consistent snapshot of all counters.
         */
        MetricSnapshot snapshot() const
        {
            std::lock_guard<std::mutex> lock(mutex_);

            MetricSnapshot snap;
            snap.counters.reserve(counters_.size());

            for (const auto &[name, counter] : counters_)
            {
                snap.counters.emplace_back(name, counter.value());
            }

            return snap;
        }

    private:
        mutable std::mutex mutex_;
        std::unordered_map<std::string, Counter> counters_;
    };
} // namespace kvmemo::metric

/**
 * This source code may not be copied, modified, or
 * distributed without explicit permission from the author.
 */