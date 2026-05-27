#pragma once

namespace clf::sim::damage {

struct Armor final {
    // Simplified equivalent armor thickness (mm RHAe).
    double front_mm = 600.0;
    double side_mm = 350.0;
    double rear_mm = 200.0;
    double turret_front_mm = 650.0;
    double turret_side_mm = 400.0;
    double turret_rear_mm = 250.0;
};

} // namespace clf::sim::damage

