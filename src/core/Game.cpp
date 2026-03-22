// ═══════════════════════════════════════════════════════════════
// Game.cpp
// ═══════════════════════════════════════════════════════════════
#include <glad/glad.h>
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

bool Game::Init(int width, int height, const std::string& title) {
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
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) { Logger::Error("GLAD failed"); return false; }
    m_windowWidth = width; m_windowHeight = height;
    glfwGetFramebufferSize(m_window, &m_windowWidth, &m_windowHeight);
    g_gameInstance = this;
    m_input = std::make_unique<InputManager>(m_window);
    m_renderer = std::make_unique<Renderer>(width, height);
    m_campaignMap = std::make_unique<CampaignMap>();
    m_turnManager = std::make_unique<TurnManager>();
    m_battleScene = std::make_unique<BattleScene>();
    m_ui = std::make_unique<UIManager>();
    if (!m_renderer->Init()) { Logger::Error("Renderer init failed"); return false; }
    if (!MapSerializer::LoadFromFile(*m_campaignMap, "maps/europe_1700.json")) {
        Logger::Warning("No map file — using generated test map");
        m_campaignMap->GenerateTestMap();
        MapSerializer::ExportCurrentMap(*m_campaignMap, "maps/europe_1700.json");
    }
    m_campaignMap->GetHeightMap().UploadToGPU(); // ensure GPU texture exists
    m_turnManager->Init(m_campaignMap.get());
    m_ui->Init(width, height);
    m_renderer->BuildMapGeometry(*m_campaignMap);
    m_state = GameState::CAMPAIGN_MAP;
    m_running = true;
    Logger::Info("Game initialized!");
    return true;
}

void Game::Run() {
    while (m_running && !glfwWindowShouldClose(m_window)) {
        float currentTime = static_cast<float>(glfwGetTime());
        float deltaTime = currentTime - m_lastFrameTime;
        m_lastFrameTime = currentTime;
        if (deltaTime > 0.1f) deltaTime = 0.1f;
        int fw, fh;
        glfwGetFramebufferSize(m_window, &fw, &fh);
        if (fw > 0 && fh > 0 && (fw != m_windowWidth || fh != m_windowHeight)) {
            m_windowWidth = fw; m_windowHeight = fh; m_renderer->OnResize(fw, fh);
        }
        ProcessInput();
        Update(deltaTime);
        Render();
        glfwSwapBuffers(m_window);
        glfwPollEvents();
    }
}

