#pragma once
/**
 * @file connection.h
 * @brief Represents a single TCP client connection.
 *
 *  Responsibilities :
 *  - Own the client socket descriptor.
 *  - Manage input/output network buffers.
 *  - Provide APIs for reading and writing data.
 *  - Track connection lifecycle.
 *
 *  Thread Safety :
 *  > Not thread-safe.
 *  > Intended to be owned by a single event-loop thread.
 *
 *  Copyright © 2026
 *  Author: Gagan Bansal
 *  ALL RIGHTS RESERVED.
 */

#include <unistd.h>
#include <string>
#include <stdexcept>

#include "../protocol/buffer.h"

namespace kvmemo::net
{
    /**
     * @brief Represents a single TCP client connection.
     */
    class Connection final
    {
    public:
        explicit Connection(int fd) : fd_(fd) {}

        Connection(const Connection &) = delete;
        Connection &operator=(const Connection &) = delete;

        Connection(Connection &&) noexcept = default;
        Connection &operator=(Connection &&) noexcept = default;

        ~Connection()
        {
            Close();
        }

        /**
         * @brief Returns socket file descriptor.
         */
        int FD() const noexcept
        {
            return fd_;
        }

        /**
         * @brief Returns input buffer.
         */
        protocol::Buffer &InputBuffer() noexcept
        {
            return input_buffer_;
        }

        /**
         * @brief Returns output buffer.
         */
        protocol::Buffer &OutputBuffer() noexcept
        {
            return output_buffer_;
        }

        /**
         * @brief Reads available data from socket into input buffer.
         */
        ssize_t ReadFromSocket()
        {
            char temp[4096];

            size_t bytes = ::read(fd_, temp, sizeof(temp));

            if (bytes > 0)
            {
                input_buffer_.Append(temp, static_cast<std::size_t>(bytes));
            }

            return bytes;
        }

        /**
         * @brief Writes buffered data to socket.
         */
        size_t WriteToSocket()
        {
            const char *data = output_buffer_.Data();
            std::size_t size = output_buffer_.ReadableBytes();

            ssize_t bytes = ::write(fd_, data, size);

            if (bytes > 0)
            {
                output_buffer_.Consume(static_cast<std::size_t>(bytes));
            }

            return bytes;
        }

        /**
         * @brief Closes the connection socket.
         */
        void Close()
        {
            if (fd_ >= 0)
            {
                ::close(fd_);
                fd_ = -1;
            }
        }

    private:
        int fd_{-1};

        protocol::Buffer input_buffer_;
        protocol::Buffer output_buffer_;
    };
} // namespace kvmemo::net

/**
 * This source code may not be copied, modified, or
 * distributed without explicit permission from the author.
 */