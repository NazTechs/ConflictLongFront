#include "ui/BattleControlPanel.hpp"

#include <imgui.h>

#include "sim/ScenarioManager.hpp"

namespace clf::ui {

bool BattleControlPanel::Render(BattleControlState& inOutState, sim::ScenarioManager& scenarioManager)
{
    bool triggered = false;

    ImGui::Begin("Battle Control");

    ImGui::Checkbox("Pause", &inOutState.paused);
    ImGui::SliderFloat("Sim speed", &inOutState.simSpeed, 0.1f, 4.0f, "%.2fx");

    int seed = static_cast<int>(inOutState.seed);
    if (ImGui::InputInt("Seed", &seed)) {
        if (seed < 0) seed = 0;
        inOutState.seed = static_cast<std::uint32_t>(seed);
    }

    ImGui::Checkbox("Randomize on restart", &inOutState.randomizeOnRestart);

    if (ImGui::Button("Restart Battle")) {
        sim::ScenarioSettings settings{};
        settings.seed = inOutState.seed;
        settings.randomizePositions = inOutState.randomizeOnRestart;
        scenarioManager.Restart(settings);
        triggered = true;
    }

    ImGui::SameLine();
    if (ImGui::Button("Randomize Positions")) {
        sim::ScenarioSettings settings{};
        settings.seed = inOutState.seed;
        scenarioManager.RandomizePositions(settings);
        triggered = true;
    }

    ImGui::End();
    return triggered;
}

} // namespace clf::ui

