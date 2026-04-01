# cnerium/server

High-level HTTP server for the Cnerium web framework.

**Header-only. Deterministic. Routing + Middleware + TCP.**

---

## Download

https://vixcpp.com/registry/pkg/cnerium/server

---

## Overview

`cnerium/server` is the execution core of the Cnerium web stack.

It integrates:

- routing (`cnerium/router`)
- middleware (`cnerium/middleware`)
- HTTP primitives (`cnerium/http`)
- TCP networking (built-in)

It is responsible for:

- accepting TCP connections
- parsing HTTP requests
- matching routes
- executing middleware
- generating HTTP responses

It is designed to be:

- minimal
- predictable
- fully deterministic
- easy to extend

---

## Why cnerium/server?

Most web servers mix:

- networking
- routing
- middleware
- request parsing
- response serialization

This leads to:

- tight coupling
- hard debugging
- unclear execution flow

`cnerium/server` separates concerns:

- **router** → matching
- **middleware** → flow control
- **server** → orchestration
- **net** → transport

Result:

- clean architecture
- predictable execution
- easy to reason about

---

## Dependencies

Depends on:

- `cnerium/http`
- `cnerium/router`
- `cnerium/middleware`
- `cnerium/json` (optional, for JSON responses)

This ensures:

- consistent request/response model
- reusable middleware
- clean routing system

---

## Installation

### Using Vix

```bash
vix add @cnerium/server
vix install
```

### Manual

```bash
git clone https://github.com/cnerium/server.git
```

Add `include/` to your project.

---

## Core Concepts

### Server

```cpp
Server server;
```

Main entry point of the framework.

### Route

```cpp
server.get("/users/:id", [](Context &ctx)
{
  auto id = ctx.params().get("id");
});
```

### Middleware

```cpp
server.use([](Context &ctx, Next next)
{
  ctx.response().set_header("X-App", "Cnerium");
  next();
});
```

### Context

```cpp
Context ctx;
```

Provides access to:

- request
- response
- route params

### Handler

```cpp
[](Context &ctx)
{
  ctx.response().text("Hello");
}
```

---

## Typical Flow

```text
TCP → parse → route → middleware → handler → response
```

---

## Example

```cpp
#include <cnerium/server/server.hpp>

using namespace cnerium::server;

int main()
{
  Server server;

  server.use([](auto &ctx, auto next)
  {
    ctx.response().set_header("X-App", "Cnerium");
    next();
  });

  server.get("/", [](Context &ctx)
  {
    ctx.response().text("Hello from Cnerium");
  });

  server.run();
}
```

---

## Routing Example

```cpp
server.get("/users/:id", [](Context &ctx)
{
  auto id = ctx.params().get("id");
  ctx.response().text(std::string(id));
});
```

---

## Middleware Example

```cpp
server.use([](Context &ctx, Next next)
{
  if (ctx.request().path() == "/admin")
  {
    ctx.response().text("Access denied");
    return;
  }

  next();
});
```

---

## JSON Response Example

```cpp
server.get("/", [](Context &ctx)
{
  ctx.response().json({
    {"ok", true},
    {"framework", "Cnerium"}
  });
});
```

---

## Error Handling

```cpp
server.set_error_handler([](Context &ctx, const std::exception &e)
{
  ctx.response().set_status(Status::internal_server_error);
  ctx.response().text("Error");
});
```

---

## Not Found

```cpp
server.set_not_found_handler([](Context &ctx)
{
  ctx.response().set_status(Status::not_found);
  ctx.response().text("Not found");
});
```

---

## Execution Rules

- routes are evaluated in insertion order
- first match wins
- middleware runs before handler
- middleware can stop execution
- exceptions are caught and converted to responses

---

## Complexity

| Operation | Complexity |
|----------|-----------|
| route matching | O(n) |
| middleware execution | O(n) |
| request handling | O(n) |

---

## Design Philosophy

- minimal server core
- deterministic execution
- separation of concerns
- no hidden magic
- composable architecture

---

## Tests

```bash
vix build
vix test
```

---

## License

MIT License
Copyright (c) Gaspard Kirira

