/**
 * @file RouteDispatch.hpp
 * @brief cnerium::server::detail — Route dispatch utilities
 *
 * @version 0.1.0
 * @author Gaspard Kirira
 * @copyright (c) 2026 Gaspard Kirira
 * @license MIT
 *
 * @details
 * Defines the internal dispatch utilities used by the Cnerium server
 * to resolve routes, execute middleware, invoke handlers, and apply
 * fallback behavior.
 *
 * Responsibilities:
 *   - find the first matching route
 *   - extract matched route parameters
 *   - execute middleware chain in insertion order
 *   - invoke the final route handler
 *   - invoke the not-found fallback when no route matches
 *   - invoke the error handler when execution fails
 *
 * Design goals:
 *   - Header-only
 *   - Internal-only implementation detail
 *   - Deterministic synchronous execution
 *   - Clear separation from the public Server API
 *
 * Notes:
 *   - Matching is performed in insertion order
 *   - The first matching route wins
 *   - Middleware runs before the final route handler
 *   - Exceptions are converted through the provided error handler
 *
 * Usage:
 * @code
 *   using namespace cnerium::server;
 *
 *   Context ctx;
 *   ctx.request().set_method(cnerium::http::Method::Get);
 *   ctx.request().set_path("/");
 *
 *   std::vector<RouteEntry> routes;
 *   std::vector<cnerium::middleware::Middleware> middleware;
 *
 *   routes.emplace_back(
 *     cnerium::router::Route(cnerium::http::Method::Get, "/"),
 *     [](Context &ctx)
 *     {
 *       ctx.response().text("Hello");
 *     });
 *
 *   cnerium::server::detail::dispatch(
 *     ctx,
 *     routes,
 *     middleware,
 *     cnerium::server::not_found,
 *     cnerium::server::default_error_handler);
 * @endcode
 */

#pragma once

#include <cstddef>
#include <functional>
#include <stdexcept>
#include <string_view>
#include <vector>

#include <cnerium/middleware/Context.hpp>
#include <cnerium/middleware/Middleware.hpp>
#include <cnerium/middleware/Next.hpp>
#include <cnerium/router/MatchResult.hpp>
#include <cnerium/server/Context.hpp>
#include <cnerium/server/ErrorHandler.hpp>
#include <cnerium/server/Handler.hpp>

namespace cnerium::server
{
  struct RouteEntry;
}

namespace cnerium::server::detail
{
  /**
   * @brief Result of route lookup.
   *
   * Stores the matched route entry pointer and its match result.
   */
  struct RouteLookupResult
  {
    /// @brief Pointer to the matched route entry, or nullptr if none matched.
    const cnerium::server::RouteEntry *entry{nullptr};

    /// @brief Match result associated with the matched route.
    cnerium::router::MatchResult match{cnerium::router::MatchResult::failure()};

    /**
     * @brief Returns true if a route was found.
     *
     * @return true if matched
     */
    [[nodiscard]] bool found() const noexcept
    {
      return entry != nullptr && match.matched();
    }

    /**
     * @brief Explicit boolean conversion.
     *
     * @return true if matched
     */
    explicit operator bool() const noexcept
    {
      return found();
    }
  };

  /**
   * @brief Find the first route matching the given method and path.
   *
   * Routes are evaluated in insertion order.
   *
   * @param routes Registered route entries
   * @param method Incoming HTTP method
   * @param path Incoming request path
   * @return RouteLookupResult Lookup result
   */
  [[nodiscard]] inline RouteLookupResult find_route(
      const std::vector<cnerium::server::RouteEntry> &routes,
      cnerium::http::Method method,
      std::string_view path)
  {
    for (const auto &entry : routes)
    {
      auto result = entry.route.match(method, path);
      if (result.matched())
      {
        return RouteLookupResult{
            &entry,
            std::move(result)};
      }
    }

    return RouteLookupResult{};
  }

  /**
   * @brief Execute the final handler after middleware chain completion.
   *
   * @param ctx Current server context
   * @param entry Matched route entry
   */
  inline void invoke_handler(
      cnerium::server::Context &ctx,
      const cnerium::server::RouteEntry &entry)
  {
    entry.handler(ctx);
  }

  /**
   * @brief Execute middleware chain recursively in insertion order.
   *
   * If all middleware call next(), the final route handler is executed.
   *
   * @param index Current middleware index
   * @param middleware Registered middleware list
   * @param mw_ctx Middleware-compatible context
   * @param ctx Full server context
   * @param entry Matched route entry
   */
  inline void run_middleware_from(
      std::size_t index,
      const std::vector<cnerium::middleware::Middleware> &middleware,
      cnerium::middleware::Context &mw_ctx,
      cnerium::server::Context &ctx,
      const cnerium::server::RouteEntry &entry)
  {
    if (index >= middleware.size())
    {
      invoke_handler(ctx, entry);
      return;
    }

    const auto next = cnerium::middleware::Next(
        [&middleware, &mw_ctx, &ctx, &entry, index]()
        {
          run_middleware_from(index + 1, middleware, mw_ctx, ctx, entry);
        });

    middleware[index](mw_ctx, next);
  }

  /**
   * @brief Execute all middleware and then the matched handler.
   *
   * @param ctx Current server context
   * @param middleware Registered middleware list
   * @param entry Matched route entry
   */
  inline void execute_chain(
      cnerium::server::Context &ctx,
      const std::vector<cnerium::middleware::Middleware> &middleware,
      const cnerium::server::RouteEntry &entry)
  {
    auto mw_ctx = ctx.middleware_context();
    run_middleware_from(0, middleware, mw_ctx, ctx, entry);
  }

  /**
   * @brief Dispatch a request context through routing, middleware, and handlers.
   *
   * Behavior:
   *   - finds the first matching route
   *   - fills route parameters into the context
   *   - executes middleware then handler
   *   - calls not-found handler if no route matches
   *   - catches exceptions and forwards them to the error handler
   *
   * @param ctx Current server context
   * @param routes Registered route entries
   * @param middleware Registered middleware list
   * @param not_found_handler Not-found fallback handler
   * @param error_handler Error handler
   */
  inline void dispatch(
      cnerium::server::Context &ctx,
      const std::vector<cnerium::server::RouteEntry> &routes,
      const std::vector<cnerium::middleware::Middleware> &middleware,
      const cnerium::server::Handler &not_found_handler,
      const cnerium::server::ErrorHandler &error_handler)
  {
    const auto lookup = find_route(
        routes,
        ctx.request().method(),
        ctx.request().path());

    if (lookup.found())
    {
      ctx.set_params(lookup.match.params());
    }
    else
    {
      ctx.params().clear();
    }

    try
    {
      if (lookup.found())
      {
        execute_chain(ctx, middleware, *lookup.entry);
      }
      else
      {
        not_found_handler(ctx);
      }
    }
    catch (const std::exception &e)
    {
      error_handler(ctx, e);
    }
    catch (...)
    {
      const std::runtime_error unknown("Unknown server error");
      error_handler(ctx, unknown);
    }
  }

} // namespace cnerium::server::detail
