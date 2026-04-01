/**
 * @file Config.hpp
 * @brief cnerium::server — Server configuration
 *
 * @version 0.2.0
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
 *   - limits for request body and headers
 *   - socket timeout configuration
 *   - keep-alive connection limits
 *
 * Design goals:
 *   - Simple and explicit configuration
 *   - Safe default values for immediate use
 *   - No dynamic allocation beyond std::string
 *   - Easily extensible for future features (TLS, runtime integration, etc.)
 *
 * Notes:
 *   - This configuration is intended to be treated as immutable
 *     once the server starts listening
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
 *   config.read_timeout_ms = 5000;
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
     * @brief Maximum allowed size of the HTTP header block.
     *
     * This includes the request line and all header lines up to
     * the terminating CRLF CRLF sequence.
     *
     * Default: 16 KB
     */
    std::size_t max_header_size{16 * 1024};

    /**
     * @brief Maximum number of HTTP requests processed on one keep-alive connection.
     *
     * Helps prevent unbounded reuse of a single client connection.
     *
     * Default: 100
     */
    std::size_t max_requests_per_connection{100};

    /**
     * @brief Socket read timeout in milliseconds.
     *
     * Applied to blocking receive operations on accepted client sockets.
     *
     * Default: 5000 ms
     */
    std::uint32_t read_timeout_ms{5000};

    /**
     * @brief Socket write timeout in milliseconds.
     *
     * Applied to blocking send operations on accepted client sockets.
     *
     * Default: 5000 ms
     */
    std::uint32_t write_timeout_ms{5000};

    /**
     * @brief Idle keep-alive timeout in milliseconds.
     *
     * Used by higher connection layers to decide how long a persistent
     * connection may remain open while waiting for the next request.
     *
     * Default: 10000 ms
     */
    std::uint32_t keep_alive_timeout_ms{10000};

    /**
     * @brief Returns true if the configuration is valid.
     *
     * Performs basic validation checks.
     *
     * @return true if valid
     */
    [[nodiscard]] bool valid() const noexcept
    {
      return !host.empty() &&
             port > 0 &&
             backlog > 0 &&
             read_buffer_size > 0 &&
             max_request_body_size > 0 &&
             max_header_size > 0 &&
             max_requests_per_connection > 0 &&
             read_timeout_ms > 0 &&
             write_timeout_ms > 0 &&
             keep_alive_timeout_ms > 0;
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
      max_header_size = 16 * 1024;
      max_requests_per_connection = 100;
      read_timeout_ms = 5000;
      write_timeout_ms = 5000;
      keep_alive_timeout_ms = 10000;
    }
  };

} // namespace cnerium::server
