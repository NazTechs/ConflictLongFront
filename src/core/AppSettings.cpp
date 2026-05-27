#include "core/AppSettings.hpp"

#include <algorithm>

namespace clf::core {

std::string ToString(render::ViewMode mode)
{
    switch (mode) {
        case render::ViewMode::Spectator: return "Spectator";
        case render::ViewMode::RedTank: return "RedTank";
        case render::ViewMode::BlueTank: return "BlueTank";
        case render::ViewMode::SelectedTank: return "SelectedTank";
        case render::ViewMode::DebugTactical: return "DebugTactical";
    }
    return "Spectator";
}

render::ViewMode ViewModeFromString(const std::string& s, render::ViewMode fallback)
{
    std::string t = s;
    std::transform(t.begin(), t.end(), t.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (t == "spectator") return render::ViewMode::Spectator;
    if (t == "redtank" || t == "red tank view") return render::ViewMode::RedTank;
    if (t == "bluetank" || t == "blue tank view") return render::ViewMode::BlueTank;
    if (t == "selectedtank" || t == "selected unit view") return render::ViewMode::SelectedTank;
    if (t == "debugtactical" || t == "debug tactical view" || t == "debug") return render::ViewMode::DebugTactical;

    return fallback;
}

} // namespace clf::core

