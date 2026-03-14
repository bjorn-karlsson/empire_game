# Empire Total Strategy

A Total War-inspired turn-based/real-time strategy game set in 18th century Europe,
built with C++ and OpenGL.

## Quick Start (Windows)

### Prerequisites

1. **Visual Studio 2022** (Community edition is free)
   - Install "Desktop development with C++" workload

2. **CMake 3.20+**
   - Download from https://cmake.org/download/
   - Or install via: `winget install Kitware.CMake`

3. **vcpkg** (package manager for C++ libraries)
   ```powershell
   cd C:\
   git clone https://github.com/microsoft/vcpkg.git
   cd vcpkg
   .\bootstrap-vcpkg.bat
   .\vcpkg integrate install
   ```

4. **Install dependencies via vcpkg**
   ```powershell
   .\vcpkg install glfw3:x64-windows glm:x64-windows glad:x64-windows
   ```

### Build

```powershell
cd empire_game
mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build . --config Debug
```

Or open the folder in Visual Studio (it supports CMake projects natively).

### Run

```powershell
.\Debug\EmpireTotalStrategy.exe
```

## Controls

| Key / Mouse        | Action                    |
|---------------------|---------------------------|
| WASD / Arrow Keys   | Pan camera                |
| Scroll wheel        | Zoom in/out               |
| Q / E               | Rotate camera             |
| Right-click drag    | Pan camera (drag)         |
| Left-click          | Select province / army    |
| Enter / Space       | End turn                  |
| Escape              | Pause / Unpause           |
| Alt+F4              | Quit                      |

## Project Architecture

```
empire_game/
├── CMakeLists.txt              # Build system
├── README.md                   # This file
│
├── include/                    # Header files
│   ├── core/                   # Engine core
│   │   ├── Game.h              # Main game loop, state machine
│   │   ├── InputManager.h      # Keyboard/mouse input
│   │   └── ResourceManager.h   # Asset loading & caching
│   │
│   ├── campaign/               # Strategic layer
│   │   ├── CampaignMap.h       # Province/faction/army management
│   │   ├── Province.h          # Territory data (economy, buildings)
│   │   ├── Faction.h           # Nations (diplomacy, treasury)
│   │   ├── Army.h              # Military units and armies
│   │   ├── Economy.h           # Income/expense calculations
│   │   └── TurnManager.h       # End-turn sequence orchestration
│   │
│   ├── battle/                 # Combat layer
│   │   ├── BattleScene.h       # Battle state management
│   │   └── BattleResolver.h    # Auto-resolve combat math
│   │
│   ├── rendering/              # Graphics
│   │   ├── Renderer.h          # Main OpenGL renderer
│   │   ├── Camera.h            # 3D strategy camera
│   │   ├── Shader.h            # GLSL shader wrapper
│   │   ├── MapRenderer.h       # Province borders, water, roads
│   │   └── UIRenderer.h        # 2D overlay rendering
│   │
│   ├── ui/                     # User interface
│   │   ├── UIManager.h         # UI state coordination
│   │   ├── ProvincePanel.h     # Province info display
│   │   ├── ArmyPanel.h         # Army info display
│   │   └── TopBar.h            # Treasury, turn counter
│   │
│   └── utils/                  # Utilities
│       ├── Logger.h            # Printf-style logging
│       └── MathUtils.h         # Geometry helpers
│
├── src/                        # Implementation files
│   ├── main.cpp                # Entry point
│   └── ... (mirrors include/)
│
├── assets/                     # Game assets (copied to build)
│   ├── shaders/                # GLSL shader files (future)
│   ├── textures/               # Province textures (future)
│   ├── maps/                   # Map geometry data (future)
│   └── fonts/                  # UI fonts (future)
│
└── data/                       # Game data files
    └── europe_campaign.json    # Campaign definition (future)
```

## Design Philosophy

### 1. Gameplay First, Graphics Last
The entire game logic (turns, economy, combat, movement) works before any
fancy rendering exists. Provinces are colored hexagons. Armies are pyramids.
This is intentional — get the *game* working, then make it pretty.

### 2. Clean Separation of Concerns
- **Game logic** (campaign/, battle/) never touches OpenGL
- **Rendering** (rendering/) takes const references and draws — never modifies state
- **Input** goes through InputManager — no raw GLFW calls in game logic
- **UI** is its own layer between game state and rendering

### 3. Data-Driven Design
Provinces, factions, units, and buildings are defined as plain structs.
The test map is hardcoded for now, but the LoadFromFile() path exists
for loading from JSON later. This means you can mod the game by editing
data files without recompiling.

### 4. Incremental Complexity
Each phase builds on the last without breaking what works:
- Phase 1: Map + provinces + economy + end turn → "it's a game"
- Phase 2: Armies + movement + selection → "it's a strategy game"
- Phase 3: Auto-resolve battles → "armies mean something"
- Phase 4: Real-time battles → "it's Total War"

## Phase Roadmap

### Phase 1 — Campaign Map Foundation ✅ (scaffolded)
- [x] Project structure and build system
- [x] OpenGL window with 3D camera
- [x] Province data structures with economy
- [x] Faction system with 8 European nations
- [x] Turn system (seasons/years from 1700)
- [ ] Render provinces as colored hexagons on screen
- [ ] Click to select provinces
- [ ] Province info panel
- [ ] Top bar with treasury/turn display
- [ ] End Turn button processing

### Phase 2 — Armies & Movement
- [x] Army and unit data structures
- [x] Starting armies for all factions
- [ ] Click to select armies
- [ ] Click-to-move with adjacency checking
- [ ] Animated army movement between provinces
- [ ] Army info panel with unit list
- [ ] Unit recruitment from provinces

### Phase 3 — Auto-Resolve Battles
- [x] Battle resolver with round-based combat
- [x] Morale and routing system
- [x] Battle scene state management
- [ ] Battle results screen with casualty report
- [ ] Province ownership changes after battles
- [ ] AI army movement (basic threat response)

### Phase 4 — Real-Time Tactical Battles
- [ ] Separate 3D battle scene with terrain
- [ ] Unit blocks with formation rendering
- [ ] Movement orders (click to move, attack-move)
- [ ] Ranged combat (musket volleys, artillery)
- [ ] Morale system (units break and rout)
- [ ] Line infantry vs cavalry vs artillery balance

## Next Steps (What To Work On First)

The scaffold compiles and runs a window. Here's what to do next
to get something playable:

1. **Get glad working** — Uncomment the gladLoadGLLoader call in Game.cpp
2. **See hexagons** — The province renderer is ready, just needs the
   OpenGL context fully initialized
3. **Click provinces** — Wire up Camera::ScreenToWorldPlane() to
   CampaignMap::GetProvinceAtWorldPos()
4. **Add text rendering** — Pick stb_truetype or FreeType, needed for
   province names and UI
5. **End turn loop** — Connect UIManager's end turn detection to
   TurnManager::EndTurn()

## Libraries Used

| Library | Purpose | Install |
|---------|---------|---------|
| GLFW 3  | Window, input, OpenGL context | `vcpkg install glfw3` |
| GLM     | Math (vectors, matrices) | `vcpkg install glm` |
| glad    | OpenGL function loader | `vcpkg install glad` |
| stb_image | Texture loading (future) | Header-only, download from GitHub |
| nlohmann/json | Data file loading (future) | `vcpkg install nlohmann-json` |
