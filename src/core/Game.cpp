// ============================================================================
// Game.cpp — Core game loop and subsystem orchestration
// ============================================================================

// OpenGL loader — MUST be first include
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "core/Game.h"
#include "core/InputManager.h"
#include "rendering/Renderer.h"
#include "rendering/Camera.h"
#include "campaign/CampaignMap.h"
#include "campaign/TurnManager.h"
#include "battle/BattleScene.h"
#include "ui/UIManager.h"
#include "utils/Logger.h"

// ─── Window resize callback ──────────────────────────────────
static Game* g_gameInstance = nullptr;

static void framebufferSizeCallback(GLFWwindow* window, int width, int height)
{
    if(width<=0||height<=0)return;
    glViewport(0, 0, width, height);
    if(g_gameInstance){
        g_gameInstance->GetRenderer()->OnResize(width,height);
        // Update stored dimensions
        // (accessed via the Game pointer trick below)
    }
}

// ─── Constructor / Destructor ─────────────────────────────────
Game::Game() = default;
Game::~Game() = default;

// ─── Initialization ───────────────────────────────────────────
bool Game::Init(int width, int height, const std::string& title)
{
    m_windowWidth  = width;
    m_windowHeight = height;

    // --- GLFW ---
    if (!glfwInit()) {
        Logger::Error("Failed to initialize GLFW");
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4); // MSAA

    m_window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (!m_window) {
        Logger::Error("Failed to create GLFW window");
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(m_window);
    glfwSetFramebufferSizeCallback(m_window, framebufferSizeCallback);
    glfwSwapInterval(1); // VSync

    // --- Load OpenGL functions via glad ---
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        Logger::Error("Failed to initialize GLAD");
        return false;
    }

    Logger::Info("OpenGL context created: %dx%d", width, height);

    // --- Create subsystems ---
    g_gameInstance = this;

    m_input       = std::make_unique<InputManager>(m_window);
    m_renderer    = std::make_unique<Renderer>(width, height);
    m_campaignMap = std::make_unique<CampaignMap>();
    m_turnManager = std::make_unique<TurnManager>();
    m_battleScene = std::make_unique<BattleScene>();
    m_ui          = std::make_unique<UIManager>();

    // --- Initialize subsystems ---
    if (!m_renderer->Init()) {
        Logger::Error("Failed to initialize renderer");
        return false;
    }

    // Load the default campaign (18th century Europe)
    if (!m_campaignMap->LoadFromFile("data/europe_campaign.json")) {
        Logger::Warning("No campaign data found — using default test map");
        m_campaignMap->GenerateTestMap();
    }

    m_turnManager->Init(m_campaignMap.get());
    m_ui->Init(width, height);

    // Build GPU geometry for the campaign map
    m_renderer->BuildMapGeometry(*m_campaignMap);

    // Start on campaign map
    m_state   = GameState::CAMPAIGN_MAP;
    m_running = true;

    Logger::Info("Game initialized successfully!");
    return true;
}

// ─── Main Game Loop ───────────────────────────────────────────
// Classic fixed-timestep loop. Update logic runs at consistent
// rate regardless of frame rate, rendering happens as fast as possible.
void Game::Run()
{
    Logger::Info("Entering main game loop...");

    while (m_running && !glfwWindowShouldClose(m_window)) {
        float currentTime = static_cast<float>(glfwGetTime());
        float deltaTime   = currentTime - m_lastFrameTime;
        m_lastFrameTime   = currentTime;
        if (deltaTime > 0.1f) deltaTime = 0.1f;

        // Sync window dimensions each frame
        int fw,fh;
        glfwGetFramebufferSize(m_window,&fw,&fh);
        if(fw>0&&fh>0&&(fw!=m_windowWidth||fh!=m_windowHeight)){
            m_windowWidth=fw;m_windowHeight=fh;
            m_renderer->OnResize(fw,fh);
        }

        ProcessInput();
        Update(deltaTime);
        Render();

        glfwSwapBuffers(m_window);
        glfwPollEvents();
    }
}

