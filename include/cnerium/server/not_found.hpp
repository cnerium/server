/**
 * @file not_found.hpp
 * @brief cnerium::server — Default 404 Not Found handler
 *
 * @version 0.2.0
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
 *   - Consistent default framework response shape
 *
 * Notes:
 *   - Called after route matching fails
 *   - Does not throw
 *   - Produces a JSON response by default
 *   - The response keeps the framework signature headers
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
 *   //   body   = {"ok":false,"error":"Not Found","framework":"cnerium"}
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
   * Sets the response status to 404 Not Found and returns
   * a simple JSON error body consistent with the framework.
   *
   * @param ctx Server request context
   */
  inline void not_found(Context &ctx)
  {
    ctx.response().set_status(cnerium::http::Status::not_found);
    ctx.response().json({{"ok", false},
                         {"error", "Not Found"},
                         {"framework", "cnerium"}});
  }

} // namespace cnerium::server
