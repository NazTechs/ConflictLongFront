# ConflictLongFront

ConflictLongFront is a C++20 foundation project for a **real-time strategy simulation** with physically meaningful units (meters, seconds, m/s).  
The focus is on clean architecture and a deterministic simulation loop (fixed timestep), with rendering and debug UI layered on top.

## Tech stack

- C++20
- CMake
- SDL3 (window, input, 2D rendering)
- Dear ImGui (debug UI)
- EnTT (ECS)
- GLM (math)
- spdlog (logging)
- nlohmann-json (data/config)

## Project layout

- `src/core/` - application, timing, camera
- `src/sim/` - ECS world, components, simulation systems (no rendering)
- `src/render/` - 2D renderer that reads sim state
- `src/ui/` - debug UI (ImGui)
- `data/` - sample JSON for units and weapons
- `assets/` - reserved for future art/sprites
- `scenarios/` - reserved for scenario definitions

## Build (Windows + VS Code + CMake + Ninja + vcpkg)

From the `ConflictLongFront/` folder:

1. Use an MSVC developer shell (so `cl.exe` is on PATH). Easiest:
   - Start Menu -> "Developer PowerShell for VS 2022"

2. Configure + build with vcpkg toolchain (manifest mode; dependencies auto-restore via `vcpkg.json`):

   ```powershell
   cmake -S . -B build -G Ninja `
     -DCMAKE_TOOLCHAIN_FILE="C:/Program Files/Microsoft Visual Studio/2022/Community/VC/vcpkg/scripts/buildsystems/vcpkg.cmake"

   cmake --build build
   ```

3. Run:

   ```powershell
   .\\build\\ConflictLongFront.exe
   ```

The build copies `data/` next to the executable automatically.

## Controls (demo)

- WASD / Arrow keys: pan camera (Spectator/Debug view)
- Mouse wheel: zoom
- Right mouse drag: pan camera (Spectator/Debug view)
- Left click: select tank
- Esc: quit

## Data files (demo)

- `data/weapons.json`:
  - `projectile`, `max_range_m`, `max_effective_range_m`, `muzzle_velocity_mps`, `reload_time_s`, `ammo`
  - `dispersion_mrad`, `penetration_at_0m_mm`, `penetration_at_1000m_mm`, `penetration_at_2000m_mm`, `penetration_at_3000m_mm`
- `data/units.json`:
  - `name`, `team_id`, `position_m`, `velocity_mps`, `radius_m`, `sensor_height_m`, `visual_range_m`, `weapon`
- `data/scenarios/default_battle.json`:
  - prototype scenario metadata (not fully wired yet)

## Current demo behavior

- 20 km x 20 km world (meters)
- Procedural terrain height grid (hills/valleys + a central ridge) with height shading/contours
- Two tanks (Red and Blue) spawned from `data/units.json` (seeded terrain)
- Fixed-timestep simulation; rendering can be variable-rate
- Vehicle movement model (speed/turn rates) + circle collision separation
- Sensor detection model with range + FOV + terrain LOS + detection time (no omniscient targeting)
- Simple “search and destroy” AI: patrol/search, scan, detect, aim, fire, reposition
- Direct-fire ballistics prototype:
  - dispersion-based hit/miss
  - penetration vs. range (simple curve)
  - component-based damage state (mobility/firepower/optics/engine/ammo) with “destroyed” outcomes
- View modes (ImGui -> `View` panel):
  - Spectator / Red Tank / Blue Tank / Selected Unit / Debug Tactical
  - In unit views, enemies are rendered only when detected (optional)
- UI panels:
  - `Battle Control`: pause, sim speed, seed, restart/randomize
  - `View`: view mode + overlay toggles
  - `Selected Unit`: live sensor/weapon/engagement/damage state
  - `Combat Log`: recent shot events and results

## Next steps (suggested)

- Unit orders (move/attack/hold)
- Pathfinding over terrain
- Expand tank types + scenario loader (JSON-driven)
- Better LOS/visibility metrics and “sensor cone” visualization
- More realistic damage zones (no HP) and armor/angle modeling
- Ballistic flight time + lead + stabilization
- Deterministic replay / lockstep mode