// ─── Input ────────────────────────────────────────────────────
void Game::ProcessInput()
{
    m_input->Update();

    // Global keybinds
    if (m_input->IsKeyPressed(GLFW_KEY_ESCAPE)) {
        if (m_state == GameState::PAUSED) {
            m_state = GameState::CAMPAIGN_MAP;
        } else if (m_state == GameState::CAMPAIGN_MAP) {
            m_state = GameState::PAUSED;
        }
    }

    // Debug: quick exit
    if (m_input->IsKeyDown(GLFW_KEY_LEFT_ALT) && m_input->IsKeyPressed(GLFW_KEY_F4)) {
        m_running = false;
    }

    // ─── Camera controls (only on campaign map) ──────────────
    if (m_state == GameState::CAMPAIGN_MAP) {
        Camera* cam = m_renderer->GetCamera();
        if (!cam) return;

        // Block most input during turn execution
        bool executing = (m_turnPhase != TurnExecPhase::IDLE);

        float dt = 0.016f;

        // WASD pan (blocked during execution)
        if (!executing) {
            float panX = 0.0f, panZ = 0.0f;
            if (m_input->IsKeyDown(GLFW_KEY_W) || m_input->IsKeyDown(GLFW_KEY_UP))    panZ -= dt;
            if (m_input->IsKeyDown(GLFW_KEY_S) || m_input->IsKeyDown(GLFW_KEY_DOWN))  panZ += dt;
            if (m_input->IsKeyDown(GLFW_KEY_A) || m_input->IsKeyDown(GLFW_KEY_LEFT))  panX -= dt;
            if (m_input->IsKeyDown(GLFW_KEY_D) || m_input->IsKeyDown(GLFW_KEY_RIGHT)) panX += dt;
            if (panX != 0.0f || panZ != 0.0f)
                cam->Pan(panX, panZ);
        }

        // Middle mouse drag → pan (always allowed)
        if (m_input->IsMouseButtonDown(2)) {
            glm::vec2 delta = m_input->GetMouseDelta();
            cam->Pan(-delta.x * dt * 0.3f, -delta.y * dt * 0.3f);
        }

        // Scroll wheel → zoom (always allowed)
        float scroll = m_input->GetScrollDelta();
        if (scroll != 0.0f)
            cam->Zoom(scroll);

        // Left click → SELECT objects (or exchange modal interaction)
        if (m_input->IsMouseButtonPressed(0) && !executing) {
            glm::vec2 mousePos = m_input->GetMousePos();
            if (m_campaignMap->IsExchangeOpen()) {
                m_campaignMap->HandleExchangeClick(
                    mousePos.x, mousePos.y,
                    (float)m_windowWidth, (float)m_windowHeight);
            } else {
                glm::vec3 worldPos = cam->ScreenToWorldPlane(
                    mousePos.x, mousePos.y,
                    (float)m_windowWidth, (float)m_windowHeight
                );
                m_campaignMap->HandleLeftClick(worldPos);
            }
        }

        // Right click → ISSUE MOVE ORDER / MERGE (not during exchange or execution)
        if (m_input->IsMouseButtonPressed(1) && !m_campaignMap->IsExchangeOpen() && !executing) {
            glm::vec2 mousePos = m_input->GetMousePos();
            glm::vec3 worldPos = cam->ScreenToWorldPlane(
                mousePos.x, mousePos.y,
                (float)m_windowWidth, (float)m_windowHeight
            );
            m_campaignMap->HandleRightClick(worldPos);
        }

        // Escape closes exchange modal
        if (m_input->IsKeyPressed(GLFW_KEY_ESCAPE) && m_campaignMap->IsExchangeOpen()) {
            m_campaignMap->CancelExchange();
        }
    }
}

