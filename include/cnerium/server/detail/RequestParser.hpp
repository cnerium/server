/**
 * @file RequestParser.hpp
 * @brief cnerium::server::detail — Raw HTTP request parser
 *
 * @version 0.1.0
 * @author Gaspard Kirira
 * @copyright (c) 2026 Gaspard Kirira
 * @license MIT
 *
 * @details
 * Defines the low-level HTTP request parser used by the Cnerium server
 * to transform raw HTTP/1.x request bytes into a high-level
 * cnerium::http::Request object.
 *
 * Responsibilities:
 *   - parse the request line
 *   - parse HTTP headers
 *   - extract the request path
 *   - extract the optional request body
 *   - populate a cnerium::http::Request instance
 *
 * Supported request format:
 *   - HTTP/1.0
 *   - HTTP/1.1
 *   - CRLF line endings
 *   - simple Content-Length bodies
 *
 * Design goals:
 *   - Header-only
 *   - Deterministic parsing
 *   - No transport dependency
 *   - Clear validation errors
 *   - Minimal and production-ready HTTP/1.x parsing for framework use
 *
 * Notes:
 *   - Chunked transfer encoding is not supported here
 *   - Multipart parsing is outside the scope of this parser
 *   - Body parsing is based on Content-Length when present
 *   - Raw wire parsing belongs to the server layer, not to Request itself
 *
 * Usage:
 * @code
 *   using namespace cnerium::server::detail;
 *
 *   std::string raw =
 *       "POST /users HTTP/1.1\r\n"
 *       "Host: localhost\r\n"
 *       "Content-Type: application/json\r\n"
 *       "Content-Length: 17\r\n"
 *       "\r\n"
 *       "{\"name\":\"Gaspard\"}";
 *
 *   auto result = RequestParser::parse(raw);
 *   if (result.ok())
 *   {
 *     auto req = std::move(result.request);
 *   }
 * @endcode
 */

#pragma once

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <cnerium/http/Method.hpp>
#include <cnerium/http/Request.hpp>
#include <cnerium/server/Config.hpp>

namespace cnerium::server::detail
{
  /**
   * @brief Result of a raw request parsing operation.
   *
   * Holds either a valid parsed request or an error message.
   */
  struct RequestParseResult
  {
    /// @brief Parsed request object.
    cnerium::http::Request request{};

    /// @brief Human-readable error message.
    std::string error{};

    /**
     * @brief Returns true if parsing succeeded.
     *
     * @return true if no error occurred
     */
    [[nodiscard]] bool ok() const noexcept
    {
      return error.empty();
    }

    /**
     * @brief Returns true if parsing failed.
     *
     * @return true if an error occurred
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
   * @brief Low-level HTTP request parser.
   *
   * Converts raw HTTP/1.x bytes into a high-level Request object.
   */
  class RequestParser
  {
  public:
    /**
     * @brief Parse a raw HTTP request using default configuration limits.
     *
     * @param raw Raw HTTP request bytes
     * @return RequestParseResult Parsing result
     */
    [[nodiscard]] static RequestParseResult parse(std::string_view raw)
    {
      return parse(raw, cnerium::server::Config{});
    }

    /**
     * @brief Parse a raw HTTP request using explicit server configuration.
     *
     * Uses Config::max_request_body_size for body validation.
     *
     * @param raw Raw HTTP request bytes
     * @param config Server configuration
     * @return RequestParseResult Parsing result
     */
    [[nodiscard]] static RequestParseResult parse(
        std::string_view raw,
        const cnerium::server::Config &config)
    {
      RequestParseResult result;

      if (raw.empty())
      {
        result.error = "empty request";
        return result;
      }

      const auto header_end = raw.find("\r\n\r\n");
      if (header_end == std::string_view::npos)
      {
        result.error = "malformed request: missing header terminator";
        return result;
      }

      const std::string_view head = raw.substr(0, header_end);
      const std::string_view body = raw.substr(header_end + 4);

      std::size_t line_pos = 0;
      const auto request_line = next_line(head, line_pos);
      if (!request_line.has_value())
      {
        result.error = "malformed request: missing request line";
        return result;
      }

      auto request_line_parse = parse_request_line(*request_line);
      if (!request_line_parse.ok)
      {
        result.error = std::move(request_line_parse.error);
        return result;
      }

      result.request.set_method(request_line_parse.method);
      result.request.set_path(std::move(request_line_parse.path));

      std::size_t declared_content_length = 0;
      bool has_content_length = false;

      while (line_pos < head.size())
      {
        const auto line = next_line(head, line_pos);
        if (!line.has_value())
        {
          break;
        }

        if (line->empty())
        {
          continue;
        }

        auto header_parse = parse_header_line(*line);
        if (!header_parse.ok)
        {
          result.error = std::move(header_parse.error);
          return result;
        }

        result.request.set_header(
            std::move(header_parse.key),
            std::move(header_parse.value));

        if (iequals(header_parse.original_key, "Content-Length"))
        {
          auto length = parse_content_length(header_parse.value);
          if (!length.has_value())
          {
            result.error = "invalid Content-Length header";
            return result;
          }

          declared_content_length = *length;
          has_content_length = true;
        }
      }

      if (has_content_length)
      {
        if (declared_content_length > config.max_request_body_size)
        {
          result.error = "request body too large";
          return result;
        }

        if (body.size() < declared_content_length)
        {
          result.error = "incomplete request body";
          return result;
        }

        result.request.set_body(std::string(body.substr(0, declared_content_length)));
      }
      else
      {
        if (body.size() > config.max_request_body_size)
        {
          result.error = "request body too large";
          return result;
        }

        result.request.set_body(std::string(body));
      }

      return result;
    }

