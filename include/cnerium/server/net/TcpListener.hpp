/**
 * @file TcpListener.hpp
 * @brief cnerium::server::net — TCP listener
 *
 * @version 0.1.0
 * @author Gaspard Kirira
 * @copyright (c) 2026 Gaspard Kirira
 * @license MIT
 *
 * @details
 * Defines the TCP listener used by the Cnerium server networking layer
 * to accept incoming client connections and hand them over to
 * TcpConnection for processing.
 *
 * Responsibilities:
 *   - create and own the listening socket
 *   - configure socket options for listening
 *   - bind to the configured host and port
 *   - accept incoming client sockets
 *   - build TcpConnection objects from accepted sockets
 *   - run a simple blocking accept loop
 *
 * Design goals:
 *   - RAII-safe
 *   - Move-only
 *   - Thin listener abstraction
 *   - No HTTP parsing logic in this class
 *
 * Notes:
 *   - This class currently provides a blocking accept loop
 *   - Connection processing is synchronous by default
 *   - Concurrency and runtime integration can be added later
 *   - Accepted connections are delegated to TcpConnection
 *
 * Usage:
 * @code
 *   using namespace cnerium::server;
 *   using namespace cnerium::server::net;
 *
 *   Server server;
 *   server.get("/", [](Context &ctx)
 *   {
 *     ctx.response().text("Hello");
 *   });
 *
 *   TcpListener listener(server);
 *   listener.start();
 *   listener.run();
 * @endcode
 */

#pragma once

#include <atomic>
#include <cstdint>
#include <string_view>
#include <utility>

#include <cnerium/server/Server.hpp>
#include <cnerium/server/net/Socket.hpp>
#include <cnerium/server/net/TcpConnection.hpp>

namespace cnerium::server::net
{
  /**
   * @brief Blocking TCP listener for the Cnerium server.
   *
   * Owns the listening socket and accepts incoming client connections.
   */
  class TcpListener
  {
  public:
    using socket_type = cnerium::server::net::Socket;
    using connection_type = cnerium::server::net::TcpConnection;
    using server_type = cnerium::server::Server;
    using config_type = cnerium::server::Config;

    /**
     * @brief Construct a listener from a server reference.
     *
     * Uses server.config() when starting.
     *
     * @param server Server instance
     */
    explicit TcpListener(const server_type &server)
        : server_(&server)
    {
    }

    /**
     * @brief Construct a listener from a server reference and explicit config.
     *
     * @param server Server instance
     * @param config Listener configuration
     */
    TcpListener(const server_type &server, config_type config)
        : server_(&server),
          config_(std::move(config)),
          has_explicit_config_(true)
    {
    }

    TcpListener(const TcpListener &) = delete;
    TcpListener &operator=(const TcpListener &) = delete;

    /**
     * @brief Move constructor.
     *
     * @param other Source listener
     */
    TcpListener(TcpListener &&other) noexcept
        : socket_(std::move(other.socket_)),
          server_(other.server_),
          config_(std::move(other.config_)),
          has_explicit_config_(other.has_explicit_config_),
          running_(other.running_.load())
    {
      other.server_ = nullptr;
      other.has_explicit_config_ = false;
      other.running_ = false;
    }

    /**
     * @brief Move assignment operator.
     *
     * @param other Source listener
     * @return TcpListener& Self
     */
    TcpListener &operator=(TcpListener &&other) noexcept
    {
      if (this != &other)
      {
        stop();

        socket_ = std::move(other.socket_);
        server_ = other.server_;
        config_ = std::move(other.config_);
        has_explicit_config_ = other.has_explicit_config_;
        running_ = other.running_.load();

        other.server_ = nullptr;
        other.has_explicit_config_ = false;
        other.running_ = false;
      }

      return *this;
    }

    /**
     * @brief Destructor.
     *
     * Stops the listener if needed.
     */
    ~TcpListener()
    {
      stop();
    }

    /**
     * @brief Returns mutable access to the listening socket.
     *
     * @return socket_type& Listening socket
     */
    [[nodiscard]] socket_type &socket() noexcept
    {
      return socket_;
    }

    /**
     * @brief Returns const access to the listening socket.
     *
     * @return const socket_type& Listening socket
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
     * @brief Returns the effective listener configuration.
     *
     * If no explicit config was provided, server.config() is used.
     *
     * @return const config_type& Effective config
     */
    [[nodiscard]] const config_type &config() const noexcept
    {
      return has_explicit_config_ ? config_ : server_->config();
    }

    /**
     * @brief Returns true if the listener is currently started.
     *
     * @return true if listening
     */
    [[nodiscard]] bool running() const noexcept
    {
      return running_.load();
    }

    /**
     * @brief Returns true if the listener is ready to accept connections.
     *
     * @return true if started and socket is valid
     */
    [[nodiscard]] bool valid() const noexcept
    {
      return server_ != nullptr && socket_.valid();
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
     * @brief Start the listener using the effective configuration.
     *
     * Creates the listening socket, applies options, binds,
     * and starts listening.
     *
     * @throws SocketError if startup fails
     */
    void start()
    {
      ensure_server();

      if (running())
      {
        return;
      }

      socket_ = socket_type::create_tcp();
      socket_.set_reuse_addr(true);
      socket_.set_reuse_port(true);

      const auto &cfg = config();
      socket_.bind(cfg.host, cfg.port);
      socket_.listen(cfg.backlog);

      running_ = true;
    }

    /**
     * @brief Stop the listener and close the listening socket.
     */
    void stop() noexcept
    {
      running_ = false;
      socket_.close();
    }

    /**
     * @brief Accept one client connection.
     *
     * @return connection_type Accepted connection
     * @throws SocketError if accept fails
     */
    [[nodiscard]] connection_type accept()
    {
      ensure_running();
      return connection_type(socket_.accept(), *server_);
    }

    /**
     * @brief Process one accepted connection.
     *
     * Accepts a client and processes exactly one request/response cycle.
     *
     * @throws SocketError if accept or connection processing fails
     */
    void run_once()
    {
      auto connection = accept();
      connection.process();
    }

    /**
     * @brief Run a blocking accept loop.
     *
     * Accepts and processes incoming connections until stop() is called
     * or an exception escapes from connection processing.
     *
     * @throws SocketError if accept fails
     */
    void run()
    {
      ensure_running();

      while (running())
      {
        auto connection = accept();
        connection.process();
      }
    }

  private:
    socket_type socket_{};
    const server_type *server_{nullptr};
    config_type config_{};
    bool has_explicit_config_{false};
    std::atomic<bool> running_{false};

    /**
     * @brief Throw if the server reference is missing.
     *
     * @throws SocketError if server is null
     */
    void ensure_server() const
    {
      if (server_ == nullptr)
      {
        throw SocketError("TCP listener has no associated server");
      }
    }

    /**
     * @brief Throw if the listener is not started.
     *
     * @throws SocketError if listener is not running
     */
    void ensure_running() const
    {
      ensure_server();

      if (!running() || !socket_.valid())
      {
        throw SocketError("TCP listener is not running");
      }
    }
  };

} // namespace cnerium::server::net
