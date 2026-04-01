/**
 * @file HttpIO.hpp
 * @brief cnerium::server::detail — HTTP parsing and serialization glue
 *
 * @version 0.2.0
 * @author Gaspard Kirira
 * @copyright (c) 2026 Gaspard Kirira
 * @license MIT
 *
 * @details
 * Defines helper utilities that connect:
 *   - raw incoming HTTP bytes
 *   - request parsing
 *   - server request handling
 *   - response finalization
 *   - response serialization
 *
 * Responsibilities:
 *   - parse raw HTTP requests into high-level Request objects
 *   - dispatch parsed requests through the server
 *   - convert parsing failures into HTTP error responses
 *   - convert handler failures into HTTP error responses
 *   - finalize high-level responses before serialization
 *   - serialize high-level Response objects back to raw HTTP bytes
 *   - expose a simple single-entry request/response flow
 *
 * Design goals:
 *   - Header-only
 *   - No direct socket dependency
 *   - Reusable from connection and listener layers
 *   - Keep network code thin and focused
 *   - Centralize request/response flow in one place
 *
 * Notes:
 *   - This file does not perform socket I/O itself
 *   - It only transforms data between raw strings and framework objects
 *   - Parsing errors are converted into HTTP 400 responses
 *   - Unexpected handler exceptions are converted into HTTP 500 responses
 *   - HEAD requests are normalized to produce headers without a response body
 *   - Framework signature headers are finalized here before serialization
 *
 * Usage:
 * @code
 *   using namespace cnerium::server;
 *   using namespace cnerium::server::detail;
 *
 *   Server server;
 *   server.get("/", [](Context &ctx)
 *   {
 *     ctx.response().text("Hello");
 *   });
 *
 *   std::string raw =
 *       "GET / HTTP/1.1\r\n"
 *       "Host: localhost\r\n"
 *       "\r\n";
 *
 *   std::string response = HttpIO::handle_raw(server, raw);
 * @endcode
 */

#pragma once

#include <exception>
#include <string>
#include <string_view>

#include <cnerium/http/Status.hpp>
#include <cnerium/server/Server.hpp>
#include <cnerium/server/detail/RequestParser.hpp>
#include <cnerium/server/detail/ResponseWriter.hpp>

namespace cnerium::server::detail
{
  /**
   * @brief Result of parsing and handling a raw HTTP request.
   *
   * Stores:
   *   - the parsed request status
   *   - the produced high-level response
   *   - the final serialized raw HTTP response
   */
  struct HttpIOResult
  {
    /// @brief Parsing result.
    RequestParseResult parse_result{};

    /// @brief High-level HTTP response.
    cnerium::http::Response response{};

    /// @brief Serialized raw HTTP response.
    std::string raw_response{};

    /**
     * @brief Returns true if the incoming request was parsed successfully.
     *
     * @return true if parsing succeeded
     */
    [[nodiscard]] bool ok() const noexcept
    {
      return parse_result.ok();
    }

    /**
     * @brief Returns true if the incoming request failed to parse.
     *
     * @return true if parsing failed
     */
    [[nodiscard]] bool failed() const noexcept
    {
      return !ok();
    }

    /**
     * @brief Explicit boolean conversion.
     *
     * @return true if parsing succeeded
     */
    explicit operator bool() const noexcept
    {
      return ok();
    }
  };

  /**
   * @brief High-level glue between raw HTTP bytes and server dispatch.
   *
   * This class centralizes the complete request/response pipeline:
   *   1. parse raw request bytes
   *   2. call the server handler
   *   3. normalize special semantics such as HEAD
   *   4. finalize response headers
   *   5. serialize the final response
   */
  class HttpIO
  {
  public:
    /**
     * @brief Parse a raw request and build a high-level response.
     *
     * Uses the server configuration as parsing input.
     *
     * @param server Server instance used for dispatch
     * @param raw Raw HTTP request bytes
     * @return HttpIOResult Full request/response processing result
     */
    [[nodiscard]] static HttpIOResult process(
        const cnerium::server::Server &server,
        std::string_view raw)
    {
      return process(server, raw, server.config());
    }

    /**
     * @brief Parse a raw request with explicit configuration and build a response.
     *
     * Behavior:
     *   - if parsing succeeds, the parsed request is dispatched through the server
     *   - if parsing fails, a 400 Bad Request response is generated
     *   - if the server handler throws, a 500 Internal Server Error response is generated
     *   - HEAD requests are normalized to return headers without a response body
     *   - framework signature and fallback headers are finalized before serialization
     *
     * @param server Server instance used for dispatch
     * @param raw Raw HTTP request bytes
     * @param config Server configuration used for parsing limits
     * @return HttpIOResult Full request/response processing result
     */
    [[nodiscard]] static HttpIOResult process(
        const cnerium::server::Server &server,
        std::string_view raw,
        const cnerium::server::Config &config)
    {
      HttpIOResult result;

      result.parse_result = RequestParser::parse(raw, config);

      if (result.parse_result.ok())
      {
        try
        {
          result.response = server.handle(std::move(result.parse_result.request));
        }
        catch (const std::exception &e)
        {
          result.response = make_internal_server_error_response(e.what());
        }
        catch (...)
        {
          result.response =
              make_internal_server_error_response("Unknown server error");
        }
      }
      else
      {
        result.response = make_bad_request_response(result.parse_result.error);
      }

      enforce_head_semantics(raw, result.response);
      finalize_response(result.response);

      result.raw_response = ResponseWriter::write(result.response);
      return result;
    }

