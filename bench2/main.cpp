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
      "%s -c 8 -t w -i 1e6 -s 42\n\n"
      "Where:\n"
      "-c number of cores to run on (will to pin 0, 1, 2 etc.), mandatory "
      "parameter between 1 and %u\n"
      "-t one character to denote the type of fetch_max, valid: "
      "s(trong), w(eak), (smar)t and h(ardware), defaults to s\n"
      "-i number of iterations, defaults to 1e6\n"
      "-s random seed, defaults to clock\n\n"
      "The example above will iterate 1e6 times using 8 threads (pinned to "
      "cores 0-7), using weak fetch_max\n\n"
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
      } else if (cpus < 1 || cpus > config::max_cpus) {
        fprintf(::stderr, "Out of range: %i\n", cpus);
        return false;
      }

      for (int j = 0; j < cpus; ++j) {
        dest.cpus.set(j);
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
      if (dest.iter < 1) {
        fprintf(::stderr, "Out of range: %lu\n", dest.iter);
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
          "%u seed\n\n",
          dest.cpus.count(), format(dest.impl), dest.iter, dest.seed);

  return true;
}

auto main(int argc, char **argv) noexcept -> int {
  using namespace std;
  config config{};
  if (parse(config, argc, argv)) {
    // translate runtime to compile-time in a large switch statement
    switch (config.impl) {
    case type_e::strong:
      return runner<type_e::strong>{}(config.cpus, config.iter, config.seed);
    case type_e::weak:
      return runner<type_e::weak>{}(config.cpus, config.iter, config.seed);
    case type_e::smart:
      return runner<type_e::smart>{}(config.cpus, config.iter, config.seed);
    case type_e::hardware:
      return runner<type_e::hardware>{}(config.cpus, config.iter, config.seed);
    }
  }

  return 1;
}