  private:
    struct RequestLineParseResult
    {
      bool ok{false};
      cnerium::http::Method method{cnerium::http::Method::Get};
      std::string path{};
      std::string error{};
    };

    struct HeaderLineParseResult
    {
      bool ok{false};
      std::string key{};
      std::string value{};
      std::string original_key{};
      std::string error{};
    };

    /**
     * @brief Read the next CRLF-terminated line from a header block.
     *
     * @param text Source text
     * @param pos Current cursor position, updated after reading
     * @return std::optional<std::string_view> Line without CRLF
     */
    [[nodiscard]] static std::optional<std::string_view> next_line(
        std::string_view text,
        std::size_t &pos)
    {
      if (pos > text.size())
      {
        return std::nullopt;
      }

      if (pos == text.size())
      {
        return std::nullopt;
      }

      const auto end = text.find("\r\n", pos);
      if (end == std::string_view::npos)
      {
        const auto line = text.substr(pos);
        pos = text.size();
        return line;
      }

      const auto line = text.substr(pos, end - pos);
      pos = end + 2;
      return line;
    }

    /**
     * @brief Parse the HTTP request line.
     *
     * Expected format:
     *   METHOD SP TARGET SP HTTP/VERSION
     *
     * @param line Request line
     * @return RequestLineParseResult Parse result
     */
    [[nodiscard]] static RequestLineParseResult parse_request_line(
        std::string_view line)
    {
      RequestLineParseResult result;

      const auto first_space = line.find(' ');
      if (first_space == std::string_view::npos)
      {
        result.error = "malformed request line: missing method separator";
        return result;
      }

      const auto second_space = line.find(' ', first_space + 1);
      if (second_space == std::string_view::npos)
      {
        result.error = "malformed request line: missing version separator";
        return result;
      }

      const auto method_sv = line.substr(0, first_space);
      const auto target_sv = line.substr(first_space + 1, second_space - first_space - 1);
      const auto version_sv = line.substr(second_space + 1);

      if (target_sv.empty())
      {
        result.error = "malformed request line: empty request target";
        return result;
      }

      if (!is_supported_http_version(version_sv))
      {
        result.error = "unsupported HTTP version";
        return result;
      }

      const auto method = parse_method(method_sv);
      if (!method.has_value())
      {
        result.error = "unsupported HTTP method";
        return result;
      }

      result.ok = true;
      result.method = *method;
      result.path = normalize_target(target_sv);
      return result;
    }

    /**
     * @brief Parse a single header line.
     *
     * Expected format:
     *   Key: Value
     *
     * @param line Header line
     * @return HeaderLineParseResult Parse result
     */
    [[nodiscard]] static HeaderLineParseResult parse_header_line(
        std::string_view line)
    {
      HeaderLineParseResult result;

      const auto colon = line.find(':');
      if (colon == std::string_view::npos)
      {
        result.error = "malformed header line: missing ':'";
        return result;
      }

      auto key = trim(line.substr(0, colon));
      auto value = trim(line.substr(colon + 1));

      if (key.empty())
      {
        result.error = "malformed header line: empty header name";
        return result;
      }

      if (!is_valid_header_name(key))
      {
        result.error = "malformed header line: invalid header name";
        return result;
      }

      result.ok = true;
      result.original_key = std::string(key);
      result.key = std::string(key);
      result.value = std::string(value);
      return result;
    }

