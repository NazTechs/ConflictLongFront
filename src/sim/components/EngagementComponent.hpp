#pragma once

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>

namespace clf::sim {

struct EngagementInfo final {
    entt::entity target = entt::null;

    double target_distance_m = 0.0;
    bool target_in_visual_range = false;
    bool target_in_weapon_range = false;

    bool has_line_of_sight = false;
    bool has_blocked_point = false;
    glm::dvec2 blocked_point_m{0.0, 0.0};
};

} // namespace clf::sim
