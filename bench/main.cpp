#include "../fetch_max.hpp"
#include "config.hpp"
#include "cpu.hpp"
#include "runner.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <string>

void usage(char const *p_, int cpus) noexcept {
  using namespace std;
  fprintf(
      ::stderr,
      "Proposal P0493 benchmark runner\n\n"
      "Example usage:\n"
      "%s -c 8 -t w -i 1e6 -s 42 -m 2.5 -r e\n\n"
      "Where:\n"
      "-c number of cores to run on (will pin to 1, 2 etc, to 0 only in "
      "the last resort), mandatory parameter between 1 and %u\n"
      "-t one character to denote the type of fetch_max, valid: "
      "s(trong), w(eak), (smar)t, h(ardware) and f(aster), defaults to s\n"
      "-i number of iterations, defaults to 1e6\n"
      "-s random seed, defaults to clock\n"
      "-m maximum sigma for calibration, default 1.0\n"
      "-r memory operation type, valid r(elaxed), c(onsume), a(cquire), "
      "(releas)e, (acq_re)l, (seq_cs)t, defaults to t\n\n"
      "The example above will iterate 1e6 times using 8 threads (pinned to "
      "cores 0-7), using weak fetch_max, max_sigma 2.5 and release\n\n"
      "Notes:\n1. benchmark results go to stdout, other messages to stderr\n"
      "2. maximum number of supported cpus is %u\n"
      "3. samples from core 0 are assumed to be noisy and are ignored\n\n",
      p_, cpus, max_cpus);
}

inline auto format(type_e i) noexcept -> const char * {
  switch (i) {
  case type_e::strong:
    return "strong";
  case type_e::weak:
    return "weak";
  case type_e::smart:
    return "smart";
  case type_e::hardware:
    return "hardware";
  case type_e::faster:
    return "faster";
  }
  return "what?";
}

inline auto format(std::memory_order i) noexcept -> const char * {
  switch (i) {
  case std::memory_order_relaxed:
    return "relaxed";
  case std::memory_order_consume:
    return "consume";
  case std::memory_order_acquire:
    return "acquire";
  case std::memory_order_release:
    return "release";
  case std::memory_order_acq_rel:
    return "acq_rel";
  case std::memory_order_seq_cst:
    return "seq_cst";
  }
  return "what?";
}

auto parse(config &dest, int argc, char **argv) noexcept -> bool {
  using namespace std;
  int detected_cpus = count_cpus();
  if (argc < 3) {
    usage(argv[0], detected_cpus);
    return false;
  }
  dest.iter = 1e6;
  dest.impl = type_e::strong;
  dest.max_sigma = 1;
  dest.operation = std::memory_order_seq_cst;

  int i = 1;
  bool seed_set = false;
  for (; i + 1 < argc; i += 2) {
    string const sel = argv[i];
    string const opt = argv[i + 1];
    if (sel == "-c") {
      size_t last = 0;
      int cpus = 0;
      try {
        cpus = stoi(opt, &last);
      } catch (std::exception &) {
        fprintf(::stderr, "Cannot parse: -c %s\n", opt.c_str());
        return false;
      }

      if (last != opt.size()) {
        fprintf(::stderr, "Cannot parse: -c %s\n", opt.c_str());
        return false;
      } else if (cpus < 1) {
        fprintf(::stderr, "Out of range (too low): -c %i\n", cpus);
        return false;
      } else if (cpus > detected_cpus) {
        fprintf(::stderr, "Out of range (too high): -c %i\n", cpus);
        return false;
      }

      // Start at CPU 1 which is first isolated
      for (int j = 1; j <= cpus; ++j) {
        dest.cpus.set(j % detected_cpus);
      }
    } else if (sel == "-t") {
      if (opt == "s") {
        dest.impl = type_e::strong;
      } else if (opt == "w") {
        dest.impl = type_e::weak;
      } else if (opt == "t") {
        dest.impl = type_e::smart;
      } else if (opt == "h") {
        dest.impl = type_e::hardware;
      } else if (opt == "f") {
        dest.impl = type_e::faster;
      } else {
        fprintf(::stderr, "Cannot parse: -t %s\n", opt.c_str());
        return false;
      }
    } else if (sel == "-i") {
      char s = 0;
      double d = 0;
      if (std::sscanf(opt.c_str(), "%lg%c", &d, &s) != 1) {
        fprintf(::stderr, "Cannot parse: -i %s\n", opt.c_str());
        return false;
      }
      dest.iter = d;
      if (dest.iter < 100) {
        fprintf(::stderr, "Out of range (too low): -i %u\n", dest.iter);
        return false;
      }
    } else if (sel == "-s") {
      seed_set = true;
      char s = 0;
      unsigned int d = 0;
      if (std::sscanf(opt.c_str(), "%u%c", &d, &s) != 1) {
        if (std::sscanf(opt.c_str(), "%x%c", &d, &s) != 1) {
          fprintf(::stderr, "Cannot parse: -s %s\n", opt.c_str());
          return false;
        }
      }
      dest.seed = (int)d;
    } else if (sel == "-m") {
      char s = 0;
      double d = 0;
      if (std::sscanf(opt.c_str(), "%lg%c", &d, &s) != 1) {
        fprintf(::stderr, "Cannot parse: -m %s\n", opt.c_str());
        return false;
      }
      if (d <= 0) {
        fprintf(::stderr, "Out of range (too low): -m %g\n", d);
        return false;
      }
      dest.max_sigma = d;
    } else if (sel == "-r") {
      if (opt == "r") {
        dest.operation = std::memory_order_relaxed;
      } else if (opt == "c") {
        dest.operation = std::memory_order_consume;
      } else if (opt == "a") {
        dest.operation = std::memory_order_acquire;
      } else if (opt == "e") {
        dest.operation = std::memory_order_release;
      } else if (opt == "l") {
        dest.operation = std::memory_order_acq_rel;
      } else if (opt == "t") {
        dest.operation = std::memory_order_seq_cst;
      } else {
        fprintf(::stderr, "Cannot parse: -r %s\n", opt.c_str());
        return false;
      }
    } else {
      usage(argv[0], detected_cpus);
      return false;
    }
  }

  if (i != argc) {
    usage(argv[0], detected_cpus);
    return false;
  }

  if (dest.cpus.count() == 0) {
    fprintf(::stderr, "Missing mandatory -c parameter\n");
    return false;
  }

  if (!seed_set) {
    constexpr long mask = std::numeric_limits<unsigned int>::max() - 1;
    dest.seed =
        int(std::chrono::system_clock::now().time_since_epoch().count() & mask);
  }

  fprintf(::stderr,
          "Will use:\n\n%lu core(s)\n"
          "%s implementation\n"
          "%s operation\n"
          "%u iterations\n"
          "%g max. sigma\n"
          "%u seed\n\n",
          dest.cpus.count(), format(dest.impl), format(dest.operation),
          dest.iter, dest.max_sigma, dest.seed);

  return true;
}

auto main(int argc, char **argv) noexcept -> int {
  using namespace std;
  config config{};
  if (parse(config, argc, argv)) {
    // translate runtime to compile-time in a large switch statement
    switch (config.impl) {
    case type_e::strong:
      return runner<type_e::strong>{}(config);
    case type_e::weak:
      return runner<type_e::weak>{}(config);
    case type_e::smart:
      return runner<type_e::smart>{}(config);
    case type_e::hardware:
      return runner<type_e::hardware>{}(config);
    case type_e::faster:
      return runner<type_e::faster>{}(config);
    }
  }

  return 1;
}
