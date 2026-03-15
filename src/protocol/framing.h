#pragma once
/**
 * @file framing.h
 * @brief Extracts complete protocol frames from the network buffer.
 *
 *  Responsibilities :
 *  - Identity complete command frames inside TCP byte stream
 *  - Extract command strings from protocol buffer.
 *  - Support incremental network reads.
 *
 *  Thread Safety :
 *  > Not Thread-Safe.
 *  > Intended to be used per connection.
 *
 *  Copyright © 2026
 *  Author: Gagan Bansal
 *  ALL RIGHT RESERVED.
 */

#include <string>
#include<algorithm>

#include "buffer.h"

namespace kvmemo::protocol
{
    /**
     * @brief Handles extraction of protocol frames from a buffer.
     */
    class Framing final
    {
    public:
        Framing() = delete;
        ~Framing() = delete;

        Framing(const Framing &) = delete;
        Framing &operator=(const Framing &) = delete;

        /**
         * @brief Extracts next complete command frame from buffer.
         *
         * @param buffer Network buffer containing incoming data.
         * @param frame Output command frame.
         *
         * @return true if a complete frame was extracted.
         * @return false if more data is required.
         */

        static bool NextFrame(Buffer &buffer, std::string &frame)
        {
            static constexpr char kDelimiter[] = "\r\n";

            const char *begin = buffer.Data();
            const char *end = begin + buffer.ReadableBytes();

            const char *pos = std::search(
                begin,
                end,
                std::begin(kDelimiter),
                std::end(kDelimiter) - 1
            );

            if (pos == end)
            {
                return false;
            }

            std::size_t frame_len = pos - begin;

            frame.assign(begin, frame_len);

            buffer.Consume(frame_len + 2);

            return true;
        }
    };
} // namespace kvmemo::protocol

/**
 * This source code may not be copied, modified, or
 * distributed without explicit permission from the author.
 */