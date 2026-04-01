/**
 * @file Handler.hpp
 * @brief cnerium::server — Route handler callable definition
 *
 * @version 0.1.0
 * @author Gaspard Kirira
 * @copyright (c) 2026 Gaspard Kirira
 * @license MIT
 *
 * @details
 * Defines the primary handler callable type used by the Cnerium server
 * to process matched HTTP routes.
 *
 * A Handler receives:
 *   - a Context object representing the current request lifecycle
 *
 * Responsibilities:
 *   - inspect the incoming request
 *   - read matched route parameters
 *   - build the outgoing response
 *
 * Design goals:
 *   - Simple and expressive API
 *   - Header-only
 *   - Easy lambda integration
 *   - Deterministic synchronous execution
 *
 * Notes:
 *   - A handler is executed after routing succeeds
 *   - A handler may fully build the response or leave defaults unchanged
 *   - Middleware execution typically happens before the final handler body
 *
 * Usage:
 * @code
 *   using namespace cnerium::server;
 *
 *   Handler hello = [](Context &ctx)
 *   {
 *     ctx.response().text("Hello from Cnerium");
 *   };
 *
 *   Handler show_user = [](Context &ctx)
 *   {
 *     auto id = ctx.params().get("id");
 *     ctx.response().text("User: " + std::string(id));
 *   };
 * @endcode
 */

#pragma once

#include <functional>

#include <cnerium/server/Context.hpp>

namespace cnerium::server
{
  /**
   * @brief Primary route handler callable type.
   *
   * Signature:
   *   void(Context&)
   */
  using Handler = std::function<void(Context &)>;

} // namespace cnerium::server
