/**
 * @file Context.hpp
 * @brief cnerium::server — Server request execution context
 *
 * @version 0.2.0
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
 *   - Provide ergonomic helpers for request and response handling
 *
 * Design goals:
 *   - Simple and explicit ownership model
 *   - One context per request lifecycle
 *   - Easy integration with router and middleware
 *   - Lightweight and extensible for future metadata
 *   - Developer-friendly API (minimal boilerplate)
 *
 * Notes:
 *   - Context owns its Request, Response, and Params objects
 *   - Route parameters are typically filled after router matching
 *   - Middleware integration is provided through middleware_context()
 *   - This class is designed for high-level handler ergonomics
 *
 * Usage:
 * @code
 *   using namespace cnerium::server;
 *
 *   Context ctx;
 *
 *   ctx.text("Hello world");
 *
 *   auto id = ctx.param("id");
 *   auto token = ctx.header("Authorization");
 * @endcode
 */

#pragma once

#include <string>
#include <string_view>
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

    // ============================================================
    // 🔵 CORE ACCESS
    // ============================================================

    [[nodiscard]] request_type &request() noexcept { return request_; }
    [[nodiscard]] const request_type &request() const noexcept { return request_; }

    [[nodiscard]] response_type &response() noexcept { return response_; }
    [[nodiscard]] const response_type &response() const noexcept { return response_; }

    [[nodiscard]] params_type &params() noexcept { return params_; }
    [[nodiscard]] const params_type &params() const noexcept { return params_; }

    // ============================================================
    // 🔥 REQUEST SHORTCUTS (DEV UX)
    // ============================================================

    /**
     * @brief Returns the HTTP method.
     */
    [[nodiscard]] cnerium::http::Method method() const noexcept
    {
      return request_.method();
    }

    /**
     * @brief Returns the request path.
     */
    [[nodiscard]] std::string_view path() const noexcept
    {
      return request_.path();
    }

    /**
     * @brief Returns the raw query string.
     */
    [[nodiscard]] std::string_view query() const noexcept
    {
      return request_.query();
    }

    /**
     * @brief Returns a header value.
     */
    [[nodiscard]] std::string_view header(std::string_view key) const noexcept
    {
      return request_.header(key);
    }

    /**
     * @brief Returns a route parameter.
     */
    [[nodiscard]] std::string_view param(std::string_view key) const noexcept
    {
      return params_.get(key);
    }

    /**
     * @brief Returns true if a route parameter exists.
     */
    [[nodiscard]] bool has_param(std::string_view key) const noexcept
    {
      return params_.contains(key);
    }

    /**
     * @brief Returns the raw request body.
     */
    [[nodiscard]] std::string_view body() const noexcept
    {
      return request_.body();
    }

    /**
     * @brief Parse request body as JSON.
     */
    [[nodiscard]] cnerium::json::value json() const
    {
      return request_.json();
    }

    /**
     * @brief Set HTTP status.
     */
    Context &status(cnerium::http::Status status)
    {
      response_.set_status(status);
      return *this;
    }

    /**
     * @brief Send plain text response.
     */
    Context &text(std::string body)
    {
      response_.text(std::move(body));
      return *this;
    }

    /**
     * @brief Send HTML response.
     */
    Context &html(std::string body)
    {
      response_.html(std::move(body));
      return *this;
    }

    /**
     * @brief Send JSON response.
     */
    Context &json(const cnerium::json::value &value)
    {
      response_.json(value);
      return *this;
    }

    /**
     * @brief Send JSON response (rvalue).
     */
    Context &json(cnerium::json::value &&value)
    {
      response_.json(std::move(value));
      return *this;
    }

    /**
     * @brief Send success JSON response.
     */
    Context &ok(std::string message = "ok")
    {
      response_.ok(std::move(message));
      return *this;
    }

    /**
     * @brief Send error JSON response.
     */
    Context &error(cnerium::http::Status status, std::string message)
    {
      response_.error(status, std::move(message));
      return *this;
    }

    /**
     * @brief Set response header.
     */
    Context &set_header(std::string key, std::string value)
    {
      response_.set_header(std::move(key), std::move(value));
      return *this;
    }

    /**
     * @brief Set Content-Type header.
     */
    Context &content_type(std::string value)
    {
      response_.content_type(std::move(value));
      return *this;
    }

    /**
     * @brief Build a middleware execution context view.
     */
    [[nodiscard]] middleware_context_type middleware_context() noexcept
    {
      return middleware_context_type(request_, response_);
    }

    void set_request(request_type request)
    {
      request_ = std::move(request);
    }

    void set_response(response_type response)
    {
      response_ = std::move(response);
    }

    void set_params(params_type params)
    {
      params_ = std::move(params);
    }

    [[nodiscard]] bool has_params() const noexcept
    {
      return !params_.empty();
    }

    /**
     * @brief Reset the context to an empty default state.
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
