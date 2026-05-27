#include "ui/SelectedUnitPanel.hpp"

#include <imgui.h>

#include "sim/Components.hpp"
#include "sim/components/TransformComponent.hpp"
#include "sim/components/VehicleComponent.hpp"
#include "sim/components/DetectionComponent.hpp"
#include "sim/components/SensorComponent.hpp"
#include "sim/components/ControlModeComponent.hpp"
#include "sim/components/WaypointPathComponent.hpp"
#include "sim/World.hpp"
#include "sim/damage/DamageComponent.hpp"

namespace clf::ui {

namespace {

const char* TeamName(int teamId)
{
    return (teamId == 0) ? "Blue" : (teamId == 1) ? "Red" : "Unknown";
}

const char* SubsystemLabel(sim::damage::SubsystemState s)
{
    using sim::damage::SubsystemState;
    switch (s) {
        case SubsystemState::Ok: return "OK";
        case SubsystemState::Damaged: return "Damaged";
        case SubsystemState::Disabled: return "Disabled";
        case SubsystemState::Destroyed: return "Destroyed";
    }
    return "OK";
}

} // namespace

void SelectedUnitPanel::Render(entt::entity selectedEntity, sim::World& world)
{
    ImGui::Begin("Selected Unit");

    auto& registry = world.Registry();
    if (selectedEntity == entt::null ||
        !registry.valid(selectedEntity) ||
        !registry.all_of<sim::Tank, sim::Transform>(selectedEntity)) {
        ImGui::TextDisabled("No tank selected (left click).");
        ImGui::End();
        return;
    }

    const auto& tank = registry.get<const sim::Tank>(selectedEntity);
    const auto& xf = registry.get<const sim::Transform>(selectedEntity);
    const double ground_m = world.GetTerrain().HeightAtXY(xf.position_m);

    ImGui::Text("Team: %s (%d)", TeamName(tank.team_id), tank.team_id);
    if (const auto* name = registry.try_get<sim::UnitName>(selectedEntity)) {
        if (!name->value.empty()) {
            ImGui::Text("Name: %s", name->value.c_str());
        }
    }

    ImGui::Separator();
    ImGui::Text("Pos: (%.1f, %.1f) m", xf.position_m.x, xf.position_m.y);
    ImGui::Text("Hull heading: %.2f rad", xf.hull_heading_rad);
    if (const auto* veh = registry.try_get<sim::Vehicle>(selectedEntity)) {
        ImGui::Text("Turret heading: %.2f rad", veh->turret_heading_rad);
        ImGui::Text("Speed: %.1f m/s", veh->speed_mps);
    }
    ImGui::Text("Ground: %.1f m", ground_m);
    ImGui::Text("Sensor: %.1f m AGL", tank.sensor_height_m);
    ImGui::Text("Visual range: %.0f m", tank.visual_range_m);

    {
        auto& mode = registry.get_or_emplace<sim::ControlModeComponent>(selectedEntity);
        auto& path = registry.get_or_emplace<sim::WaypointPathComponent>(selectedEntity);

        ImGui::Separator();
        ImGui::TextUnformatted("Control");
        ImGui::Text("Mode: %s", (mode.mode == sim::ControlMode::Manual) ? "Manual" : "Automatic");
        ImGui::Text("Waypoints: %d", static_cast<int>(path.waypoints.size()));

        if (ImGui::Button("Set Manual")) {
            mode.mode = sim::ControlMode::Manual;
        }
        ImGui::SameLine();
        if (ImGui::Button("Set Automatic")) {
            mode.mode = sim::ControlMode::Automatic;
            path.waypoints.clear();
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear Waypoints")) {
            path.waypoints.clear();
        }
    }

    if (const auto* gun = registry.try_get<sim::DirectFireGun>(selectedEntity)) {
        ImGui::Separator();
        ImGui::TextUnformatted("Weapon");
        ImGui::Text("Weapon id: %s", gun->weapon_id.c_str());
        ImGui::Text("Projectile: %s", gun->projectile_name.empty() ? "(none)" : gun->projectile_name.c_str());
        ImGui::Text("Ammo: %d", gun->ammo);
        ImGui::Text("Reload: %.2f / %.2f s", gun->reload_remaining_s, gun->reload_time_s);
        ImGui::Text("Max range: %.0f m", gun->max_range_m);
    }

    if (const auto* engagement = registry.try_get<sim::EngagementInfo>(selectedEntity)) {
        ImGui::Separator();
        ImGui::TextUnformatted("Engagement");
        ImGui::Text("Target distance: %.0f m", engagement->target_distance_m);
        ImGui::Text("LOS: %s", engagement->has_line_of_sight ? "YES" : "NO");
        ImGui::Text("In visual range: %s", engagement->target_in_visual_range ? "YES" : "NO");
        ImGui::Text("In weapon range: %s", engagement->target_in_weapon_range ? "YES" : "NO");
    }

    if (const auto* dmg = registry.try_get<sim::damage::DamageState>(selectedEntity)) {
        ImGui::Separator();
        ImGui::TextUnformatted("Damage");
        ImGui::Text("Mobility: %s", SubsystemLabel(dmg->mobility));
        ImGui::Text("Firepower: %s", SubsystemLabel(dmg->firepower));
        ImGui::Text("Optics: %s", SubsystemLabel(dmg->optics));
        ImGui::Text("Engine: %s", SubsystemLabel(dmg->engine));
        ImGui::Text("Ammo: %s", SubsystemLabel(dmg->ammo));
        ImGui::Text("Final: %s", dmg->destroyed ? "Destroyed" : "Operational");
    }

    if (const auto* sensor = registry.try_get<sim::Sensor>(selectedEntity)) {
        if (const auto* det = registry.try_get<sim::Detection>(selectedEntity)) {
            ImGui::Separator();
            ImGui::TextUnformatted("Detection");
            ImGui::Text("Sensor range: %.0f m", sensor->range_m);
            ImGui::Text("FOV: %.0f deg", sensor->fov_rad * (180.0 / 3.141592653589793));
            ImGui::Text("Target in range: %s", det->target_in_sensor_range ? "YES" : "NO");
            ImGui::Text("Target in FOV: %s", det->target_in_fov ? "YES" : "NO");
            ImGui::Text("Target LOS: %s", det->target_has_los ? "YES" : "NO");
            ImGui::Text("Detected: %s", det->target_detected ? "YES" : "NO");
            if (det->required_detect_s > 0.0) {
                ImGui::ProgressBar(static_cast<float>(det->detect_progress_s / det->required_detect_s), ImVec2(-1, 0), "Detect progress");
            }
        }
    }

    ImGui::End();
}

} // namespace clf::ui
