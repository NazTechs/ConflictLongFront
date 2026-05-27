#pragma once

#include <filesystem>
#include <cstdint>
#include <unordered_map>
#include <string>
#include <vector>

#include <entt/entity/registry.hpp>

#include "sim/DebugDraw.hpp"
#include "sim/terrain/Terrain.hpp"
#include "sim/weapons/CombatLog.hpp"

namespace clf::sim {

class World final {
public:
    World();

    bool LoadData(const std::filesystem::path& dataDir);
    void LoadFallbackSample();
    void RestartFromLoadedData(std::uint32_t seed);
    void RandomizeTankPositions(std::uint32_t seed);

    void Step(double dtSeconds);

    std::size_t UnitCount() const;
    double SimulationTimeSeconds() const;

    entt::registry& Registry();
    const entt::registry& Registry() const;

    const terrain::Terrain& GetTerrain() const;
    terrain::Terrain& GetTerrain();

    const std::vector<DebugLine>& DebugLines() const;
    const weapons::CombatLog& GetCombatLog() const;

private:
    struct WeaponDef {
        std::string projectile_name;
        double max_range_m = 0.0;
        double muzzle_velocity_mps = 0.0;
        double reload_time_s = 0.0;
        int ammo = 0;

        // Ballistics / lethality (prototype, data-driven).
        double max_effective_range_m = 0.0;
        double dispersion_mrad = 0.8;
        double pen_at_0m_mm = 650.0;
        double pen_at_1000m_mm = 600.0;
        double pen_at_2000m_mm = 520.0;
        double pen_at_3000m_mm = 450.0;
    };

    bool LoadWeapons(const std::filesystem::path& filePath);
    bool LoadUnits(const std::filesystem::path& filePath);
    void ClearSimState();

    struct UnitDef {
        std::string name;
        int teamId = 0;
        double health = 100.0;
        double headingRad = 0.0;
        double radius_m = 6.0;
        double sensorHeight_m = 2.5;
        double visualRange_m = 5'000.0;
        double px = 0.0;
        double py = 0.0;
        double vx = 0.0;
        double vy = 0.0;
        std::string weaponId;
    };

    void SpawnTankFromDef(const std::string& name,
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
                          const WeaponDef& weapon);

    entt::registry m_registry;
    std::unordered_map<std::string, WeaponDef> m_weapons;
    std::vector<UnitDef> m_units;
    terrain::Terrain m_terrain;
    std::vector<DebugLine> m_debugLines;
    weapons::CombatLog m_combatLog;

    double m_simTimeSeconds = 0.0;
    std::uint32_t m_seed = 1337;
    // Demo world bounds in meters.
    double m_worldHalfExtentMeters = terrain::Terrain::kWorldSizeMeters * 0.5;
};

} // namespace clf::sim
