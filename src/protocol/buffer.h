#pragma once

/**
 * @file buffer.h
 * @brief Dynamic byte buffer used for network protocol parsing.
 *
 * Responsibilities :
 *  - Store raw incoming/outgoing bytes
 *  - Provide efficient append operations
 *  - Allow incremental reads during protocol parsing
 *  - Support consumption of processed bytes
 *
 * Thread Safety :
 *  - NOT thread-safe
 *  - Each connection owns its buffer instance
 *
 *  Copyright © 2026
 *  Author: Gagan Bansal
 *  ALL RIGHTS RESERVED. 
 */

#include <vector>
#include <string>
#include <cstring>
#include <stdexcept>

namespace kvmemo::protocol {

class Buffer final {
public:
    Buffer() = default;

    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    Buffer(Buffer&&) noexcept = default;
    Buffer& operator=(Buffer&&) noexcept = default;

    ~Buffer() = default;

    /**
     * @brief Appends raw bytes into the buffer.
     */
    void Append(const char* data, std::size_t len) {
        storage_.insert(storage_.end(), data, data + len);
    }

    /**
     * @brief Appends a string to buffer.
     */
    void Append(const std::string& data) {
        storage_.insert(storage_.end(), data.begin(), data.end());
    }

    /**
     * @brief Returns pointer to readable data.
     */
    const char* Data() const noexcept {
        return storage_.data() + read_pos_;
    }

    /**
     * @brief Number of readable bytes.
     */
    std::size_t ReadableBytes() const noexcept {
        return storage_.size() - read_pos_;
    }

    /**
     * @brief Consumes bytes that have been processed.
     */
    void Consume(std::size_t len) {
        if (len > ReadableBytes()) {
            throw std::out_of_range("Buffer consume beyond readable data");
        }

        read_pos_ += len;

        // reclaim memory when fully consumed
        if (read_pos_ == storage_.size()) {
            storage_.clear();
            read_pos_ = 0;
        }
    }

    /**
     * @brief Clears entire buffer.
     */
    void Clear() noexcept {
        storage_.clear();
        read_pos_ = 0;
    }

private:
    std::vector<char> storage_;
    std::size_t read_pos_ = 0;
};

} // namespace kvmemo::protocol


/**
 * This source code may not be copied, modified, or
 * distributed without explicit permission from the author.
 */