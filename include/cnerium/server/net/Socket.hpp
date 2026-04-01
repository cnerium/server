/**
 * @file Socket.hpp
 * @brief cnerium::server::net — TCP socket wrapper
 *
 * @version 0.2.0
 * @author Gaspard Kirira
 * @copyright (c) 2026 Gaspard Kirira
 * @license MIT
 *
 * @details
 * Defines a small RAII wrapper around a native TCP socket descriptor
 * used internally by the Cnerium server networking layer.
 *
 * Responsibilities:
 *   - own a native socket descriptor
 *   - close the descriptor automatically
 *   - provide basic read/write operations
 *   - provide common socket option helpers
 *   - expose explicit validity and lifecycle control
 *   - provide timeout helpers for read and write operations
 *
 * Design goals:
 *   - Lightweight
 *   - RAII-safe
 *   - Move-only
 *   - Minimal abstraction over POSIX sockets
 *   - Clear low-level networking API for higher server layers
 *
 * Notes:
 *   - This implementation targets POSIX platforms
 *   - Windows support can be added later through a dedicated backend
 *   - This class does not perform HTTP parsing
 *   - Higher-level connection management belongs to TcpConnection
 *   - Keep-alive HTTP semantics are handled above this class
 *
 * Usage:
 * @code
 *   using namespace cnerium::server::net;
 *
 *   Socket sock = Socket::create_tcp();
 *   sock.set_reuse_addr(true);
 *   sock.set_read_timeout_ms(5000);
 *   sock.set_write_timeout_ms(5000);
 * @endcode
 */

#pragma once

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#ifdef TCP_NODELAY
#include <netinet/tcp.h>
#endif

namespace cnerium::server::net
{
  /**
   * @brief Exception type thrown for socket-related failures.
   */
  class SocketError : public std::runtime_error
  {
  public:
    /**
     * @brief Construct a socket error with a message.
     *
     * @param message Error message
     */
    explicit SocketError(std::string message)
        : std::runtime_error(std::move(message))
    {
    }
  };

  /**
   * @brief RAII wrapper around a native TCP socket descriptor.
   *
   * Owns the file descriptor and closes it automatically.
   */
  class Socket
  {
  public:
    using native_handle_type = int;

    /// @brief Invalid socket descriptor value.
    static constexpr native_handle_type invalid_handle = -1;

    /**
     * @brief Default constructor.
     *
     * Creates an invalid socket.
     */
    Socket() noexcept = default;

    /**
     * @brief Construct from an existing native socket descriptor.
     *
     * @param fd Native socket descriptor
     */
    explicit Socket(native_handle_type fd) noexcept
        : fd_(fd)
    {
    }

    Socket(const Socket &) = delete;
    Socket &operator=(const Socket &) = delete;

    /**
     * @brief Move constructor.
     *
     * @param other Source socket
     */
    Socket(Socket &&other) noexcept
        : fd_(other.release())
    {
    }

    /**
     * @brief Move assignment operator.
     *
     * @param other Source socket
     * @return Socket& Self
     */
    Socket &operator=(Socket &&other) noexcept
    {
      if (this != &other)
      {
        close();
        fd_ = other.release();
      }
      return *this;
    }

    /**
     * @brief Destructor.
     *
     * Closes the socket if still open.
     */
    ~Socket()
    {
      close();
    }

    /**
     * @brief Create a new TCP socket.
     *
     * @return Socket Newly created socket
     * @throws SocketError if creation fails
     */
    [[nodiscard]] static Socket create_tcp()
    {
      const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
      if (fd < 0)
      {
        throw SocketError("failed to create socket: " + last_error_string());
      }

      return Socket(fd);
    }

    /**
     * @brief Returns the native socket descriptor.
     *
     * @return native_handle_type Native descriptor
     */
    [[nodiscard]] native_handle_type native_handle() const noexcept
    {
      return fd_;
    }

