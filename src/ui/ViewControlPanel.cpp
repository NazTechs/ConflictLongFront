#include "ui/ViewControlPanel.hpp"

#include <imgui.h>

#include "core/Camera3D.hpp"

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

void ViewControlPanel::Render(ViewControlState& inOutState,
                              clf::core::SimulationDebugSettings& inOutDebugSettings,
                              clf::core::Camera3D& camera3D,
                              bool hasSelection)
{
    ImGui::Begin("View");

    const char* renderLabels[] = {"Top-down 2D", "Perspective 3D"};
    int rm = static_cast<int>(inOutState.renderMode);
    if (ImGui::Combo("Render mode", &rm, renderLabels, IM_ARRAYSIZE(renderLabels))) {
        inOutState.renderMode = static_cast<clf::render::RenderMode>(rm);
    }

    if (inOutState.renderMode == clf::render::RenderMode::Perspective3D) {
        ImGui::Separator();
        ImGui::TextUnformatted("3D camera");

        float yaw = camera3D.YawRad();
        float pitch = camera3D.PitchRad();
        float dist = camera3D.DistanceMeters();

        if (ImGui::SliderFloat("Yaw (rad)", &yaw, -3.14159f, 3.14159f)) {
            camera3D.Orbit(yaw - camera3D.YawRad(), 0.0f);
        }
        if (ImGui::SliderFloat("Pitch (rad)", &pitch, 0.17f, 1.48f)) {
            camera3D.Orbit(0.0f, pitch - camera3D.PitchRad());
        }
        if (ImGui::SliderFloat("Distance (m)", &dist, 50.0f, 8000.0f)) {
            camera3D.Zoom(dist - camera3D.DistanceMeters());
        }

        ImGui::Checkbox("Follow selected", &inOutState.followSelected);
        if (!hasSelection) {
            ImGui::BeginDisabled();
        }
        if (ImGui::Button("Focus selected")) {
            inOutState.requestFocusSelected = true;
        }
        if (!hasSelection) {
            ImGui::EndDisabled();
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset camera")) {
            camera3D.Reset();
        }
    }

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
    ImGui::Checkbox("Show blocked LOS point", &inOutDebugSettings.showBlockedLosPoint);
    ImGui::Checkbox("Show projectile traces", &inOutDebugSettings.showShotTraces);
    ImGui::Checkbox("Highlight selection", &inOutDebugSettings.highlightSelection);
    ImGui::Checkbox("Show damage zones", &inOutDebugSettings.showDamageZones);
    ImGui::Checkbox("Show fog of war", &inOutDebugSettings.showFogOfWar);
    if (inOutDebugSettings.showFogOfWar) {
        const char* fogLabels[] = {"Spectator", "Selected unit", "Selected team"};
        int fogMode = static_cast<int>(inOutDebugSettings.fogViewMode);
        if (ImGui::Combo("Fog view", &fogMode, fogLabels, IM_ARRAYSIZE(fogLabels))) {
            inOutDebugSettings.fogViewMode = static_cast<clf::core::FogViewMode>(fogMode);
        }
        if (inOutDebugSettings.fogViewMode == clf::core::FogViewMode::SelectedTeam) {
            ImGui::SliderInt("Fog team", &inOutDebugSettings.fogTeamId, 0, 1);
        }
        ImGui::Checkbox("Show explored memory", &inOutDebugSettings.fogShowExploredMemory);
    }
    ImGui::Checkbox("Show search waypoints", &inOutDebugSettings.showSearchWaypoints);
    ImGui::Checkbox("Show movement vectors", &inOutDebugSettings.showMovementVectors);
    ImGui::Checkbox("Show AI state", &inOutDebugSettings.showAiState);
    ImGui::Checkbox("Show detection debug", &inOutDebugSettings.showDetectionDebug);
    ImGui::Checkbox("Show collision bounds", &inOutDebugSettings.showCollisionBounds);

    ImGui::Separator();
    ImGui::TextUnformatted("Terrain");
    ImGui::Checkbox("Show terrain height overlay", &inOutDebugSettings.showTerrainHeightOverlay);
    if (inOutDebugSettings.showTerrainHeightOverlay) {
        ImGui::Checkbox("Contour bands", &inOutDebugSettings.terrainContourBands);
    }

    ImGui::End();
}

} // namespace clf::ui
