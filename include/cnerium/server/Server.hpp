/**
 * @file Server.hpp
 * @brief cnerium::server — Main HTTP server core
 *
 * @version 0.1.0
 * @author Gaspard Kirira
 * @copyright (c) 2026 Gaspard Kirira
 * @license MIT
 *
 * @details
 * Defines the main Cnerium server core responsible for:
 *   - storing routes and their handlers
 *   - storing global middleware
 *   - dispatching incoming requests
 *   - filling matched route parameters
 *   - invoking handlers
 *   - handling errors and 404 fallbacks
 *   - starting and running the TCP listener
 *
 * This class is the high-level execution core of the framework.
 * It owns the logical server state and can also manage the underlying
 * network listener used to accept and process incoming TCP connections.
 *
 * Responsibilities:
 *   - register routes by HTTP method and path
 *   - register global middleware
 *   - match requests against registered routes
 *   - execute middleware chain in insertion order
 *   - execute the matched route handler
 *   - build a valid HTTP response in all cases
 *   - start and stop the listening socket
 *   - run blocking accept loops through TcpListener
 *
 * Design goals:
 *   - Header-only
 *   - Deterministic and synchronous execution
 *   - Clear separation between routing and transport
 *   - Easy extension for runtime integration
 *
 * Notes:
 *   - Routes are evaluated in insertion order
 *   - The first matching route wins
 *   - Middleware is global and runs before the final route handler
 *   - If no route matches, the not_found handler is called
 *   - If an exception is thrown, the error handler is called
 *   - Network I/O is delegated to cnerium::server::net::TcpListener
 *
 * Usage:
 * @code
 *   using namespace cnerium::server;
 *
 *   Server server;
 *
 *   server.use([](cnerium::middleware::Context &ctx, cnerium::middleware::Next next)
 *   {
 *     ctx.response().set_header("X-Powered-By", "Cnerium");
 *     next();
 *   });
 *
 *   server.get("/", [](Context &ctx)
 *   {
 *     ctx.response().text("Hello, world!");
 *   });
 *
 *   server.listen();
 * @endcode
 */

#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <cnerium/http/Method.hpp>
#include <cnerium/http/Request.hpp>
#include <cnerium/http/Response.hpp>
#include <cnerium/middleware/Middleware.hpp>
#include <cnerium/router/Route.hpp>
#include <cnerium/server/Config.hpp>
#include <cnerium/server/Context.hpp>
#include <cnerium/server/ErrorHandler.hpp>
#include <cnerium/server/Handler.hpp>
#include <cnerium/server/not_found.hpp>

namespace cnerium::server
{
  namespace net
  {
    class TcpListener;
  }

  /**
   * @brief Internal route entry stored by the server.
   *
   * Associates:
   *   - a route definition used for method/path matching
   *   - a handler executed when the route matches
   *
   * Design goals:
   *   - Simple aggregation object
   *   - No hidden behavior
   *   - Cheap to move and store in a vector
   *
   * Notes:
   *   - Stored in insertion order
   *   - Used internally by cnerium::server::Server
   *   - A route entry is considered valid if its handler is set
   */
  struct RouteEntry
  {
    using route_type = cnerium::router::Route;
    using handler_type = cnerium::server::Handler;

    /// @brief Default constructor.
    RouteEntry() = default;

    /**
     * @brief Construct a route entry from route and handler.
     *
     * @param route Route definition
     * @param handler Route handler
     */
    RouteEntry(route_type route, handler_type handler)
        : route(std::move(route)),
          handler(std::move(handler))
    {
    }

    /**
     * @brief Returns true if the route entry has a valid handler.
     *
     * @return true if handler is set
     */
    [[nodiscard]] bool valid() const noexcept
    {
      return static_cast<bool>(handler);
    }

    /**
     * @brief Reset the route entry to default state.
     */
    void clear()
    {
      route.clear();
      handler = nullptr;
    }

    /// @brief Route definition used for matching.
    route_type route{};

    /// @brief Handler executed when the route matches.
    handler_type handler{};
  };

} // namespace cnerium::server

#include <cnerium/server/detail/RouteDispatch.hpp>

