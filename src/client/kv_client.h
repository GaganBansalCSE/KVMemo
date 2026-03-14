#pragma once
/**
 * @file kv_client.h
 * @brief Simple TCP client for interacting with KVMemo server.
 *
 * Responsibilities
 *  - Establish TCP connection to server
 *  - Send commands to server
 *  - Receive responses
 *  - Provide simple KV API for users

 * Thread Safety
 *  - Not thread-safe
 *  - Intended for single-thread CLI usage
 * 
 *  Copyright © 2026
 *  Author: Gagan Bansal
 *  ALL RIGHTS RESERVED. 
 */

#include <string>
#include <optional>
#include <cstring>
#include <stdexcept>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

namespace kvmemo::client {

class KVClient final {
public:

    KVClient(std::string host, int port)
        : host_(std::move(host)),
          port_(port),
          socket_fd_(-1) {}

    KVClient(const KVClient&) = delete;
    KVClient& operator=(const KVClient&) = delete;

    KVClient(KVClient&&) noexcept = default;
    KVClient& operator=(KVClient&&) noexcept = default;

    ~KVClient()
    {
        if (socket_fd_ != -1) {
            close(socket_fd_);
        }
    }

    /**
     * @brief Establish TCP connection to server.
     */
    void Connect()
    {
        socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);

        if (socket_fd_ < 0) {
            throw std::runtime_error("Failed to create socket");
        }

        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port_);

        if (inet_pton(AF_INET, host_.c_str(), &server_addr.sin_addr) <= 0) {
            throw std::runtime_error("Invalid server address");
        }

        if (connect(socket_fd_,
                    reinterpret_cast<sockaddr*>(&server_addr),
                    sizeof(server_addr)) < 0) {
            throw std::runtime_error("Connection failed");
        }
    }

    /**
     * @brief Sends raw command to server.
     */
    std::string SendCommand(const std::string& command)
    {
        std::string wire = command + "\r\n";

        send(socket_fd_, wire.data(), wire.size(), 0);

        char buffer[4096];

        ssize_t bytes = recv(socket_fd_, buffer, sizeof(buffer) - 1, 0);

        if (bytes <= 0) {
            throw std::runtime_error("Server closed connection");
        }

        buffer[bytes] = '\0';

        return std::string(buffer);
    }

    /**
     * @brief SET key value
     */
    void Set(const std::string& key, const std::string& value)
    {
        SendCommand("SET " + key + " " + value);
    }

    /**
     * @brief GET key
     */
    std::optional<std::string> Get(const std::string& key)
    {
        auto resp = SendCommand("GET " + key);

        if (resp.rfind("+", 0) == 0) {
            return resp.substr(1);
        }

        return std::nullopt;
    }

    /**
     * @brief DEL key
     */
    void Delete(const std::string& key)
    {
        SendCommand("DEL " + key);
    }

private:

    std::string host_;
    int port_;

    int socket_fd_;
};

} // namespace kvmemo::client

/**
 * This source code may not be copied, modified, or
 * distributed without explicit permission from the author.
 */