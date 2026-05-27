#pragma once

#include <cstdint>
#include <filesystem>

namespace clf::sim {

class World;

struct ScenarioSettings final {
    std::uint32_t seed = 1337;
    bool randomizePositions = false;
};

class ScenarioManager final {
public:
    explicit ScenarioManager(World& world);

    bool LoadDefaultScenario(const std::filesystem::path& scenariosDir);

    void Restart(const ScenarioSettings& settings);
    void RandomizePositions(const ScenarioSettings& settings);

    const ScenarioSettings& CurrentSettings() const;

private:
    World& m_world;
    ScenarioSettings m_current{};

    // Cache paths so Restart can re-load.
    std::filesystem::path m_dataDir;
};

} // namespace clf::sim

