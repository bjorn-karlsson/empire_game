#include "ui/UIManager.h"
#include "core/InputManager.h"
#include "utils/Logger.h"

// Forward-declared in header; include only when we actually use it
class Renderer;

UIManager::UIManager()  = default;
UIManager::~UIManager() = default;

void UIManager::Init(int screenWidth, int screenHeight)
{
    m_screenWidth  = screenWidth;
    m_screenHeight = screenHeight;
    Logger::Info("UI initialized (%dx%d)", screenWidth, screenHeight);
}

void UIManager::Update(float deltaTime, const InputManager& input)
{
    m_endTurnClicked = false;

    // End Turn: press Enter or Space
    if (input.IsKeyPressed(GLFW_KEY_ENTER) || input.IsKeyPressed(GLFW_KEY_SPACE)) {
        m_endTurnClicked = true;
        Logger::Info("End Turn clicked!");
    }

    // TODO: Check mouse clicks on UI buttons
    // TODO: Show/hide province panel based on selection
    // TODO: Show/hide army panel based on selection
}

void UIManager::Render(Renderer& renderer)
{
    // TODO: Render UI elements
    // Phase 1: Simple colored rectangles for:
    //   - Top bar: faction name, treasury, turn/date display
    //   - End Turn button (bottom right)
    //   - Province info panel (bottom left, when province selected)
    //   - Army panel (right side, when army selected)
    //   - Minimap (top right corner)
    //
    // For text rendering, you'll need a font renderer.
    // Options:
    //   1. stb_truetype (lightweight, good for learning)
    //   2. FreeType + custom atlas
    //   3. ImGui overlay for quick prototyping
}

void UIManager::RenderPauseOverlay(Renderer& renderer)
{
    // TODO: Semi-transparent dark overlay with "PAUSED" text
    // and resume/quit buttons
}
