#include "ui/DebugUI.hpp"

#include <imgui.h>

namespace clf::ui {

void DebugUI::Render(const DebugUIState& state)
{
    ImGui::Begin("Debug");
    ImGui::Text("FPS: %.1f", state.fps);
    ImGui::Text("Units: %d", state.unitCount);
    ImGui::Text("Sim time: %.2f s", state.simTimeSeconds);
    ImGui::Text("Zoom: %.2f px/m", state.zoomPixelsPerMeter);
    ImGui::End();
}

} // namespace clf::ui

