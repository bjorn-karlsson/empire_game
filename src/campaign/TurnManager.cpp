#include "campaign/TurnManager.h"
#include "campaign/CampaignMap.h"
#include "utils/Logger.h"

void TurnManager::Init(CampaignMap* map)
{
    m_map = map;
}

void TurnManager::EndTurn()
{
    if (!m_map) return;
    m_processing = true;

    Logger::Info("--- End Turn ---");

    // Phase 1: AI factions act
    ProcessAITurns();

    // Phase 2: Economy update
    ProcessEconomy();

    // Phase 3: Random events
    ProcessEvents();

    // Phase 4: Advance turn counter
    m_map->ProcessTurn();

    m_processing = false;
}

void TurnManager::ProcessAITurns()
{
    // TODO: AI decision making
    // For each non-player faction:
    //   - Evaluate threats (enemy armies near borders)
    //   - Move armies toward objectives
    //   - Build infrastructure in provinces
    //   - Declare war / seek alliances based on opinion scores
    Logger::Info("  AI turns processing... (not yet implemented)");
}

void TurnManager::ProcessEconomy()
{
    // Province economy updates are handled in CampaignMap::ProcessTurn()
    Logger::Info("  Economy updated");
}

void TurnManager::ProcessEvents()
{
    // TODO: Random events
    // - Rebellions in low public order provinces
    // - Plague/famine reducing population
    // - Trade windfall for coastal provinces
    // - Diplomatic incidents
    Logger::Info("  Events processed (none yet)");
}
