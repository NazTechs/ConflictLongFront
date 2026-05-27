#include "ui/ViewControlPanel.hpp"

#include <imgui.h>

namespace clf::ui {

namespace {

const char* ViewModeLabel(clf::render::ViewMode mode)
{
    switch (mode) {
        case clf::render::ViewMode::Spectator: return "Spectator";
        case clf::render::ViewMode::RedTank: return "Red Tank View";
        case clf::render::ViewMode::BlueTank: return "Blue Tank View";
        case clf::render::ViewMode::SelectedTank: return "Selected Unit View";
        case clf::render::ViewMode::DebugTactical: return "Debug Tactical View";
    }
    return "Spectator";
}

} // namespace

void ViewControlPanel::Render(ViewControlState& inOutState, clf::core::SimulationDebugSettings& inOutDebugSettings)
{
    ImGui::Begin("View");

    const char* current = ViewModeLabel(inOutState.viewMode);
    if (ImGui::BeginCombo("View mode", current)) {
        for (int i = 0; i <= static_cast<int>(clf::render::ViewMode::DebugTactical); ++i) {
            const auto mode = static_cast<clf::render::ViewMode>(i);
            const bool selected = (mode == inOutState.viewMode);
            if (ImGui::Selectable(ViewModeLabel(mode), selected)) {
                inOutState.viewMode = mode;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Visibility rules");
    ImGui::Checkbox("Show all units in debug view", &inOutDebugSettings.showAllUnitsInDebugView);
    ImGui::Checkbox("Show only detected units in unit view", &inOutDebugSettings.showOnlyDetectedUnitsInUnitView);

    ImGui::Separator();
    ImGui::TextUnformatted("Overlays");
    ImGui::Checkbox("Show LOS rays", &inOutDebugSettings.showLosRay);
    ImGui::Checkbox("Show weapon range", &inOutDebugSettings.showWeaponRange);
    ImGui::Checkbox("Show sensor cone", &inOutDebugSettings.showSensorCone);
    ImGui::Checkbox("Show projectile traces", &inOutDebugSettings.showShotTraces);
    ImGui::Checkbox("Show damage zones", &inOutDebugSettings.showDamageZones);

    ImGui::Separator();
    ImGui::TextUnformatted("Terrain");
    ImGui::Checkbox("Show terrain height overlay", &inOutDebugSettings.showTerrainHeightOverlay);
    if (inOutDebugSettings.showTerrainHeightOverlay) {
        ImGui::Checkbox("Contour bands", &inOutDebugSettings.terrainContourBands);
    }

    ImGui::End();
}

} // namespace clf::ui

