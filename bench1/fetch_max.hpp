#pragma once

#include "config.hpp"

#include <algorithm>
#include <atomic>

template <config::impl_e> struct atomic_fetch_max;

template <> struct atomic_fetch_max<config::strong> final {
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

template <> struct atomic_fetch_max<config::weak> final {
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

template <> struct atomic_fetch_max<config::smart> final {
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
template <> struct atomic_fetch_max<config::hardware>;
