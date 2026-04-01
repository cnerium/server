/**
 * @file Context.hpp
 * @brief cnerium::server — Server request execution context
 *
 * @version 0.1.0
 * @author Gaspard Kirira
 * @copyright (c) 2026 Gaspard Kirira
 * @license MIT
 *
 * @details
 * Defines the high-level execution context used by the Cnerium server
 * during a single HTTP request/response lifecycle.
 *
 * A Context instance groups together:
 *   - the incoming HTTP request
 *   - the outgoing HTTP response
 *   - the route parameters extracted by the router
 *
 * Responsibilities:
 *   - Hold request state for the current execution
 *   - Hold response state to be sent back to the client
 *   - Expose matched route parameters
 *   - Provide a bridge to the middleware execution context
 *
 * Design goals:
 *   - Simple and explicit ownership model
 *   - One context per request lifecycle
 *   - Easy integration with router and middleware
 *   - Lightweight and extensible for future metadata
 *
 * Notes:
 *   - Context owns its Request, Response, and Params objects
 *   - Route parameters are typically filled after router matching
 *   - Middleware integration is provided through middleware_context()
 *
 * Usage:
 * @code
 *   using namespace cnerium::server;
 *   using namespace cnerium::http;
 *
 *   Context ctx;
 *   ctx.request().set_method(Method::Get);
 *   ctx.request().set_path("/users/42");
 *   ctx.params().set("id", "42");
 *
 *   auto id = ctx.params().get("id");
 *   ctx.response().text("User id: " + std::string(id));
 * @endcode
 */

#pragma once

#include <utility>

#include <cnerium/http/Request.hpp>
#include <cnerium/http/Response.hpp>
#include <cnerium/middleware/Context.hpp>
#include <cnerium/router/Params.hpp>

namespace cnerium::server
{
  /**
   * @brief Execution context for a single server request lifecycle.
   *
   * Owns the HTTP request, HTTP response, and matched route parameters.
   */
  class Context
  {
  public:
    using request_type = cnerium::http::Request;
    using response_type = cnerium::http::Response;
    using params_type = cnerium::router::Params;
    using middleware_context_type = cnerium::middleware::Context;

    /// @brief Default constructor.
    Context() = default;

    /**
     * @brief Construct a context from request and response.
     *
     * Route parameters start empty.
     *
     * @param request HTTP request
     * @param response HTTP response
     */
    Context(request_type request, response_type response)
        : request_(std::move(request)),
          response_(std::move(response))
    {
    }

    /**
     * @brief Construct a context from request, response, and params.
     *
     * @param request HTTP request
     * @param response HTTP response
     * @param params Matched route parameters
     */
    Context(request_type request,
            response_type response,
            params_type params)
        : request_(std::move(request)),
          response_(std::move(response)),
          params_(std::move(params))
    {
    }

    /**
     * @brief Returns mutable access to the HTTP request.
     *
     * @return request_type& Current request
     */
    [[nodiscard]] request_type &request() noexcept
    {
      return request_;
    }

    /**
     * @brief Returns const access to the HTTP request.
     *
     * @return const request_type& Current request
     */
    [[nodiscard]] const request_type &request() const noexcept
    {
      return request_;
    }

    /**
     * @brief Returns mutable access to the HTTP response.
     *
     * @return response_type& Current response
     */
    [[nodiscard]] response_type &response() noexcept
    {
      return response_;
    }

    /**
     * @brief Returns const access to the HTTP response.
     *
     * @return const response_type& Current response
     */
    [[nodiscard]] const response_type &response() const noexcept
    {
      return response_;
    }

    /**
     * @brief Returns mutable access to the matched route parameters.
     *
     * @return params_type& Route parameters
     */
    [[nodiscard]] params_type &params() noexcept
    {
      return params_;
    }

    /**
     * @brief Returns const access to the matched route parameters.
     *
     * @return const params_type& Route parameters
     */
    [[nodiscard]] const params_type &params() const noexcept
    {
      return params_;
    }

    /**
     * @brief Replace the full request object.
     *
     * @param request New request
     */
    void set_request(request_type request)
    {
      request_ = std::move(request);
    }

    /**
     * @brief Replace the full response object.
     *
     * @param response New response
     */
    void set_response(response_type response)
    {
      response_ = std::move(response);
    }

    /**
     * @brief Replace the full route parameters object.
     *
     * @param params New route parameters
     */
    void set_params(params_type params)
    {
      params_ = std::move(params);
    }

    /**
     * @brief Returns true if route parameters are present.
     *
     * @return true if params are not empty
     */
    [[nodiscard]] bool has_params() const noexcept
    {
      return !params_.empty();
    }

    /**
     * @brief Build a middleware execution context view.
     *
     * This creates a non-owning middleware::Context referencing
     * this context's request and response objects.
     *
     * @return middleware_context_type Middleware-compatible context
     */
    [[nodiscard]] middleware_context_type middleware_context() noexcept
    {
      return middleware_context_type(request_, response_);
    }

    /**
     * @brief Build a const middleware execution context view.
     *
     * Since middleware::Context requires mutable references,
     * this overload is intentionally omitted.
     */

    /**
     * @brief Reset the context to an empty default state.
     *
     * Clears request, response, and route parameters.
     */
    void clear()
    {
      request_.clear();
      response_.clear();
      params_.clear();
    }

  private:
    request_type request_{};
    response_type response_{};
    params_type params_{};
  };

} // namespace cnerium::server
