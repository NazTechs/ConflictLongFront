#pragma once

#include <cstdint>

namespace clf::sim::damage {

enum class SubsystemState : std::uint8_t {
    Ok = 0,
    Damaged,
    Disabled,
    Destroyed,
};

struct DamageState final {
    SubsystemState mobility = SubsystemState::Ok;
    SubsystemState firepower = SubsystemState::Ok;
    SubsystemState optics = SubsystemState::Ok;
    SubsystemState engine = SubsystemState::Ok;
    SubsystemState ammo = SubsystemState::Ok;

    bool destroyed = false;
};

} // namespace clf::sim::damage

