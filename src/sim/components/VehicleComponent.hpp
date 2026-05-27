#pragma once

namespace clf::sim {

struct Vehicle final {
    double speed_mps = 0.0;

    double max_speed_mps = 20.0;
    double accel_mps2 = 3.0;
    double brake_mps2 = 5.0;

    double hull_turn_rate_radps = 0.35;   // ~20 deg/s
    double turret_turn_rate_radps = 0.70; // ~40 deg/s

    double turret_heading_rad = 0.0;
};

struct VehicleControl final {
    double desired_speed_mps = 0.0;
    double desired_hull_heading_rad = 0.0;
    double desired_turret_heading_rad = 0.0;
};

} // namespace clf::sim

