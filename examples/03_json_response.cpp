/**
 * @file 03_json_response.cpp
 * @brief JSON response example for cnerium::server
 *
 * @details
 * Demonstrates:
 *   - JSON responses
 *   - request method branching
 *   - route parameters with JSON payloads
 *
 * Test with:
 *   curl http://127.0.0.1:8080/
 *   curl http://127.0.0.1:8080/users/42
 */

#include <iostream>
#include <string>

#include <cnerium/server/server.hpp>
#include <cnerium/json/json.hpp>

namespace
{
  cnerium::server::Server build_server()
  {
    using cnerium::json::array;
    using cnerium::json::object;
    using cnerium::json::value;
    using cnerium::server::Context;
    using cnerium::server::Server;

    Server server;

    server.get("/", [](Context &ctx)
               { ctx.response().json(object{
                                         {"ok", true},
                                         {"framework", "Cnerium"},
                                         {"version", "0.1.0"}},
                                     true); });

    server.get("/users/:id", [](Context &ctx)
               {
                 const auto id = ctx.params().get("id");

                 ctx.response().json(object{
                     {"ok", true},
                     {"user", object{
                         {"id", std::string(id)},
                         {"name", "Gaspard"},
                         {"skills", array{"C++", "HTTP", "Systems"}}
                     }}
                 }, true); });

    server.get("/health", [](Context &ctx)
               { ctx.response().json(object{
                     {"status", "ok"},
                     {"uptime", "ready"}}); });

    return server;
  }

  void print_startup_message(const cnerium::server::Config &config)
  {
    std::cout << "JSON response example started\n";
    std::cout << "Listening on http://" << config.host << ":" << config.port << "/\n";
    std::cout << "Try:\n";
    std::cout << "  curl http://" << config.host << ":" << config.port << "/\n";
    std::cout << "  curl http://" << config.host << ":" << config.port << "/users/42\n";
    std::cout << "  curl http://" << config.host << ":" << config.port << "/health\n";
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
