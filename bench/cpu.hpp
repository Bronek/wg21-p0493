#pragma once

#include <cstdio>

#include <pthread.h>     // for pthread_setaffinity_np
#include <sys/sysinfo.h> // for get_nprocs

static constexpr int max_cpus = 128;

inline auto pin_cpu(int cpu) noexcept -> bool {
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(cpu, &cpuset);
  if (0 != pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset)) {
    std::fprintf(::stderr, "Unable to pin CPU: %i\n\n", cpu);
    return false;
  }
  return true;
}

inline auto count_cpus() noexcept -> int { return get_nprocs(); }
