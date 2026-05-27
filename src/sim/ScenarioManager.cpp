#include "sim/ScenarioManager.hpp"

#include <spdlog/spdlog.h>

#include "sim/World.hpp"

namespace clf::sim {

ScenarioManager::ScenarioManager(World& world)
    : m_world(world)
{
}

bool ScenarioManager::LoadDefaultScenario(const std::filesystem::path& scenariosDir)
{
    // For now, the “scenario” is just the existing JSON data directory next to the executable.
    // This keeps it additive: later we can load `data/scenarios/*.json` and override spawn lists, AI flags, etc.
    (void)scenariosDir;
    m_dataDir = std::filesystem::path{};
    return true;
}

void ScenarioManager::Restart(const ScenarioSettings& settings)
{
    m_current = settings;

    // World::LoadData reads JSON from a dir; Application already resolves `.../data`.
    // Here we assume the World already has a valid data dir and has loaded at least once.
    // If loading fails, fallback sample is used.
    (void)m_dataDir;

    spdlog::info("Restart battle (seed {}, randomize {})", settings.seed, settings.randomizePositions ? "yes" : "no");
    m_world.RestartFromLoadedData(settings.seed);

    if (settings.randomizePositions) {
        m_world.RandomizeTankPositions(settings.seed);
    }
}

void ScenarioManager::RandomizePositions(const ScenarioSettings& settings)
{
    m_current = settings;
    spdlog::info("Randomize positions (seed {})", settings.seed);
    m_world.RestartFromLoadedData(settings.seed);
    m_world.RandomizeTankPositions(settings.seed);
}

const ScenarioSettings& ScenarioManager::CurrentSettings() const
{
    return m_current;
}

} // namespace clf::sim
