#include "sim/World.hpp"

#include <algorithm>
#include <fstream>
#include <random>
#include <vector>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "sim/Components.hpp"
#include "sim/WeaponSystem.hpp"
#include "sim/DetectionSystem.hpp"
#include "sim/components/CollisionComponent.hpp"
#include "sim/components/DetectionComponent.hpp"
#include "sim/components/SensorComponent.hpp"
#include "sim/components/TransformComponent.hpp"
#include "sim/components/VehicleComponent.hpp"
#include "sim/ai/TankAIComponent.hpp"
#include "sim/ai/TankAISystem.hpp"
#include "sim/damage/ArmorComponent.hpp"
#include "sim/damage/DamageComponent.hpp"
#include "sim/physics/CollisionSystem.hpp"
#include "sim/physics/MovementSystem.hpp"

namespace clf::sim {

using json = nlohmann::json;

namespace {

constexpr double kTerrainCellSize_m = 20.0;
constexpr int kTerrainSamplesPerSide = static_cast<int>(terrain::Terrain::kWorldSizeMeters / kTerrainCellSize_m) + 1;

constexpr double kHalfWorld_m = terrain::Terrain::kWorldSizeMeters * 0.5;

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

    ClearSimState();
    m_units.clear();

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
    ClearSimState();
    m_units.clear();

    WeaponDef mbt120{};
    mbt120.projectile_name = "APFSDS";
    mbt120.max_range_m = 3'500.0;
    mbt120.max_effective_range_m = 3'500.0;
    mbt120.muzzle_velocity_mps = 1'600.0;
    mbt120.reload_time_s = 6.0;
    mbt120.ammo = 18;
    mbt120.dispersion_mrad = 0.9;
    mbt120.pen_at_0m_mm = 700.0;
    mbt120.pen_at_1000m_mm = 650.0;
    mbt120.pen_at_2000m_mm = 560.0;
    mbt120.pen_at_3000m_mm = 480.0;

    m_weapons.emplace("mbt_120mm", mbt120);

    UnitDef blue{};
    blue.name = "Blue";
    blue.teamId = 0;
    blue.headingRad = 0.0;
    blue.px = -3'000.0;
    blue.py = 0.0;
    blue.weaponId = "mbt_120mm";
    m_units.push_back(blue);

    UnitDef red{};
    red.name = "Red";
    red.teamId = 1;
    red.headingRad = 3.141592653589793;
    red.px = +3'000.0;
    red.py = 0.0;
    red.weaponId = "mbt_120mm";
    m_units.push_back(red);

