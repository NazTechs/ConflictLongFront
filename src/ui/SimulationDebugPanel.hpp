#pragma once

#include <entt/entity/entity.hpp>

namespace clf::core {
struct SimulationDebugSettings;
}

namespace clf::sim {
class World;
}

namespace clf::ui {

struct SimulationDebugPanelState final {
    float fps = 0.0f;
    int unitCount = 0;
    double simTimeSeconds = 0.0;
    double zoomPixelsPerMeter = 0.0;
};

class SimulationDebugPanel final {
public:
    void Render(const SimulationDebugPanelState& state,
                core::SimulationDebugSettings& inOutSettings,
                entt::entity& inOutSelectedEntity,
                const sim::World& world);
};

} // namespace clf::ui

