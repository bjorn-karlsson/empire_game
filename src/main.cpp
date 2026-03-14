// ============================================================================
// Empire Total Strategy - Main Entry Point
// A Total War-inspired strategy game set in 18th century Europe
// ============================================================================

#include "core/Game.h"
#include "utils/Logger.h"

int main(int argc, char* argv[])
{
    Logger::Init();
    Logger::Info("=== Empire Total Strategy v0.1 ===");

    Game game;

    if (!game.Init(1280, 720, "Empire Total Strategy")) {
        Logger::Error("Failed to initialize game!");
        return -1;
    }

    game.Run();
    game.Shutdown();

    Logger::Info("Game shut down cleanly.");
    return 0;
}
