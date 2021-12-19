#pragma once

#include "../fetch_max.hpp"
#include "config.hpp"
#include "latch.hpp"
#include "stats.hpp"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <random>
#include <stdexcept>
#include <thread>
#include <vector>

template <type_e Impl_> struct runner final {
  template <int iters, typename Prng_, typename Dist_, typename F_>
  static auto sample(Prng_ &r, Dist_ &dist, F_ &&fn) noexcept -> double {
    // data sink which the compiler cannot opitmize away
    int volatile sink = 0;

    auto const start = std::chrono::high_resolution_clock::now();
    for (int j = 0; j < iters; ++j) {
      sink = fn(dist(r));
    }
    auto const done = std::chrono::high_resolution_clock::now();
    auto const time = (double)done.time_since_epoch().count() -
                      start.time_since_epoch().count();
    (void)sink;
    return (time / (double)iters);
  }

  // Max should be placed on different memory pages on different runs
  static constexpr std::size_t max_alignment = 0x1000; // 4K
  struct alignas(max_alignment) max_holder final {
    std::atomic<int> max{};

    void reset(int threads) noexcept {
      max = 0;
      // Must reset() first because we reuse memory locations
      latch1.reset();
      latch1.reset(new (&latches_[0]) latch_t(threads));
      latch2.reset();
      latch2.reset(new (&latches_[1]) latch_t(threads));
    }
    void arrive_and_wait() noexcept { latch1->arrive_and_wait(1); }
    void count_down() noexcept { latch2->count_down(1); }
    void wait() const noexcept { latch2->wait(); }

  private:
    using latch_t = ::latch; // In the absence of std::latch at this time

    struct alignas(alignof(latch_t)) placeholder final {
      char _b[sizeof(latch_t)];
    } latches_[2];
    struct destroyer final {
      void operator()(latch_t *p) noexcept { p->~latch_t(); }
    };
    std::unique_ptr<latch_t, destroyer> latch1{};
    std::unique_ptr<latch_t, destroyer> latch2{};
  };
  static_assert(sizeof(max_holder) == max_alignment);

  auto operator()(config const &config) noexcept -> int {
    static constexpr atomic_fetch_max<Impl_> fetch_max{};
    static constexpr int inner_iters = 10'000;
    static constexpr int warmup_iters = 100;
    static constexpr std::size_t array_size = 0x100;

    std::array<max_holder, array_size> max_array{};
    int const num_threads = config.cpus.count();
    for (auto &m : max_array) {
      m.reset(num_threads);
    }

    std::atomic<bool> error = {0};
    auto const runs = 1 + (config.iter - 1) / inner_iters;
    alignas(64) std::array<std::vector<double>, max_cpus> results = {};

    {
      std::vector<std::thread> threads;

      // emulate the key part of std::jthread
      struct join_t {
        std::vector<std::thread> *threads;
        ~join_t() {
          for (auto &t : *threads) {
            t.join();
          }
        }
      } join{&threads};

      for (std::size_t i = 0; i < config.cpus.size(); ++i) {
        if (!config.cpus.test(i)) {
          continue;
        }

        threads.emplace_back([&, cpu = i]() noexcept {
          auto &samples = results[cpu];
          samples.resize(runs);
          std::ranlux24 r{config.seed + cpu};
          std::uniform_int_distribution<int> dist(0, 2e9);

          if (!pin_cpu(cpu)) {
            error = true;
          }

          for (int i = 0, j = 0; j < runs; ++j) {
            auto &m = max_array[i];
            sample<warmup_iters>(r, dist, [&](int n) noexcept -> int {
              return fetch_max(&m.max, n, std::memory_order_relaxed);
            });

            m.arrive_and_wait();
            if (!error) {
              samples[j] =
                  sample<inner_iters>(r, dist, [&](int n) noexcept -> int {
                    return fetch_max(&m.max, n, config.operation);
                  });
            }

            m.count_down();
            i = (i + 1) % array_size;
          }
        });
      }

      for (int i = 0, j = 0; j < runs; ++j) {
        max_array[i].wait();
        max_array[i].reset(num_threads);
        i = (i + 1) % array_size;
      }

      if (error) {
        return 1;
      }
    } // join all threads

    if (!pin_cpu(1)) {
      return 2;
    }

    double prng_cost = 0; // PRNG cost
    double best_stdev = 1e6;
    static constexpr int max_tries = 5;
    for (int i = 0; i < max_tries; ++i) {
      std::ranlux24 r{(std::size_t)config.seed};
      std::uniform_int_distribution<int> dist(0, 2e9);
      sample<warmup_iters>(r, dist, [&](int i) noexcept -> int { return i; });

      stats s1{};
      for (int i = 0; i < runs; ++i) {
        s1.push(sample<inner_iters>(r, dist,
                                    [&](int i) noexcept -> int { return i; }));
      }

      double const stdev = s1.stdev();
      if (stdev < best_stdev) {
        best_stdev = stdev;
        prng_cost = std::max(0.0, (s1.mean() - stdev * 5));
        if (stdev <= config.max_sigma) {
          break;
        }
      }
    }

    if (best_stdev <= config.max_sigma) {
      std::fprintf(::stderr, "Calibration: %g (%g)\n\n", prng_cost, best_stdev);
    } else {
      std::fprintf(::stderr, "Calibration failed: best %g, required %g\n\n",
                   best_stdev, config.max_sigma);
      return 3;
    }

    stats s2{};
    // Ignore samples collected from CPU 0 - too much noise, by design.
    // This CPU is not isolated and does all the other work.
    for (std::size_t i = 1; i < config.cpus.size(); ++i) {
      for (auto r : results[i]) {
        s2.push(r - prng_cost);
      }
    }

    std::printf("%u\t%g\t%g\n", num_threads, s2.mean(), s2.stdev());
    return 0;
  }
};
