#pragma once

#include "../fetch_max.hpp"
#include "../stats.hpp"
#include "config.hpp"

#include <atomic>
#include <chrono>
#include <limits>
#include <memory>
#include <random>
#include <stdexcept>
#include <thread>
#include <vector>

#include <pthread.h> // for pthread_getaffinity_np

template <type_e Impl_> struct runner final {
  static auto pin_cpu(int cpu) noexcept -> bool {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu, &cpuset);
    if (0 != pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset)) {
      std::fprintf(::stderr, "Unable to pin CPU: %i\n\n", cpu);
      return false;
    }
    return true;
  }

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

  auto operator()(config const &config) noexcept -> int {

    // one and the only shared cache line for tests
    std::atomic<int> max{std::numeric_limits<int>::min()};
    static constexpr atomic_fetch_max<Impl_> fetch_max{};
    static constexpr int inner_iters = 10'000;

    alignas(64) std::array<std::vector<double>, config::max_cpus> results = {};

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
      } join(&threads);

      static constexpr uint64_t error = config::max_cpus << 1;
      std::atomic<uint64_t> started = {0};
      std::atomic<int> starter = {0};

      for (size_t i = 0; i < config.cpus.size(); ++i) {
        if (!config.cpus.test(i)) {
          continue;
        }

        threads.emplace_back([&, cpu = i]() noexcept {
          results[cpu].resize((config.iter / inner_iters) + 1);
          if (!pin_cpu(cpu)) {
            started |= error;
            return;
          }

          // Choose a random mean between 1 and 6
          std::ranlux24 r{config.seed + cpu};
          std::uniform_int_distribution<int> dist(-1e9, 1e9);
          // warm up PRNG
          for (int i = 0; i < 100; ++i) {
            fetch_max(&max, dist(r), std::memory_order_relaxed);
          }

          started += 1;

          // spin to ensure all threads kick off at the same time
          while (starter.load(std::memory_order_acquire) == 0) {
          }

          for (auto &s : results[cpu]) {
            s = sample<inner_iters>(r, dist, [&](int i) noexcept -> int {
              return fetch_max(&max, i, std::memory_order_release);
            });
          }
        });
      }

      while (started < config.cpus.count()) {
      }
      starter = 1;

      if (started > config::max_cpus) {
        return 1;
      }
    } // join all threads

    if (!pin_cpu(1)) {
      return 2;
    }

    double prng_cost = 0; // PRNG cost
    while (true) {
      std::ranlux24 r{(std::size_t)config.seed};
      std::uniform_int_distribution<int> dist(-1e9, 1e9);
      // warm up PRNG
      for (int j = 0; j < 100; ++j) {
        (void)dist(r);
      }

      std::vector<double> samples = {};
      samples.resize((config.iter / inner_iters) + 1);
      for (auto &s : samples) {
        s = sample<inner_iters>(r, dist,
                                [&](int i) noexcept -> int { return i; });
      }

      stats s1{};
      for (auto s : samples) {
        s1.push(s);
      }

      if (s1.stdev() < config.max_sigma) {
        prng_cost = std::max(0.0, (s1.mean() - s1.stdev() * 3));
        std::fprintf(::stderr, "Calibration: %g (%g)\n\n", prng_cost,
                     s1.stdev());
        break;
      }
    }

    stats s2{};
    for (size_t i = 0; i < config.cpus.size(); ++i) {
      if (!config.cpus.test(i)) {
        continue;
      }
      if (i == 0) {
        // Ignore samples collected from CPU 0 - too much noise, by design.
        // This CPU is not isolated and does all the other work.
        continue;
      }
      for (auto r : results[i]) {
        s2.push(r - prng_cost);
      }
    }

    std::printf("%lu\t%g\t%g\n", config.cpus.count(), s2.mean(), s2.stdev());
    return 0;
  }
};
