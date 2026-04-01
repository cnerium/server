/**
 * @file version.hpp
 * @brief cnerium::server — Version information
 *
 * @version 0.1.0
 * @author Gaspard Kirira
 * @copyright (c) 2026 Gaspard Kirira
 * @license MIT
 *
 * @details
 * Defines compile-time and runtime version information
 * for the Cnerium Server module.
 *
 * Responsibilities:
 *   - expose semantic version components
 *   - provide string representations
 *   - allow compile-time checks if needed
 *
 * Design goals:
 *   - Header-only
 *   - Zero overhead
 *   - Easy integration in logs, diagnostics, and tooling
 *
 * Usage:
 * @code
 *   using namespace cnerium::server;
 *
 *   auto v = version_string(); // "0.1.0"
 * @endcode
 */

#pragma once

#include <string>

namespace cnerium::server
{
  /// @brief Major version number.
  inline constexpr int version_major = 0;

  /// @brief Minor version number.
  inline constexpr int version_minor = 1;

  /// @brief Patch version number.
  inline constexpr int version_patch = 0;

  /**
   * @brief Return the version as a string.
   *
   * Format: "major.minor.patch"
   *
   * @return std::string Version string
   */
  [[nodiscard]] inline std::string version_string()
  {
    return std::to_string(version_major) + "." +
           std::to_string(version_minor) + "." +
           std::to_string(version_patch);
  }

  /**
   * @brief Return the version as a single integer.
   *
   * Format: major * 10000 + minor * 100 + patch
   *
   * Example:
   *   0.1.0 -> 100
   *
   * @return int Encoded version
   */
  [[nodiscard]] inline constexpr int version()
  {
    return version_major * 10000 +
           version_minor * 100 +
           version_patch;
  }

} // namespace cnerium::server