    /**
     * @brief Parse a Content-Length header value.
     *
     * @param value Header value
     * @return std::optional<std::size_t> Parsed value if valid
     */
    [[nodiscard]] static std::optional<std::size_t> parse_content_length(
        std::string_view value)
    {
      std::size_t parsed = 0;

      if (value.empty())
      {
        return std::nullopt;
      }

      const char *begin = value.data();
      const char *end = value.data() + value.size();

      auto [ptr, ec] = std::from_chars(begin, end, parsed);
      if (ec != std::errc{} || ptr != end)
      {
        return std::nullopt;
      }

      return parsed;
    }

    /**
     * @brief Parse a request method.
     *
     * @param method HTTP method token
     * @return std::optional<cnerium::http::Method> Parsed method
     */
    [[nodiscard]] static std::optional<cnerium::http::Method> parse_method(
        std::string_view method)
    {
      using method_type = cnerium::http::Method;

      if (method == "GET")
        return method_type::Get;
      if (method == "POST")
        return method_type::Post;
      if (method == "PUT")
        return method_type::Put;
      if (method == "PATCH")
        return method_type::Patch;
      if (method == "DELETE")
        return method_type::Delete;
      if (method == "HEAD")
        return method_type::Head;
      if (method == "OPTIONS")
        return method_type::Options;

      return std::nullopt;
    }

    /**
     * @brief Check whether an HTTP version is supported.
     *
     * @param version Version token
     * @return true if supported
     */
    [[nodiscard]] static bool is_supported_http_version(
        std::string_view version) noexcept
    {
      return version == "HTTP/1.1" || version == "HTTP/1.0";
    }

    /**
     * @brief Normalize a request target into a request path.
     *
     * Rules:
     *   - empty target becomes "/"
     *   - origin-form is preserved
     *   - missing leading slash is fixed
     *
     * @param target Raw request target
     * @return std::string Normalized path
     */
    [[nodiscard]] static std::string normalize_target(
        std::string_view target)
    {
      if (target.empty())
      {
        return "/";
      }

      std::string out(target);

      if (out.front() != '/')
      {
        out.insert(out.begin(), '/');
      }

      return out;
    }

    /**
     * @brief Trim whitespace from both ends of a string view.
     *
     * @param value Input view
     * @return std::string_view Trimmed view
     */
    [[nodiscard]] static std::string_view trim(std::string_view value) noexcept
    {
      while (!value.empty() && is_space(value.front()))
      {
        value.remove_prefix(1);
      }

      while (!value.empty() && is_space(value.back()))
      {
        value.remove_suffix(1);
      }

      return value;
    }

    /**
     * @brief Case-insensitive equality for ASCII header names.
     *
     * @param a First string
     * @param b Second string
     * @return true if equal ignoring case
     */
    [[nodiscard]] static bool iequals(
        std::string_view a,
        std::string_view b) noexcept
    {
      if (a.size() != b.size())
      {
        return false;
      }

      for (std::size_t i = 0; i < a.size(); ++i)
      {
        if (ascii_lower(a[i]) != ascii_lower(b[i]))
        {
          return false;
        }
      }

      return true;
    }

    /**
     * @brief Validate a header field-name.
     *
     * Uses a conservative token validation suitable for HTTP headers.
     *
     * @param name Header name
     * @return true if valid
     */
    [[nodiscard]] static bool is_valid_header_name(
        std::string_view name) noexcept
    {
      if (name.empty())
      {
        return false;
      }

      for (char ch : name)
      {
        const unsigned char c = static_cast<unsigned char>(ch);

        const bool is_alpha_num = std::isalnum(c) != 0;
        const bool is_tchar =
            ch == '!' || ch == '#' || ch == '$' || ch == '%' ||
            ch == '&' || ch == '\'' || ch == '*' || ch == '+' ||
            ch == '-' || ch == '.' || ch == '^' || ch == '_' ||
            ch == '`' || ch == '|' || ch == '~';

        if (!is_alpha_num && !is_tchar)
        {
          return false;
        }
      }

      return true;
    }

    /**
     * @brief Check whether a character is HTTP whitespace.
     *
     * @param ch Character
     * @return true if whitespace
     */
    [[nodiscard]] static bool is_space(char ch) noexcept
    {
      return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
    }

    /**
     * @brief Convert an ASCII character to lowercase.
     *
     * @param ch Input character
     * @return char Lowercased character
     */
    [[nodiscard]] static char ascii_lower(char ch) noexcept
    {
      const unsigned char c = static_cast<unsigned char>(ch);
      return static_cast<char>(std::tolower(c));
    }
  };

} // namespace cnerium::server::detail
