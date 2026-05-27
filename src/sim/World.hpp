#pragma once

#include <filesystem>
#include <unordered_map>
#include <string>
#include <vector>

#include <entt/entity/registry.hpp>

#include "sim/DebugDraw.hpp"
#include "sim/terrain/Terrain.hpp"

namespace clf::sim {

class World final {
public:
    World();

    bool LoadData(const std::filesystem::path& dataDir);
    void LoadFallbackSample();

    void Step(double dtSeconds);

    std::size_t UnitCount() const;
    double SimulationTimeSeconds() const;

    entt::registry& Registry();
    const entt::registry& Registry() const;

    const terrain::Terrain& GetTerrain() const;
    terrain::Terrain& GetTerrain();

    const std::vector<DebugLine>& DebugLines() const;

private:
    struct WeaponDef {
        std::string projectile_name;
        double max_range_m = 0.0;
        double muzzle_velocity_mps = 0.0;
        double reload_time_s = 0.0;
        int ammo = 0;
    };

    bool LoadWeapons(const std::filesystem::path& filePath);
    bool LoadUnits(const std::filesystem::path& filePath);

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
    terrain::Terrain m_terrain;
    std::vector<DebugLine> m_debugLines;

    double m_simTimeSeconds = 0.0;
    // Demo world bounds in meters.
    double m_worldHalfExtentMeters = terrain::Terrain::kWorldSizeMeters * 0.5;
};

} // namespace clf::sim
