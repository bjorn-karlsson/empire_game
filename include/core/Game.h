#pragma once

#include <string>
#include <memory>
#include <vector>
#include "editor/MapEditor.h"

struct GLFWwindow;

class Renderer;
class InputManager;
class CampaignMap;
class BattleScene;
class UIManager;
class TurnManager;

enum class GameState {
    MAIN_MENU,
    CAMPAIGN_MAP,
    BATTLE,
    DIPLOMACY,
    PAUSED
};

enum class TurnExecPhase {
    IDLE,
    PLAYER_MOVES,
    AI_FACTION,
    WAITING_MOVE,
    DONE
};

class Game {
public:
    Game();
    ~Game();

    bool Init(int width, int height, const std::string& title);
    void Run();
    void Shutdown();

    void SetState(GameState newState);
    GameState GetState() const { return m_state; }

    Renderer* GetRenderer()    const { return m_renderer.get(); }
    InputManager* GetInput()     const { return m_input.get(); }
    CampaignMap* GetCampaignMap() const { return m_campaignMap.get(); }

    int GetWindowWidth()  const { return m_windowWidth; }
    int GetWindowHeight() const { return m_windowHeight; }

private:
    void ProcessInput();
    void Update(float deltaTime);
    void Render();

    GLFWwindow* m_window = nullptr;
    int m_windowWidth = 1280;
    int m_windowHeight = 720;

    GameState m_state = GameState::MAIN_MENU;
    bool m_running = false;

    std::unique_ptr<Renderer>     m_renderer;
    std::unique_ptr<InputManager> m_input;
    std::unique_ptr<CampaignMap>  m_campaignMap;
    std::unique_ptr<BattleScene>  m_battleScene;
    std::unique_ptr<UIManager>    m_ui;
    std::unique_ptr<TurnManager>  m_turnManager;

    float m_lastFrameTime = 0.0f;

    // Turn execution state machine
    TurnExecPhase m_turnPhase = TurnExecPhase::IDLE;
    int m_execFactionIdx = 0;
    int m_execArmyIdx = 0;
    int m_followArmyId = -1;
    float m_turnExecTimer = 0.0f;
    bool m_cameraLocked = false;
    float m_savedCamDist = 0;
    std::vector<std::string> m_aiFactionOrder;
    std::string m_currentAIFaction;

    // Map editor
    MapEditor m_editor;
};