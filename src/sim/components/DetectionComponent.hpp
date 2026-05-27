#pragma once

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>

namespace clf::sim {

struct Detection final {
    entt::entity current_target = entt::null;
    bool target_detected = false;

    double target_distance_m = 0.0;
    bool target_in_sensor_range = false;
    bool target_in_fov = false;
    bool target_has_los = false;

    // Detection “lock” accumulation while conditions are satisfied.
    double detect_progress_s = 0.0;
    double required_detect_s = 0.0;

    // Memory after losing contact.
    double memory_remaining_s = 0.0;
    glm::dvec2 last_known_target_pos_m{0.0, 0.0};
};

} // namespace clf::sim

