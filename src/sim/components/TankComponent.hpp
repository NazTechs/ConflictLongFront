#pragma once

#include <string>

namespace clf::sim {

struct Tank final {
    int team_id = 0;
    double health = 100.0;

    // Visual/debug representation in meters.
    double radius_m = 6.0;

    // Height of the sensor/optic above the terrain surface, used for LOS.
    double sensor_height_m = 2.5;

    // Visual detection range (separate from weapon range).
    double visual_range_m = 5'000.0;
};

struct UnitName final {
    std::string value;
};

} // namespace clf::sim
