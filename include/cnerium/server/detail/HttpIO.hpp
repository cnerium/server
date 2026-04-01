/**
 * @file HttpIO.hpp
 * @brief cnerium::server::detail — HTTP parsing and serialization glue
 *
 * @version 0.1.0
 * @author Gaspard Kirira
 * @copyright (c) 2026 Gaspard Kirira
 * @license MIT
 *
 * @details
 * Defines small helper utilities that connect:
 *   - raw incoming HTTP bytes
 *   - request parsing
 *   - server request handling
 *   - response serialization
 *
 * Responsibilities:
 *   - parse raw HTTP requests into high-level Request objects
 *   - dispatch parsed requests through the server
 *   - serialize high-level Response objects back to raw HTTP bytes
 *   - expose a simple single-entry request/response flow
 *
 * Design goals:
 *   - Header-only
 *   - No direct socket dependency
 *   - Reusable from connection and listener layers
 *   - Keep network code thin and focused
 *
 * Notes:
 *   - This file does not perform I/O itself
 *   - It only transforms data between raw strings and framework objects
 *   - Parsing errors are converted into HTTP error responses
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
   */
  class HttpIO
  {
  public:
    /**
     * @brief Parse a raw request and build a high-level response.
     *
     * If parsing fails, a 400 Bad Request response is generated.
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
     * If parsing fails, a 400 Bad Request response is generated.
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
        result.response = server.handle(std::move(result.parse_result.request));
      }
      else
      {
        result.response = make_bad_request_response(result.parse_result.error);
      }

      result.raw_response = ResponseWriter::write(result.response);
      return result;
    }

    /**
     * @brief Parse, dispatch, and serialize a raw HTTP request in one call.
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
     * @param message Human-readable parse error
     * @return cnerium::http::Response Error response
     */
    [[nodiscard]] static cnerium::http::Response make_bad_request_response(
        std::string_view message)
    {
      cnerium::http::Response response;
      response.set_status(cnerium::http::Status::bad_request);
      response.text(std::string(message.empty() ? "Bad Request" : message));
      return response;
    }
  };

} // namespace cnerium::server::detail
