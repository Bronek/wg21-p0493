#include <atomic>
#include <type_traits>
#include <cstdint>

#include "fetch_max.hpp"

template <typename T, uint64_t Size>
struct queue_t {
  static_assert(std::is_nothrow_default_constructible<T>::value);
  static_assert(std::is_nothrow_copy_constructible<T>::value);
  static_assert(std::is_nothrow_swappable<T>::value);

  using element_t = T;
  static constexpr uint64_t size = Size;

  struct entry_t {
    element_t item {};                            // a queue element
    std::atomic<int> tag {-1};              // its generation number
  };

  entry_t elts[size] = {};                    // a bounded array
  std::atomic<int> back {-1};

  friend void enqueue(queue_t& queue, element_t x) noexcept {
    int i = queue.back.load() + 1;          // get a slot in the array for the new element
    while (true) {
      // exchange the new element with slots value if that slot has not been used
      int empty = -1;                       // expected tag for an empty slot
      auto& e = queue.elts[i % size];
      // use two-step write: first store an odd value while we are writing the new element
      if (std::atomic_compare_exchange_strong(&e.tag, &empty, (i / size) * 2 + 1)) {
        using std::swap;
        swap(x, e.item);
        e.tag.store((i / size) * 2);        // done writing, switch tag to even (ie. ready)
        break;
      }
      ++i;
    }
    atomic_fetch_max(&queue.back, i);  // reset the value of back
  }

  friend auto dequeue(queue_t& queue) noexcept -> element_t {
    while (true) {                          // keep trying until an element is found
      int range = queue.back.load();        // search up to back slots
      for (int i = 0; i <= range; i++) {
        int ready = (i / size) * 2;         // expected even tag for ready slot
        auto& e = queue.elts[i % size];
        // use two-step read: first store -2 while we are reading the element
        if (std::atomic_compare_exchange_strong(&e.tag, &ready, -2)) {
          using std::swap;
          element_t ret{};
          swap(ret, e.item);
          e.tag.store(-1);                  // done reading, switch tag to -1 (ie. empty)
          return ret;
        }
      }
    }
  }
};
