#pragma once
#include <cstddef>
#include <vector>

namespace server
{
  struct Node
  {
    std::size_t id{};
    std::vector<std::size_t> children{};
  };

  inline std::vector<Node> make_chain(std::size_t n)
  {
    std::vector<Node> nodes;
    nodes.reserve(n);

    for (std::size_t i = 0; i < n; ++i)
      nodes.push_back(Node{i, {}});

    for (std::size_t i = 0; i + 1 < n; ++i)
      nodes[i].children.push_back(i + 1);

    return nodes;
  }
}
