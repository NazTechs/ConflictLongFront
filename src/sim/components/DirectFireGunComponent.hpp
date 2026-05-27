#pragma once

#include <string>

namespace clf::sim {

// Direct-fire gun (tank cannon) with simple reload + ammo state.
struct DirectFireGun final {
    std::string weapon_id;
    std::string projectile_name;

    double max_range_m = 0.0;
    double muzzle_velocity_mps = 0.0;
    double reload_time_s = 0.0;

    int ammo = 0;
    double reload_remaining_s = 0.0;
};

} // namespace clf::sim