namespace cnerium::server
{
  /**
   * @brief High-level HTTP server execution core.
   *
   * Stores routes, middleware, handlers, dispatch logic,
   * and manages the network listener.
   */
  class Server
  {
  public:
    using method_type = cnerium::http::Method;
    using request_type = cnerium::http::Request;
    using response_type = cnerium::http::Response;
    using context_type = cnerium::server::Context;
    using handler_type = cnerium::server::Handler;
    using error_handler_type = cnerium::server::ErrorHandler;
    using middleware_type = cnerium::middleware::Middleware;
    using route_entry_type = cnerium::server::RouteEntry;
    using route_storage_type = std::vector<route_entry_type>;
    using middleware_storage_type = std::vector<middleware_type>;
    using route_iterator = route_storage_type::iterator;
    using const_route_iterator = route_storage_type::const_iterator;
    using middleware_iterator = middleware_storage_type::iterator;
    using const_middleware_iterator = middleware_storage_type::const_iterator;

    /// @brief Default constructor.
    Server() = default;

    /**
     * @brief Construct a server with explicit configuration.
     *
     * @param config Server configuration
     */
    explicit Server(Config config)
        : config_(std::move(config))
    {
    }

    Server(const Server &) = delete;
    Server &operator=(const Server &) = delete;

    /**
     * @brief Move constructor.
     *
     * @param other Source server
     */
    Server(Server &&other) noexcept
        : config_(std::move(other.config_)),
          routes_(std::move(other.routes_)),
          middleware_(std::move(other.middleware_)),
          error_handler_(std::move(other.error_handler_)),
          not_found_handler_(std::move(other.not_found_handler_)),
          listener_(std::move(other.listener_))
    {
    }

    /**
     * @brief Move assignment operator.
     *
     * @param other Source server
     * @return Server& Self
     */
    Server &operator=(Server &&other) noexcept
    {
      if (this != &other)
      {
        stop();

        config_ = std::move(other.config_);
        routes_ = std::move(other.routes_);
        middleware_ = std::move(other.middleware_);
        error_handler_ = std::move(other.error_handler_);
        not_found_handler_ = std::move(other.not_found_handler_);
        listener_ = std::move(other.listener_);
      }

      return *this;
    }

    /**
     * @brief Destructor.
     *
     * Stops the listener if needed.
     */
    ~Server();

    /**
     * @brief Returns mutable access to the server configuration.
     *
     * @return Config& Configuration
     */
    [[nodiscard]] Config &config() noexcept
    {
      return config_;
    }

    /**
     * @brief Returns const access to the server configuration.
     *
     * @return const Config& Configuration
     */
    [[nodiscard]] const Config &config() const noexcept
    {
      return config_;
    }

    /**
     * @brief Replace the current server configuration.
     *
     * The listener must not be running when changing network configuration.
     *
     * @param config New configuration
     * @return Server& Self for chaining
     */
    Server &set_config(Config config)
    {
      if (listening())
      {
        throw std::logic_error("cannot change server config while listening");
      }

      config_ = std::move(config);
      return *this;
    }

    /**
     * @brief Register a global middleware.
     *
     * Middleware runs in insertion order before the final route handler.
     *
     * @param middleware Middleware callable
     * @return Server& Self for chaining
     */
    Server &use(middleware_type middleware)
    {
      middleware_.push_back(std::move(middleware));
      return *this;
    }

    /**
     * @brief Set the error handler.
     *
     * @param handler Error handler callable
     * @return Server& Self for chaining
     */
    Server &set_error_handler(error_handler_type handler)
    {
      error_handler_ = std::move(handler);
      return *this;
    }

    /**
     * @brief Set the not-found fallback handler.
     *
     * @param handler Fallback handler
     * @return Server& Self for chaining
     */
    Server &set_not_found_handler(handler_type handler)
    {
      not_found_handler_ = std::move(handler);
      return *this;
    }

    /**
     * @brief Add a route with explicit method and pattern.
     *
     * Routes are evaluated in the same order they are added.
     *
     * @param method HTTP method
     * @param pattern Route pattern
     * @param handler Route handler
     * @return RouteEntry& Stored route entry
     */
    route_entry_type &add(method_type method,
                          std::string pattern,
                          handler_type handler)
    {
      routes_.emplace_back(
          cnerium::router::Route(method, std::move(pattern)),
          std::move(handler));
      return routes_.back();
    }

