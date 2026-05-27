#pragma once

#include <cstdint>

namespace clf::sim {
class ScenarioManager;
struct ScenarioSettings;
}

namespace clf::ui {

struct BattleControlState final {
    bool paused = false;
    float simSpeed = 1.0f;
    std::uint32_t seed = 1337;
    bool randomizeOnRestart = false;
};

class BattleControlPanel final {
public:
    // Returns true if a restart/randomize action was triggered.
    bool Render(BattleControlState& inOutState, sim::ScenarioManager& scenarioManager);
};

} // namespace clf::ui
