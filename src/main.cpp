// ============================================================================
// Empire Total Strategy - Main Entry Point
// A Total War-inspired strategy game set in 18th century Europe
// ============================================================================

#include "core/Game.h"
#include "utils/Logger.h"
#include <GLFW/glfw3.h>

int main(int argc, char* argv[])
{
    Logger::Init();
    Logger::Info("=== Empire Total Strategy v0.2 ===");

    // Get monitor resolution for near-fullscreen
    if (!glfwInit()) { Logger::Error("GLFW init failed"); return -1; }
    const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
    int winW = mode->width - 20;    // leave small border
    int winH = mode->height - 80;   // leave space for taskbar
    glfwTerminate(); // Game::Init will re-init

    Game game;
    if (!game.Init(winW, winH, "Empire Total Strategy")) {
        Logger::Error("Failed to initialize game!");
        return -1;
    }

    game.Run();
    game.Shutdown();

    Logger::Info("Game shut down cleanly.");
    return 0;
}
