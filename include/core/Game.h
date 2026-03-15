#pragma once

#include <string>
#include <memory>
#include <vector>

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

// Turn execution phases
enum class TurnExecPhase {
    IDLE,           // player is playing
    PLAYER_MOVES,   // executing player's scheduled movements
    AI_FACTION,     // executing one AI faction's moves
    WAITING_MOVE,   // waiting for current army to finish moving
    DONE            // all factions done, advance turn
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

    // Turn execution state machine
    TurnExecPhase m_turnPhase = TurnExecPhase::IDLE;
    int m_execFactionIdx = 0;     // which AI faction we're processing
    int m_execArmyIdx = 0;        // which army within that faction
    int m_followArmyId = -1;      // camera follows this army
    float m_turnExecTimer = 0.0f; // timer for delays between moves
    bool m_cameraLocked = false;
    float m_savedCamDist = 0;     // saved zoom before lock
    std::vector<std::string> m_aiFactionOrder;  
    std::string m_currentAIFaction;             
};
