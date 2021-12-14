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
  auto operator()(config const &config) noexcept -> int {
    static constexpr int inner_iters = 10000;

    std::array<std::vector<double>, config::max_cpus> results = {};
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

      for (size_t i = 0; i < config.cpus.size(); ++i) {
        if (!config.cpus.test(i)) {
          continue;
        }

        threads.emplace_back([&, cpu = i]() noexcept {
          results[cpu].reserve((config.iter / inner_iters) + 1);
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

          for (std::size_t i = 0; i < config.iter; i += inner_iters) {
            auto const start = std::chrono::high_resolution_clock::now();
            for (std::size_t j = 0; j < inner_iters; ++j) {
              (void)fetch_max(&max, dist(r), std::memory_order_release);
            }
            auto const done = std::chrono::high_resolution_clock::now();
            auto const time = (double)done.time_since_epoch().count() -
                              start.time_since_epoch().count();
            results[cpu].push_back(time / inner_iters);
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

    double minimum_3s = 0; // three sigma minimum cost of PRNG
    while (true) {
      std::ranlux24 r{(std::size_t)config.seed};
      std::uniform_int_distribution<int> dist(-1e9, 1e9);
      // warm up PRNG
      for (int j = 0; j < 100; ++j) {
        (void)dist(r);
      }

      stats s1{};
      for (int i = 0; i < 100; ++i) {
        auto const start = std::chrono::high_resolution_clock::now();
        for (int j = 0; j < inner_iters; ++j) {
          (void)dist(r);
        }
        auto const done = std::chrono::high_resolution_clock::now();
        auto const time = (double)done.time_since_epoch().count() -
                          start.time_since_epoch().count();
        s1.push(time / (double)inner_iters);
      }

      if (s1.stdev() < config.max_sigma) {
        minimum_3s = std::max(0.0, (s1.mean() - s1.stdev() * 3));
        std::fprintf(::stderr, "Calibration: %g (%g)\n\n", minimum_3s, s1.stdev());
        break;
      }
    }

    stats s2{};
    for (size_t i = 0; i < config.cpus.size(); ++i) {
      if (!config.cpus.test(i)) {
        continue;
      }
      for (auto r : results[i]) {
        s2.push(r - minimum_3s);
      }
    }

    std::printf("%lu\t%g\t%g\n", config.cpus.count(), s2.mean(), s2.stdev());
    return 0;
  }
};
