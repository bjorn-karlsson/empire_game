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
    glViewport(0, 0, width, height);
    // TODO: notify renderer/camera of resize
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

        // Cap delta to avoid spiral of death on lag spikes
        if (deltaTime > 0.1f) deltaTime = 0.1f;

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

        float dt = 0.016f; // approximate, fine for input

        // WASD / Arrow keys → pan camera
        float panX = 0.0f, panZ = 0.0f;
        if (m_input->IsKeyDown(GLFW_KEY_W) || m_input->IsKeyDown(GLFW_KEY_UP))    panZ -= dt;
        if (m_input->IsKeyDown(GLFW_KEY_S) || m_input->IsKeyDown(GLFW_KEY_DOWN))  panZ += dt;
        if (m_input->IsKeyDown(GLFW_KEY_A) || m_input->IsKeyDown(GLFW_KEY_LEFT))  panX -= dt;
        if (m_input->IsKeyDown(GLFW_KEY_D) || m_input->IsKeyDown(GLFW_KEY_RIGHT)) panX += dt;

        if (panX != 0.0f || panZ != 0.0f)
            cam->Pan(panX, panZ);

        // Right-click drag → pan camera (only if NOT dragging to move army)
        if (m_input->IsMouseButtonDown(2)) { // middle mouse
            glm::vec2 delta = m_input->GetMouseDelta();
            cam->Pan(-delta.x * dt * 0.3f, -delta.y * dt * 0.3f);
        }

        // Scroll wheel → zoom
        float scroll = m_input->GetScrollDelta();
        if (scroll != 0.0f)
            cam->Zoom(scroll);

        // Left click → SELECT objects (armies, cities)
        if (m_input->IsMouseButtonPressed(0)) {
            glm::vec2 mousePos = m_input->GetMousePos();
            glm::vec3 worldPos = cam->ScreenToWorldPlane(
                mousePos.x, mousePos.y,
                (float)m_windowWidth, (float)m_windowHeight
            );
            m_campaignMap->HandleLeftClick(worldPos);
        }

        // Right click → ISSUE MOVE ORDER to selected army
        if (m_input->IsMouseButtonPressed(1)) {
            glm::vec2 mousePos = m_input->GetMousePos();
            glm::vec3 worldPos = cam->ScreenToWorldPlane(
                mousePos.x, mousePos.y,
                (float)m_windowWidth, (float)m_windowHeight
            );
            m_campaignMap->HandleRightClick(worldPos);
        }
    }
}

// ─── Update ───────────────────────────────────────────────────
void Game::Update(float deltaTime)
{
    // Update camera smoothing
    if (m_renderer->GetCamera())
        m_renderer->GetCamera()->Update(deltaTime);

    switch (m_state) {
        case GameState::CAMPAIGN_MAP:
            m_campaignMap->Update(deltaTime, *m_input);
            m_ui->Update(deltaTime, *m_input);

            // End turn handling
            if (m_ui->IsEndTurnButtonClicked()) {
                m_turnManager->EndTurn();
                m_ui->ClearEndTurnClick();
            }

            // Check if a battle should trigger
            if (m_campaignMap->HasPendingBattle()) {
                auto battleData = m_campaignMap->GetPendingBattle();
                m_battleScene->Setup(battleData);
                m_state = GameState::BATTLE;
            }
            break;

        case GameState::BATTLE:
            m_battleScene->Update(deltaTime, *m_input);
            if (m_battleScene->IsFinished()) {
                auto result = m_battleScene->GetResult();
                m_campaignMap->ApplyBattleResult(result);
                m_state = GameState::CAMPAIGN_MAP;
            }
            break;

        case GameState::PAUSED:
            // Only UI updates in pause
            break;

        case GameState::MAIN_MENU:
            // TODO: main menu logic
            break;

        default:
            break;
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
            m_renderer->RenderBattle(*m_battleScene);
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
