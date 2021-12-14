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
  auto operator()(std::bitset<config::max_cpus> const &cpus, //
                  std::size_t iter,                          //
                  int seed) noexcept -> int {
    std::array<double, config::max_cpus> results = {};
    std::atomic<int> max = std::numeric_limits<int>::min();
    static constexpr atomic_fetch_max<Impl_> fetch_max{};

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

      for (size_t i = 0; i < cpus.size(); ++i) {
        if (!cpus.test(i)) {
          continue;
        }

        threads.emplace_back([&, cpu = i]() noexcept {
          cpu_set_t cpuset;
          CPU_ZERO(&cpuset);
          CPU_SET(cpu, &cpuset);
          if (0 !=
              pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset)) {
            std::fprintf(::stderr, "Unable to pin CPU: %li\n\n", cpu);
            started |= error;
            return;
          }

          // Choose a random mean between 1 and 6
          std::ranlux24 r{seed + cpu};
          std::uniform_int_distribution<int> dist(-1e9, 1e9);
          // warm up PRNG
          for (int i = 0; i < 100; ++i) {
            fetch_max(&max, dist(r), std::memory_order_relaxed);
          }

          started += 1;

          // spin to ensure all threads kick off at the same time
          while (starter.load(std::memory_order_acquire) == 0) {
          }

          std::size_t i = 0; // count iterations
          auto const start = std::chrono::high_resolution_clock::now();
          while (i < iter) {
            fetch_max(&max, dist(r), std::memory_order_release);
            i++;
          }
          auto const done = std::chrono::high_resolution_clock::now();
          auto const time = (double)done.time_since_epoch().count() -
                            start.time_since_epoch().count();

          // store time per iteration
          results[cpu] = time / i;
        });
      }

      while (started < cpus.count()) {
      }
      starter = 1;

      if (started > config::max_cpus) {
        return 1;
      }
    } // join all threads

    double minimum_3s = 0; // three sigma minimum cost of PRNG
    double max_s = 0.5;    // maximum sigma to complete calibration
    while (true) {
      std::ranlux24 r{(std::size_t)seed};
      std::uniform_int_distribution<int> dist(-1e9, 1e9);
      // warm up PRNG
      for (int j = 0; j < 100; ++j) {
        (void)dist(r);
      }

      static constexpr int calib_inner_iters = 10000;
      stats s1{};
      for (int i = 0; i < 100; ++i) {
        auto const start = std::chrono::high_resolution_clock::now();
        for (int j = 0; j < calib_inner_iters; ++j) {
          (void)dist(r);
        }
        auto const done = std::chrono::high_resolution_clock::now();
        auto const time = (double)done.time_since_epoch().count() -
                          start.time_since_epoch().count();
        s1.push(time / (double)calib_inner_iters);
      }

      if (s1.stdev() < max_s) {
        minimum_3s = std::max(0.0, (s1.mean() - s1.stdev() * 3));
        std::fprintf(::stderr, "Calibration: %g\n\n", minimum_3s);
        break;
      }
    }

    stats s2{};
    for (size_t i = 0; i < cpus.size(); ++i) {
      if (!cpus.test(i)) {
        continue;
      }
      auto const r = results[i];
      s2.push(r - minimum_3s);
    }

    std::printf("%lu\t%g\t(%g)\n", cpus.count(), s2.mean(), s2.stdev());
    return 0;
  }
};
