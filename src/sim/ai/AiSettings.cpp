#include "sim/ai/AiSettings.hpp"

#include <fstream>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace clf::sim::ai {

using json = nlohmann::json;

namespace {

template <typename T>
T Clamp(T v, T lo, T hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

} // namespace

AiSettings LoadAiSettingsOrDefaults(const std::filesystem::path& path)
{
    AiSettings out{};

    std::ifstream f(path);
    if (!f.is_open()) {
        spdlog::info("AI settings not found at '{}', using defaults.", path.string());
        return out;
    }

    json j;
    try {
        j = json::parse(f);
    } catch (const std::exception& e) {
        spdlog::warn("Failed to parse AI settings '{}': {}", path.string(), e.what());
        return out;
    }

    if (j.contains("search") && j["search"].is_object()) {
        const auto& s = j["search"];
        out.search.min_waypoint_distance_m = s.value("min_waypoint_distance_m", out.search.min_waypoint_distance_m);
        out.search.max_waypoint_distance_m = s.value("max_waypoint_distance_m", out.search.max_waypoint_distance_m);
        out.search.waypoint_reached_radius_m = s.value("waypoint_reached_radius_m", out.search.waypoint_reached_radius_m);
        out.search.stuck_timeout_s = s.value("stuck_timeout_s", out.search.stuck_timeout_s);
        out.search.last_known_position_timeout_s = s.value("last_known_position_timeout_s", out.search.last_known_position_timeout_s);
    }

    if (j.contains("movement") && j["movement"].is_object()) {
        const auto& m = j["movement"];
        out.movement.default_max_speed_mps = m.value("default_max_speed_mps", out.movement.default_max_speed_mps);
        out.movement.default_acceleration_mps2 = m.value("default_acceleration_mps2", out.movement.default_acceleration_mps2);
        out.movement.default_turn_rate_deg_s = m.value("default_turn_rate_deg_s", out.movement.default_turn_rate_deg_s);
    }

    if (j.contains("turret") && j["turret"].is_object()) {
        const auto& t = j["turret"];
        out.turret.scan_speed_deg_s = t.value("scan_speed_deg_s", out.turret.scan_speed_deg_s);
        out.turret.scan_arc_deg = t.value("scan_arc_deg", out.turret.scan_arc_deg);
    }

    if (j.contains("fog_of_war") && j["fog_of_war"].is_object()) {
        const auto& fow = j["fog_of_war"];
        out.fog_of_war.enabled = fow.value("enabled", out.fog_of_war.enabled);
        out.fog_of_war.grid_resolution_m = fow.value("grid_resolution_m", out.fog_of_war.grid_resolution_m);
        out.fog_of_war.update_rate_hz = fow.value("update_rate_hz", out.fog_of_war.update_rate_hz);
        out.fog_of_war.visible_brightness = fow.value("visible_brightness", out.fog_of_war.visible_brightness);
        out.fog_of_war.known_brightness = fow.value("known_brightness", out.fog_of_war.known_brightness);
        out.fog_of_war.unknown_brightness = fow.value("unknown_brightness", out.fog_of_war.unknown_brightness);
    }

    // Basic sanity clamps (avoid invalid / extreme values causing stalls).
    out.search.min_waypoint_distance_m = Clamp(out.search.min_waypoint_distance_m, 10.0, 15'000.0);
    out.search.max_waypoint_distance_m = Clamp(out.search.max_waypoint_distance_m, out.search.min_waypoint_distance_m, 20'000.0);
    out.search.waypoint_reached_radius_m = Clamp(out.search.waypoint_reached_radius_m, 5.0, 500.0);
    out.search.stuck_timeout_s = Clamp(out.search.stuck_timeout_s, 1.0, 60.0);
    out.search.last_known_position_timeout_s = Clamp(out.search.last_known_position_timeout_s, 0.0, 300.0);

    out.movement.default_max_speed_mps = Clamp(out.movement.default_max_speed_mps, 0.1, 40.0);
    out.movement.default_acceleration_mps2 = Clamp(out.movement.default_acceleration_mps2, 0.1, 10.0);
    out.movement.default_turn_rate_deg_s = Clamp(out.movement.default_turn_rate_deg_s, 1.0, 180.0);

    out.turret.scan_speed_deg_s = Clamp(out.turret.scan_speed_deg_s, 1.0, 180.0);
    out.turret.scan_arc_deg = Clamp(out.turret.scan_arc_deg, 10.0, 360.0);

    out.fog_of_war.grid_resolution_m = Clamp(out.fog_of_war.grid_resolution_m, 50.0, 500.0);
    out.fog_of_war.update_rate_hz = Clamp(out.fog_of_war.update_rate_hz, 0.5, 30.0);
    out.fog_of_war.visible_brightness = Clamp(out.fog_of_war.visible_brightness, 0.0, 1.0);
    out.fog_of_war.known_brightness = Clamp(out.fog_of_war.known_brightness, 0.0, 1.0);
    out.fog_of_war.unknown_brightness = Clamp(out.fog_of_war.unknown_brightness, 0.0, 1.0);

    return out;
}

} // namespace clf::sim::ai
