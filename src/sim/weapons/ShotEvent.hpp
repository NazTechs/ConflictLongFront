#pragma once

#include <cstdint>
#include <string>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>

namespace clf::sim::weapons {

struct ShotEvent final {
    double sim_time_s = 0.0;

    entt::entity shooter = entt::null;
    entt::entity target = entt::null;

    std::string weapon_id;
    std::string projectile_name;

    double distance_m = 0.0;
    bool hit = false;
    bool penetrated = false;

    glm::dvec2 origin_m{0.0, 0.0};
    glm::dvec2 aim_point_m{0.0, 0.0};
    glm::dvec2 impact_point_m{0.0, 0.0};

    std::string result_summary;
};

} // namespace clf::sim::weapons

