#include "../fetch_max.hpp"
#include "config.hpp"
#include "runner.hpp"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <string>

void usage(char const *p_) noexcept {
  using namespace std;
  fprintf(
      ::stderr,
      "Proposal P0493 benchmark runner\n\n"
      "Example usage:\n"
      "%s -c 8 -t w -i 1e6 -s 42 -m 0.5\n\n"
      "Where:\n"
      "-c number of cores to run on (will to pin 0, 1, 2 etc.), mandatory "
      "parameter between 1 and %u\n"
      "-t one character to denote the type of fetch_max, valid: "
      "s(trong), w(eak), (smar)t and h(ardware), defaults to s\n"
      "-i number of iterations, defaults to 1e6\n"
      "-s random seed, defaults to clock\n"
      "-m maximum sigma for calibration, default 1.0\n\n"
      "The example above will iterate 1e6 times using 8 threads (pinned to "
      "cores 0-7), using weak fetch_max and max_sigma 0.5\n\n"
      "Note: benchmark results go to stdout, all other messages to stderr\n\n",
      p_, config::max_cpus);
}

auto parse(config &dest, int argc, char **argv) noexcept -> bool {
  using namespace std;
  if (argc < 3) {
    usage(argv[0]);
    return false;
  }
  dest.iter = 1e6;
  dest.impl = type_e::strong;
  dest.max_sigma = 1;

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
      }

      // Start at CPU 1 which is first isolated
      for (int j = 1; j <= cpus; ++j) {
        dest.cpus.set(j % config::max_cpus);
      }

      if ((int)dest.cpus.count() != cpus) {
        fprintf(::stderr, "Out of range (too high): -c %i\n", cpus);
        return false;
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
        fprintf(::stderr, "Out of range (too low): -i %lu\n", dest.iter);
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
      dest.max_sigma = d;
      if (dest.max_sigma <= 0) {
        fprintf(::stderr, "Out of range (too low): -m %lu\n", dest.iter);
        return false;
      }
    } else {
      usage(argv[0]);
      return false;
    }
  }

  if (i != argc) {
    usage(argv[0]);
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
          "%lu iterations\n"
          "%g max. sigma\n"
          "%u seed\n\n",
          dest.cpus.count(), format(dest.impl), dest.iter, dest.max_sigma,
          dest.seed);

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
    }
  }

  return 1;
}
