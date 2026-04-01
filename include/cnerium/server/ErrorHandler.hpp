/**
 * @file ErrorHandler.hpp
 * @brief cnerium::server — Error handling callable definition
 *
 * @version 0.2.0
 * @author Gaspard Kirira
 * @copyright (c) 2026 Gaspard Kirira
 * @license MIT
 *
 * @details
 * Defines the error handler callable used by the Cnerium server
 * to process exceptions and unexpected failures during request handling.
 *
 * An ErrorHandler receives:
 *   - a Context object representing the current request lifecycle
 *   - the exception that occurred
 *
 * Responsibilities:
 *   - convert exceptions into HTTP responses
 *   - ensure a valid response is always produced
 *   - prevent crashes from propagating to the server loop
 *
 * Design goals:
 *   - Simple and expressive API
 *   - Safe default behavior
 *   - Easy override by users
 *   - No dependency on transport layer
 *   - Consistent default framework error shape
 *
 * Notes:
 *   - Called when a handler or middleware throws
 *   - Should always produce a valid HTTP response
 *   - Default implementation returns a 500 Internal Server Error
 *   - Default implementation emits JSON for consistency with the framework
 *
 * Usage:
 * @code
 *   using namespace cnerium::server;
 *
 *   ErrorHandler handler = [](Context &ctx, const std::exception &e)
 *   {
 *     ctx.response().set_status(cnerium::http::Status::internal_server_error);
 *     ctx.response().json({
 *       {"ok", false},
 *       {"error", "Something went wrong"},
 *       {"details", e.what()},
 *       {"framework", "cnerium"}
 *     });
 *   };
 * @endcode
 */

#pragma once

#include <exception>
#include <functional>

#include <cnerium/http/Status.hpp>
#include <cnerium/server/Context.hpp>

namespace cnerium::server
{
  /**
   * @brief Primary error handler callable type.
   *
   * Signature:
   *   void(Context&, const std::exception&)
   */
  using ErrorHandler = std::function<void(Context &, const std::exception &)>;

  /**
   * @brief Default error handler implementation.
   *
   * Produces a generic 500 Internal Server Error response
   * using the default framework JSON error shape.
   *
   * @param ctx Server context
   * @param e   Thrown exception
   */
  inline void default_error_handler(Context &ctx, const std::exception &e)
  {
    ctx.response().set_status(cnerium::http::Status::internal_server_error);
    ctx.response().json({{"ok", false},
                         {"error", "Internal Server Error"},
                         {"details", e.what()},
                         {"framework", "cnerium"}});
  }

} // namespace cnerium::server
