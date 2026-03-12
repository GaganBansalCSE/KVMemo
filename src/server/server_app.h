#pragma once
/**
 *  Copyright © 2026 KVMemo
 *  Author: Gagan Bansal
 *  ALL RIGHTS RESERVED.
 */
#include <string>
#include <stdexcept>

#include "../net/tcp_server.h"
#include "../protocol/framing.h"
#include "../protocol/parser.h"
#include "../protocol/serializer.h"
#include "../protocol/response.h"
#include "../core/kv_engine.h"
#include "dispatcher.h"

namespace kvmemo::server
{
    /**
     * @brief Main server application
     */

    class ServerApp final
    {
    public:
        explicit ServerApp(int port) : server_(port), engine_(std::make_unique<core::ShardManager>(),
                                                              std::make_unique<core::TTLIndex>(),
                                                              std::make_unique<eviction::EvictionManager>()),
                                       dispatcher_(engine_) {}

        ServerApp(const ServerApp &) = delete;
        ServerApp &operator=(const ServerApp &) = delete;

        ServerApp(ServerApp &&) noexcept = default;
        ServerApp &operator=(ServerApp &&) noexcept = default;

        ~ServerApp() = default;

        /**
         * @brief Starts the server runtime loop.
         */
        void Run()
        {
            server_.Start();

            while (true)
            {
                server_.Accept();
                ProcessConnections();
            }
        }

    private:
        /**
         * @brief Processes requests for all active connections.
         */
        void ProcessConnections()
        {
            auto &manager = server_.Connection();

            for (int fd = 0; fd < 65536; fd++)
            {
                ConnectionSafeProcess(manager, fd);
            }
        }

        void ConnectionSafeProcess(net::ConnectionManager &manager, int fd)
        {
            try
            {
                auto *conn = manager.Get(fd);

                if (!conn)
                {
                    return;
                }

                if (conn->ReadFromSocket() <= 0)
                {
                    manager.Remove(fd);
                    return;
                }

                std::string frame;

                while (protocol::Framing::NextFrame(conn->InputBuffer(), frame))
                {
                    auto request = protocol::Parser::Parse(frame);

                    protocol::Response response = dispatcher_.Dispatch(request);

                    std::string wire = protocol::Serializer::Serialize(response);

                    conn->OutputBuffer().Append(wire);

                    conn->WriteToSocket();
                }
            }
            catch (...)
            {
                manager.Remove(fd);
            }
        }

    private:
        Dispatcher dispatcher_;
        net::TcpServer server_;
        core::KVEngine engine_;
    };
} // namespace kvmemo::server

/**
 * This source code may not be copied, modified, or
 * distributed without explicit permission from the author.
 */