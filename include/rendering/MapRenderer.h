#pragma once

// ─── Map Renderer ─────────────────────────────────────────────
// Specialized rendering for the campaign map:
//   - Province borders (line rendering)
//   - Water/ocean areas
//   - Roads between provinces
//   - Province name labels (requires font rendering)
//   - Fog of war overlay
//
// This class will grow as we add visual polish to the map.
// Separated from the main Renderer to keep things organized.
class MapRenderer {
public:
    void Init();
    void RenderBorders();
    void RenderWater();
    void RenderRoads();
    void RenderFogOfWar();
};
