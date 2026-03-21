// ═══════════════════════════════════════════════════════════════
// Game.cpp — Main game loop, input, state machine, editor
// ═══════════════════════════════════════════════════════════════
#include "core/Game.h"
#include "core/InputManager.h"
#include "rendering/Renderer.h"
#include "rendering/Camera.h"
#include "campaign/CampaignMap.h"
#include "campaign/MapSerializer.h"
#include "battle/BattleScene.h"
#include "campaign/TurnManager.h"
#include "ui/UIManager.h"
#include "utils/Logger.h"
#include <GLFW/glfw3.h>

static Game* g_gameInstance = nullptr;

Game::Game() = default;
Game::~Game() { Shutdown(); }

// ─── Initialization ───────────────────────────────────────────
bool Game::Init(int width, int height, const std::string& title)
{
    Logger::Info("Initializing game: %s (%dx%d)", title.c_str(), width, height);

    if (!glfwInit()) { Logger::Error("GLFW init failed"); return false; }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);

    m_window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (!m_window) { Logger::Error("Window creation failed"); glfwTerminate(); return false; }

    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        Logger::Error("Failed to initialize GLAD"); return false;
    }

    m_windowWidth = width; m_windowHeight = height;
    glfwGetFramebufferSize(m_window, &m_windowWidth, &m_windowHeight);

    // --- Create subsystems ---
    g_gameInstance = this;

    m_input = std::make_unique<InputManager>(m_window);
    m_renderer = std::make_unique<Renderer>(width, height);
    m_campaignMap = std::make_unique<CampaignMap>();
    m_turnManager = std::make_unique<TurnManager>();
    m_battleScene = std::make_unique<BattleScene>();
    m_ui = std::make_unique<UIManager>();

    // --- Initialize subsystems ---
    if (!m_renderer->Init()) {
        Logger::Error("Failed to initialize renderer");
        return false;
    }

    // Load map from JSON, fall back to hardcoded test map
    if (!MapSerializer::LoadFromFile(*m_campaignMap, "maps/europe_1700.json")) {
        Logger::Warning("No map file found — using generated test map");
        m_campaignMap->GenerateTestMap();
        // Export the generated map so we have a JSON to edit
        MapSerializer::ExportCurrentMap(*m_campaignMap, "maps/europe_1700.json");
    }

    m_turnManager->Init(m_campaignMap.get());
    m_ui->Init(width, height);

    // Build GPU geometry for the campaign map
    m_renderer->BuildMapGeometry(*m_campaignMap);

    // Start on campaign map
    m_state = GameState::CAMPAIGN_MAP;
    m_running = true;

    Logger::Info("Game initialized successfully!");
    return true;
}