    /**
     * @brief Returns true if the socket is valid.
     *
     * @return true if valid
     */
    [[nodiscard]] bool valid() const noexcept
    {
      return fd_ != invalid_handle;
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
     * @brief Release ownership of the native descriptor.
     *
     * After this call, the Socket becomes invalid and will not close
     * the released descriptor.
     *
     * @return native_handle_type Released descriptor
     */
    [[nodiscard]] native_handle_type release() noexcept
    {
      const auto out = fd_;
      fd_ = invalid_handle;
      return out;
    }

    /**
     * @brief Replace the current native descriptor.
     *
     * Closes the currently owned descriptor first.
     *
     * @param fd New descriptor
     */
    void reset(native_handle_type fd = invalid_handle) noexcept
    {
      if (fd_ != fd)
      {
        close();
        fd_ = fd;
      }
    }

    /**
     * @brief Close the socket if valid.
     */
    void close() noexcept
    {
      if (valid())
      {
        ::close(fd_);
        fd_ = invalid_handle;
      }
    }

    /**
     * @brief Shut down socket communication.
     *
     * @param how SHUT_RD, SHUT_WR, or SHUT_RDWR
     * @throws SocketError if shutdown fails
     */
    void shutdown(int how = SHUT_RDWR)
    {
      ensure_valid("shutdown");

      if (::shutdown(fd_, how) < 0)
      {
        throw SocketError("failed to shutdown socket: " + last_error_string());
      }
    }

    /**
     * @brief Bind the socket to an IPv4 host and port.
     *
     * @param host IPv4 address string
     * @param port TCP port
     * @throws SocketError if bind fails
     */
    void bind(std::string_view host, std::uint16_t port)
    {
      ensure_valid("bind");

      sockaddr_in addr{};
      addr.sin_family = AF_INET;
      addr.sin_port = htons(port);

      if (::inet_pton(AF_INET, std::string(host).c_str(), &addr.sin_addr) != 1)
      {
        throw SocketError("invalid IPv4 address: " + std::string(host));
      }

      if (::bind(fd_,
                 reinterpret_cast<const sockaddr *>(&addr),
                 sizeof(addr)) < 0)
      {
        throw SocketError("failed to bind socket: " + last_error_string());
      }
    }

    /**
     * @brief Mark the socket as listening.
     *
     * @param backlog Listen backlog
     * @throws SocketError if listen fails
     */
    void listen(int backlog)
    {
      ensure_valid("listen");

      if (::listen(fd_, backlog) < 0)
      {
        throw SocketError("failed to listen on socket: " + last_error_string());
      }
    }

    /**
     * @brief Accept an incoming connection.
     *
     * @return Socket Accepted client socket
     * @throws SocketError if accept fails
     */
    [[nodiscard]] Socket accept()
    {
      ensure_valid("accept");

      const int client_fd = ::accept(fd_, nullptr, nullptr);
      if (client_fd < 0)
      {
        throw SocketError("failed to accept connection: " + last_error_string());
      }

      return Socket(client_fd);
    }

    /**
     * @brief Connect the socket to an IPv4 remote endpoint.
     *
     * @param host Remote IPv4 address
     * @param port Remote TCP port
     * @throws SocketError if connect fails
     */
    void connect(std::string_view host, std::uint16_t port)
    {
      ensure_valid("connect");

      sockaddr_in addr{};
      addr.sin_family = AF_INET;
      addr.sin_port = htons(port);

      if (::inet_pton(AF_INET, std::string(host).c_str(), &addr.sin_addr) != 1)
      {
        throw SocketError("invalid IPv4 address: " + std::string(host));
      }

      if (::connect(fd_,
                    reinterpret_cast<const sockaddr *>(&addr),
                    sizeof(addr)) < 0)
      {
        throw SocketError("failed to connect socket: " + last_error_string());
      }
    }

    /**
     * @brief Receive bytes into a buffer.
     *
     * @param buffer Destination buffer
     * @param size Maximum bytes to read
     * @return ssize_t Number of bytes read, 0 on EOF
     * @throws SocketError if recv fails
     */
    [[nodiscard]] ssize_t recv(void *buffer, std::size_t size)
    {
      ensure_valid("recv");

      const auto n = ::recv(fd_, buffer, size, 0);
      if (n < 0)
      {
        throw SocketError("failed to receive from socket: " + last_error_string());
      }

      return n;
    }

    /**
     * @brief Send bytes from a buffer.
     *
     * @param buffer Source buffer
     * @param size Number of bytes to send
     * @return ssize_t Number of bytes written
     * @throws SocketError if send fails
     */
    [[nodiscard]] ssize_t send(const void *buffer, std::size_t size)
    {
      ensure_valid("send");

      const auto n = ::send(fd_, buffer, size, 0);
      if (n < 0)
      {
        throw SocketError("failed to send to socket: " + last_error_string());
      }

      return n;
    }

    /**
     * @brief Send a full string view.
     *
     * This method retries until all bytes are sent or a failure occurs.
     *
     * @param data Data to send
     * @throws SocketError if sending fails or connection breaks unexpectedly
     */
    void send_all(std::string_view data)
    {
      std::size_t sent = 0;

      while (sent < data.size())
      {
        const auto n = send(data.data() + sent, data.size() - sent);
        if (n <= 0)
        {
          throw SocketError("failed to send all bytes");
        }

        sent += static_cast<std::size_t>(n);
      }
    }

    /**
     * @brief Set SO_REUSEADDR.
     *
     * @param enabled Whether to enable the option
     * @throws SocketError if setsockopt fails
     */
    void set_reuse_addr(bool enabled)
    {
      set_bool_option(SOL_SOCKET, SO_REUSEADDR, enabled, "SO_REUSEADDR");
    }

    /**
     * @brief Set SO_REUSEPORT when available.
     *
     * @param enabled Whether to enable the option
     * @throws SocketError if setsockopt fails
     */
    void set_reuse_port(bool enabled)
    {
#ifdef SO_REUSEPORT
      set_bool_option(SOL_SOCKET, SO_REUSEPORT, enabled, "SO_REUSEPORT");
#else
      (void)enabled;
#endif
    }

    /**
     * @brief Set TCP_NODELAY when available.
     *
     * @param enabled Whether to enable the option
     * @throws SocketError if setsockopt fails
     */
    void set_tcp_no_delay(bool enabled)
    {
#ifdef TCP_NODELAY
      set_bool_option(IPPROTO_TCP, TCP_NODELAY, enabled, "TCP_NODELAY");
#else
      (void)enabled;
#endif
    }

    /**
     * @brief Set SO_KEEPALIVE.
     *
     * This is the kernel TCP keepalive mechanism, not HTTP keep-alive.
     *
     * @param enabled Whether to enable the option
     * @throws SocketError if setsockopt fails
     */
    void set_keep_alive(bool enabled)
    {
      set_bool_option(SOL_SOCKET, SO_KEEPALIVE, enabled, "SO_KEEPALIVE");
    }

    /**
     * @brief Set the receive timeout in milliseconds.
     *
     * Applies a socket-level timeout for blocking receive operations.
     *
     * @param timeout_ms Timeout in milliseconds
     * @throws SocketError if setsockopt fails
     */
    void set_read_timeout_ms(std::uint32_t timeout_ms)
    {
      set_timeval_option(SOL_SOCKET, SO_RCVTIMEO, timeout_ms, "SO_RCVTIMEO");
    }

    /**
     * @brief Set the send timeout in milliseconds.
     *
     * Applies a socket-level timeout for blocking send operations.
     *
     * @param timeout_ms Timeout in milliseconds
     * @throws SocketError if setsockopt fails
     */
    void set_write_timeout_ms(std::uint32_t timeout_ms)
    {
      set_timeval_option(SOL_SOCKET, SO_SNDTIMEO, timeout_ms, "SO_SNDTIMEO");
    }

    /**
     * @brief Set blocking mode.
     *
     * Currently kept as a future extension point.
     * A fcntl-based implementation can be added later.
     *
     * @param enabled Whether blocking mode should be enabled
     */
    void set_blocking(bool enabled) noexcept
    {
      (void)enabled;
    }

    /**
     * @brief Returns a readable description of the last system socket error.
     *
     * @return std::string Error message
     */
    [[nodiscard]] static std::string last_error_string()
    {
      return std::strerror(errno);
    }

  private:
    native_handle_type fd_{invalid_handle};

    /**
     * @brief Throws if the socket is invalid.
     *
     * @param operation Operation name
     * @throws SocketError if socket is invalid
     */
    void ensure_valid(std::string_view operation) const
    {
      if (!valid())
      {
        throw SocketError(
            "cannot " + std::string(operation) + " on invalid socket");
      }
    }

    /**
     * @brief Set a boolean socket option.
     *
     * @param level Socket option level
     * @param option Socket option name
     * @param enabled Whether to enable the option
     * @param option_name Human-readable option name
     * @throws SocketError if setsockopt fails
     */
    void set_bool_option(
        int level,
        int option,
        bool enabled,
        std::string_view option_name)
    {
      ensure_valid("setsockopt");

      const int value = enabled ? 1 : 0;
      if (::setsockopt(fd_,
                       level,
                       option,
                       &value,
                       static_cast<socklen_t>(sizeof(value))) < 0)
      {
        throw SocketError(
            "failed to set " + std::string(option_name) + ": " + last_error_string());
      }
    }

    /**
     * @brief Set a timeval-based socket option from milliseconds.
     *
     * @param level Socket option level
     * @param option Socket option name
     * @param timeout_ms Timeout in milliseconds
     * @param option_name Human-readable option name
     * @throws SocketError if setsockopt fails
     */
    void set_timeval_option(
        int level,
        int option,
        std::uint32_t timeout_ms,
        std::string_view option_name)
    {
      ensure_valid("setsockopt");

      timeval tv{};
      tv.tv_sec = static_cast<time_t>(timeout_ms / 1000);
      tv.tv_usec = static_cast<suseconds_t>((timeout_ms % 1000) * 1000);

      if (::setsockopt(fd_,
                       level,
                       option,
                       &tv,
                       static_cast<socklen_t>(sizeof(tv))) < 0)
      {
        throw SocketError(
            "failed to set " + std::string(option_name) + ": " + last_error_string());
      }
    }
  };

} // namespace cnerium::server::net
