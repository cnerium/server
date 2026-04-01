/**
 * @file TcpConnection.hpp
 * @brief cnerium::server::net — TCP connection handler
 *
 * @version 0.2.0
 * @author Gaspard Kirira
 * @copyright (c) 2026 Gaspard Kirira
 * @license MIT
 *
 * @details
 * Defines the TCP connection object used by the Cnerium server
 * networking layer to process one accepted client connection.
 *
 * Responsibilities:
 *   - own an accepted client socket
 *   - read raw HTTP request bytes from the socket
 *   - support multiple request/response exchanges on the same connection
 *   - delegate parsing and dispatch to HttpIO
 *   - write the serialized HTTP response back to the socket
 *   - manage simple HTTP/1.0 and HTTP/1.1 keep-alive behavior
 *
 * Design goals:
 *   - RAII-safe
 *   - Move-only
 *   - Thin network layer
 *   - No routing or middleware logic inside this class
 *   - Deterministic request framing
 *
 * Supported behavior:
 *   - one or more HTTP/1.x requests per TCP connection
 *   - Content-Length request bodies
 *   - Connection: close
 *   - Connection: keep-alive
 *   - HTTP/1.1 persistent connections by default
 *   - HTTP/1.0 non-persistent connections by default
 *
 * Notes:
 *   - This class does not support chunked request bodies yet
 *   - HTTP parsing and serialization are delegated to server::detail::HttpIO
 *   - Request pipelining is not fully implemented as an advanced scheduling model;
 *     however, multiple sequential requests on the same connection are supported
 *   - A persistent input buffer is kept between requests so extra bytes already
 *     received from the socket are not lost
 *   - Runtime limits are fully controlled by cnerium::server::Config
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

#include <charconv>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
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
   * Owns the client socket and processes one or more HTTP request/response
   * cycles depending on keep-alive semantics.
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
          server_(other.server_),
          input_buffer_(std::move(other.input_buffer_))
    {
      other.server_ = nullptr;
      other.input_buffer_.clear();
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
        input_buffer_ = std::move(other.input_buffer_);

        other.server_ = nullptr;
        other.input_buffer_.clear();
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
     * Reads one or more raw HTTP requests, dispatches each through the server,
     * writes each raw HTTP response, and keeps the connection alive when the
     * protocol rules and headers allow it.
     *
     * Behavior:
     *   - HTTP/1.1 stays alive by default unless Connection: close is present
     *   - HTTP/1.0 closes by default unless Connection: keep-alive is present
     *   - the server stops after a safety limit of requests per connection
     *   - if the peer closes the connection cleanly, processing stops normally
     *
     * @throws SocketError if network operations fail unexpectedly
     */
    void process()
    {
      ensure_valid();

      const auto &cfg = server_->config();
      std::size_t handled_requests = 0;

      while (handled_requests < cfg.max_requests_per_connection)
      {
        const auto raw_request = read_request();
        if (!raw_request.has_value())
        {
          break;
        }

        std::string raw_response =
            cnerium::server::detail::HttpIO::handle_raw(
                *server_,
                *raw_request,
                cfg);

        const bool keep_alive =
            should_keep_connection_alive(
                *raw_request,
                raw_response,
                handled_requests + 1,
                cfg);

        raw_response = ensure_connection_header(raw_response, keep_alive);
        write_response(raw_response);

        ++handled_requests;

        if (!keep_alive)
        {
          break;
        }
      }

      close();
    }

    /**
     * @brief Read one complete raw HTTP request from the socket.
     *
     * This method preserves unread bytes in an internal buffer so that
     * keep-alive connections can process the next request without losing
     * data already received from the network.
     *
     * @return std::optional<std::string> Complete raw HTTP request, or
     *         std::nullopt if the peer closed the connection before a new
     *         request started
     * @throws SocketError if reading fails or the request exceeds limits
     */
    [[nodiscard]] std::optional<std::string> read_request()
    {
      ensure_valid();

      const auto &cfg = server_->config();
      ensure_buffer_within_limit(cfg);

      if (const auto extracted = extract_complete_request(input_buffer_, cfg))
      {
        return extracted;
      }

      std::string chunk;
      chunk.resize(cfg.read_buffer_size);

      while (true)
      {
        const auto n = socket_.recv(chunk.data(), chunk.size());

        if (n == 0)
        {
          if (input_buffer_.empty())
          {
            return std::nullopt;
          }

          if (has_any_headers(input_buffer_))
          {
            throw SocketError("connection closed while reading incomplete HTTP request");
          }

          return std::nullopt;
        }

        input_buffer_.append(chunk.data(), static_cast<std::size_t>(n));
        ensure_buffer_within_limit(cfg);

        if (const auto extracted = extract_complete_request(input_buffer_, cfg))
        {
          return extracted;
        }
      }
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
    std::string input_buffer_{};

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
     * @brief Ensure the buffered request data remains within configured limits.
     *
     * This enforces a global upper bound for the in-memory request currently
     * being assembled:
     *   - header section must fit within cfg.max_header_size
     *   - body section must fit within cfg.max_request_body_size
     *
     * @param cfg Server configuration
     * @throws SocketError if buffered data exceeds the allowed request size budget
     */
    void ensure_buffer_within_limit(const config_type &cfg) const
    {
      const std::size_t max_total_size =
          cfg.max_header_size + cfg.max_request_body_size;

      if (input_buffer_.size() > max_total_size)
      {
        throw SocketError("incoming request exceeds allowed size");
      }

      if (input_buffer_.find("\r\n\r\n") == std::string::npos &&
          input_buffer_.size() > cfg.max_header_size)
      {
        throw SocketError("request headers too large");
      }
    }

    /**
     * @brief Extract one complete raw HTTP request from a byte buffer.
     *
     * If a full request is available, it is removed from the buffer and returned.
     * If not enough bytes are available yet, returns std::nullopt.
     *
     * Request framing rules:
     *   - headers end at CRLF CRLF
     *   - if Content-Length is present, exactly that many bytes are included
     *   - otherwise the request ends at the header terminator
     *
     * @param buffer Persistent connection input buffer
     * @param cfg Server configuration
     * @return std::optional<std::string> Complete raw request if available
     * @throws SocketError if the request headers or body exceed configured limits
     */
    [[nodiscard]] static std::optional<std::string> extract_complete_request(
        std::string &buffer,
        const config_type &cfg)
    {
      const auto header_end = buffer.find("\r\n\r\n");
      if (header_end == std::string::npos)
      {
        if (buffer.size() > cfg.max_header_size)
        {
          throw SocketError("request headers too large");
        }

        return std::nullopt;
      }

      const std::size_t headers_size = header_end + 4;
      if (headers_size > cfg.max_header_size)
      {
        throw SocketError("request headers too large");
      }

      const auto headers = std::string_view(buffer.data(), headers_size);

      std::size_t content_length = 0;
      bool has_content_length = false;

      if (const auto parsed = try_extract_content_length(headers))
      {
        has_content_length = true;
        content_length = *parsed;
      }

      if (has_content_length && content_length > cfg.max_request_body_size)
      {
        throw SocketError("request body too large");
      }

      const std::size_t total_size =
          headers_size + (has_content_length ? content_length : 0);

      if (total_size > cfg.max_header_size + cfg.max_request_body_size)
      {
        throw SocketError("incoming request exceeds allowed size");
      }

      if (buffer.size() < total_size)
      {
        return std::nullopt;
      }

      std::string raw_request = buffer.substr(0, total_size);
      buffer.erase(0, total_size);
      return raw_request;
    }

    /**
     * @brief Return true if the buffer already contains any apparent HTTP header data.
     *
     * @param buffer Raw buffered bytes
     * @return true if the buffer is non-empty
     */
    [[nodiscard]] static bool has_any_headers(std::string_view buffer) noexcept
    {
      return !buffer.empty();
    }

    /**
     * @brief Decide whether the connection should remain open after a response.
     *
     * Rules applied:
     *   - HTTP/1.1 is persistent by default
     *   - HTTP/1.0 is non-persistent by default
     *   - request Connection header can override default behavior
     *   - response Connection header can force close or keep-alive
     *   - request count is bounded by cfg.max_requests_per_connection
     *
     * @param raw_request Raw serialized request
     * @param raw_response Raw serialized response
     * @param next_request_count Number of requests that will have been served
     *        if this response is sent
     * @param cfg Server configuration
     * @return true if the connection should stay open
     */
    [[nodiscard]] static bool should_keep_connection_alive(
        std::string_view raw_request,
        std::string_view raw_response,
        std::size_t next_request_count,
        const config_type &cfg) noexcept
    {
      if (next_request_count >= cfg.max_requests_per_connection)
      {
        return false;
      }

      const bool request_is_http11 = is_http_1_1(raw_request);
      const auto request_connection = find_header_value(raw_request, "Connection");
      const auto response_connection = find_header_value(raw_response, "Connection");

      bool keep_alive = request_is_http11;

      if (request_connection.has_value())
      {
        if (header_value_contains_token(*request_connection, "close"))
        {
          keep_alive = false;
        }
        else if (header_value_contains_token(*request_connection, "keep-alive"))
        {
          keep_alive = true;
        }
      }

      if (response_connection.has_value())
      {
        if (header_value_contains_token(*response_connection, "close"))
        {
          keep_alive = false;
        }
        else if (header_value_contains_token(*response_connection, "keep-alive"))
        {
          keep_alive = true;
        }
      }

      return keep_alive;
    }

    /**
     * @brief Ensure a raw HTTP response contains an explicit Connection header.
     *
     * If a Connection header already exists, the response is returned unchanged.
     * Otherwise the method injects either:
     *   - Connection: keep-alive
     *   - Connection: close
     *
     * @param raw_response Serialized raw HTTP response
     * @param keep_alive Whether the response should advertise persistence
     * @return std::string Response with an explicit Connection header
     */
    [[nodiscard]] static std::string ensure_connection_header(
        std::string raw_response,
        bool keep_alive)
    {
      if (find_header_value(raw_response, "Connection").has_value())
      {
        return raw_response;
      }

      const auto header_end = raw_response.find("\r\n\r\n");
      if (header_end == std::string::npos)
      {
        return raw_response;
      }

      std::string header;
      header.reserve(32);
      header += "\r\nConnection: ";
      header += keep_alive ? "keep-alive" : "close";

      raw_response.insert(header_end, header);
      return raw_response;
    }

    /**
     * @brief Return true if the request line indicates HTTP/1.1.
     *
     * @param raw_message Raw HTTP request or response
     * @return true if the start line contains HTTP/1.1
     */
    [[nodiscard]] static bool is_http_1_1(std::string_view raw_message) noexcept
    {
      const auto line_end = raw_message.find("\r\n");
      if (line_end == std::string_view::npos)
      {
        return false;
      }

      const auto first_line = raw_message.substr(0, line_end);
      return first_line.find("HTTP/1.1") != std::string_view::npos;
    }

    /**
     * @brief Find a header value inside a raw HTTP message.
     *
     * Search is case-insensitive on the header name.
     *
     * @param raw_message Raw HTTP request or response
     * @param key Header name to search
     * @return std::optional<std::string_view> Header value if found
     */
    [[nodiscard]] static std::optional<std::string_view> find_header_value(
        std::string_view raw_message,
        std::string_view key) noexcept
    {
      const auto header_end = raw_message.find("\r\n\r\n");
      if (header_end == std::string_view::npos)
      {
        return std::nullopt;
      }

      const auto headers = raw_message.substr(0, header_end);
      std::size_t line_start = 0;
      bool first_line = true;

      while (line_start < headers.size())
      {
        const auto line_end = headers.find("\r\n", line_start);
        const auto effective_end =
            line_end == std::string_view::npos ? headers.size() : line_end;

        const auto line = headers.substr(line_start, effective_end - line_start);

        line_start =
            line_end == std::string_view::npos ? headers.size() : line_end + 2;

        if (first_line)
        {
          first_line = false;
          continue;
        }

        if (line.empty())
        {
          continue;
        }

        const auto colon = line.find(':');
        if (colon == std::string_view::npos)
        {
          continue;
        }

        const auto current_key = trim(line.substr(0, colon));
        const auto current_value = trim(line.substr(colon + 1));

        if (iequals(current_key, key))
        {
          return current_value;
        }
      }

      return std::nullopt;
    }

    /**
     * @brief Return true if a comma-separated header value contains a token.
     *
     * Matching is case-insensitive and trims surrounding ASCII whitespace.
     *
     * Examples:
     *   - "keep-alive"
     *   - "close"
     *   - "Upgrade, keep-alive"
     *
     * @param value Raw header value
     * @param token Token to search
     * @return true if token is present
     */
    [[nodiscard]] static bool header_value_contains_token(
        std::string_view value,
        std::string_view token) noexcept
    {
      while (!value.empty())
      {
        const auto comma = value.find(',');
        const auto part = trim(
            comma == std::string_view::npos
                ? value
                : value.substr(0, comma));

        if (iequals(part, token))
        {
          return true;
        }

        if (comma == std::string_view::npos)
        {
          break;
        }

        value.remove_prefix(comma + 1);
      }

      return false;
    }

    /**
     * @brief Try to extract Content-Length from raw request headers.
     *
     * The value is parsed tolerantly after trimming ASCII whitespace.
     *
     * @param headers Raw header block including request line
     * @return std::optional<std::size_t> Parsed content length if present and valid
     */
    [[nodiscard]] static std::optional<std::size_t> try_extract_content_length(
        std::string_view headers)
    {
      std::size_t line_start = 0;
      bool first_line = true;

      while (line_start < headers.size())
      {
        const auto line_end = headers.find("\r\n", line_start);
        if (line_end == std::string_view::npos)
        {
          break;
        }

        const auto line = headers.substr(line_start, line_end - line_start);
        line_start = line_end + 2;

        if (first_line)
        {
          first_line = false;
          continue;
        }

        if (line.empty())
        {
          continue;
        }

        const auto colon = line.find(':');
        if (colon == std::string_view::npos)
        {
          continue;
        }

        auto key = trim(line.substr(0, colon));
        auto value = trim(line.substr(colon + 1));

        if (iequals(key, "Content-Length"))
        {
          while (!value.empty() &&
                 (value.back() == ' ' ||
                  value.back() == '\t' ||
                  value.back() == '\r' ||
                  value.back() == '\n'))
          {
            value.remove_suffix(1);
          }

          if (value.empty())
          {
            return std::nullopt;
          }

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
