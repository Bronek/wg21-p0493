#pragma once

#include "../fetch_max.hpp"
#include "../stats.hpp"
#include "config.hpp"
#include "queue.hpp"

#include <atomic>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

#include <pthread.h> // for pthread_getaffinity_np

struct dummy {
  long payload[7] = {};
};

template <std::size_t Size_, type_e Impl_> struct runner final {
  auto operator()(std::bitset<config::max_cpus> const &cpus) noexcept -> int {
    using namespace std;

    auto queue = make_unique<queue_t<dummy, Size_, atomic_fetch_max<Impl_>>>();
    std::array<double, config::max_cpus> results = {};

    {
      vector<thread> threads;

      // emulate the key part of std::jthread
      struct join_t {
        vector<thread> *threads;
        ~join_t() {
          for (auto &t : *threads) {
            t.join();
          }
        }
      } join(&threads);

      static constexpr uint64_t error = config::max_cpus << 1;
      atomic<uint64_t> started = {0};
      atomic<int> starter = {0};

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

          started += 1;

          // spin to ensure all threads kick off at the same time
          while (starter.load(std::memory_order_acquire) == 0) {
          }

          std::size_t i = 0; // count iterations
          auto const start = std::chrono::high_resolution_clock::now();
          while (enqueue(*queue, dummy{})) {
            i++;
          }
          if (i == 0) {
            return;
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

    stats s{};
    int bad = 0;
    for (size_t i = 0; i < cpus.size(); ++i) {
      if (!cpus.test(i)) {
        continue;
      }
      if (results[i] == 0) {
        bad += 1;
        continue;
      }
      auto const r = results[i];
      s.push(r);
    }

    std::printf("%lu\t%g\t%g (%i)\n", cpus.count(), s.mean(), s.stdev(), bad);
    return 0;
  }
};
