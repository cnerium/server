/**
 * @file Config.hpp
 * @brief cnerium::server — Server configuration
 *
 * @version 0.1.0
 * @author Gaspard Kirira
 * @copyright (c) 2026 Gaspard Kirira
 * @license MIT
 *
 * @details
 * Defines the configuration object used to initialize and control
 * the behavior of the Cnerium HTTP server.
 *
 * A Config instance provides:
 *   - network binding settings (host, port)
 *   - connection backlog size
 *   - buffer sizes for I/O operations
 *   - limits for request body size
 *
 * Design goals:
 *   - Simple and explicit configuration
 *   - Safe default values for immediate use
 *   - No dynamic allocation
 *   - Easily extensible for future features (TLS, timeouts, etc.)
 *
 * Notes:
 *   - This configuration is immutable once the server starts
 *   - Defaults are tuned for development and small deployments
 *   - Advanced tuning can be added without breaking API
 *
 * Usage:
 * @code
 *   using namespace cnerium::server;
 *
 *   Config config;
 *   config.host = "0.0.0.0";
 *   config.port = 8080;
 *
 *   Server server(config);
 *   server.listen();
 * @endcode
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace cnerium::server
{
  /**
   * @brief Configuration object for the HTTP server.
   *
   * Holds all runtime parameters required to initialize
   * and operate the server.
   */
  struct Config
  {
    /**
     * @brief Host address to bind the server to.
     *
     * Examples:
     *   - "127.0.0.1"  → local only
     *   - "0.0.0.0"    → all interfaces
     *
     * Default: "127.0.0.1"
     */
    std::string host{"127.0.0.1"};

    /**
     * @brief TCP port to listen on.
     *
     * Default: 8080
     */
    std::uint16_t port{8080};

    /**
     * @brief Maximum number of pending connections in the listen queue.
     *
     * Passed to the underlying listen() system call.
     *
     * Default: 128
     */
    int backlog{128};

    /**
     * @brief Size of the read buffer for incoming data.
     *
     * Controls how much data is read per socket read operation.
     *
     * Default: 8 KB
     */
    std::size_t read_buffer_size{8 * 1024};

    /**
     * @brief Maximum allowed size of the HTTP request body.
     *
     * Used to prevent excessive memory usage or abuse.
     *
     * Default: 1 MB
     */
    std::size_t max_request_body_size{1024 * 1024};

    /**
     * @brief Returns true if the configuration is valid.
     *
     * Performs basic validation checks.
     *
     * @return true if valid
     */
    [[nodiscard]] bool valid() const noexcept
    {
      return !host.empty() && port > 0 && backlog > 0 && read_buffer_size > 0 && max_request_body_size > 0;
    }

    /**
     * @brief Reset configuration to default values.
     */
    void reset() noexcept
    {
      host = "127.0.0.1";
      port = 8080;
      backlog = 128;
      read_buffer_size = 8 * 1024;
      max_request_body_size = 1024 * 1024;
    }
  };

} // namespace cnerium::server
