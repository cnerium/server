/**
 * @file TcpConnection.hpp
 * @brief cnerium::server::net — TCP connection handler
 *
 * @version 0.1.0
 * @author Gaspard Kirira
 * @copyright (c) 2026 Gaspard Kirira
 * @license MIT
 *
 * @details
 * Defines the TCP connection object used by the Cnerium server
 * networking layer to process a single client connection.
 *
 * Responsibilities:
 *   - own an accepted client socket
 *   - read raw HTTP request bytes from the socket
 *   - delegate parsing and dispatch to HttpIO
 *   - write the serialized HTTP response back to the socket
 *   - provide a simple request/response connection lifecycle
 *
 * Design goals:
 *   - RAII-safe
 *   - Move-only
 *   - Thin network layer
 *   - No routing or middleware logic inside this class
 *
 * Notes:
 *   - This class handles one connection lifecycle
 *   - It currently processes one HTTP request and one HTTP response
 *   - Keep-alive and request pipelining can be added later
 *   - HTTP parsing and serialization are delegated to server::detail::HttpIO
 *
 * Usage:
 * @code
 *   using namespace cnerium::server;
 *   using namespace cnerium::server::net;
 *
 *   Socket client = listener.accept();
 *   TcpConnection conn(std::move(client), server);
 *   conn.process();
 * @endcode
 */

#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

#include <cnerium/server/Config.hpp>
#include <cnerium/server/Server.hpp>
#include <cnerium/server/detail/HttpIO.hpp>
#include <cnerium/server/net/Socket.hpp>

namespace cnerium::server::net
{
  /**
   * @brief Represents one accepted TCP client connection.
   *
   * Owns the client socket and processes one request/response cycle.
   */
  class TcpConnection
  {
  public:
    using socket_type = cnerium::server::net::Socket;
    using server_type = cnerium::server::Server;
    using config_type = cnerium::server::Config;

    /**
     * @brief Construct a connection from a client socket and server reference.
     *
     * @param socket Accepted client socket
     * @param server Server instance used for request dispatch
     */
    TcpConnection(socket_type socket, const server_type &server)
        : socket_(std::move(socket)),
          server_(&server)
    {
    }

    TcpConnection(const TcpConnection &) = delete;
    TcpConnection &operator=(const TcpConnection &) = delete;

    /**
     * @brief Move constructor.
     *
     * @param other Source connection
     */
    TcpConnection(TcpConnection &&other) noexcept
        : socket_(std::move(other.socket_)),
          server_(other.server_)
    {
      other.server_ = nullptr;
    }

    /**
     * @brief Move assignment operator.
     *
     * @param other Source connection
     * @return TcpConnection& Self
     */
    TcpConnection &operator=(TcpConnection &&other) noexcept
    {
      if (this != &other)
      {
        socket_ = std::move(other.socket_);
        server_ = other.server_;
        other.server_ = nullptr;
      }
      return *this;
    }

    /**
     * @brief Returns mutable access to the underlying socket.
     *
     * @return socket_type& Client socket
     */
    [[nodiscard]] socket_type &socket() noexcept
    {
      return socket_;
    }

    /**
     * @brief Returns const access to the underlying socket.
     *
     * @return const socket_type& Client socket
     */
    [[nodiscard]] const socket_type &socket() const noexcept
    {
      return socket_;
    }

    /**
     * @brief Returns the associated server.
     *
     * @return const server_type& Server reference
     */
    [[nodiscard]] const server_type &server() const noexcept
    {
      return *server_;
    }

    /**
     * @brief Returns true if the connection is valid.
     *
     * @return true if socket and server reference are valid
     */
    [[nodiscard]] bool valid() const noexcept
    {
      return socket_.valid() && server_ != nullptr;
    }

    /**
     * @brief Explicit boolean conversion.
     *
     * @return true if valid
     */
    explicit operator bool() const noexcept
    {
      return valid();
    }

    /**
     * @brief Process the connection.
     *
     * Reads one raw HTTP request, dispatches it through the server,
     * and writes one raw HTTP response.
     *
     * @throws SocketError if network operations fail
     */
    void process()
    {
      ensure_valid();

      const std::string raw_request = read_request();
      const std::string raw_response =
          cnerium::server::detail::HttpIO::handle_raw(*server_, raw_request, server_->config());

      write_response(raw_response);
    }

