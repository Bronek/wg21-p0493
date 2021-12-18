#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>

/**
 * @brief polyfill for C++20 std::latch
 */
struct latch {
  static constexpr std::ptrdiff_t max() noexcept {
    return std::numeric_limits<decltype(_count)::value_type>::max();
  }

  explicit latch(std::ptrdiff_t expected) : _count(expected) {}
  latch(const latch &) = delete;
  ~latch() = default;

  void count_down(std::ptrdiff_t n = 1) noexcept {
    lock l{_mutex};
    (void)_count_down(n, l);
  }

  [[nodiscard]] auto try_wait() const noexcept -> bool {
    return _count.load() == 0;
  }

  void wait() const {
    lock l{_mutex};
    _wait(l);
  }

  void arrive_and_wait(std::ptrdiff_t n = 1) {
    lock l{_mutex};
    if (_count_down(n, l)) {
      return;
    }
    _wait(l);
  }

private:
  std::atomic<std::ptrdiff_t> _count;
  std::mutex mutable _mutex{};
  std::condition_variable mutable _cond{};
  using lock = std::unique_lock<decltype(_mutex)>;

  [[nodiscard]] auto _count_down(std::ptrdiff_t n, lock & /**/) noexcept
      -> bool {
    if (_count.fetch_sub(n) == n) {
      _cond.notify_all();
      return true;
    }
    return false;
  }

  void _wait(lock &l) const {
    if (_count == 0) {
      return;
    }
    _cond.wait(l, [this]() noexcept -> bool { return this->try_wait(); });
  }
};
