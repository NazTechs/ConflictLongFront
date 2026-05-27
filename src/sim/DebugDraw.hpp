#pragma once

#include <cstdint>

#include <glm/vec2.hpp>

namespace clf::sim {

struct DebugLine final {
    glm::dvec2 a_m{0.0, 0.0};
    glm::dvec2 b_m{0.0, 0.0};
    double ttl_s = 0.0;

    std::uint8_t r = 255;
    std::uint8_t g = 255;
    std::uint8_t b = 255;
    std::uint8_t a = 255;
};

} // namespace clf::sim