    /**
     * @brief Add an already constructed route entry.
     *
     * @param entry Route entry
     * @return RouteEntry& Stored route entry
     */
    route_entry_type &add(route_entry_type entry)
    {
      routes_.push_back(std::move(entry));
      return routes_.back();
    }

    /**
     * @brief Register a GET route.
     */
    route_entry_type &get(std::string pattern, handler_type handler)
    {
      return add(method_type::Get, std::move(pattern), std::move(handler));
    }

    /**
     * @brief Register a POST route.
     */
    route_entry_type &post(std::string pattern, handler_type handler)
    {
      return add(method_type::Post, std::move(pattern), std::move(handler));
    }

    /**
     * @brief Register a PUT route.
     */
    route_entry_type &put(std::string pattern, handler_type handler)
    {
      return add(method_type::Put, std::move(pattern), std::move(handler));
    }

    /**
     * @brief Register a PATCH route.
     */
    route_entry_type &patch(std::string pattern, handler_type handler)
    {
      return add(method_type::Patch, std::move(pattern), std::move(handler));
    }

    /**
     * @brief Register a DELETE route.
     */
    route_entry_type &del(std::string pattern, handler_type handler)
    {
      return add(method_type::Delete, std::move(pattern), std::move(handler));
    }

    /**
     * @brief Register a HEAD route.
     */
    route_entry_type &head(std::string pattern, handler_type handler)
    {
      return add(method_type::Head, std::move(pattern), std::move(handler));
    }

    /**
     * @brief Register an OPTIONS route.
     */
    route_entry_type &options(std::string pattern, handler_type handler)
    {
      return add(method_type::Options, std::move(pattern), std::move(handler));
    }

    /**
     * @brief Dispatch a request and return the generated response.
     *
     * @param request Incoming HTTP request
     * @return response_type Outgoing HTTP response
     */
    [[nodiscard]] response_type handle(request_type request) const
    {
      context_type ctx;
      ctx.set_request(std::move(request));
      handle(ctx);
      return ctx.response();
    }

    /**
     * @brief Dispatch a request/response context in place.
     *
     * Route matching, middleware execution, handler invocation,
     * not-found fallback, and error handling all happen here.
     *
     * @param ctx Server execution context
     */
    void handle(context_type &ctx) const
    {
      cnerium::server::detail::dispatch(
          ctx,
          routes_,
          middleware_,
          not_found_handler_,
          error_handler_);
    }

    /**
     * @brief Returns true if at least one route matches.
     *
     * @param method Incoming HTTP method
     * @param path Incoming request path
     * @return true if a route matches
     */
    [[nodiscard]] bool matches(method_type method,
                               std::string_view path) const
    {
      return cnerium::server::detail::find_route(routes_, method, path).found();
    }

    /**
     * @brief Start the TCP listener with the current configuration.
     *
     * @throws std::logic_error if already listening
     */
    void start();

    /**
     * @brief Alias for start().
     */
    void listen()
    {
      start();
    }

    /**
     * @brief Run the blocking accept loop.
     *
     * Starts the listener automatically if needed.
     */
    void run();

    /**
     * @brief Accept and process exactly one connection.
     *
     * Starts the listener automatically if needed.
     */
    void run_once();

    /**
     * @brief Stop the listener and close the listening socket.
     */
    void stop() noexcept;

    /**
     * @brief Returns true if the TCP listener is active.
     *
     * @return true if listening
     */
    [[nodiscard]] bool listening() const noexcept;

    /**
     * @brief Returns the number of registered routes.
     *
     * @return std::size_t Route count
     */
    [[nodiscard]] std::size_t route_count() const noexcept
    {
      return routes_.size();
    }

    /**
     * @brief Returns the number of registered middleware.
     *
     * @return std::size_t Middleware count
     */
    [[nodiscard]] std::size_t middleware_count() const noexcept
    {
      return middleware_.size();
    }