// ─── Update ───────────────────────────────────────────────────
void Game::Update(float deltaTime)
{
    Camera* cam = m_renderer->GetCamera();
    if (cam) cam->Update(deltaTime);

    // Safety timer to prevent freeze
    static float safetyTimer = 0;
    if (m_turnPhase != TurnExecPhase::IDLE) {
        safetyTimer += deltaTime;
        if (safetyTimer > 8.0f) {
            Logger::Warning("Turn execution timeout!");
            m_campaignMap->StopAllArmies();
            m_campaignMap->ProcessTurn();
            m_turnPhase = TurnExecPhase::IDLE;
            m_cameraLocked = false;
            if(cam && m_savedCamDist > 0) cam->SetDistance(m_savedCamDist);
            safetyTimer = 0;
        }
    } else { safetyTimer = 0; }

    switch (m_state) {
    case GameState::CAMPAIGN_MAP:
    {
        if (m_turnPhase == TurnExecPhase::IDLE) {
            m_campaignMap->Update(deltaTime, *m_input);
            m_ui->Update(deltaTime, *m_input);

            if (m_ui->IsEndTurnButtonClicked() || m_input->IsKeyPressed(GLFW_KEY_SPACE) ||
                m_input->IsKeyPressed(GLFW_KEY_ENTER)) {
                m_ui->ClearEndTurnClick();
                m_campaignMap->StopAllArmies();
                m_turnPhase = TurnExecPhase::PLAYER_MOVES;
                m_followArmyId = -1;
                m_turnExecTimer = 0.1f;
                if (cam) m_savedCamDist = cam->GetDistance();
                m_cameraLocked = true;

                // Prepare AI faction order
                m_aiFactionOrder = m_campaignMap->GetAIFactionIds();
                m_execFactionIdx = 0;
                m_currentAIFaction.clear();
            }
        }
        // ── PLAYER MOVES: execute player's scheduled armies one by one ──
        else if (m_turnPhase == TurnExecPhase::PLAYER_MOVES) {
            m_campaignMap->Update(deltaTime, *m_input);
            if (m_followArmyId >= 0) {
                const Army* fa = m_campaignMap->GetArmy(m_followArmyId);
                if (fa && fa->isMoving) {
                    if (cam) cam->SetTarget(fa->worldPosition + glm::vec3(0, 0, 1));
                }
                else {
                    m_followArmyId = -1;
                    m_turnExecTimer = 0.3f;
                }
            }
            else {
                m_turnExecTimer -= deltaTime;
                if (m_turnExecTimer <= 0) {
                    const Faction* pf = m_campaignMap->GetPlayerFaction();
                    int nextId = pf ? m_campaignMap->StartNextScheduledArmy(pf->id) : -1;
                    if (nextId >= 0) {
                        m_followArmyId = nextId;
                        const Army* a = m_campaignMap->GetArmy(nextId);
                        if (a && cam) { cam->SetTarget(a->worldPosition + glm::vec3(0, 0, 1)); cam->SetDistance(12); }
                    }
                    else {
                        // Player done → start first AI faction
                        m_execFactionIdx = 0;
                        m_turnPhase = TurnExecPhase::AI_FACTION;
                        m_turnExecTimer = 0.3f;
                        m_followArmyId = -1;
                        m_currentAIFaction.clear();
                    }
                }
            }
        }
        // ── AI FACTION: process one faction at a time, sequentially ──
        else if (m_turnPhase == TurnExecPhase::AI_FACTION) {
            m_campaignMap->Update(deltaTime, *m_input);

            if (m_followArmyId >= 0) {
                // Watching an AI army move
                const Army* fa = m_campaignMap->GetArmy(m_followArmyId);
                if (fa && fa->isMoving) {
                    if (cam) cam->SetTarget(fa->worldPosition + glm::vec3(0, 0, 1));
                }
                else {
                    m_followArmyId = -1;
                    m_turnExecTimer = 0.2f;
                }
            }
            else {
                m_turnExecTimer -= deltaTime;
                if (m_turnExecTimer <= 0) {

                    // If we haven't issued orders for the current faction yet, do it now
                    if (m_currentAIFaction.empty() && m_execFactionIdx < (int)m_aiFactionOrder.size()) {
                        m_currentAIFaction = m_aiFactionOrder[m_execFactionIdx];
                        const Faction* f = m_campaignMap->GetFaction(m_currentAIFaction);
                        if (f) Logger::Info("--- %s's turn ---", f->name.c_str());
                        m_campaignMap->RunAIForFaction(m_currentAIFaction);
                        m_turnExecTimer = 0.2f;
                    }
                    // Try to start/follow the next army for this faction
                    else if (!m_currentAIFaction.empty()) {
                        // Check if any army of this faction is still moving
                        bool anyMoving = false;
                        for (const auto& a : m_campaignMap->GetArmies()) {
                            if (a.factionId == m_currentAIFaction && a.isMoving) {
                                m_followArmyId = a.id;
                                if (cam) { cam->SetTarget(a.worldPosition + glm::vec3(0, 0, 1)); cam->SetDistance(12); }
                                anyMoving = true; break;
                            }
                        }

                        if (!anyMoving) {
                            // Try starting next scheduled army for this faction
                            int nextId = m_campaignMap->StartNextScheduledArmy(m_currentAIFaction);
                            if (nextId >= 0) {
                                m_followArmyId = nextId;
                                const Army* a = m_campaignMap->GetArmy(nextId);
                                if (a && cam) { cam->SetTarget(a->worldPosition + glm::vec3(0, 0, 1)); cam->SetDistance(12); }
                            }
                            else {
                                // This faction is done → advance to next faction
                                Logger::Info("--- %s's turn complete ---", m_currentAIFaction.c_str());
                                m_execFactionIdx++;
                                m_currentAIFaction.clear();
                                m_turnExecTimer = 0.4f;

                                // Check if ALL factions are done
                                if (m_execFactionIdx >= (int)m_aiFactionOrder.size()) {
                                    m_campaignMap->ProcessTurn();
                                    m_turnPhase = TurnExecPhase::IDLE;
                                    m_cameraLocked = false;
                                    if (cam && m_savedCamDist > 0) cam->SetDistance(m_savedCamDist);
                                    Logger::Info("=== New turn — fatigue restored ===");
                                }
                            }
                        }
                    }
                }
            }
        }

        if (m_campaignMap->HasPendingBattle()) {
            auto battleData = m_campaignMap->GetPendingBattle();
            m_battleScene->Setup(battleData, *m_campaignMap);
            if (cam) cam->SetTarget(m_battleScene->GetBattleWorldPos() + glm::vec3(0, 0, 1));
            m_state = GameState::BATTLE;
            m_cameraLocked = false;
        }
        break;
    }
        case GameState::BATTLE:
            m_battleScene->Update(deltaTime, *m_input);
            if (m_input->IsMouseButtonPressed(0)) {
                glm::vec2 mp = m_input->GetMousePos();
                m_battleScene->HandleClick(mp.x, mp.y, (float)m_windowWidth, (float)m_windowHeight);
            }
            if (m_battleScene->IsFinished()) {
                m_campaignMap->ApplyBattleResult(m_battleScene->GetResult());
                m_state = GameState::CAMPAIGN_MAP;
            }
            break;

        case GameState::PAUSED: break;
        case GameState::MAIN_MENU: break;
        default: break;
    }
}

