#pragma once
/**
 * @file dispatcher.h
 * @brief Routes protocol requests to KVEngine operations.
 *
 * Responsibilities :
 * - Interpret parsed client requests.
 * - Map commands to KVEngine operations.
 * - Produce protocol responses.
 *
 * Thread Safety :
 *  > Thread-safe if KVEngine is thread-safe.
 *  > Stateless dispatcher.
 *
 *  Copyright © 2026 KVMemo
 *  Author: Gagan Bansal
 *  ALL RIGHTS RESERVED.
 */

#include <string>
#include <optional>
#include <stdexcept>

#include "../protocol/request.h"
#include "../protocol/response.h"
#include "../core/kv_engine.h"

namespace kvmemo::server
{
    class Dispatcher final
    {
    public:
        /**
         * @brief Constructs dispatcher with KV engine dependency.
         */
        explicit Dispatcher(core::KVEngine &engine) : engine_(engine) {}

        Dispatcher(const Dispatcher &) = delete;
        Dispatcher &operator=(const Dispatcher &) = delete;

        Dispatcher(Dispatcher &&) noexcept = default;
        Dispatcher &operator=(Dispatcher &&) noexcept = delete;

        ~Dispatcher() = default;

        /**
         * @brief Dispatch request to correct engine command.
         */
        protocol::Response Dispatch(const protocol::Request &request)
        {
            if (request.Empty())
            {
                return protocol::Response::Error("Empty Command");
            }

            const std::string &cmd = request.Command();

            if (cmd == "SET")
            {
                return HandleSet(request);
            }

            if (cmd == "GET")
            {
                return HandleGet(request);
            }

            if (cmd == "DEL")
            {
                return HandleDelete(request);
            }

            if (cmd == "SETEX")
            {
                return HandleSetEx(request);
            }

            return protocol::Response::Error("Unknown command");
        }

    private:
        protocol::Response HandleSet(const protocol::Request &req)
        {
            if (req.ArgCount() < 2)
            {
                return protocol::Response::Error("SET requires key and value");
            }

            const std::string &key = req.Arg(0);
            const std::string &value = req.Arg(1);

            engine_.Set(key, value);

            return protocol::Response::Ok();
        }

        protocol::Response HandleGet(const protocol::Request &req)
        {
            if (req.ArgCount() < 1)
            {
                return protocol::Response::Error("Get requires key");
            }

            const std::string &key = req.Arg(0);

            auto value = engine_.Get(key);

            if (!value.has_value())
            {
                return protocol::Response::Error("Key not found");
            }

            return protocol::Response::Ok(value.value());
        }

        protocol::Response HandleSetEx(const protocol::Request &req)
        {
            if (req.ArgCount() < 3)
            {
                return protocol::Response::Error("SETEX requires key, ttl_ms and value");
            }

            const std::string &key = req.Arg(0);
            const std::string &ttl_str = req.Arg(1);
            const std::string &value = req.Arg(2);

            uint64_t ttl_ms = 0;
            try
            {
                if (!ttl_str.empty() && ttl_str[0] == '-')
                {
                    return protocol::Response::Error("SETEX ttl_ms must be a positive integer");
                }
                unsigned long long parsed = std::stoull(ttl_str);
                if (parsed == 0)
                {
                    return protocol::Response::Error("SETEX ttl_ms must be a positive integer");
                }
                ttl_ms = static_cast<uint64_t>(parsed);
            }
            catch (const std::exception &)
            {
                return protocol::Response::Error("SETEX ttl_ms must be a valid integer");
            }

            engine_.Set(key, value, ttl_ms);

            return protocol::Response::Ok();
        }

        protocol::Response HandleDelete(const protocol::Request &req)
        {
            if (req.ArgCount() < 1)
            {
                return protocol::Response::Error("DEL requires key");
            }

            const std::string &key = req.Arg(0);

            engine_.Delete(key);

            return protocol::Response::Ok();
        }

    private:
        core::KVEngine &engine_;
    };
} // namespace kvmemo::server

/**
 * This source code may not be copied, modified, or
 * distributed without explicit permission from the author.
 */