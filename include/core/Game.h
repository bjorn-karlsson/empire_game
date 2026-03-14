#pragma once

#include <string>
#include <memory>

// Forward declarations - keeps compile times fast
struct GLFWwindow;

class Renderer;
class InputManager;
class CampaignMap;
class BattleScene;
class UIManager;
class TurnManager;

// ─── Game States ──────────────────────────────────────────────
enum class GameState {
    MAIN_MENU,
    CAMPAIGN_MAP,
    BATTLE,
    DIPLOMACY,
    PAUSED
};

// ─── Core Game Class ──────────────────────────────────────────
// Owns the game loop, window, and all major subsystems.
// Think of this as the "director" — it doesn't do rendering
// or gameplay logic itself, it coordinates who runs when.
class Game {
public:
    Game();
    ~Game();

    bool Init(int width, int height, const std::string& title);
    void Run();
    void Shutdown();

    // State transitions
    void SetState(GameState newState);
    GameState GetState() const { return m_state; }

    // Access to subsystems (other systems may need these)
    Renderer*    GetRenderer()    const { return m_renderer.get(); }
    InputManager* GetInput()     const { return m_input.get(); }
    CampaignMap* GetCampaignMap() const { return m_campaignMap.get(); }

    int GetWindowWidth()  const { return m_windowWidth; }
    int GetWindowHeight() const { return m_windowHeight; }

private:
    void ProcessInput();
    void Update(float deltaTime);
    void Render();

    // Window
    GLFWwindow* m_window = nullptr;
    int m_windowWidth  = 1280;
    int m_windowHeight = 720;

    // State
    GameState m_state = GameState::MAIN_MENU;
    bool m_running = false;

    // Subsystems (order matters for initialization!)
    std::unique_ptr<Renderer>     m_renderer;
    std::unique_ptr<InputManager> m_input;
    std::unique_ptr<CampaignMap>  m_campaignMap;
    std::unique_ptr<BattleScene>  m_battleScene;
    std::unique_ptr<UIManager>    m_ui;
    std::unique_ptr<TurnManager>  m_turnManager;

    // Timing
    float m_lastFrameTime = 0.0f;
};