void Game::ProcessInput() {
    m_input->Update();
    bool ctrlHeld = m_input->IsKeyDown(GLFW_KEY_LEFT_CONTROL) || m_input->IsKeyDown(GLFW_KEY_RIGHT_CONTROL);

    if (m_input->IsKeyPressed(GLFW_KEY_ESCAPE)) {
        if (m_campaignMap->IsExchangeOpen()) m_campaignMap->CancelExchange();
        else if (m_editor.isActive) { m_editor.Toggle(); Logger::Info("Editor: OFF"); }
        else if (m_state == GameState::PAUSED) m_state = GameState::CAMPAIGN_MAP;
        else if (m_state == GameState::CAMPAIGN_MAP) m_state = GameState::PAUSED;
    }
    if (m_input->IsKeyDown(GLFW_KEY_LEFT_ALT) && m_input->IsKeyPressed(GLFW_KEY_F4)) m_running = false;

    if (m_input->IsKeyPressed(GLFW_KEY_E) && !ctrlHeld && m_state == GameState::CAMPAIGN_MAP
        && m_turnPhase == TurnExecPhase::IDLE && !m_campaignMap->IsExchangeOpen()) {
        m_editor.Toggle();
        Logger::Info("Editor: %s", m_editor.isActive ? "ON" : "OFF");
    }

    // Editor keys
    if (m_editor.isActive && m_state == GameState::CAMPAIGN_MAP) {
        if (m_input->IsKeyPressed(GLFW_KEY_1)) m_editor.HandleKeyPress(GLFW_KEY_1, false, *m_campaignMap, *m_renderer);
        if (m_input->IsKeyPressed(GLFW_KEY_2)) m_editor.HandleKeyPress(GLFW_KEY_2, false, *m_campaignMap, *m_renderer);
        if (m_input->IsKeyPressed(GLFW_KEY_3)) m_editor.HandleKeyPress(GLFW_KEY_3, false, *m_campaignMap, *m_renderer);
        if (m_input->IsKeyPressed(GLFW_KEY_4)) m_editor.HandleKeyPress(GLFW_KEY_4, false, *m_campaignMap, *m_renderer);
        if (m_input->IsKeyPressed(GLFW_KEY_5)) m_editor.HandleKeyPress(GLFW_KEY_5, false, *m_campaignMap, *m_renderer);
        if (m_input->IsKeyPressed(GLFW_KEY_DELETE)) m_editor.HandleKeyPress(GLFW_KEY_DELETE, false, *m_campaignMap, *m_renderer);
        if (!ctrlHeld && m_input->IsKeyPressed(GLFW_KEY_R)) m_editor.HandleKeyPress(GLFW_KEY_R, false, *m_campaignMap, *m_renderer);
        if (!ctrlHeld && m_input->IsKeyPressed(GLFW_KEY_Q)) m_editor.HandleKeyPress(GLFW_KEY_Q, false, *m_campaignMap, *m_renderer);
        if (!ctrlHeld && m_input->IsKeyPressed(GLFW_KEY_F)) m_editor.HandleKeyPress(GLFW_KEY_F, false, *m_campaignMap, *m_renderer);
        if (!ctrlHeld && m_input->IsKeyPressed(GLFW_KEY_G)) m_editor.HandleKeyPress(GLFW_KEY_G, false, *m_campaignMap, *m_renderer);
        if (ctrlHeld && m_input->IsKeyPressed(GLFW_KEY_Z)) m_editor.HandleKeyPress(GLFW_KEY_Z, true, *m_campaignMap, *m_renderer);
        if (ctrlHeld && m_input->IsKeyPressed(GLFW_KEY_Y)) m_editor.HandleKeyPress(GLFW_KEY_Y, true, *m_campaignMap, *m_renderer);
        if (ctrlHeld && m_input->IsKeyPressed(GLFW_KEY_S)) m_editor.HandleKeyPress(GLFW_KEY_S, true, *m_campaignMap, *m_renderer);
        if (ctrlHeld && m_input->IsKeyPressed(GLFW_KEY_L)) m_editor.HandleKeyPress(GLFW_KEY_L, true, *m_campaignMap, *m_renderer);
        if (ctrlHeld && m_input->IsKeyPressed(GLFW_KEY_N)) m_editor.HandleKeyPress(GLFW_KEY_N, true, *m_campaignMap, *m_renderer);
        if (ctrlHeld && m_input->IsKeyPressed(GLFW_KEY_DELETE)) m_editor.HandleKeyPress(GLFW_KEY_DELETE, true, *m_campaignMap, *m_renderer);
    }

    if (m_state == GameState::CAMPAIGN_MAP) {
        Camera* cam = m_renderer->GetCamera();
        if (!cam) return;
        bool executing = (m_turnPhase != TurnExecPhase::IDLE);
        float dt = 0.016f;

        // Camera pan — WASD works unless Ctrl held
        if (!executing) {
            float panX = 0, panZ = 0;
            if (m_input->IsKeyDown(GLFW_KEY_UP)) panZ -= dt;
            if (m_input->IsKeyDown(GLFW_KEY_DOWN)) panZ += dt;
            if (m_input->IsKeyDown(GLFW_KEY_LEFT)) panX -= dt;
            if (m_input->IsKeyDown(GLFW_KEY_RIGHT)) panX += dt;
            if (!ctrlHeld) {
                if (m_input->IsKeyDown(GLFW_KEY_W)) panZ -= dt;
                if (m_input->IsKeyDown(GLFW_KEY_S)) panZ += dt;
                if (m_input->IsKeyDown(GLFW_KEY_A)) panX -= dt;
                if (m_input->IsKeyDown(GLFW_KEY_D)) panX += dt;
            }
            if (panX != 0 || panZ != 0) cam->Pan(panX, panZ);
        }

        if (m_input->IsMouseButtonDown(2)) {
            glm::vec2 delta = m_input->GetMouseDelta();
            cam->Pan(-delta.x * dt * 0.3f, -delta.y * dt * 0.3f);
        }

        float scroll = m_input->GetScrollDelta();

        // ══════════════════════════════════════════════════════
        // EDITOR MODE
        // ══════════════════════════════════════════════════════
        if (m_editor.isActive && !executing) {
            m_editor.SetScreenInfo(cam->GetViewProjectionMatrix(), (float)m_windowWidth, (float)m_windowHeight);
            glm::vec2 mousePos = m_input->GetMousePos();
            glm::vec3 worldPos = cam->ScreenToWorldPlane(mousePos.x, mousePos.y, (float)m_windowWidth, (float)m_windowHeight);

            // Height brush: scroll changes radius, not zoom
            if (m_editor.currentTool == MapEditor::Tool::HEIGHT_BRUSH && scroll != 0) {
                m_editor.HandleScrollInEditor(scroll);
                scroll = 0;
            }

            // Apply remaining scroll as zoom
            if (scroll != 0) cam->Zoom(scroll);

            m_editor.HandleMouseMove(mousePos, *m_campaignMap);

            // ── HEIGHT BRUSH painting ──
            if (m_editor.currentTool == MapEditor::Tool::HEIGHT_BRUSH) {
                if (m_input->IsMouseButtonPressed(0) || m_input->IsMouseButtonPressed(1)) {
                    if (!m_editor.isPainting) {
                        m_editor.SaveUndoState(*m_campaignMap);
                        m_editor.isPainting = true;
                    }
                }
                if (m_input->IsMouseButtonDown(0)) {
                    m_editor.PaintHeight(worldPos, *m_campaignMap, true);
                    m_campaignMap->GetHeightMap().UploadToGPU();
                }
                if (m_input->IsMouseButtonDown(1) && !m_editor.isDragging) {
                    m_editor.PaintHeight(worldPos, *m_campaignMap, false);
                    m_campaignMap->GetHeightMap().UploadToGPU();
                }
                if ((m_input->IsMouseButtonReleased(0) || m_input->IsMouseButtonReleased(1)) && m_editor.isPainting) {
                    m_editor.isPainting = false;
                    m_editor.RebuildGeometry(*m_campaignMap, *m_renderer);
                }
                return;
            }

            // ── Non-brush tools ──
            if (m_input->IsMouseButtonPressed(0)) {
                m_editor.HandleLeftClick(worldPos, mousePos, *m_campaignMap);
                if (m_editor.geometryDirty) m_editor.RebuildGeometry(*m_campaignMap, *m_renderer);
            }
            if (m_input->IsMouseButtonPressed(1)) {
                m_editor.HandleRightPress(worldPos, mousePos, *m_campaignMap);
            }
            if (m_input->IsMouseButtonDown(1) && (m_editor.isDragging || m_editor.isDraggingCity)) {
                m_editor.HandleRightDrag(worldPos, *m_campaignMap);
            }
            if (m_input->IsMouseButtonReleased(1)) {
                if (m_editor.isDragging || m_editor.isDraggingCity) {
                    m_editor.HandleRightRelease(*m_campaignMap);
                    m_editor.RebuildGeometry(*m_campaignMap, *m_renderer);
                }
            }
            return;
        }

        // Apply zoom for non-editor mode
        if (scroll != 0) cam->Zoom(scroll);

        // ══════════════════════════════════════════════════════
        // NORMAL GAME
        // ══════════════════════════════════════════════════════
        if (m_input->IsMouseButtonPressed(0) && !executing) {
            glm::vec2 mousePos = m_input->GetMousePos();
            if (m_campaignMap->IsExchangeOpen()) {
                m_campaignMap->HandleExchangeClick(mousePos.x, mousePos.y, (float)m_windowWidth, (float)m_windowHeight);
            }
            else {
                glm::vec3 worldPos = cam->ScreenToWorldPlane(mousePos.x, mousePos.y, (float)m_windowWidth, (float)m_windowHeight);
                m_campaignMap->HandleLeftClick(worldPos);
            }
        }
        if (m_input->IsMouseButtonPressed(1) && !m_campaignMap->IsExchangeOpen() && !executing) {
            glm::vec2 mousePos = m_input->GetMousePos();
            glm::vec3 worldPos = cam->ScreenToWorldPlane(mousePos.x, mousePos.y, (float)m_windowWidth, (float)m_windowHeight);
            m_campaignMap->HandleRightClick(worldPos);
        }
    }
}

