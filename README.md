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
- Esc: quit

## Current demo behavior

- Loads `data/weapons.json` and `data/units.json`
- Spawns a handful of tanks (ECS entities)
- Runs a fixed-timestep simulation and integrates velocity in **m/s**
- Draws tanks as simple rectangles
- Shows a debug panel with FPS, unit count, simulation time, and zoom level

## Next steps (suggested)

- Terrain grid + pathfinding
- Order system (move/attack/hold)
- Weapon firing and hit effects (still in meters/seconds)
- Scenario loader
- Deterministic replay / lockstep mode