// ─── Main Game Loop ───────────────────────────────────────────
void Game::Run()
{
    Logger::Info("Entering main game loop...");

    while (m_running && !glfwWindowShouldClose(m_window)) {
        float currentTime = static_cast<float>(glfwGetTime());
        float deltaTime = currentTime - m_lastFrameTime;
        m_lastFrameTime = currentTime;
        if (deltaTime > 0.1f) deltaTime = 0.1f;

        // Sync window dimensions each frame
        int fw, fh;
        glfwGetFramebufferSize(m_window, &fw, &fh);
        if (fw > 0 && fh > 0 && (fw != m_windowWidth || fh != m_windowHeight)) {
            m_windowWidth = fw; m_windowHeight = fh;
            m_renderer->OnResize(fw, fh);
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
        if (m_campaignMap->IsExchangeOpen()) {
            m_campaignMap->CancelExchange();
        }
        else if (m_editor.isActive) {
            m_editor.Toggle();
            Logger::Info("Editor: OFF");
        }
        else if (m_state == GameState::PAUSED) {
            m_state = GameState::CAMPAIGN_MAP;
        }
        else if (m_state == GameState::CAMPAIGN_MAP) {
            m_state = GameState::PAUSED;
        }
    }

    // Debug: quick exit
    if (m_input->IsKeyDown(GLFW_KEY_LEFT_ALT) && m_input->IsKeyPressed(GLFW_KEY_F4)) {
        m_running = false;
    }

    // Toggle editor with E key
    if (m_input->IsKeyPressed(GLFW_KEY_E) && m_state == GameState::CAMPAIGN_MAP
        && m_turnPhase == TurnExecPhase::IDLE && !m_campaignMap->IsExchangeOpen()) {
        m_editor.Toggle();
        Logger::Info("Editor: %s", m_editor.isActive ? "ON" : "OFF");
    }

    // Editor key handling (1/2/3 tools, S save, L load, R rebuild)
    if (m_editor.isActive && m_state == GameState::CAMPAIGN_MAP) {
        if (m_input->IsKeyPressed(GLFW_KEY_1)) m_editor.HandleKeyPress(GLFW_KEY_1, *m_campaignMap, *m_renderer);
        if (m_input->IsKeyPressed(GLFW_KEY_2)) m_editor.HandleKeyPress(GLFW_KEY_2, *m_campaignMap, *m_renderer);
        if (m_input->IsKeyPressed(GLFW_KEY_3)) m_editor.HandleKeyPress(GLFW_KEY_3, *m_campaignMap, *m_renderer);
        if (m_input->IsKeyPressed(GLFW_KEY_S)) m_editor.HandleKeyPress(GLFW_KEY_S, *m_campaignMap, *m_renderer);
        if (m_input->IsKeyPressed(GLFW_KEY_L)) m_editor.HandleKeyPress(GLFW_KEY_L, *m_campaignMap, *m_renderer);
        if (m_input->IsKeyPressed(GLFW_KEY_R)) m_editor.HandleKeyPress(GLFW_KEY_R, *m_campaignMap, *m_renderer);
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
            // Don't pan with S key when editor is active (S = save)
            if (m_editor.isActive) {
                panZ = 0; panX = 0;
                if (m_input->IsKeyDown(GLFW_KEY_W) || m_input->IsKeyDown(GLFW_KEY_UP))    panZ -= dt;
                if (m_input->IsKeyDown(GLFW_KEY_DOWN))  panZ += dt;
                if (m_input->IsKeyDown(GLFW_KEY_LEFT))  panX -= dt;
                if (m_input->IsKeyDown(GLFW_KEY_RIGHT)) panX += dt;
            }
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

        // ── EDITOR MODE: intercept mouse input ──
        if (m_editor.isActive && !executing) {
            // Left click
            if (m_input->IsMouseButtonPressed(0)) {
                glm::vec2 mousePos = m_input->GetMousePos();
                glm::vec3 worldPos = cam->ScreenToWorldPlane(
                    mousePos.x, mousePos.y,
                    (float)m_windowWidth, (float)m_windowHeight);
                m_editor.HandleLeftClick(worldPos, *m_campaignMap);
            }

            // Left release — finish drag, rebuild geometry
            if (m_input->IsMouseButtonReleased(0) && m_editor.isDragging) {
                m_editor.HandleLeftRelease(*m_campaignMap);
                m_editor.RebuildGeometry(*m_campaignMap, *m_renderer);
            }

            // Drag while held
            if (m_input->IsMouseButtonDown(0) && m_editor.isDragging) {
                glm::vec2 mousePos = m_input->GetMousePos();
                glm::vec3 worldPos = cam->ScreenToWorldPlane(
                    mousePos.x, mousePos.y,
                    (float)m_windowWidth, (float)m_windowHeight);
                m_editor.HandleDrag(worldPos, *m_campaignMap);
            }

            // Right click (delete vertex in delete mode)
            if (m_input->IsMouseButtonPressed(1)) {
                glm::vec2 mousePos = m_input->GetMousePos();
                glm::vec3 worldPos = cam->ScreenToWorldPlane(
                    mousePos.x, mousePos.y,
                    (float)m_windowWidth, (float)m_windowHeight);
                m_editor.HandleRightClick(worldPos, *m_campaignMap);
                m_editor.RebuildGeometry(*m_campaignMap, *m_renderer);
            }

            return; // Don't process normal game clicks while in editor
        }

        // ── NORMAL GAME MODE ──

        // Left click → SELECT objects (or exchange modal interaction)
        if (m_input->IsMouseButtonPressed(0) && !executing) {
            glm::vec2 mousePos = m_input->GetMousePos();
            if (m_campaignMap->IsExchangeOpen()) {
                m_campaignMap->HandleExchangeClick(
                    mousePos.x, mousePos.y,
                    (float)m_windowWidth, (float)m_windowHeight);
            }
            else {
                glm::vec3 worldPos = cam->ScreenToWorldPlane(
                    mousePos.x, mousePos.y,
                    (float)m_windowWidth, (float)m_windowHeight);
                m_campaignMap->HandleLeftClick(worldPos);
            }
        }

        // Right click → ISSUE MOVE ORDER / MERGE (not during exchange or execution)
        if (m_input->IsMouseButtonPressed(1) && !m_campaignMap->IsExchangeOpen() && !executing) {
            glm::vec2 mousePos = m_input->GetMousePos();
            glm::vec3 worldPos = cam->ScreenToWorldPlane(
                mousePos.x, mousePos.y,
                (float)m_windowWidth, (float)m_windowHeight);
            m_campaignMap->HandleRightClick(worldPos);
        }
    }
}

// ─── Update ───────────────────────────────────────────────────
void Game::Update(float deltaTime)
{
    Camera* cam = m_renderer->GetCamera();
    if (cam) {
        cam->Update(deltaTime);
        // Prevent camera from going underground
        float minDist = 3.0f;
        if (cam->GetDistance() < minDist) cam->SetDistance(minDist);
    }

    // Safety timer to prevent freeze during turn execution
    static float safetyTimer = 0;
    if (m_turnPhase != TurnExecPhase::IDLE) {
        safetyTimer += deltaTime;
        if (safetyTimer > 8.0f) {
            Logger::Warning("Turn execution timeout!");
            m_campaignMap->StopAllArmies();
            m_campaignMap->ProcessTurn();
            m_turnPhase = TurnExecPhase::IDLE;
            m_cameraLocked = false;
            if (cam && m_savedCamDist > 0) cam->SetDistance(m_savedCamDist);
            safetyTimer = 0;
        }
    }
    else { safetyTimer = 0; }

    switch (m_state) {
    case GameState::CAMPAIGN_MAP:
    {
        // ── IDLE: normal gameplay ──
        if (m_turnPhase == TurnExecPhase::IDLE) {
            m_campaignMap->Update(deltaTime, *m_input);
            m_ui->Update(deltaTime, *m_input);

            // End Turn trigger (don't trigger during editor mode)
            if (!m_editor.isActive &&
                (m_ui->IsEndTurnButtonClicked() || m_input->IsKeyPressed(GLFW_KEY_SPACE) ||
                    m_input->IsKeyPressed(GLFW_KEY_ENTER))) {
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

        // Check for pending battles (any phase)
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

            // Follow the retreating army (if any) before resuming AI
            m_followArmyId = -1;
            for (const auto& a : m_campaignMap->GetArmies()) {
                if (a.isMoving) {
                    m_followArmyId = a.id;
                    if (cam) { cam->SetTarget(a.worldPosition + glm::vec3(0, 0, 1)); cam->SetDistance(12); }
                    m_turnExecTimer = 0;
                    break;
                }
            }
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

        // Editor overlay: vertex dots + status text
        if (m_editor.isActive) {
            m_renderer->RenderEditorOverlay(*m_campaignMap,
                m_editor.selectedProvinceIdx, m_editor.selectedVertexIdx);
            m_renderer->DrawScreenText(m_editor.GetStatusText(),
                10, 40, 1.0f, { 1.0f, 1.0f, 0.5f, 1.0f });
        }

        m_ui->Render(*m_renderer);
        break;

    case GameState::BATTLE:
        m_renderer->RenderCampaignMap(*m_campaignMap);
        m_renderer->RenderBattle(*m_battleScene);
        break;

    case GameState::PAUSED:
        m_renderer->RenderCampaignMap(*m_campaignMap);
        m_ui->RenderPauseOverlay(*m_renderer);
        break;

    case GameState::MAIN_MENU:
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