#pragma once

#include <cstdint>
#include <filesystem>

namespace clf::sim::ai {

struct SearchSettings final {
    double min_waypoint_distance_m = 1000.0;
    double max_waypoint_distance_m = 4000.0;
    double waypoint_reached_radius_m = 120.0;
    double stuck_timeout_s = 6.0;
    double last_known_position_timeout_s = 30.0;
};

struct MovementSettings final {
    double default_max_speed_mps = 12.0;
    double default_acceleration_mps2 = 2.0;
    double default_turn_rate_deg_s = 25.0;
};

struct TurretScanSettings final {
    double scan_speed_deg_s = 20.0;
    double scan_arc_deg = 120.0;
};

struct FogOfWarSettings final {
    bool enabled = true;
    double grid_resolution_m = 100.0;
    double update_rate_hz = 8.0;
    double visible_brightness = 1.0;
    double known_brightness = 0.45;
    double unknown_brightness = 0.15;
};

struct AiSettings final {
    SearchSettings search{};
    MovementSettings movement{};
    TurretScanSettings turret{};
    FogOfWarSettings fog_of_war{};
};

AiSettings LoadAiSettingsOrDefaults(const std::filesystem::path& path);

} // namespace clf::sim::ai