    RestartFromLoadedData(1337);
}

void World::RestartFromLoadedData(std::uint32_t seed)
{
    m_seed = seed;
    m_registry.clear();
    m_debugLines.clear();
    m_combatLog.Clear();
    m_simTimeSeconds = 0.0;

    m_terrain.GenerateProcedural(seed);

    for (const auto& u : m_units) {
        const auto weaponIt = m_weapons.find(u.weaponId);
        if (weaponIt == m_weapons.end()) {
            continue;
        }
        SpawnTankFromDef(u.name, u.teamId, u.health, u.headingRad, u.radius_m, u.sensorHeight_m, u.visualRange_m,
                         u.px, u.py, u.vx, u.vy, u.weaponId, weaponIt->second);
    }
}

void World::RandomizeTankPositions(std::uint32_t seed)
{
    // Randomize current tanks (preserves teams/weapons) but resets motion.
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> dist(-kHalfWorld_m + 200.0, +kHalfWorld_m - 200.0);

    // Simple O(n^2) rejection sampling (fine for small unit counts).
    std::vector<glm::dvec2> placed;

    auto view = m_registry.view<Tank, Transform, Collision, Vehicle, VehicleControl>();
    for (const auto e : view) {
        const auto& tank = view.get<Tank>(e);
        auto& xf = view.get<Transform>(e);
        auto& veh = view.get<Vehicle>(e);
        auto& ctrl = view.get<VehicleControl>(e);
        const double radius = std::max(10.0, tank.radius_m);

        glm::dvec2 p{0.0, 0.0};
        bool ok = false;
        for (int attempt = 0; attempt < 500; ++attempt) {
            p = glm::dvec2(dist(rng), dist(rng));

            // Avoid very steep spots by sampling a small neighborhood and checking slope.
            const double h0 = m_terrain.HeightAtXY(p);
            const double hx = m_terrain.HeightAtXY(p + glm::dvec2(20.0, 0.0));
            const double hy = m_terrain.HeightAtXY(p + glm::dvec2(0.0, 20.0));
            const double sx = std::abs(hx - h0) / 20.0;
            const double sy = std::abs(hy - h0) / 20.0;
            if (std::max(sx, sy) > 0.30) { // ~17 degrees slope
                continue;
            }

            ok = true;
            for (const auto& other : placed) {
                const glm::dvec2 d = p - other;
                const double d2 = d.x * d.x + d.y * d.y;
                const double minSep = radius * 2.4;
                if (d2 < minSep * minSep) {
                    ok = false;
                    break;
                }
            }
            if (ok) {
                break;
            }
        }

        if (!ok) {
            // Fallback: just clamp into the map (still avoid overlap best-effort).
            p = glm::dvec2(dist(rng), dist(rng));
        }

        placed.push_back(p);
        xf.position_m = p;
        veh.speed_mps = 0.0;
        ctrl.desired_speed_mps = 0.0;
    }
}

void World::Step(double dtSeconds)
{
    UpdateDetection(m_registry, m_terrain, dtSeconds);
    ai::UpdateTankAI(m_registry, m_terrain, dtSeconds, m_seed);

    physics::UpdateMovement(m_registry, dtSeconds);
    physics::ResolveCollisions(m_registry);
    physics::ClampToMap(m_registry, m_worldHalfExtentMeters);

    weapon::UpdateDirectFire(m_registry, m_terrain, dtSeconds, m_simTimeSeconds, &m_combatLog, m_debugLines);

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

const weapons::CombatLog& World::GetCombatLog() const
{
    return m_combatLog;
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
        def.max_effective_range_m = w.value("max_effective_range_m", def.max_range_m);
        def.muzzle_velocity_mps = w.value("muzzle_velocity_mps", w.value("projectile_speed_mps", 0.0));
        def.reload_time_s = w.value("reload_time_s", 0.0);
        def.ammo = w.value("ammo", 0);

        def.dispersion_mrad = w.value("dispersion_mrad", def.dispersion_mrad);
        def.pen_at_0m_mm = w.value("penetration_at_0m_mm", def.pen_at_0m_mm);
        def.pen_at_1000m_mm = w.value("penetration_at_1000m_mm", def.pen_at_1000m_mm);
        def.pen_at_2000m_mm = w.value("penetration_at_2000m_mm", def.pen_at_2000m_mm);
        def.pen_at_3000m_mm = w.value("penetration_at_3000m_mm", def.pen_at_3000m_mm);

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

        UnitDef def{};
        def.name = name;
        def.teamId = teamId;
        def.health = health;
        def.headingRad = heading;
        def.radius_m = radius_m;
        def.sensorHeight_m = sensorHeight_m;
        def.visualRange_m = visualRange_m;
        def.px = pos[0];
        def.py = pos[1];
        def.vx = vel[0];
        def.vy = vel[1];
        def.weaponId = weaponId;
        m_units.push_back(def);
    }

    RestartFromLoadedData(1337);
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
        .team_id = teamId,
        .health = health,
        .radius_m = radius_m,
        .sensor_height_m = sensorHeight_m,
        .visual_range_m = visualRange_m,
    });

    m_registry.emplace<Transform>(e, Transform{
        .position_m = glm::dvec2(px, py),
        .hull_heading_rad = headingRad,
    });

    Vehicle vehicle{};
    vehicle.speed_mps = std::sqrt(vx * vx + vy * vy);
    vehicle.turret_heading_rad = headingRad;
    m_registry.emplace<Vehicle>(e, vehicle);

    m_registry.emplace<VehicleControl>(e, VehicleControl{
        .desired_speed_mps = vehicle.speed_mps,
        .desired_hull_heading_rad = headingRad,
        .desired_turret_heading_rad = headingRad,
    });

    m_registry.emplace<Collision>(e, Collision{.radius_m = radius_m});

    m_registry.emplace<Sensor>(e, Sensor{
        .range_m = visualRange_m,
    });
    m_registry.emplace<Detection>(e, Detection{});
    m_registry.emplace<ai::TankAI>(e, ai::TankAI{});

    // Prototype armor/damage setup (data-driven tank types come later).
    m_registry.emplace<damage::Armor>(e, damage::Armor{});
    m_registry.emplace<damage::DamageState>(e, damage::DamageState{});

    if (!name.empty()) {
        m_registry.emplace<UnitName>(e, UnitName{.value = name});
    }

    m_registry.emplace<DirectFireGun>(e, DirectFireGun{
        .weapon_id = weaponId,
        .projectile_name = weapon.projectile_name,
        .max_range_m = weapon.max_range_m,
        .max_effective_range_m = weapon.max_effective_range_m,
        .muzzle_velocity_mps = weapon.muzzle_velocity_mps,
        .reload_time_s = weapon.reload_time_s,
        .dispersion_mrad = weapon.dispersion_mrad,
        .pen_at_0m_mm = weapon.pen_at_0m_mm,
        .pen_at_1000m_mm = weapon.pen_at_1000m_mm,
        .pen_at_2000m_mm = weapon.pen_at_2000m_mm,
        .pen_at_3000m_mm = weapon.pen_at_3000m_mm,
        .ammo = weapon.ammo,
        .reload_remaining_s = 0.0,
    });
}

void World::ClearSimState()
{
    m_registry.clear();
    m_weapons.clear();
    m_debugLines.clear();
    m_combatLog.Clear();
    m_simTimeSeconds = 0.0;
    m_seed = 1337;

    m_terrain.GenerateProcedural(1337);
}

} // namespace clf::sim
