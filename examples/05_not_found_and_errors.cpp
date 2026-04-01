/**
 * @file 05_not_found_and_errors.cpp
 * @brief Not-found and error handling example for cnerium::server
 *
 * @details
 * Demonstrates:
 *   - custom not-found handler
 *   - custom error handler
 *   - exception to HTTP response conversion
 *
 * Test with:
 *   curl -i http://127.0.0.1:8080/
 *   curl -i http://127.0.0.1:8080/missing
 *   curl -i http://127.0.0.1:8080/boom
 */

#include <iostream>
#include <stdexcept>

#include <cnerium/server/server.hpp>
#include <cnerium/http/Status.hpp>
#include <cnerium/json/json.hpp>

namespace
{
  cnerium::server::Server build_server()
  {
    using cnerium::json::object;
    using cnerium::server::Context;
    using cnerium::server::Server;

    Server server;

    server.set_not_found_handler([](Context &ctx)
                                 {
                                   ctx.response().set_status(cnerium::http::Status::not_found);
                                   ctx.response().json(object{
                                       {"ok", false},
                                       {"error", "Route not found"},
                                       {"path", std::string(ctx.request().path())}
                                   }, true); });

    server.set_error_handler([](Context &ctx, const std::exception &e)
                             {
                               ctx.response().set_status(cnerium::http::Status::internal_server_error);
                               ctx.response().json(object{
                                   {"ok", false},
                                   {"error", "Internal Server Error"},
                                   {"message", std::string(e.what())}
                               }, true); });

    server.get("/", [](Context &ctx)
               { ctx.response().text("Error handling demo"); });

    server.get("/boom", [](Context &)
               { throw std::runtime_error("Something exploded in the handler"); });

    return server;
  }

  void print_startup_message(const cnerium::server::Config &config)
  {
    std::cout << "Not-found and errors example started\n";
    std::cout << "Listening on http://" << config.host << ":" << config.port << "/\n";
    std::cout << "Try:\n";
    std::cout << "  curl -i http://" << config.host << ":" << config.port << "/\n";
    std::cout << "  curl -i http://" << config.host << ":" << config.port << "/missing\n";
    std::cout << "  curl -i http://" << config.host << ":" << config.port << "/boom\n";
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
