#pragma once

#include <filesystem>
#include <unordered_map>
#include <string>

#include <entt/entity/registry.hpp>

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

private:
    struct WeaponDef {
        double range_m = 0.0;
        double reload_time_s = 0.0;
        double projectile_speed_mps = 0.0;
    };

    bool LoadWeapons(const std::filesystem::path& filePath);
    bool LoadUnits(const std::filesystem::path& filePath);

    void SpawnTankFromDef(int teamId, double health, double headingRad,
                          double px, double py, double vx, double vy,
                          const WeaponDef& weapon);

    entt::registry m_registry;
    std::unordered_map<std::string, WeaponDef> m_weapons;

    double m_simTimeSeconds = 0.0;
    // Demo world bounds in meters.
    double m_worldHalfExtentMeters = 120.0;
};

} // namespace clf::sim