void Game::Update(float deltaTime) {
    Camera* cam = m_renderer->GetCamera();
    if (cam) { cam->Update(deltaTime); if (cam->GetDistance() < 3.0f) cam->SetDistance(3.0f); }

    static float safetyTimer = 0;
    if (m_turnPhase != TurnExecPhase::IDLE) {
        safetyTimer += deltaTime;
        if (safetyTimer > 8.0f) {
            Logger::Warning("Turn execution timeout!");
            m_campaignMap->StopAllArmies(); m_campaignMap->ProcessTurn();
            m_turnPhase = TurnExecPhase::IDLE; m_cameraLocked = false;
            if (cam && m_savedCamDist > 0) cam->SetDistance(m_savedCamDist);
            safetyTimer = 0;
        }
    }
    else safetyTimer = 0;

    switch (m_state) {
    case GameState::CAMPAIGN_MAP: {
        if (m_turnPhase == TurnExecPhase::IDLE) {
            m_campaignMap->Update(deltaTime, *m_input);
            m_ui->Update(deltaTime, *m_input);
            if (!m_editor.isActive && (m_ui->IsEndTurnButtonClicked() || m_input->IsKeyPressed(GLFW_KEY_SPACE) || m_input->IsKeyPressed(GLFW_KEY_ENTER))) {
                m_ui->ClearEndTurnClick(); m_campaignMap->StopAllArmies();
                m_turnPhase = TurnExecPhase::PLAYER_MOVES; m_followArmyId = -1; m_turnExecTimer = 0.1f;
                if (cam) m_savedCamDist = cam->GetDistance(); m_cameraLocked = true;
                m_aiFactionOrder = m_campaignMap->GetAIFactionIds(); m_execFactionIdx = 0; m_currentAIFaction.clear();
            }
        }
        else if (m_turnPhase == TurnExecPhase::PLAYER_MOVES) {
            m_campaignMap->Update(deltaTime, *m_input);
            if (m_followArmyId >= 0) {
                const Army* fa = m_campaignMap->GetArmy(m_followArmyId);
                if (fa && fa->isMoving) { if (cam) cam->SetTarget(fa->worldPosition + glm::vec3(0, 0, 1)); }
                else { m_followArmyId = -1; m_turnExecTimer = 0.3f; }
            }
            else {
                m_turnExecTimer -= deltaTime;
                if (m_turnExecTimer <= 0) {
                    const Faction* pf = m_campaignMap->GetPlayerFaction();
                    int nextId = pf ? m_campaignMap->StartNextScheduledArmy(pf->id) : -1;
                    if (nextId >= 0) {
                        m_followArmyId = nextId; const Army* a = m_campaignMap->GetArmy(nextId);
                        if (a && cam) { cam->SetTarget(a->worldPosition + glm::vec3(0, 0, 1)); cam->SetDistance(12); }
                    }
                    else { m_execFactionIdx = 0; m_turnPhase = TurnExecPhase::AI_FACTION; m_turnExecTimer = 0.3f; m_followArmyId = -1; m_currentAIFaction.clear(); }
                }
            }
        }
        else if (m_turnPhase == TurnExecPhase::AI_FACTION) {
            m_campaignMap->Update(deltaTime, *m_input);
            if (m_followArmyId >= 0) {
                const Army* fa = m_campaignMap->GetArmy(m_followArmyId);
                if (fa && fa->isMoving) { if (cam) cam->SetTarget(fa->worldPosition + glm::vec3(0, 0, 1)); }
                else { m_followArmyId = -1; m_turnExecTimer = 0.2f; }
            }
            else {
                m_turnExecTimer -= deltaTime;
                if (m_turnExecTimer <= 0) {
                    if (m_currentAIFaction.empty() && m_execFactionIdx < (int)m_aiFactionOrder.size()) {
                        m_currentAIFaction = m_aiFactionOrder[m_execFactionIdx];
                        const Faction* f = m_campaignMap->GetFaction(m_currentAIFaction);
                        if (f) Logger::Info("--- %s's turn ---", f->name.c_str());
                        m_campaignMap->RunAIForFaction(m_currentAIFaction); m_turnExecTimer = 0.2f;
                    }
                    else if (!m_currentAIFaction.empty()) {
                        bool anyMoving = false;
                        for (const auto& a : m_campaignMap->GetArmies()) {
                            if (a.factionId == m_currentAIFaction && a.isMoving) {
                                m_followArmyId = a.id; if (cam) { cam->SetTarget(a.worldPosition + glm::vec3(0, 0, 1)); cam->SetDistance(12); } anyMoving = true; break;
                            }
                        }
                        if (!anyMoving) {
                            int nextId = m_campaignMap->StartNextScheduledArmy(m_currentAIFaction);
                            if (nextId >= 0) {
                                m_followArmyId = nextId; const Army* a = m_campaignMap->GetArmy(nextId);
                                if (a && cam) { cam->SetTarget(a->worldPosition + glm::vec3(0, 0, 1)); cam->SetDistance(12); }
                            }
                            else {
                                m_execFactionIdx++; m_currentAIFaction.clear(); m_turnExecTimer = 0.4f;
                                if (m_execFactionIdx >= (int)m_aiFactionOrder.size()) {
                                    m_campaignMap->ProcessTurn(); m_turnPhase = TurnExecPhase::IDLE; m_cameraLocked = false;
                                    if (cam && m_savedCamDist > 0) cam->SetDistance(m_savedCamDist);
                                }
                            }
                        }
                    }
                }
            }
        }
        if (m_campaignMap->HasPendingBattle()) {
            auto bd = m_campaignMap->GetPendingBattle(); m_battleScene->Setup(bd, *m_campaignMap);
            if (cam) cam->SetTarget(m_battleScene->GetBattleWorldPos() + glm::vec3(0, 0, 1));
            m_state = GameState::BATTLE; m_cameraLocked = false;
        }
        break;
    }
    case GameState::BATTLE:
        m_battleScene->Update(deltaTime, *m_input);
        if (m_input->IsMouseButtonPressed(0)) { glm::vec2 mp = m_input->GetMousePos(); m_battleScene->HandleClick(mp.x, mp.y, (float)m_windowWidth, (float)m_windowHeight); }
        if (m_battleScene->IsFinished()) {
            m_campaignMap->ApplyBattleResult(m_battleScene->GetResult()); m_state = GameState::CAMPAIGN_MAP;
            m_followArmyId = -1;
            for (const auto& a : m_campaignMap->GetArmies()) {
                if (a.isMoving) {
                    m_followArmyId = a.id; Camera* cam = m_renderer->GetCamera();
                    if (cam) { cam->SetTarget(a.worldPosition + glm::vec3(0, 0, 1)); cam->SetDistance(12); } m_turnExecTimer = 0; break;
                }
            }
        }
        break;
    case GameState::PAUSED: break;
    case GameState::MAIN_MENU: break;
    default: break;
    }
}

