#pragma once
/**
 * @file command_handler.h
 * @brief Abstract interface for server command handlers.
 * 
 * Responsibilities : 
 * - Define the execution interface for commands.
 * - Encapsulate command logic into separate classes.
 * - Provide extensibility for new commands.
 * 
 * Thread Safety :
 * > Depends on KVEngine implementation.
 * > Handlers themselves are stateless.
 * 
 *  Copyright © 2026
 *  Author: Gagan Bansal
 *  ALL RIGHTS RESERVED.
 */

#include "../protocol/request.h"
#include "../protocol/response.h"
#include "../core/kv_engine.h"

namespace kvmemo::server
{
    /**
     * @brief Abstarct command execution interface.
     */
    class CommandHandler {
        public:
        CommandHandler() = default;

        CommandHandler(const CommandHandler&) = delete;
        CommandHandler& operator=(const CommandHandler&) = delete;

        CommandHandler(CommandHandler&&) noexcept = default;
        CommandHandler& operator=(CommandHandler&&) noexcept = default;

        virtual ~CommandHandler() = default;

        /**
         * @brief Executes the command.
         * 
         * @param request Parsed client request.
         * @param engine KVEngine instance
         * 
         * @return protocol::Response
         */
        virtual protocol::Response Execute(
            const protocol::Request& request,
            core::KVEngine& engine
        ) = 0;
    };
} // namespace kvmemo::server


/**
 * This source code may not be copied, modified, or
 * distributed without explicit permission from the author.
 */