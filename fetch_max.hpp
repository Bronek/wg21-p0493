#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>

/**
 * @brief enum for different implementation types
 */
enum class type_e : std::size_t { strong = 0, weak, smart, hardware, faster };

/**
 * @brief templated fetch_max implementation, with varying semantics
 *
 * @tparam mpl_e implementation type
 */
template <type_e> struct atomic_fetch_max;

inline constexpr std::memory_order drop_release(std::memory_order m) noexcept {
  return (m == std::memory_order_release ? std::memory_order_relaxed
                                         : ((m == std::memory_order_acq_rel ||
                                             m == std::memory_order_seq_cst)
                                                ? std::memory_order_acquire
                                                : m));
}

template <> struct atomic_fetch_max<type_e::strong> final {
  template <typename T>
  auto operator()(std::atomic<T> *pv, typename std::atomic<T>::value_type v,
                  std::memory_order m) const noexcept -> T {
    using std::max;

    auto t = pv->load(drop_release(m));
    while (!pv->compare_exchange_weak(t, max(v, t), m, drop_release(m)))
      ;
    return t;
  }
};

template <> struct atomic_fetch_max<type_e::weak> final {
  template <typename T>
  auto operator()(std::atomic<T> *pv, typename std::atomic<T>::value_type v,
                  std::memory_order m) const noexcept -> T {
    using std::max;

    auto t = pv->load(drop_release(m));
    while (max(v, t) != t || (m == std::memory_order_release || //
                              m == std::memory_order_acq_rel || //
                              m == std::memory_order_seq_cst)) {
      if (pv->compare_exchange_weak(t, v, m, drop_release(m))) {
        break;
      }
    }
    return t;
  }
};

template <> struct atomic_fetch_max<type_e::smart> final {
  template <typename T>
  auto operator()(std::atomic<T> *pv, typename std::atomic<T>::value_type v,
                  std::memory_order m) const noexcept -> T {
    using std::max;

    auto const mr = drop_release(m);
    auto t = (mr != m) ? pv->fetch_add(0, m) : pv->load(mr);
    while (max(v, t) != t) {
      if (pv->compare_exchange_weak(t, v, m, mr))
        return t;
    }

    return t;
  }
};

template <> struct atomic_fetch_max<type_e::hardware> final {
#if defined(__ARM_ARCH_8A)
  auto operator()(std::atomic<int> *pv, int v,
                  std::memory_order m) const noexcept -> int {
    using namespace std;
    int x;
    if (m == memory_order_relaxed)
      __asm__ __volatile__("ldsmax %w1,%w0,[%2]" : "=r"(x) : "r"(v), "r"(pv) :);
    else if (m == memory_order_release)
      __asm__ __volatile__("ldsmaxl %w1,%w0,[%2]"
                           : "=r"(x)
                           : "r"(v), "r"(pv)
                           :);
    else if (m == memory_order_acquire || m == memory_order_consume)
      __asm__ __volatile__("ldsmaxa %w1,%w0,[%2]"
                           : "=r"(x)
                           : "r"(v), "r"(pv)
                           :);
    else if (m == memory_order_acq_rel || m == memory_order_seq_cst)
      __asm__ __volatile__("ldsmaxal %w1,%w0,[%2]"
                           : "=r"(x)
                           : "r"(v), "r"(pv)
                           :);
    else {
      __builtin_unreachable();
    }
    return x;
  }
#else
  auto operator()(std::atomic<int> *, int, std::memory_order) const noexcept
      -> int {
    __builtin_unreachable();
  }
#endif
};

template <> struct atomic_fetch_max<type_e::faster> final {
  auto operator()(std::atomic<int> *pv, int v,
                  std::memory_order m) const noexcept -> int {
    using namespace std;
    using std::max;

    if (m == std::memory_order_release || //
        m == std::memory_order_acq_rel || //
        m == std::memory_order_seq_cst) {
      return atomic_fetch_max<type_e::hardware>{}(pv, v, m);
    } else {
      auto t = pv->load(drop_release(m));
      if (max(t, v) != t) {
        return atomic_fetch_max<type_e::hardware>{}(pv, v, m);
      }
      return t;
    }
  }
};
