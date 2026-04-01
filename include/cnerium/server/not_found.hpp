/**
 * @file not_found.hpp
 * @brief cnerium::server — Default 404 Not Found handler
 *
 * @version 0.1.0
 * @author Gaspard Kirira
 * @copyright (c) 2026 Gaspard Kirira
 * @license MIT
 *
 * @details
 * Defines the default handler used by the Cnerium server when
 * no route matches the incoming HTTP request.
 *
 * Responsibilities:
 *   - produce a valid 404 Not Found response
 *   - provide a simple default fallback behavior
 *   - keep routing failure handling centralized
 *
 * Design goals:
 *   - Minimal and explicit fallback
 *   - Safe default behavior
 *   - Easy override by users or higher-level server APIs
 *   - No dependency on transport details
 *
 * Notes:
 *   - Called after route matching fails
 *   - Does not throw
 *   - Produces a plain text response by default
 *
 * Usage:
 * @code
 *   using namespace cnerium::server;
 *
 *   Context ctx;
 *   not_found(ctx);
 *
 *   // Response:
 *   //   status = 404
 *   //   body   = "Not Found"
 * @endcode
 */

#pragma once

#include <cnerium/http/Status.hpp>
#include <cnerium/server/Context.hpp>

namespace cnerium::server
{
  /**
   * @brief Default 404 fallback handler.
   *
   * Sets the response status to 404 Not Found and
   * returns a simple plain text body.
   *
   * @param ctx Server request context
   */
  inline void not_found(Context &ctx)
  {
    ctx.response().set_status(cnerium::http::Status::not_found);
    ctx.response().text("Not Found");
  }

} // namespace cnerium::server
