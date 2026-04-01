/**
 * @file 01_hello_server.cpp
 * @brief Basic hello world server example for cnerium::server
 *
 * @details
 * Demonstrates:
 *   - server creation
 *   - global middleware registration
 *   - route registration
 *   - simple text response
 *   - blocking TCP listener startup
 *
 * Test with:
 *   curl http://127.0.0.1:8080/
 */

#include <iostream>

#include <cnerium/server/server.hpp>
#include <cnerium/middleware/middleware.hpp>

namespace
{
  cnerium::server::Server build_server()
  {
    using cnerium::middleware::Context;
    using cnerium::middleware::Next;
    using cnerium::server::Server;

    Server server;

    server.use([](Context &ctx, Next next)
               {
                 ctx.response().set_header("X-Powered-By", "Cnerium");
                 next(); });

    server.get("/", [](cnerium::server::Context &ctx)
               { ctx.response().text("Hello from Cnerium"); });

    return server;
  }

  void print_startup_message(const cnerium::server::Config &config)
  {
    std::cout << "Cnerium server example started\n";
    std::cout << "Listening on http://" << config.host << ":" << config.port << "/\n";
    std::cout << "Try: curl http://" << config.host << ":" << config.port << "/\n";
  }

  void run_server()
  {
    auto server = build_server();

    print_startup_message(server.config());
    server.run();
  }
} // namespace

int main()
{
  try
  {
    run_server();
  }
  catch (const std::exception &e)
  {
    std::cerr << "Server error: " << e.what() << '\n';
    return 1;
  }

  return 0;
}
