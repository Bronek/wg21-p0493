#include "config.hpp"
#include "queue.hpp"
#include "runner.hpp"

#include <cstdint>
#include <cstdio>
#include <string>

void usage(char const *p_) noexcept {
  using namespace std;
  fprintf(
      ::stderr,
      "Proposal P0493 benchmark runner\n\n"
      "Example usage:\n"
      "%s -c 8 -i w -s l\n\n"
      "Where:\n"
      "-c number of cores to run on (will to pin 0, 1, 2 etc.), mandatory "
      "parameter between 1 and %u\n"
      "-i one character to denote the implementation of fetch_max, valid: "
      "s(trong), w(eak), (smar)t and h(ardware), defaults to s\n"
      "-s one character to denote the size of the queue, valid: s(mall), "
      "m(edium), l(arge) and x(tra-large), defaults to m\n\n"
      "The example above will fill a large queue using 8 threads (pinned to "
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
  dest.size = config::medium;
  dest.impl = config::strong;

  int i = 1;
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
    } else if (sel == "-i") {
      if (opt == "s") {
        dest.impl = config::strong;
      } else if (opt == "w") {
        dest.impl = config::weak;
      } else if (opt == "t") {
        dest.impl = config::smart;
      } else if (opt == "h") {
        dest.impl = config::hardware;
        fprintf(::stderr, "Not implemented: -i %s\n", format(dest.impl));
        return false;
      } else {
        fprintf(::stderr, "Cannot parse: -i %s\n", opt.c_str());
        return false;
      }
    } else if (sel == "-s") {
      if (opt == "s") {
        dest.size = config::small;
      } else if (opt == "m") {
        dest.size = config::medium;
      } else if (opt == "l") {
        dest.size = config::large;
      } else if (opt == "x") {
        dest.size = config::xlarge;
      } else {
        fprintf(::stderr, "Cannot parse: -s %s\n", opt.c_str());
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

  fprintf(::stderr,
          "Will use:\n\n%lu core(s)\n"
          "%s implementation\n"
          "%lu sized queue\n\n",
          dest.cpus.count(), format(dest.impl), dest.size);

  return true;
}

auto main(int argc, char **argv) noexcept -> int {
  using namespace std;
  config config{};
  if (parse(config, argc, argv)) {
    // translate runtime to compile-time in a large switch statement
    switch (config.size + config.impl) {
    case (config::small + config::strong):
      return runner<config::small, config::strong>{}(config.cpus);
    case (config::small + config::weak):
      return runner<config::small, config::weak>{}(config.cpus);
    case (config::small + config::smart):
      return runner<config::small, config::smart>{}(config.cpus);
    case (config::medium + config::strong):
      return runner<config::medium, config::strong>{}(config.cpus);
    case (config::medium + config::weak):
      return runner<config::medium, config::weak>{}(config.cpus);
    case (config::medium + config::smart):
      return runner<config::medium, config::smart>{}(config.cpus);
    case (config::large + config::strong):
      return runner<config::large, config::strong>{}(config.cpus);
    case (config::large + config::weak):
      return runner<config::large, config::weak>{}(config.cpus);
    case (config::large + config::smart):
      return runner<config::large, config::smart>{}(config.cpus);
    case (config::xlarge + config::strong):
      return runner<config::xlarge, config::strong>{}(config.cpus);
    case (config::xlarge + config::weak):
      return runner<config::xlarge, config::weak>{}(config.cpus);
    case (config::xlarge + config::smart):
      return runner<config::xlarge, config::smart>{}(config.cpus);
    }
  }

  return 1;
}
