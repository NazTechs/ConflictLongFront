#include "sim/damage/PenetrationModel.hpp"

#include <algorithm>

namespace clf::sim::damage {

double PenetrationAtRangeMm(const PenetrationProfile& profile, double range_m)
{
    if (range_m <= 0.0) {
        return profile.pen_at_0m_mm;
    }

    struct Point { double m; double pen; };
    const Point pts[] = {
        {0.0, profile.pen_at_0m_mm},
        {1000.0, profile.pen_at_1000m_mm},
        {2000.0, profile.pen_at_2000m_mm},
        {3000.0, profile.pen_at_3000m_mm},
    };

    const double r = std::clamp(range_m, 0.0, 3000.0);
    for (int i = 0; i < 3; ++i) {
        if (r >= pts[i].m && r <= pts[i + 1].m) {
            const double t = (r - pts[i].m) / (pts[i + 1].m - pts[i].m);
            return pts[i].pen + (pts[i + 1].pen - pts[i].pen) * t;
        }
    }
    return profile.pen_at_3000m_mm;
}

} // namespace clf::sim::damage

