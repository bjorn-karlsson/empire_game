#pragma once

class Renderer;
class InputManager;

// ─── UI Manager ───────────────────────────────────────────────
// Handles all 2D UI overlay rendering: province info panels,
// army panels, top bar (treasury, turn counter), buttons.
//
// For Phase 1-2, UI will be rendered as simple colored quads
// with text (once we add a font renderer). No need for a full
// UI framework — keep it simple.
//
// Later you could integrate ImGui for debug UI, but the game
// UI should be custom-rendered for that Total War feel.
class UIManager {
public:
    UIManager();
    ~UIManager();

    void Init(int screenWidth, int screenHeight);
    void Update(float deltaTime, const InputManager& input);
    void Render(Renderer& renderer);
    void RenderPauseOverlay(Renderer& renderer);

    // UI state
    bool IsEndTurnButtonClicked() const { return m_endTurnClicked; }
    void ClearEndTurnClick() { m_endTurnClicked = false; }

private:
    int m_screenWidth  = 1280;
    int m_screenHeight = 720;

    bool m_endTurnClicked = false;
    bool m_showProvincePanel = false;
    bool m_showArmyPanel = false;
};
