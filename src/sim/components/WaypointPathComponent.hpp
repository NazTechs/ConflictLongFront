#pragma once

#include <vector>

#include <glm/vec2.hpp>

namespace clf::sim {

struct WaypointPathComponent final {
    std::vector<glm::dvec2> waypoints;
    double arrival_radius_m = 25.0;
};

} // namespace clf::sim

