#pragma once

class UIRenderer;
struct Faction;

// ─── Top Bar ──────────────────────────────────────────────────
// Persistent bar at the top of the screen showing:
//   - Faction name and flag
//   - Treasury (gold) and income per turn
//   - Current turn / season / year
//   - Minimap toggle
class TopBar {
public:
    void Update(const Faction* playerFaction, int turn,
                const char* season, const char* year);
    void Render(UIRenderer& renderer);

private:
    const Faction* m_faction = nullptr;
    int m_turn = 0;
    const char* m_season = "Spring";
    const char* m_year = "1700";
};
