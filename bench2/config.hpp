#pragma once

#include "../fetch_max.hpp"

#include <bitset>
#include <cstdint>

struct config {
  static constexpr int max_cpus = 64;

  type_e impl;
  std::bitset<max_cpus> cpus{};
  std::size_t iter{0};
  int seed{0};
  double max_sigma{0};
};
