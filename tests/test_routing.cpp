#include <cassert>
#include <cnerium/server/server.hpp>
#include <cnerium/http/Method.hpp>
#include <cnerium/http/Status.hpp>

using namespace cnerium::server;

int main()
{
  Server server;

  server.get("/", [](Context &ctx)
             { ctx.response().text("home"); });

  cnerium::http::Request req;
  req.set_method(cnerium::http::Method::Get);
  req.set_path("/");

  auto res = server.handle(std::move(req));

  assert(res.status() == cnerium::http::Status::ok);
  assert(res.body() == "home");

  return 0;
}