void Game::Render() {
    m_renderer->BeginFrame();
    switch (m_state) {
    case GameState::CAMPAIGN_MAP:
        m_renderer->RenderCampaignMap(*m_campaignMap);
        if (m_editor.isActive) {
            m_renderer->RenderEditorOverlay(*m_campaignMap,
                m_editor.selectedProvinceIdx, m_editor.selectedVertexIdx,
                m_editor.selectedObstacleIdx, m_editor.selectedObsVertexIdx,
                m_editor.hoverProvinceIdx, m_editor.hoverVertexIdx,
                m_editor.hoverObstacleIdx, m_editor.hoverObsVertexIdx);
            m_renderer->RenderEditorHUD(m_editor.GetToolName(), m_editor.GetSelectionInfo(*m_campaignMap),
                m_editor.geometryDirty, (int)m_campaignMap->GetProvinces().size(), (int)m_campaignMap->GetObstacles().size());
        }
        m_ui->Render(*m_renderer);
        break;
    case GameState::BATTLE:
        m_renderer->RenderCampaignMap(*m_campaignMap); m_renderer->RenderBattle(*m_battleScene); break;
    case GameState::PAUSED:
        m_renderer->RenderCampaignMap(*m_campaignMap); m_ui->RenderPauseOverlay(*m_renderer); break;
    default: break;
    }
    m_renderer->EndFrame();
}

void Game::Shutdown() {
    m_ui.reset(); m_battleScene.reset(); m_turnManager.reset(); m_campaignMap.reset(); m_renderer.reset(); m_input.reset();
    if (m_window) { glfwDestroyWindow(m_window); m_window = nullptr; } glfwTerminate();
}

void Game::SetState(GameState s) { m_state = s; }