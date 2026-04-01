/**
 * @file ResponseWriter.hpp
 * @brief cnerium::server::detail — Raw HTTP response writer
 *
 * @version 0.1.0
 * @author Gaspard Kirira
 * @copyright (c) 2026 Gaspard Kirira
 * @license MIT
 *
 * @details
 * Defines the low-level HTTP response writer used by the Cnerium server
 * to serialize a high-level cnerium::http::Response object into a raw
 * HTTP/1.x wire-format string.
 *
 * Responsibilities:
 *   - write the HTTP status line
 *   - write response headers
 *   - automatically provide Content-Length when missing
 *   - append the response body
 *
 * Supported output format:
 *   - HTTP/1.1
 *   - CRLF line endings
 *   - plain Content-Length bodies
 *
 * Design goals:
 *   - Header-only
 *   - Deterministic serialization
 *   - No transport dependency
 *   - Minimal and production-ready HTTP/1.x output
 *
 * Notes:
 *   - Chunked transfer encoding is not generated here
 *   - Connection management headers are left to higher layers
 *   - If Content-Length is not already present, it is added automatically
 *   - Existing headers are preserved as-is
 *
 * Usage:
 * @code
 *   using namespace cnerium::server::detail;
 *
 *   cnerium::http::Response res;
 *   res.set_status(cnerium::http::Status::ok);
 *   res.text("Hello from Cnerium");
 *
 *   std::string raw = ResponseWriter::write(res);
 * @endcode
 */

#pragma once

#include <charconv>
#include <string>
#include <string_view>

#include <cnerium/http/Response.hpp>
#include <cnerium/http/Status.hpp>

namespace cnerium::server::detail
{
  /**
   * @brief Low-level HTTP response serializer.
   *
   * Converts a high-level Response object into a raw HTTP/1.1 response.
   */
  class ResponseWriter
  {
  public:
    /**
     * @brief Serialize a response to raw HTTP/1.1 text.
     *
     * @param response High-level HTTP response
     * @return std::string Raw HTTP response
     */
    [[nodiscard]] static std::string write(const cnerium::http::Response &response)
    {
      std::string out;
      reserve_output(out, response);

      write_status_line(out, response);
      write_headers(out, response);
      write_content_length_if_missing(out, response);
      out += "\r\n";
      out.append(response.body().data(), response.body().size());

      return out;
    }

  private:
    /**
     * @brief Reserve a reasonable output capacity before serialization.
     *
     * @param out Output buffer
     * @param response Response being serialized
     */
    static void reserve_output(std::string &out,
                               const cnerium::http::Response &response)
    {
      out.reserve(128 + response.body().size());
    }

    /**
     * @brief Write the HTTP status line.
     *
     * Example:
     *   HTTP/1.1 200 OK\r\n
     *
     * @param out Output buffer
     * @param response Response being serialized
     */
    static void write_status_line(std::string &out,
                                  const cnerium::http::Response &response)
    {
      out += "HTTP/1.1 ";
      append_int(out, cnerium::http::to_int(response.status()));
      out.push_back(' ');
      out.append(cnerium::http::reason_phrase(response.status()));
      out += "\r\n";
    }

    /**
     * @brief Write all existing response headers.
     *
     * Headers are emitted in their stored order.
     *
     * @param out Output buffer
     * @param response Response being serialized
     */
    static void write_headers(std::string &out,
                              const cnerium::http::Response &response)
    {
      for (const auto &header : response.headers())
      {
        out.append(header.first);
        out += ": ";
        out.append(header.second);
        out += "\r\n";
      }
    }

    /**
     * @brief Write Content-Length if it is not already present.
     *
     * @param out Output buffer
     * @param response Response being serialized
     */
    static void write_content_length_if_missing(
        std::string &out,
        const cnerium::http::Response &response)
    {
      if (response.has_header("Content-Length"))
      {
        return;
      }

      out += "Content-Length: ";
      append_size(out, response.body().size());
      out += "\r\n";
    }

    /**
     * @brief Append an integer to the output buffer.
     *
     * @param out Output buffer
     * @param value Integer value
     */
    static void append_int(std::string &out, int value)
    {
      char buffer[32];
      auto [ptr, ec] = std::to_chars(buffer, buffer + sizeof(buffer), value);
      if (ec == std::errc{})
      {
        out.append(buffer, ptr);
      }
    }

    /**
     * @brief Append a size value to the output buffer.
     *
     * @param out Output buffer
     * @param value Size value
     */
    static void append_size(std::string &out, std::size_t value)
    {
      char buffer[32];
      auto [ptr, ec] = std::to_chars(buffer, buffer + sizeof(buffer), value);
      if (ec == std::errc{})
      {
        out.append(buffer, ptr);
      }
    }
  };

} // namespace cnerium::server::detail
