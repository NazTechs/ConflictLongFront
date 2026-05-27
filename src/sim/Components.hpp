#pragma once

#include <glm/vec2.hpp>

namespace clf::sim {

struct Tank final {
    glm::dvec2 position_m{0.0, 0.0};
    glm::dvec2 velocity_mps{0.0, 0.0};
    double heading_rad = 0.0;
    double health = 100.0;
    int team_id = 0;
};

struct Weapon final {
    double range_m = 0.0;
    double reload_time_s = 0.0;
    double projectile_speed_mps = 0.0;
};

} // namespace clf::sim

