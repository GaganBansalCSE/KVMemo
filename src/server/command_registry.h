#pragma once

/**
 * @file command_registry.h
 * @brief Registry for server command handlers.
 *
 * Responsibilities :
 *  - Store mapping of command names to handlers.
 *  - Provide lookup for command execution.
 *  - Allow command registration during server startup.
 *
 *  Thread Safety :
 *  > Not thread-safe during registration.
 *  > Read-only during server runtime.
 *
 *  Copyright © 2026
 *  Author: Gagan Bansal
 *  ALL RIGHTS RESERVED.
 */

#include <string>
#include <unordered_map>
#include <memory>
#include <stdexcept>

#include "command_handler.h"

namespace kvmemo::server
{
    class CommandRegistry final
    {
    public:
        CommandRegistry() = default;

        CommandRegistry(const CommandRegistry &) = delete;
        CommandRegistry &operator=(const CommandRegistry &) = delete;

        CommandRegistry(CommandRegistry &&) noexcept = default;
        CommandRegistry &operator=(CommandRegistry &&) noexcept = default;

        ~CommandRegistry() = default;

        /**
         * @brief Registers a command handler.
         */
        void Register(const std::string &command,
                      std::unique_ptr<CommandHandler> handler)
        {
            handlers_.emplace(command, std::move(handler));
        }

        /**
         * @brief Returns handler for command.
         */
        CommandHandler *Get(const std::string &command)
        {
            auto it = handlers_.find(command);

            if (it == handlers_.end())
            {
                return nullptr;
            }

            return it->second.get();
        }

        /**
         * @brief Check if commands exists.
         */
        bool Exists(const std::string &command) const
        {
            return handlers_.find(command) != handlers_.end();
        }

    private:
        std::unordered_map<std::string, std::unique_ptr<CommandHandler>> handlers_;
    };
} // namespace kvmemo::server

/**
 * This source code may not be copied, modified, or
 * distributed without explicit permission from the author.
 */