    /**
     * @brief Returns true if no routes are registered.
     *
     * @return true if empty
     */
    [[nodiscard]] bool empty() const noexcept
    {
      return routes_.empty();
    }

    /**
     * @brief Reserve storage for route entries.
     *
     * @param n Number of route entries
     */
    void reserve_routes(std::size_t n)
    {
      routes_.reserve(n);
    }

    /**
     * @brief Reserve storage for middleware entries.
     *
     * @param n Number of middleware entries
     */
    void reserve_middleware(std::size_t n)
    {
      middleware_.reserve(n);
    }

    /**
     * @brief Remove all registered routes.
     */
    void clear_routes() noexcept
    {
      routes_.clear();
    }

    /**
     * @brief Remove all registered middleware.
     */
    void clear_middleware() noexcept
    {
      middleware_.clear();
    }

    /**
     * @brief Reset the server to its default logical state.
     *
     * Clears routes and middleware, restores default handlers,
     * and stops the network listener.
     */
    void clear()
    {
      stop();
      routes_.clear();
      middleware_.clear();
      error_handler_ = default_error_handler;
      not_found_handler_ = cnerium::server::not_found;
    }

    /**
     * @brief Returns const access to the route storage.
     *
     * @return const route_storage_type& Routes
     */
    [[nodiscard]] const route_storage_type &routes() const noexcept
    {
      return routes_;
    }

    /**
     * @brief Returns mutable access to the route storage.
     *
     * @return route_storage_type& Routes
     */
    [[nodiscard]] route_storage_type &routes() noexcept
    {
      return routes_;
    }

    /**
     * @brief Returns const access to the middleware storage.
     *
     * @return const middleware_storage_type& Middleware
     */
    [[nodiscard]] const middleware_storage_type &middleware() const noexcept
    {
      return middleware_;
    }

    /**
     * @brief Returns mutable access to the middleware storage.
     *
     * @return middleware_storage_type& Middleware
     */
    [[nodiscard]] middleware_storage_type &middleware() noexcept
    {
      return middleware_;
    }

    /// @brief Mutable begin iterator over routes.
    [[nodiscard]] route_iterator begin() noexcept
    {
      return routes_.begin();
    }

    /// @brief Mutable end iterator over routes.
    [[nodiscard]] route_iterator end() noexcept
    {
      return routes_.end();
    }

    /// @brief Const begin iterator over routes.
    [[nodiscard]] const_route_iterator begin() const noexcept
    {
      return routes_.begin();
    }

    /// @brief Const end iterator over routes.
    [[nodiscard]] const_route_iterator end() const noexcept
    {
      return routes_.end();
    }

    /// @brief Const begin iterator over routes.
    [[nodiscard]] const_route_iterator cbegin() const noexcept
    {
      return routes_.cbegin();
    }

    /// @brief Const end iterator over routes.
    [[nodiscard]] const_route_iterator cend() const noexcept
    {
      return routes_.cend();
    }

  private:
    Config config_{};
    route_storage_type routes_{};
    middleware_storage_type middleware_{};
    error_handler_type error_handler_{default_error_handler};
    handler_type not_found_handler_{cnerium::server::not_found};
    std::unique_ptr<cnerium::server::net::TcpListener> listener_{};
  };

} // namespace cnerium::server

#include <stdexcept>
#include <cnerium/server/net/TcpListener.hpp>

namespace cnerium::server
{
  inline Server::~Server()
  {
    stop();
  }

  inline void Server::start()
  {
    if (listening())
    {
      throw std::logic_error("server is already listening");
    }

    listener_ = std::make_unique<cnerium::server::net::TcpListener>(*this);
    listener_->start();
  }

  inline void Server::run()
  {
    if (!listening())
    {
      start();
    }

    listener_->run();
  }

  inline void Server::run_once()
  {
    if (!listening())
    {
      start();
    }

    listener_->run_once();
  }

  inline void Server::stop() noexcept
  {
    if (listener_)
    {
      listener_->stop();
      listener_.reset();
    }
  }

  inline bool Server::listening() const noexcept
  {
    return listener_ && listener_->running();
  }

} // namespace cnerium::server
