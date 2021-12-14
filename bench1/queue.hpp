#pragma once

#include <atomic>
#include <type_traits>
#include <vector>

#include <cstdint>
#include <cstdio>

template <typename T_, size_t Size_, typename FetchMax_> struct queue_t {
  static_assert(std::is_nothrow_default_constructible_v<T_>);
  static_assert(std::is_nothrow_copy_constructible_v<T_>);
  static_assert(std::is_nothrow_swappable_v<T_>);

  using element_t = T_;
  static constexpr int size = Size_;
  static constexpr FetchMax_ fetch_max = {};

  struct entry_t {
    element_t item{};         // a queue element
    std::atomic<int> tag{-1}; // its generation number
  };

  entry_t elts[size] = {}; // a bounded array
  std::atomic<int> back{-1};

  explicit queue_t() noexcept {
    // Visit elements to ensure that backing memory is:
    // 1. not over-committed
    // 2. paged in and
    // 3. stored in TLB
    int d = 1;
    for (size_t i = 0; i < size; i += 16) {
      auto &e = elts[i];
      d *= e.tag.load(std::memory_order_relaxed);
    }
    std::fprintf(::stderr, "Check: %i\n\n", d);
  }

  friend auto enqueue(queue_t &queue, element_t x) noexcept -> bool {
    // get a slot in the array for the new element
    int i = queue.back.load(std::memory_order_acquire);
    while (true) {
      if (++i >= size) {
        return false;
      }
      // exchange the new element with slots value if that slot has not been
      // used
      int empty = -1; // expected tag for an empty slot
      auto &e = queue.elts[i % size];
      // use two-step write: first store an odd value while we are writing the
      // new element
      if (e.tag.compare_exchange_strong(empty, (i / size) * 2 + 1,
                                        std::memory_order_acq_rel,
                                        std::memory_order_acquire)) {
        using std::swap;
        swap(x, e.item);
        // done writing, switch tag to even (ie. ready)
        e.tag.store((i / size) * 2, std::memory_order_seq_cst);
        break;
      }
    }
    // reset the value of back
    fetch_max(&queue.back, i, std::memory_order_release);
    return true;
  }

  friend auto dequeue(queue_t &queue) noexcept -> element_t {
    while (true) {                   // keep trying until an element is found
      int range = queue.back.load(); // search up to back slots
      for (int i = 0; i <= range; i++) {
        int ready = (i / size) * 2; // expected even tag for ready slot
        auto &e = queue.elts[i % size];
        // use two-step read: first store -2 while we are reading the element
        if (std::atomic_compare_exchange_strong(&e.tag, &ready, -2)) {
          using std::swap;
          element_t ret{};
          swap(ret, e.item);
          e.tag.store(-1); // done reading, switch tag to -1 (ie. empty)
          return ret;
        }
      }
    }
  }
};
