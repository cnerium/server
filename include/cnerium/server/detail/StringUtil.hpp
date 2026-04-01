/**
 * @file StringUtil.hpp
 * @brief cnerium::server::detail — String utility helpers
 *
 * @version 0.1.0
 * @author Gaspard Kirira
 * @copyright (c) 2026 Gaspard Kirira
 * @license MIT
 *
 * @details
 * Defines small string utility helpers used internally by the
 * Cnerium server implementation.
 *
 * Responsibilities:
 *   - trim ASCII whitespace
 *   - compare strings case-insensitively for ASCII
 *   - convert ASCII characters and strings to lowercase
 *   - validate HTTP token-like names
 *
 * Design goals:
 *   - Header-only
 *   - Lightweight
 *   - No external dependencies
 *   - Focused on HTTP/server internal needs
 *
 * Notes:
 *   - Utilities here are ASCII-oriented, which is sufficient
 *     for HTTP method names, header names, and protocol tokens
 *   - This file is intended for internal server use only
 *
 * Usage:
 * @code
 *   using namespace cnerium::server::detail;
 *
 *   auto trimmed = trim("  hello  ");
 *   bool same = iequals("Content-Type", "content-type");
 *   bool valid = is_valid_token("Content-Length");
 * @endcode
 */

#pragma once

#include <cctype>
#include <string>
#include <string_view>

namespace cnerium::server::detail
{
  /**
   * @brief Returns true if a character is ASCII whitespace.
   *
   * Recognized characters:
   *   - space
   *   - horizontal tab
   *   - carriage return
   *   - line feed
   *
   * @param ch Input character
   * @return true if whitespace
   */
  [[nodiscard]] inline bool is_space(char ch) noexcept
  {
    return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
  }

  /**
   * @brief Convert one ASCII character to lowercase.
   *
   * Non-ASCII behavior is not guaranteed and not required
   * for the intended HTTP use cases.
   *
   * @param ch Input character
   * @return char Lowercased character
   */
  [[nodiscard]] inline char ascii_lower(char ch) noexcept
  {
    const unsigned char c = static_cast<unsigned char>(ch);
    return static_cast<char>(std::tolower(c));
  }

  /**
   * @brief Return a lowercase copy of an ASCII string.
   *
   * @param value Input string
   * @return std::string Lowercased copy
   */
  [[nodiscard]] inline std::string to_lower(std::string_view value)
  {
    std::string out;
    out.reserve(value.size());

    for (char ch : value)
    {
      out.push_back(ascii_lower(ch));
    }

    return out;
  }

  /**
   * @brief Trim ASCII whitespace from both ends of a string view.
   *
   * @param value Input string view
   * @return std::string_view Trimmed view
   */
  [[nodiscard]] inline std::string_view trim(std::string_view value) noexcept
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
   * @brief Compare two strings case-insensitively for ASCII.
   *
   * @param a First string
   * @param b Second string
   * @return true if equal ignoring ASCII case
   */
  [[nodiscard]] inline bool iequals(std::string_view a,
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
   * @brief Returns true if the string is a valid HTTP token-like name.
   *
   * This conservative validation is suitable for header names
   * and similar protocol identifiers.
   *
   * Allowed:
   *   ALPHA / DIGIT / ! # $ % & ' * + - . ^ _ ` | ~
   *
   * @param name Input token
   * @return true if valid
   */
  [[nodiscard]] inline bool is_valid_token(std::string_view name) noexcept
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

} // namespace cnerium::server::detail
