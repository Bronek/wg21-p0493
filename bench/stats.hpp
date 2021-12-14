#pragma once

#include <cmath>

struct stats final {
    int n = 0;
    double old_m = 0.0;
    double new_m = 0.0;
    double old_s = 0.0;
    double new_s = 0.0;

    void push(double x) noexcept {
        n += 1;
        if (n == 1) {
            old_m = new_m = x;
            old_s = 0;
        } else {
            new_m = old_m + (x - old_m) / n;
            new_s = old_s + (x - old_m) * (x - new_m);
        
            old_m = new_m;
            old_s = new_s;
        }
    }

    auto mean() noexcept -> double {
        return (n ? new_m : 0.0);
    }

    auto stdev() noexcept -> double {
       return std::sqrt(n > 1 ? new_s / (n - 1) : 0.0);
    }
};