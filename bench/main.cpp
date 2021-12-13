#include "queue.hpp"

#include <atomic>
#include <bitset>
#include <chrono>
#include <stdexcept>
#include <thread>
#include <vector>

#include <cstdint>
#include <cstdio>
#include <cstring>

#include <pthread.h> // for pthread_getaffinity_np

struct config {
  std::bitset<64> cpus;
  uint64_t size;

  friend auto parse(config &dest, std::string c, std::string s) noexcept
      -> bool {
    if (c.empty() || s.empty()) {
      return false;
    }

    try {
      dest.cpus = decltype(dest.cpus){c};

      decltype(config::size) multiplier = 1;
      auto last = --s.end();
      switch (*last) {
      case 'k':
        multiplier = 1000;
        s.erase(last, s.end());
        break;
      case 'K':
        multiplier = 1024;
        s.erase(last, s.end());
        break;
      case 'm':
        multiplier = 1000'000;
        s.erase(last, s.end());
        break;
      case 'M':
        multiplier = 1024 * 1024;
        s.erase(last, s.end());
        break;
      }
      size_t n = 0;
      dest.size = stoull(s, &n);
      if (n != s.size()) {
        throw std::invalid_argument("Unable to parse " + s);
      }
      dest.size *= multiplier;
    } catch (std::exception const &e) {
      std::fprintf(::stderr, "Bad program argument: %s\n\n", e.what());
      return false;
    }

    std::printf("Cores: %s (%lu)\n", dest.cpus.to_string().c_str(),
                dest.cpus.count());
    std::printf("Size: %lu\n", dest.size);
    return true;
  }
};

auto run(config const &c) noexcept -> int {
  using namespace std;
  vector<thread> threads;
  std::atomic<uint64_t> started = {0};
  std::atomic<int> starter = {0};
  std::atomic<uint64_t> completed = {0};
  struct dummy {
    constexpr dummy() noexcept = default;
    long payload[7] = {};
  };

  queue_t<dummy> queue{c.size};
  constexpr int error = 512;

  for (size_t i = 0; i < c.cpus.size(); ++i) {
    if (!c.cpus.test(i)) {
      continue;
    }

    threads.emplace_back([&, cpu = i, count = (int)c.cpus.count(),
                          size = (int64_t)c.size]() noexcept {
      cpu_set_t cpuset;
      CPU_ZERO(&cpuset);
      CPU_SET(cpu, &cpuset);
      if (0 !=
          pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset)) {
        std::fprintf(::stderr, "Unable to pin CPU: %li\n\n", cpu);
        started += error;
        return;
      }
      std::printf("thread %lu on %li\n", pthread_self(), cpu);

      started += 1;

      // spin to ensure all threads kick off at the same time
      while (starter.load(std::memory_order_relaxed) == 0) {
      }

      while (enqueue(queue, dummy{},
                     [size](int64_t count) noexcept { return count < size; })) {
      }

      completed += 1;
    });
  }

  while (started < c.cpus.count()) {
  }

  // check if error
  if (started >= error) {
    starter = 1;
    for (auto &t : threads) {
      t.join();
    }
    return 1;
  }

  starter = 1;
  auto const start = std::chrono::high_resolution_clock::now();

  while (completed < c.cpus.count()) {
  }
  auto const done = std::chrono::high_resolution_clock::now();
  std::fprintf(::stderr, "%g\n",
               double(done.time_since_epoch().count() -
                      start.time_since_epoch().count()) /
                   c.size);

  for (auto &t : threads) {
    t.join();
  }
  return 0;
}

void usage(char const *p_) noexcept {
  using namespace std;
  puts("Proposal P0493 benchmark runner\n\nExample usage:\n");
  printf("%s -c 1110 -s 100K\n\n", p_);
  puts("Where:\n-c cores to run on (as parsed by bitset ctor)");
  puts("-s size of the queue to fill, with optional multiplier suffix: k=1e3, "
       "K=2^10, m=1e6, M=2^20, g=1e9, G=2^30\n");
  puts("The example above will fill 102400 large queue using 3 threads, "
       "running on cores 1, 2, and 3 (skipping core 0)\n");
}

auto main(int argc, char **argv) noexcept -> int {
  using namespace std;
  if (argc == 5 &&
      (strlen(argv[1]) == 2 && argv[1][0] == '-' && argv[1][1] == 'c') &&
      (strlen(argv[3]) == 2 && argv[3][0] == '-' && argv[3][1] == 's')) {
    config c = {};
    if (parse(c, argv[2], argv[4])) {
      return run(c);
    }
  } else {
    usage(argv[0]);
  }

  return 1;
}