    /**
     * @brief Parse, dispatch, and serialize a raw HTTP request in one call.
     *
     * Uses the server configuration.
     *
     * @param server Server instance used for dispatch
     * @param raw Raw HTTP request bytes
     * @return std::string Serialized raw HTTP response
     */
    [[nodiscard]] static std::string handle_raw(
        const cnerium::server::Server &server,
        std::string_view raw)
    {
      return process(server, raw).raw_response;
    }

    /**
     * @brief Parse, dispatch, and serialize a raw HTTP request in one call.
     *
     * Uses an explicit parsing configuration.
     *
     * @param server Server instance used for dispatch
     * @param raw Raw HTTP request bytes
     * @param config Server configuration used for parsing limits
     * @return std::string Serialized raw HTTP response
     */
    [[nodiscard]] static std::string handle_raw(
        const cnerium::server::Server &server,
        std::string_view raw,
        const cnerium::server::Config &config)
    {
      return process(server, raw, config).raw_response;
    }

    /**
     * @brief Serialize a high-level response object to raw HTTP.
     *
     * @param response High-level response
     * @return std::string Serialized raw HTTP response
     */
    [[nodiscard]] static std::string write(
        const cnerium::http::Response &response)
    {
      return ResponseWriter::write(response);
    }

    /**
     * @brief Parse a raw HTTP request only.
     *
     * Uses default parser configuration.
     *
     * @param raw Raw HTTP request bytes
     * @return RequestParseResult Parsing result
     */
    [[nodiscard]] static RequestParseResult parse(
        std::string_view raw)
    {
      return RequestParser::parse(raw);
    }

    /**
     * @brief Parse a raw HTTP request using explicit configuration.
     *
     * @param raw Raw HTTP request bytes
     * @param config Server configuration
     * @return RequestParseResult Parsing result
     */
    [[nodiscard]] static RequestParseResult parse(
        std::string_view raw,
        const cnerium::server::Config &config)
    {
      return RequestParser::parse(raw, config);
    }

  private:
    /**
     * @brief Build a 400 Bad Request response.
     *
     * The response is emitted as JSON for consistency with the framework-level
     * error format.
     *
     * @param message Human-readable parse error
     * @return cnerium::http::Response Error response
     */
    [[nodiscard]] static cnerium::http::Response make_bad_request_response(
        std::string_view message)
    {
      cnerium::http::Response response;
      response.set_status(cnerium::http::Status::bad_request);
      response.json({{"ok", false},
                     {"error", message.empty() ? "Bad Request" : std::string(message)},
                     {"framework", "cnerium"}});
      return response;
    }

    /**
     * @brief Build a 500 Internal Server Error response.
     *
     * The response is emitted as JSON for consistency with the framework-level
     * error format.
     *
     * @param details Human-readable error detail
     * @return cnerium::http::Response Error response
     */
    [[nodiscard]] static cnerium::http::Response make_internal_server_error_response(
        std::string_view details)
    {
      cnerium::http::Response response;
      response.set_status(cnerium::http::Status::internal_server_error);
      response.json({{"ok", false},
                     {"error", "Internal Server Error"},
                     {"details", std::string(details)},
                     {"framework", "cnerium"}});
      return response;
    }

    /**
     * @brief Finalize response headers before serialization.
     *
     * Finalization rules:
     *   - apply framework signature headers
     *   - ensure a fallback Content-Type when missing
     *
     * @param response High-level response to finalize
     */
    static void finalize_response(cnerium::http::Response &response)
    {
      response.with_cnerium_signature();

      if (!response.has_header("Content-Type"))
      {
        response.set_header("Content-Type", "text/plain; charset=utf-8");
      }
    }

    /**
     * @brief Enforce HEAD response semantics.
     *
     * HEAD responses must not carry a message body. This helper detects HEAD
     * requests from the raw request line and clears the body while preserving
     * the status and headers.
     *
     * @param raw_request Raw serialized HTTP request
     * @param response High-level response to normalize
     */
    static void enforce_head_semantics(
        std::string_view raw_request,
        cnerium::http::Response &response)
    {
      if (is_head_request(raw_request))
      {
        response.set_body("");
      }
    }

    /**
     * @brief Return true if the raw request starts with the HEAD method.
     *
     * This is a lightweight fast-path check based on the request line prefix.
     *
     * @param raw_request Raw serialized HTTP request
     * @return true if the request method is HEAD
     */
    [[nodiscard]] static bool is_head_request(
        std::string_view raw_request) noexcept
    {
      return raw_request.rfind("HEAD ", 0) == 0;
    }
  };

} // namespace cnerium::server::detail
