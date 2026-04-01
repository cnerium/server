#include <cassert>
#include <string>

#include <cnerium/server/server.hpp>
#include <cnerium/http/Method.hpp>

using namespace cnerium::server;

int main()
{
  Server server;

  server.get("/users/:id", [](Context &ctx)
             {
               auto id = ctx.params().get("id");
               ctx.response().text(std::string(id)); });

  cnerium::http::Request req;
  req.set_method(cnerium::http::Method::Get);
  req.set_path("/users/42");

  auto res = server.handle(std::move(req));

  assert(res.body() == "42");

  return 0;
}
