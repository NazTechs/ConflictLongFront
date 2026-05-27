#include "ui/SimulationDebugPanel.hpp"

#include <imgui.h>

#include "core/SimulationDebugSettings.hpp"
#include "sim/Components.hpp"
#include "sim/World.hpp"

namespace clf::ui {

namespace {

const char* TeamName(int teamId)
{
    return (teamId == 0) ? "Blue" : (teamId == 1) ? "Red" : "Unknown";
}

} // namespace

void SimulationDebugPanel::Render(const SimulationDebugPanelState& state,
                                 core::SimulationDebugSettings& inOutSettings,
                                 entt::entity& inOutSelectedEntity,
                                 const sim::World& world)
{
    ImGui::Begin("Simulation Debug");

    ImGui::Text("FPS: %.1f", state.fps);
    ImGui::Text("Units: %d", state.unitCount);
    ImGui::Text("Sim time: %.2f s", state.simTimeSeconds);
    ImGui::Text("Zoom: %.4f px/m", state.zoomPixelsPerMeter);

    ImGui::Separator();
    ImGui::TextUnformatted("Overlays");
    ImGui::Checkbox("Show weapon range", &inOutSettings.showWeaponRange);
    ImGui::Checkbox("Show visual range", &inOutSettings.showVisualRange);
    ImGui::Checkbox("Show LOS ray", &inOutSettings.showLosRay);
    ImGui::Checkbox("Show blocked LOS point", &inOutSettings.showBlockedLosPoint);
    ImGui::Checkbox("Show shot traces", &inOutSettings.showShotTraces);
    ImGui::Checkbox("Highlight selection", &inOutSettings.highlightSelection);

    ImGui::Separator();
    ImGui::TextUnformatted("Terrain");
    ImGui::Checkbox("Show height overlay", &inOutSettings.showTerrainHeightOverlay);
    if (inOutSettings.showTerrainHeightOverlay) {
        ImGui::Checkbox("Contour bands", &inOutSettings.terrainContourBands);
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Selection");

    if (ImGui::Button("Clear selection")) {
        inOutSelectedEntity = entt::null;
    }

    const auto& registry = world.Registry();
    if (inOutSelectedEntity == entt::null || !registry.valid(inOutSelectedEntity) || !registry.all_of<sim::Tank>(inOutSelectedEntity)) {
        ImGui::TextDisabled("No tank selected (left click to select).");
        ImGui::End();
        return;
    }

    const auto& tank = registry.get<const sim::Tank>(inOutSelectedEntity);
    const double ground_m = world.GetTerrain().HeightAtXY(tank.position_m);
    ImGui::Text("Team: %s (%d)", TeamName(tank.team_id), tank.team_id);
    ImGui::Text("Pos: (%.1f, %.1f) m", tank.position_m.x, tank.position_m.y);
    ImGui::Text("Heading: %.2f rad", tank.heading_rad);
    ImGui::Text("Ground height: %.1f m", ground_m);
    ImGui::Text("Sensor height: %.1f m AGL", tank.sensor_height_m);

    if (const auto* name = registry.try_get<sim::UnitName>(inOutSelectedEntity)) {
        if (!name->value.empty()) {
            ImGui::Text("Name: %s", name->value.c_str());
        }
    }

    if (const auto* gun = registry.try_get<sim::DirectFireGun>(inOutSelectedEntity)) {
        ImGui::Separator();
        ImGui::TextUnformatted("Weapon");
        ImGui::Text("Weapon id: %s", gun->weapon_id.c_str());
        ImGui::Text("Projectile: %s", gun->projectile_name.empty() ? "(none)" : gun->projectile_name.c_str());
        ImGui::Text("Ammo: %d", gun->ammo);
        ImGui::Text("Reload: %.2f / %.2f s", gun->reload_remaining_s, gun->reload_time_s);
        ImGui::Text("Max range: %.0f m", gun->max_range_m);
    }

    if (const auto* engagement = registry.try_get<sim::EngagementInfo>(inOutSelectedEntity)) {
        ImGui::Separator();
        ImGui::TextUnformatted("Engagement");

        if (engagement->target == entt::null || !registry.valid(engagement->target) || !registry.all_of<sim::Tank>(engagement->target)) {
            ImGui::TextDisabled("Target: (none)");
        } else {
            const auto& targetTank = registry.get<const sim::Tank>(engagement->target);
            ImGui::Text("Target: %s (%d)", TeamName(targetTank.team_id), targetTank.team_id);
            ImGui::Text("Distance: %.0f m", engagement->target_distance_m);
            ImGui::Text("LOS: %s", engagement->has_line_of_sight ? "YES" : "NO");
            ImGui::Text("In visual range: %s", engagement->target_in_visual_range ? "YES" : "NO");
            ImGui::Text("In weapon range: %s", engagement->target_in_weapon_range ? "YES" : "NO");
        }
    }

    ImGui::End();
}

} // namespace clf::ui

