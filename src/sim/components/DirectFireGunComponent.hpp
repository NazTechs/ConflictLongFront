#pragma once

#include <string>

namespace clf::sim {

// Direct-fire gun (tank cannon) with simple reload + ammo state.
struct DirectFireGun final {
    std::string weapon_id;
    std::string projectile_name;

    double max_range_m = 0.0;
    double max_effective_range_m = 0.0;
    double muzzle_velocity_mps = 0.0;
    double reload_time_s = 0.0;

    // Prototype lethality parameters (data-driven).
    double dispersion_mrad = 0.8;
    double pen_at_0m_mm = 650.0;
    double pen_at_1000m_mm = 600.0;
    double pen_at_2000m_mm = 520.0;
    double pen_at_3000m_mm = 450.0;

    int ammo = 0;
    double reload_remaining_s = 0.0;
};

} // namespace clf::sim
