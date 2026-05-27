#pragma once

namespace clf::sim {

struct Sensor final {
    double range_m = 5'000.0;
    double fov_rad = 2.0943951023931953; // 120 deg

    // Detection time increases with distance (linear between near/far).
    double detect_time_near_s = 0.6;
    double detect_time_far_s = 4.0;
    double detect_near_m = 500.0;
};

} // namespace clf::sim
