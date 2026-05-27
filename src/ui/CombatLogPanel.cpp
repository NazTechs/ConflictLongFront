#include "ui/CombatLogPanel.hpp"

#include <imgui.h>

#include "sim/World.hpp"

namespace clf::ui {

void CombatLogPanel::Render(const sim::World& world)
{
    ImGui::Begin("Combat Log");

    const auto& events = world.GetCombatLog().Events();
    ImGui::Text("Events: %d", static_cast<int>(events.size()));
    ImGui::Separator();

    ImGui::BeginChild("combatlog_scroller", ImVec2(0, 0), false);
    for (auto it = events.rbegin(); it != events.rend(); ++it) {
        const auto& e = *it;
        ImGui::Text("[%.1fs] %s @ %.0fm: %s",
                    e.sim_time_s,
                    e.projectile_name.empty() ? e.weapon_id.c_str() : e.projectile_name.c_str(),
                    e.distance_m,
                    e.result_summary.c_str());
    }
    ImGui::EndChild();

    ImGui::End();
}

} // namespace clf::ui

