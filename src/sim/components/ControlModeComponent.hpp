#pragma once

namespace clf::sim {

enum class ControlMode : int {
    Automatic = 0,
    Manual = 1,
};

struct ControlModeComponent final {
    ControlMode mode = ControlMode::Automatic;
};

} // namespace clf::sim

