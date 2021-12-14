#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>

/**
 * @brief enum for different implementation types
 */
enum class type_e : std::size_t { strong = 0, weak, smart, hardware };

inline auto format(type_e i) noexcept -> const char * {
  switch (i) {
  case type_e::strong:
    return "strong";
  case type_e::weak:
    return "weak";
  case type_e::smart:
    return "smart";
  case type_e::hardware:
    return "hardware";
  }
  return "what?";
}

/**
 * @brief templated fetch_max implementation, with varying semantics
 *
 * @tparam mpl_e implementation type
 */
template <type_e> struct atomic_fetch_max;

template <> struct atomic_fetch_max<type_e::strong> final {
  template <typename T>
  auto operator()(std::atomic<T> *pv, typename std::atomic<T>::value_type v,
                  std::memory_order m) const noexcept -> T {
    using std::max;

    auto t = pv->load(m);
    while (!pv->compare_exchange_weak(t, max(v, t), m, m))
      ;
    return t;
  }
};

template <> struct atomic_fetch_max<type_e::weak> final {
  template <typename T>
  auto operator()(std::atomic<T> *pv, typename std::atomic<T>::value_type v,
                  std::memory_order m) const noexcept -> T {
    using std::max;

    auto t = pv->load(m);
    while (max(v, t) != t) {
      if (pv->compare_exchange_weak(t, v, m, m))
        break;
    }
    return t;
  }
};

template <> struct atomic_fetch_max<type_e::smart> final {
  template <typename T>
  auto operator()(std::atomic<T> *pv, typename std::atomic<T>::value_type v,
                  std::memory_order m) const noexcept -> T {
    using std::max;

    auto t = pv->load(m);
    while (max(v, t) != t) {
      if (pv->compare_exchange_weak(t, v, m, m))
        return t;
    }

    // additional dummy write for release operation
    if (m == std::memory_order_release || //
        m == std::memory_order_acq_rel || //
        m == std::memory_order_seq_cst)
      pv->fetch_add(0, m);

    return t;
  }
};

// TODO
template <> struct atomic_fetch_max<type_e::hardware> final {
  auto operator()(std::atomic<int> *, int, std::memory_order) const noexcept
      -> int {
    __builtin_unreachable();
  }
};
