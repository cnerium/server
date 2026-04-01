/**
 * @file server.hpp
 * @brief cnerium::server — Main public header for the Server module
 *
 * @version 0.1.0
 * @author Gaspard Kirira
 * @copyright (c) 2026 Gaspard Kirira
 * @license MIT
 *
 * @details
 * Aggregates the main public headers of the Cnerium Server module.
 *
 * Include this file when you want access to the full high-level server API:
 *   - version information
 *   - server configuration
 *   - request execution context
 *   - route handler definition
 *   - error handler definition
 *   - default not-found handler
 *   - main server class
 *
 * Notes:
 *   - Internal networking and parsing details are not exposed here
 *   - This is the preferred include for end users of the module
 *
 * Usage:
 * @code
 *   #include <cnerium/server/server.hpp>
 *
 *   using namespace cnerium::server;
 *
 *   Server server;
 *
 *   server.get("/", [](Context &ctx)
 *   {
 *     ctx.response().text("Hello from Cnerium");
 *   });
 *
 *   server.listen();
 * @endcode
 */

#pragma once

#include <cnerium/server/version.hpp>
#include <cnerium/server/Config.hpp>
#include <cnerium/server/Context.hpp>
#include <cnerium/server/Handler.hpp>
#include <cnerium/server/ErrorHandler.hpp>
#include <cnerium/server/not_found.hpp>
#include <cnerium/server/Server.hpp>
