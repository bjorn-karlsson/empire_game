#pragma once

#include <glm/glm.hpp>
#include <string>

// ─── UI Renderer ──────────────────────────────────────────────
// Low-level 2D rendering for UI elements.
// Uses orthographic projection to draw directly in screen space.
//
// Provides primitives:
//   - Colored rectangles (panels, buttons, health bars)
//   - Text rendering (via bitmap font or stb_truetype)
//   - Textured quads (icons, portraits)
class UIRenderer {
public:
    void Init(int screenWidth, int screenHeight);

    void BeginUI(); // Switch to orthographic projection
    void EndUI();   // Restore 3D projection

    // Primitives
    void DrawRect(float x, float y, float width, float height, const glm::vec4& color);
    void DrawRectOutline(float x, float y, float width, float height,
                         const glm::vec4& color, float thickness = 1.0f);
    // void DrawText(const std::string& text, float x, float y, float scale,
    //               const glm::vec3& color);  // Needs font system

private:
    int m_screenWidth = 1280;
    int m_screenHeight = 720;
    unsigned int m_quadVAO = 0;
    unsigned int m_quadVBO = 0;
};
