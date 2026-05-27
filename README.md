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

- WASD / Arrow keys: pan camera
- Mouse wheel: zoom
- Right mouse drag: pan camera
- Left click: select tank
- Esc: quit

## Data files (demo)

- `data/weapons.json`:
  - `projectile`, `max_range_m`, `muzzle_velocity_mps`, `reload_time_s`, `ammo`
- `data/units.json`:
  - `name`, `team_id`, `position_m`, `velocity_mps`, `radius_m`, `sensor_height_m`, `visual_range_m`, `weapon`

## Current demo behavior

- 20 km x 20 km world (meters)
- Procedural terrain height grid (hills/valleys + a central ridge) with height shading/contours
- Two tanks (Red and Blue) spawned from `data/units.json`
- Direct-fire gun model with ammo + reload from `data/weapons.json`
- Terrain-aware line-of-sight (LOS) sampling between tanks
- Deterministic firing when: in weapon range + LOS clear + ammo > 0 + reload ready
- Debug overlays for selected tank:
  - weapon range circle
  - visual range circle
  - LOS ray (green = clear, red = blocked) + blocked point marker
  - shot traces when firing
- ImGui panel shows FPS/unit count/sim time/zoom and selection + overlay toggles

## Next steps (suggested)

- Unit orders (move/attack/hold)
- Pathfinding over terrain
- Hit/damage + later armor/penetration
- Scenario loader
- Deterministic replay / lockstep mode
