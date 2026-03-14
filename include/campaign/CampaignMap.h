#pragma once
#include "campaign/Province.h"
#include "campaign/Faction.h"
#include "campaign/Army.h"
#include <vector>
#include <unordered_map>
#include <string>
#include <optional>

class InputManager;

struct BattleSetupData { Army*attacker=nullptr;Army*defender=nullptr;int provinceId=-1; };
struct BattleResult { bool attackerWon=false;int attackerCasualties=0,defenderCasualties=0,provinceId=-1; };

struct TerrainObstacle {
    std::string name, type;
    std::vector<glm::vec3> vertices;
    glm::vec3 center, color;
};

// ─── Navigation Grid ──────────────────────────────────────────
// 2D grid for A* pathfinding around obstacles.
// Each cell is passable or not. A* finds shortest path on this grid.
struct NavGrid {
    static constexpr float CELL = 0.3f;        // cell size in world units
    static constexpr int W = 80, H = 80;       // grid dimensions
    static constexpr float OX = -10.0f;        // world origin X
    static constexpr float OZ = -7.0f;         // world origin Z
    bool passable[W][H] = {};

    int toGX(float x) const { return glm::clamp((int)((x-OX)/CELL),0,W-1); }
    int toGZ(float z) const { return glm::clamp((int)((z-OZ)/CELL),0,H-1); }
    float toWX(int gx) const { return OX+gx*CELL+CELL*0.5f; }
    float toWZ(int gz) const { return OZ+gz*CELL+CELL*0.5f; }
    glm::vec3 toWorld(int gx,int gz) const { return {toWX(gx),0,toWZ(gz)}; }
    bool inBounds(int gx,int gz) const { return gx>=0&&gx<W&&gz>=0&&gz<H; }
};

// ─── Reachable Cell (for movement mesh) ───────────────────────
struct ReachableCell {
    int gx, gz;
    float dist; // walked distance from army
};

class CampaignMap {
public:
    CampaignMap(); ~CampaignMap();
    bool LoadFromFile(const std::string&);
    void GenerateTestMap();
    void Update(float dt, const InputManager&);
    void ProcessTurn();

    Province* GetProvince(int id);
    const Province* GetProvince(int id) const;
    Province* GetProvinceAtWorldPos(const glm::vec3&);
    const std::vector<Province>& GetProvinces() const { return m_provinces; }

    Faction* GetFaction(const std::string&);
    const Faction* GetFaction(const std::string&) const;
    Faction* GetPlayerFaction();
    const Faction* GetPlayerFaction() const;
    const std::vector<Faction>& GetFactions() const { return m_factions; }

    Army* GetArmy(int id);
    const Army* GetArmy(int id) const;
    const std::vector<Army>& GetArmies() const { return m_armies; }

    // Terrain
    const std::vector<TerrainObstacle>& GetObstacles() const { return m_obstacles; }
    bool IsPointPassable(const glm::vec3&) const;
    bool IsPointOnLand(const glm::vec3&) const;
    const NavGrid& GetNavGrid() const { return m_navGrid; }

    // A* pathfinding on nav grid (returns world-space waypoints)
    std::vector<glm::vec3> FindPathWorld(const glm::vec3& from, const glm::vec3& to) const;

    // Movement mesh: flood-fill reachable cells from army position
    std::vector<ReachableCell> GetReachableCells(int armyId) const;

    // Battle
    bool HasPendingBattle() const { return m_pendingBattle.has_value(); }
    BattleSetupData GetPendingBattle();
    void ApplyBattleResult(const BattleResult&);

    // AI
    void RunAI();
    void DetectBattles();
    void CaptureProvince(int provinceId, const std::string& newOwner);
    void DestroyArmy(int armyId);

    // Selection
    int GetSelectedProvinceId() const { return m_selectedProvinceId; }
    int GetSelectedArmyId() const { return m_selectedArmyId; }
    glm::vec3 GetSelectionWorldPos() const { return m_selectionWorldPos; }

    // HUD info
    std::string GetNotification() const { return m_notification; }
    float GetNotificationTimer() const { return m_notifTimer; }

    // Input
    void HandleLeftClick(const glm::vec3&);
    void HandleRightClick(const glm::vec3&);
    void HandleClick(const glm::vec3& w) { HandleLeftClick(w); }

    int GetCurrentTurn() const { return m_currentTurn; }
    std::string GetCurrentSeason() const;
    std::string GetCurrentYear() const;

    void MoveArmy(int,int);
    void RecruitUnit(int,UnitType);
    std::vector<Army*> GetArmiesInProvince(int);

private:
    void BuildNavGrid();
    void UpdateArmyPositions(float dt);
    void UpdateArmyProvince(Army&);
    void CheckForBattles();
    void SetNotification(const std::string& msg);

    std::vector<Province> m_provinces;
    std::vector<Faction> m_factions;
    std::vector<Army> m_armies;
    std::vector<TerrainObstacle> m_obstacles;
    NavGrid m_navGrid;

    int m_nextArmyId=1, m_nextUnitId=1, m_currentTurn=1;
    int m_selectedProvinceId=-1, m_selectedArmyId=-1;
    glm::vec3 m_selectionWorldPos={0,0,0};
    std::optional<BattleSetupData> m_pendingBattle;

    // Notifications
    std::string m_notification;
    float m_notifTimer = 0;
};
