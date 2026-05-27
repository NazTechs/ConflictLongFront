#pragma once

#include <glm/vec2.hpp>

namespace clf::sim {

struct Transform final {
    glm::dvec2 position_m{0.0, 0.0};
    double hull_heading_rad = 0.0;
};

} // namespace clf::sim

