#include <cassert>

#include <cnerium/server/server.hpp>
#include <cnerium/middleware/middleware.hpp>
#include <cnerium/http/Method.hpp>

using namespace cnerium::server;
using namespace cnerium::middleware;

int main()
{
  Server server;

  server.use([](cnerium::middleware::Context &ctx, Next next)
             {
               ctx.response().set_header("X-Test", "yes");
               next(); });

  server.get("/", [](cnerium::server::Context &ctx)
             { ctx.response().text("ok"); });

  cnerium::http::Request req;
  req.set_method(cnerium::http::Method::Get);
  req.set_path("/");

  auto res = server.handle(std::move(req));

  assert(res.header("X-Test") == "yes");
  assert(res.body() == "ok");

  return 0;
}
