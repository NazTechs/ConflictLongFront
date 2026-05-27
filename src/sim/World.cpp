#include "sim/World.hpp"

#include <algorithm>
#include <fstream>
#include <vector>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "sim/Components.hpp"
#include "sim/WeaponSystem.hpp"
#include "sim/Systems.hpp"

namespace clf::sim {

using json = nlohmann::json;

namespace {

constexpr double kTerrainCellSize_m = 20.0;
constexpr int kTerrainSamplesPerSide = static_cast<int>(terrain::Terrain::kWorldSizeMeters / kTerrainCellSize_m) + 1;

} // namespace

World::World()
    : m_terrain(kTerrainSamplesPerSide, kTerrainCellSize_m)
{
    m_terrain.GenerateProcedural(1337);
}

bool World::LoadData(const std::filesystem::path& dataDir)
{
    const auto weaponsPath = dataDir / "weapons.json";
    const auto unitsPath = dataDir / "units.json";

    m_registry.clear();
    m_weapons.clear();
    m_simTimeSeconds = 0.0;

    m_terrain.GenerateProcedural(1337);

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
    m_terrain.GenerateProcedural(1337);

    WeaponDef mbt120{};
    mbt120.projectile_name = "APFSDS";
    mbt120.max_range_m = 3'500.0;
    mbt120.muzzle_velocity_mps = 1'600.0;
    mbt120.reload_time_s = 6.0;
    mbt120.ammo = 18;

    m_weapons.emplace("mbt_120mm", mbt120);

    // Place the tanks on opposite sides of the central ridge (x=0) so LOS is often blocked.
    SpawnTankFromDef("Blue", 0, 100.0, 0.0, 6.0, 2.5, 5'000.0, -3'000.0, 0.0, 0.0, 0.0, "mbt_120mm", mbt120);
    SpawnTankFromDef("Red", 1, 100.0, 3.141592653589793, 6.0, 2.5, 5'000.0, +3'000.0, 0.0, 0.0, 0.0, "mbt_120mm", mbt120);
}

void World::Step(double dtSeconds)
{
    systems::IntegrateTanks(m_registry, dtSeconds);

    const glm::dvec2 minMeters{-m_worldHalfExtentMeters, -m_worldHalfExtentMeters};
    const glm::dvec2 maxMeters{+m_worldHalfExtentMeters, +m_worldHalfExtentMeters};
    systems::BounceTanksInAabb(m_registry, minMeters, maxMeters);

    weapon::UpdateDirectFire(m_registry, m_terrain, dtSeconds, m_debugLines);

    // Age out debug lines (shot traces, etc.).
    for (auto& line : m_debugLines) {
        line.ttl_s -= dtSeconds;
    }
    m_debugLines.erase(
        std::remove_if(m_debugLines.begin(), m_debugLines.end(), [](const DebugLine& l) { return l.ttl_s <= 0.0; }),
        m_debugLines.end());

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

const terrain::Terrain& World::GetTerrain() const
{
    return m_terrain;
}

terrain::Terrain& World::GetTerrain()
{
    return m_terrain;
}

const std::vector<DebugLine>& World::DebugLines() const
{
    return m_debugLines;
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
        def.projectile_name = w.value("projectile", w.value("projectile_name", std::string{}));

        // Backward compatible keys:
        // - Old: range_m, projectile_speed_mps
        // - New: max_range_m, muzzle_velocity_mps
        def.max_range_m = w.value("max_range_m", w.value("range_m", 0.0));
        def.muzzle_velocity_mps = w.value("muzzle_velocity_mps", w.value("projectile_speed_mps", 0.0));
        def.reload_time_s = w.value("reload_time_s", 0.0);
        def.ammo = w.value("ammo", 0);

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

        const std::string name = u.value("name", "");
        const int teamId = u.value("team_id", 0);
        const double health = u.value("health", 100.0);
        const double heading = u.value("heading_rad", 0.0);
        const std::string weaponId = u.value("weapon", "");
        const double radius_m = u.value("radius_m", 6.0);
        const double sensorHeight_m = u.value("sensor_height_m", 2.5);
        const double visualRange_m = u.value("visual_range_m", 5'000.0);

        const auto pos = u.value("position_m", std::vector<double>{0.0, 0.0});
        const auto vel = u.value("velocity_mps", std::vector<double>{0.0, 0.0});
        if (pos.size() != 2 || vel.size() != 2) {
            continue;
        }

        const auto weaponIt = m_weapons.find(weaponId);
        if (weaponIt == m_weapons.end()) {
            continue;
        }

        SpawnTankFromDef(name, teamId, health, heading, radius_m, sensorHeight_m, visualRange_m,
                         pos[0], pos[1], vel[0], vel[1], weaponId, weaponIt->second);
    }

    return UnitCount() > 0;
}

void World::SpawnTankFromDef(const std::string& name,
                             int teamId,
                             double health,
                             double headingRad,
                             double radius_m,
                             double sensorHeight_m,
                             double visualRange_m,
                             double px,
                             double py,
                             double vx,
                             double vy,
                             const std::string& weaponId,
                             const WeaponDef& weapon)
{
    const auto e = m_registry.create();
    m_registry.emplace<Tank>(e, Tank{
        .position_m = glm::dvec2(px, py),
        .velocity_mps = glm::dvec2(vx, vy),
        .heading_rad = headingRad,
        .team_id = teamId,
        .health = health,
        .radius_m = radius_m,
        .sensor_height_m = sensorHeight_m,
        .visual_range_m = visualRange_m,
    });

    if (!name.empty()) {
        m_registry.emplace<UnitName>(e, UnitName{.value = name});
    }

    m_registry.emplace<DirectFireGun>(e, DirectFireGun{
        .weapon_id = weaponId,
        .projectile_name = weapon.projectile_name,
        .max_range_m = weapon.max_range_m,
        .muzzle_velocity_mps = weapon.muzzle_velocity_mps,
        .reload_time_s = weapon.reload_time_s,
        .ammo = weapon.ammo,
        .reload_remaining_s = 0.0,
    });
}

} // namespace clf::sim
