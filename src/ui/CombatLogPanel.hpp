#pragma once

namespace clf::sim {
class World;
}

namespace clf::ui {

class CombatLogPanel final {
public:
    void Render(const sim::World& world);
};

} // namespace clf::ui

