#pragma once

#include <cstdint>

#include <entt/entity/fwd.hpp>
#include <entt/entity/registry.hpp>

#include "sim/visibility/VisibilityMask.hpp"

namespace clf::sim {
struct Tank;
struct Transform;
struct Vehicle;
struct Sensor;
namespace terrain {
class Terrain;
}
} // namespace clf::sim

namespace clf::sim::visibility {

struct FogOfWarParams final {
    double grid_resolution_m = 100.0;
    double update_rate_hz = 8.0;
};

struct FogOfWarStats final {
    int cells_tested = 0;
    int cells_visible = 0;
};

class FogOfWarSystem final {
public:
    FogOfWarSystem() = default;

    void Configure(const FogOfWarParams& p);
    void Reset();

    // Updates the mask for the given viewer (viewer-dependent).
    void Update(entt::registry& registry, const terrain::Terrain& terrain, entt::entity viewer, double dtSeconds);

    VisibilityMask& Mask() { return m_mask; }
    const VisibilityMask& Mask() const { return m_mask; }
    FogOfWarStats Stats() const { return m_stats; }

private:
    FogOfWarParams m_params{};
    VisibilityMask m_mask{};
    FogOfWarStats m_stats{};

    double m_accum_s = 0.0;
    entt::entity m_lastViewer = entt::null;

    void EnsureMaskSized(double worldSizeMeters);
};

} // namespace clf::sim::visibility
