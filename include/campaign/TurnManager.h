#pragma once

class CampaignMap;

// ─── Turn Manager ─────────────────────────────────────────────
// Orchestrates the end-turn sequence:
// 1. Player ends turn
// 2. AI factions take their turns (move armies, build, diplomacy)
// 3. Economy/growth updates
// 4. Events fire (rebellions, trade deals, etc.)
// 5. New turn begins
class TurnManager {
public:
    void Init(CampaignMap* map);
    void EndTurn();   // Called when player clicks "End Turn"
    bool IsProcessing() const { return m_processing; }

private:
    void ProcessAITurns();
    void ProcessEconomy();
    void ProcessEvents();

    CampaignMap* m_map = nullptr;
    bool m_processing = false;
};