    /**
     * @brief Read one raw HTTP request from the socket.
     *
     * @return std::string Raw HTTP request bytes
     * @throws SocketError if reading fails
     */
    [[nodiscard]] std::string read_request()
    {
      ensure_valid();

      const auto &cfg = server_->config();
      std::string data;
      data.reserve(cfg.read_buffer_size);

      std::string chunk;
      chunk.resize(cfg.read_buffer_size);

      std::size_t content_length = 0;
      bool has_content_length = false;
      bool headers_complete = false;
      std::size_t expected_total_size = 0;

      while (true)
      {
        const auto n = socket_.recv(chunk.data(), chunk.size());

        if (n == 0)
        {
          break;
        }

        data.append(chunk.data(), static_cast<std::size_t>(n));

        if (!headers_complete)
        {
          const auto header_end = data.find("\r\n\r\n");
          if (header_end != std::string::npos)
          {
            headers_complete = true;

            const auto headers = std::string_view(data.data(), header_end + 4);
            if (const auto parsed = try_extract_content_length(headers))
            {
              has_content_length = true;
              content_length = *parsed;
              expected_total_size = (header_end + 4) + content_length;
            }
            else
            {
              expected_total_size = header_end + 4;
            }
          }
        }

        if (headers_complete)
        {
          if (has_content_length)
          {
            if (data.size() >= expected_total_size)
            {
              break;
            }
          }
          else
          {
            break;
          }
        }

        if (data.size() > cfg.max_request_body_size + 16 * 1024)
        {
          throw SocketError("incoming request exceeds allowed size");
        }
      }

      return data;
    }

    /**
     * @brief Write a full raw HTTP response to the socket.
     *
     * @param raw_response Serialized HTTP response
     * @throws SocketError if writing fails
     */
    void write_response(std::string_view raw_response)
    {
      ensure_valid();
      socket_.send_all(raw_response);
    }

    /**
     * @brief Close the client socket.
     */
    void close() noexcept
    {
      socket_.close();
    }

  private:
    socket_type socket_{};
    const server_type *server_{nullptr};

    /**
     * @brief Throw if the connection is invalid.
     *
     * @throws SocketError if invalid
     */
    void ensure_valid() const
    {
      if (!valid())
      {
        throw SocketError("invalid TCP connection");
      }
    }

    /**
     * @brief Try to extract Content-Length from raw request headers.
     *
     * @param headers Raw header block including request line
     * @return std::optional<std::size_t> Parsed content length if present and valid
     */
    [[nodiscard]] static std::optional<std::size_t> try_extract_content_length(
        std::string_view headers)
    {
      std::size_t line_start = 0;

      while (line_start < headers.size())
      {
        const auto line_end = headers.find("\r\n", line_start);
        if (line_end == std::string_view::npos)
        {
          break;
        }

        const auto line = headers.substr(line_start, line_end - line_start);
        line_start = line_end + 2;

        if (line.empty())
        {
          continue;
        }

        const auto colon = line.find(':');
        if (colon == std::string_view::npos)
        {
          continue;
        }

        const auto key = trim(line.substr(0, colon));
        const auto value = trim(line.substr(colon + 1));

        if (iequals(key, "Content-Length"))
        {
          std::size_t parsed = 0;
          const char *begin = value.data();
          const char *end = value.data() + value.size();
          const auto [ptr, ec] = std::from_chars(begin, end, parsed);

          if (ec == std::errc{} && ptr == end)
          {
            return parsed;
          }

          return std::nullopt;
        }
      }

      return std::nullopt;
    }

    /**
     * @brief Trim ASCII whitespace from both ends of a string view.
     *
     * @param value Input view
     * @return std::string_view Trimmed view
     */
    [[nodiscard]] static std::string_view trim(std::string_view value) noexcept
    {
      while (!value.empty() &&
             (value.front() == ' ' || value.front() == '\t' ||
              value.front() == '\r' || value.front() == '\n'))
      {
        value.remove_prefix(1);
      }

      while (!value.empty() &&
             (value.back() == ' ' || value.back() == '\t' ||
              value.back() == '\r' || value.back() == '\n'))
      {
        value.remove_suffix(1);
      }

      return value;
    }

    /**
     * @brief Compare two ASCII strings case-insensitively.
     *
     * @param a First string
     * @param b Second string
     * @return true if equal ignoring ASCII case
     */
    [[nodiscard]] static bool iequals(std::string_view a,
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
     * @brief Convert one ASCII character to lowercase.
     *
     * @param ch Input character
     * @return char Lowercased character
     */
    [[nodiscard]] static char ascii_lower(char ch) noexcept
    {
      const unsigned char c = static_cast<unsigned char>(ch);
      if (c >= 'A' && c <= 'Z')
      {
        return static_cast<char>(c - 'A' + 'a');
      }
      return static_cast<char>(c);
    }
  };

} // namespace cnerium::server::net
