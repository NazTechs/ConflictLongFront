#include "sim/World.hpp"

#include <fstream>
#include <random>
#include <vector>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "sim/Components.hpp"
#include "sim/Systems.hpp"

namespace clf::sim {

using json = nlohmann::json;

World::World() = default;

bool World::LoadData(const std::filesystem::path& dataDir)
{
    const auto weaponsPath = dataDir / "weapons.json";
    const auto unitsPath = dataDir / "units.json";

    m_registry.clear();
    m_weapons.clear();
    m_simTimeSeconds = 0.0;

    if (!LoadWeapons(weaponsPath)) {
        return false;
    }
    if (!LoadUnits(unitsPath)) {
        return false;
    }
    return true;
}

void World::LoadFallbackSample()
{
    m_registry.clear();
    m_weapons.clear();
    m_simTimeSeconds = 0.0;

    WeaponDef cannon120{};
    cannon120.range_m = 2500.0;
    cannon120.reload_time_s = 4.0;
    cannon120.projectile_speed_mps = 1200.0;

    WeaponDef cannon105{};
    cannon105.range_m = 1800.0;
    cannon105.reload_time_s = 5.0;
    cannon105.projectile_speed_mps = 1000.0;

    std::mt19937_64 rng{1337};
    std::uniform_real_distribution<double> pos(-80.0, 80.0);
    std::uniform_real_distribution<double> vel(-6.0, 6.0);

    for (int i = 0; i < 8; ++i) {
        const int team = (i < 4) ? 0 : 1;
        const auto& weapon = (i % 2 == 0) ? cannon120 : cannon105;
        SpawnTankFromDef(team, 100.0, 0.0, pos(rng), pos(rng), vel(rng), vel(rng), weapon);
    }
}

void World::Step(double dtSeconds)
{
    systems::IntegrateTanks(m_registry, dtSeconds);

    const glm::dvec2 minMeters{-m_worldHalfExtentMeters, -m_worldHalfExtentMeters};
    const glm::dvec2 maxMeters{+m_worldHalfExtentMeters, +m_worldHalfExtentMeters};
    systems::BounceTanksInAabb(m_registry, minMeters, maxMeters);

    m_simTimeSeconds += dtSeconds;
}

std::size_t World::UnitCount() const
{
    return m_registry.view<const Tank>().size();
}

double World::SimulationTimeSeconds() const
{
    return m_simTimeSeconds;
}

entt::registry& World::Registry()
{
    return m_registry;
}

const entt::registry& World::Registry() const
{
    return m_registry;
}

bool World::LoadWeapons(const std::filesystem::path& filePath)
{
    std::ifstream f(filePath);
    if (!f.is_open()) {
        spdlog::error("Unable to open weapons file '{}'", filePath.string());
        return false;
    }

    json j;
    try {
        j = json::parse(f);
    } catch (const std::exception& e) {
        spdlog::error("Failed to parse weapons file '{}': {}", filePath.string(), e.what());
        return false;
    }

    if (!j.contains("weapons") || !j["weapons"].is_object()) {
        spdlog::error("weapons.json must contain an object field 'weapons'");
        return false;
    }

    for (auto it = j["weapons"].begin(); it != j["weapons"].end(); ++it) {
        const std::string weaponId = it.key();
        const json& w = it.value();

        WeaponDef def{};
        def.range_m = w.value("range_m", 0.0);
        def.reload_time_s = w.value("reload_time_s", 0.0);
        def.projectile_speed_mps = w.value("projectile_speed_mps", 0.0);

        m_weapons.emplace(weaponId, def);
    }

    return !m_weapons.empty();
}

bool World::LoadUnits(const std::filesystem::path& filePath)
{
    std::ifstream f(filePath);
    if (!f.is_open()) {
        spdlog::error("Unable to open units file '{}'", filePath.string());
        return false;
    }

    json j;
    try {
        j = json::parse(f);
    } catch (const std::exception& e) {
        spdlog::error("Failed to parse units file '{}': {}", filePath.string(), e.what());
        return false;
    }

    if (!j.contains("units") || !j["units"].is_array()) {
        spdlog::error("units.json must contain an array field 'units'");
        return false;
    }

    for (const json& u : j["units"]) {
        const std::string type = u.value("type", "");
        if (type != "tank") {
            continue;
        }

        const int teamId = u.value("team_id", 0);
        const double health = u.value("health", 100.0);
        const double heading = u.value("heading_rad", 0.0);
        const std::string weaponId = u.value("weapon", "");

        const auto pos = u.value("position_m", std::vector<double>{0.0, 0.0});
        const auto vel = u.value("velocity_mps", std::vector<double>{0.0, 0.0});
        if (pos.size() != 2 || vel.size() != 2) {
            continue;
        }

        const auto weaponIt = m_weapons.find(weaponId);
        if (weaponIt == m_weapons.end()) {
            continue;
        }

        SpawnTankFromDef(teamId, health, heading, pos[0], pos[1], vel[0], vel[1], weaponIt->second);
    }

    return UnitCount() > 0;
}

void World::SpawnTankFromDef(int teamId, double health, double headingRad,
                             double px, double py, double vx, double vy,
                             const WeaponDef& weapon)
{
    const auto e = m_registry.create();
    m_registry.emplace<Tank>(e, Tank{
        .position_m = glm::dvec2(px, py),
        .velocity_mps = glm::dvec2(vx, vy),
        .heading_rad = headingRad,
        .health = health,
        .team_id = teamId,
    });
    m_registry.emplace<Weapon>(e, Weapon{
        .range_m = weapon.range_m,
        .reload_time_s = weapon.reload_time_s,
        .projectile_speed_mps = weapon.projectile_speed_mps,
    });
}

} // namespace clf::sim
