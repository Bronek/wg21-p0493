#include <algorithm>
#include <atomic>


template <typename T>
auto atomic_fetch_max_explicit(std::atomic<T> *pv,
                               typename std::atomic<T>::value_type v,
                               std::memory_order m) noexcept -> T {
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
