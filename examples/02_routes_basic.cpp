/**
 * @file 02_routes_basic.cpp
 * @brief Basic route registration example for cnerium::server
 *
 * @details
 * Demonstrates:
 *   - multiple route registration
 *   - route parameters
 *   - text responses
 *
 * Test with:
 *   curl http://127.0.0.1:8080/
 *   curl http://127.0.0.1:8080/about
 *   curl http://127.0.0.1:8080/users/42
 */

#include <iostream>
#include <string>

#include <cnerium/server/server.hpp>

namespace
{
  cnerium::server::Server build_server()
  {
    using cnerium::server::Context;
    using cnerium::server::Server;

    Server server;

    server.get("/", [](Context &ctx)
               { ctx.response().text("Home page"); });

    server.get("/about", [](Context &ctx)
               { ctx.response().text("About Cnerium"); });

    server.get("/users/:id", [](Context &ctx)
               {
                 const auto id = ctx.params().get("id");
                 ctx.response().text("User id: " + std::string(id)); });

    server.get("/shops/:shop_id/products/:product_id", [](Context &ctx)
               {
                 const auto shop_id = ctx.params().get("shop_id");
                 const auto product_id = ctx.params().get("product_id");

                 ctx.response().text(
                     "Shop: " + std::string(shop_id) +
                     ", Product: " + std::string(product_id)); });

    return server;
  }

  void print_startup_message(const cnerium::server::Config &config)
  {
    std::cout << "Routes example started\n";
    std::cout << "Listening on http://" << config.host << ":" << config.port << "/\n";
    std::cout << "Try:\n";
    std::cout << "  curl http://" << config.host << ":" << config.port << "/\n";
    std::cout << "  curl http://" << config.host << ":" << config.port << "/about\n";
    std::cout << "  curl http://" << config.host << ":" << config.port << "/users/42\n";
    std::cout << "  curl http://" << config.host << ":" << config.port << "/shops/7/products/15\n";
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