// ─── Render ───────────────────────────────────────────────────
void Game::Render()
{
    m_renderer->BeginFrame();

    switch (m_state) {
        case GameState::CAMPAIGN_MAP:
            m_renderer->RenderCampaignMap(*m_campaignMap);
            m_ui->Render(*m_renderer);
            break;

        case GameState::BATTLE:
            m_renderer->RenderCampaignMap(*m_campaignMap); // map visible behind
            m_renderer->RenderBattle(*m_battleScene);      // battle UI overlay
            break;

        case GameState::PAUSED:
            m_renderer->RenderCampaignMap(*m_campaignMap);
            m_ui->RenderPauseOverlay(*m_renderer);
            break;

        case GameState::MAIN_MENU:
            // TODO: render menu
            break;

        default:
            break;
    }

    m_renderer->EndFrame();
}

// ─── Shutdown ─────────────────────────────────────────────────
void Game::Shutdown()
{
    Logger::Info("Shutting down...");

    // Subsystems are cleaned up by unique_ptr destructors
    m_ui.reset();
    m_battleScene.reset();
    m_turnManager.reset();
    m_campaignMap.reset();
    m_renderer.reset();
    m_input.reset();

    if (m_window) {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }
    glfwTerminate();
}

// ─── State Management ─────────────────────────────────────────
void Game::SetState(GameState newState)
{
    Logger::Info("Game state: %d -> %d", (int)m_state, (int)newState);
    m_state = newState;
}
