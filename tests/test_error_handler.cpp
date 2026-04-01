#include <cassert>
#include <stdexcept>

#include <cnerium/server/server.hpp>
#include <cnerium/http/Method.hpp>
#include <cnerium/http/Status.hpp>

using namespace cnerium::server;

int main()
{
  Server server;

  server.set_error_handler([](Context &ctx, const std::exception &)
                           {
                             ctx.response().set_status(cnerium::http::Status::internal_server_error);
                             ctx.response().text("error"); });

  server.get("/boom", [](Context &)
             { throw std::runtime_error("fail"); });

  cnerium::http::Request req;
  req.set_method(cnerium::http::Method::Get);
  req.set_path("/boom");

  auto res = server.handle(std::move(req));

  assert(res.status() == cnerium::http::Status::internal_server_error);
  assert(res.body() == "error");

  return 0;
}
