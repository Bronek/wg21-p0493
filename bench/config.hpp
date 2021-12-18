#pragma once

#include "../fetch_max.hpp"

#include <atomic>
#include <bitset>
#include <cstdint>

struct config {
  static constexpr int max_cpus = 64;

  type_e impl;
  std::bitset<max_cpus> cpus{};
  std::size_t iter{0};
  int seed{0};
  double max_sigma{0};
  std::memory_order operation{std::memory_order_seq_cst};
};
