#pragma once

namespace clf::sim::damage {

// Simple penetration model: linear interpolation from known reference points.
struct PenetrationProfile final {
    double pen_at_0m_mm = 650.0;
    double pen_at_1000m_mm = 600.0;
    double pen_at_2000m_mm = 520.0;
    double pen_at_3000m_mm = 450.0;
};

double PenetrationAtRangeMm(const PenetrationProfile& profile, double range_m);

} // namespace clf::sim::damage

