#include "core/SettingsManager.hpp"

#include <fstream>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace clf::core {

using json = nlohmann::json;

namespace {

void EnsureDir(const std::filesystem::path& p)
{
    std::error_code ec;
    std::filesystem::create_directories(p, ec);
}

} // namespace

SettingsManager::SettingsManager(std::filesystem::path settingsDir)
    : m_settingsDir(std::move(settingsDir))
{
}

const std::filesystem::path& SettingsManager::SettingsDir() const
{
    return m_settingsDir;
}

std::filesystem::path SettingsManager::UserSettingsPath() const
{
    return m_settingsDir / "user_settings.json";
}

std::filesystem::path SettingsManager::ImGuiIniPath() const
{
    return m_settingsDir / "imgui.ini";
}

AppSettings SettingsManager::LoadOrDefaults() const
{
    EnsureDir(m_settingsDir);

    AppSettings out{};
    const auto path = UserSettingsPath();
    std::ifstream f(path);
    if (!f.is_open()) {
        return out;
    }

    json j;
    try {
        j = json::parse(f);
    } catch (const std::exception& e) {
        spdlog::warn("Failed to parse settings '{}': {}", path.string(), e.what());
        return out;
    }

    out.viewMode = ViewModeFromString(j.value("view_mode", "Spectator"), out.viewMode);
    out.renderMode = static_cast<render::RenderMode>(j.value("render_mode", static_cast<int>(out.renderMode)));
    out.cam3d_yaw_rad = j.value("cam3d_yaw_rad", out.cam3d_yaw_rad);
    out.cam3d_pitch_rad = j.value("cam3d_pitch_rad", out.cam3d_pitch_rad);
    out.cam3d_distance_m = j.value("cam3d_distance_m", out.cam3d_distance_m);
    out.cam3d_target_x_m = j.value("cam3d_target_x_m", out.cam3d_target_x_m);
    out.cam3d_target_z_m = j.value("cam3d_target_z_m", out.cam3d_target_z_m);
    out.cam3d_follow_selected = j.value("cam3d_follow_selected", out.cam3d_follow_selected);
    out.paused = j.value("paused", out.paused);
    out.simulationSpeed = j.value("simulation_speed", out.simulationSpeed);
    out.lastRandomSeed = j.value("last_random_seed", out.lastRandomSeed);
    out.randomizeOnRestart = j.value("randomize_on_restart", out.randomizeOnRestart);

    auto& d = out.debug;
    d.showWeaponRange = j.value("show_weapon_range", d.showWeaponRange);
    d.showVisualRange = j.value("show_visual_range", d.showVisualRange);
    d.showLosRay = j.value("show_los_rays", d.showLosRay);
    d.showSensorCone = j.value("show_sensor_cone", d.showSensorCone);
    d.showBlockedLosPoint = j.value("show_blocked_los_point", d.showBlockedLosPoint);
    d.showShotTraces = j.value("show_projectile_traces", d.showShotTraces);
    d.showFogOfWar = j.value("show_fog_of_war", d.showFogOfWar);
    d.fogViewMode = static_cast<FogViewMode>(j.value("fog_view_mode", static_cast<int>(d.fogViewMode)));
    d.fogTeamId = j.value("fog_team_id", d.fogTeamId);
    d.fogShowExploredMemory = j.value("fog_show_explored_memory", d.fogShowExploredMemory);
    d.highlightSelection = j.value("highlight_selection", d.highlightSelection);

    d.showTerrainHeightOverlay = j.value("show_terrain_height_overlay", d.showTerrainHeightOverlay);
    d.terrainContourBands = j.value("terrain_contour_bands", d.terrainContourBands);

    d.showSearchWaypoints = j.value("show_search_waypoints", d.showSearchWaypoints);
    d.showMovementVectors = j.value("show_movement_vectors", d.showMovementVectors);
    d.showAiState = j.value("show_ai_state", d.showAiState);
    d.showDetectionDebug = j.value("show_detection_debug", d.showDetectionDebug);
    d.showCollisionBounds = j.value("show_collision_bounds", d.showCollisionBounds);
    d.showDamageZones = j.value("show_damage_zones", d.showDamageZones);

    d.showOnlyDetectedUnitsInUnitView = j.value("show_only_detected_units", d.showOnlyDetectedUnitsInUnitView);
    d.showAllUnitsInDebugView = j.value("show_all_units_in_debug_view", d.showAllUnitsInDebugView);

    return out;
}

void SettingsManager::Save(const AppSettings& settings) const
{
    EnsureDir(m_settingsDir);

    json j;
    j["view_mode"] = ToString(settings.viewMode);
    j["render_mode"] = static_cast<int>(settings.renderMode);
    j["cam3d_yaw_rad"] = settings.cam3d_yaw_rad;
    j["cam3d_pitch_rad"] = settings.cam3d_pitch_rad;
    j["cam3d_distance_m"] = settings.cam3d_distance_m;
    j["cam3d_target_x_m"] = settings.cam3d_target_x_m;
    j["cam3d_target_z_m"] = settings.cam3d_target_z_m;
    j["cam3d_follow_selected"] = settings.cam3d_follow_selected;
    j["paused"] = settings.paused;
    j["simulation_speed"] = settings.simulationSpeed;
    j["last_random_seed"] = settings.lastRandomSeed;
    j["randomize_on_restart"] = settings.randomizeOnRestart;

    const auto& d = settings.debug;
    j["show_weapon_range"] = d.showWeaponRange;
    j["show_visual_range"] = d.showVisualRange;
    j["show_los_rays"] = d.showLosRay;
    j["show_sensor_cone"] = d.showSensorCone;
    j["show_blocked_los_point"] = d.showBlockedLosPoint;
    j["show_projectile_traces"] = d.showShotTraces;
    j["show_fog_of_war"] = d.showFogOfWar;
    j["fog_view_mode"] = static_cast<int>(d.fogViewMode);
    j["fog_team_id"] = d.fogTeamId;
    j["fog_show_explored_memory"] = d.fogShowExploredMemory;
    j["highlight_selection"] = d.highlightSelection;

    j["show_terrain_height_overlay"] = d.showTerrainHeightOverlay;
    j["terrain_contour_bands"] = d.terrainContourBands;

    j["show_search_waypoints"] = d.showSearchWaypoints;
    j["show_movement_vectors"] = d.showMovementVectors;
    j["show_ai_state"] = d.showAiState;
    j["show_detection_debug"] = d.showDetectionDebug;
    j["show_collision_bounds"] = d.showCollisionBounds;
    j["show_damage_zones"] = d.showDamageZones;

    j["show_only_detected_units"] = d.showOnlyDetectedUnitsInUnitView;
    j["show_all_units_in_debug_view"] = d.showAllUnitsInDebugView;

    const auto path = UserSettingsPath();
    std::ofstream o(path);
    if (!o.is_open()) {
        spdlog::warn("Failed to write settings '{}'", path.string());
        return;
    }
    o << j.dump(2) << "\n";
}

} // namespace clf::core
