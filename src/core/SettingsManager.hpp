#pragma once

#include <filesystem>

#include "core/AppSettings.hpp"

namespace clf::core {

class SettingsManager final {
public:
    explicit SettingsManager(std::filesystem::path settingsDir);

    const std::filesystem::path& SettingsDir() const;
    std::filesystem::path UserSettingsPath() const;
    std::filesystem::path ImGuiIniPath() const;

    AppSettings LoadOrDefaults() const;
    void Save(const AppSettings& settings) const;

private:
    std::filesystem::path m_settingsDir;
};

} // namespace clf::core

