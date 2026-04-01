/**
 * @file 04_middleware_pipeline.cpp
 * @brief Middleware pipeline example for cnerium::server
 *
 * @details
 * Demonstrates:
 *   - multiple global middleware
 *   - header mutation
 *   - request inspection
 *   - short-circuit behavior
 *
 * Test with:
 *   curl -i http://127.0.0.1:8080/
 *   curl -i http://127.0.0.1:8080/admin
 *   curl -i -H "X-Auth: secret" http://127.0.0.1:8080/admin
 */

#include <iostream>
#include <string_view>

#include <cnerium/server/server.hpp>
#include <cnerium/middleware/middleware.hpp>
#include <cnerium/http/Status.hpp>

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

    server.use([](Context &ctx, Next next)
               {
                 ctx.response().set_header("X-Request-Path",
                                           std::string(ctx.request().path()));
                 next(); });

    server.use([](Context &ctx, Next next)
               {
                 if (ctx.request().path() == "/admin")
                 {
                   const std::string_view auth = ctx.request().header("X-Auth");
                   if (auth != "secret")
                   {
                     ctx.response().set_status(cnerium::http::Status::unauthorized);
                     ctx.response().text("Unauthorized");
                     return;
                   }
                 }

                 next(); });

    server.get("/", [](cnerium::server::Context &ctx)
               { ctx.response().text("Public route"); });

    server.get("/admin", [](cnerium::server::Context &ctx)
               { ctx.response().text("Welcome to admin"); });

    return server;
  }

  void print_startup_message(const cnerium::server::Config &config)
  {
    std::cout << "Middleware pipeline example started\n";
    std::cout << "Listening on http://" << config.host << ":" << config.port << "/\n";
    std::cout << "Try:\n";
    std::cout << "  curl -i http://" << config.host << ":" << config.port << "/\n";
    std::cout << "  curl -i http://" << config.host << ":" << config.port << "/admin\n";
    std::cout << "  curl -i -H \"X-Auth: secret\" http://" << config.host << ":" << config.port << "/admin\n";
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
