#include <atomic>
#include <type_traits>
#include <vector>

#include <cstdint>
#include <cstdio>

#include "fetch_max.hpp"

template <typename T> struct queue_t {
  static_assert(std::is_nothrow_default_constructible_v<T>);
  static_assert(std::is_nothrow_copy_constructible_v<T>);
  static_assert(std::is_nothrow_swappable_v<T>);

  using element_t = T;
  const uint64_t size;

  struct entry_t {
    element_t item{}; // a queue element
    int tag{-1};      // its generation number
  };
  using tag_t = std::atomic_ref<int>;

  std::vector<entry_t> elts;
  explicit queue_t(uint64_t size) noexcept : size(size) {
    // will crash if we cannot allocate enough memory - this is good
    elts.resize(size, entry_t{});
    // visit elements to ensure the memory is 1. allocated 2. paged in and
    // 3. TLB up-to-date
    int d = 1;
    for (size_t i = 0; i < elts.size(); i += 32) {
      auto& e = elts[i];
      d *= (int volatile)e.tag;
    }
    std::printf("Check: %i\n", d);
  }

  std::atomic<int64_t> back{-1};

  template <typename Full_>
  friend auto enqueue(queue_t &queue, element_t x, Full_ const &fn) noexcept -> bool {
    // get a slot in the array for the new element
    int i = queue.back.load(std::memory_order_relaxed) + 1;
    while (true) {
      // expected tag for an empty slot
      int empty = -1;
      auto &e = queue.elts[i % queue.size];
      auto tag = tag_t(e.tag);
      // first store an odd value while we are writing the new element
      if (tag.compare_exchange_strong(empty, (i / queue.size) * 2 + 1,
                                      std::memory_order_relaxed, //
                                      std::memory_order_relaxed)) {
        using std::swap;
        swap(x, e.item);
        // done writing, switch tag to even (ie. ready)
        tag.store((i / queue.size) * 2, std::memory_order_seq_cst);
        break;
      } else if (!fn(i)) {
        // are we done yet?
        return false;
      }
      ++i;
    }
    // reset the value of back
    atomic_fetch_max_explicit(&queue.back, i, std::memory_order_relaxed);
    return true;
  }

  friend auto dequeue(queue_t &queue) noexcept -> element_t {
    while (true) {                   // keep trying until an element is found
      int range = queue.back.load(); // search up to back slots
      for (int i = 0; i <= range; i++) {
        int ready = (i / queue.size) * 2; // expected even tag for ready slot
        auto &e = queue.elts[i % queue.size];
        auto tag = tag_t(e.tag);
        // use two-step read: first store -2 while we are reading the element
        if (tag.compare_exchange_strong(ready, -2)) {
          using std::swap;
          element_t ret{};
          swap(ret, e.item);
          tag.store(-1); // done reading, switch tag to -1 (ie. empty)
          return ret;
        }
      }
    }
  }
};
