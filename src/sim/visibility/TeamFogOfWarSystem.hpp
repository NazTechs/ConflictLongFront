#pragma once

#include <array>

#include <entt/entity/fwd.hpp>
#include <entt/entity/registry.hpp>

#include "sim/visibility/VisibilityMask.hpp"

namespace clf::sim::terrain {
class Terrain;
}

namespace clf::sim::visibility {

struct TeamFogParams final {
    double grid_resolution_m = 100.0;
    double update_rate_hz = 8.0;
    int max_teams = 2;
};

class TeamFogOfWarSystem final {
public:
    TeamFogOfWarSystem() = default;

    void Configure(const TeamFogParams& p);
    void Reset();
    void Update(entt::registry& registry, const terrain::Terrain& terrain, double dtSeconds);

    VisibilityMask& MaskForTeam(int teamId);
    const VisibilityMask& MaskForTeam(int teamId) const;

private:
    TeamFogParams m_params{};
    std::vector<VisibilityMask> m_teamMasks;
    double m_accum_s = 0.0;

    void EnsureMasksSized(double worldSizeMeters);
};

} // namespace clf::sim::visibility

