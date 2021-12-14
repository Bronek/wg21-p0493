#pragma once

#include <bitset>
#include <cstdint>

struct config {
  // sizes are selected to fit into a L2/L3 cache of a modern server-type CPU
  static constexpr std::size_t small = (1 << 12);  // 4'096 entries
  static constexpr std::size_t medium = (1 << 15); // 32'768 entries
  static constexpr std::size_t large = (1 << 18);  // 262'144 entries
  static constexpr std::size_t xlarge = (1 << 21);  // 2'097'152 entries
  static constexpr int max_cpus = 64;

  std::bitset<max_cpus> cpus{};
  std::size_t size{};
  enum impl_e { strong=0, weak, smart, hardware } impl;
};

inline auto format(config::impl_e i) noexcept -> const char * {
  switch (i) {
  case config::strong:
    return "strong";
  case config::weak:
    return "weak";
  case config::smart:
    return "smart";
  case config::hardware:
    return "hardware";
  }
  return "what?";
}
