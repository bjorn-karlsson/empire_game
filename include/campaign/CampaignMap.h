#pragma once
#include "campaign/Province.h"
#include "campaign/Faction.h"
#include "campaign/Army.h"
#include "campaign/HeightMap.h"
#include <vector>
#include <unordered_map>
#include <string>
#include <optional>

class InputManager;

struct BattleSetupData { Army* attacker = nullptr; Army* defender = nullptr; int provinceId = -1; };
struct BattleResult {
    bool attackerWon = false;
    int attackerCasualties = 0, defenderCasualties = 0, provinceId = -1;
    int attackerId = -1, defenderId = -1;
    bool isRetreat = false;
};
struct TerrainObstacle {
    std::string name, type;
    std::vector<glm::vec3> vertices;
    glm::vec3 center, color;
};

// Foreign countries surrounding France (non-playable, just rendered)
struct ForeignTerritory {
    std::string name;
    std::vector<glm::vec3> vertices;
    glm::vec3 center, color;
};

// ─── Navigation Grid ──────────────────────────────────────────
// 2D grid for A* pathfinding around obstacles.
// Each cell is passable or not. A* finds shortest path on this grid.
struct NavGrid {
    static constexpr float CELL = 0.3f;        // cell size in world units
    static constexpr int W = 120, H = 120;
    static constexpr float OX = -12.0f;
    static constexpr float OZ = -12.0f;
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


    // Mutable access for editor
    std::vector<Province>& GetProvincesEditable() { return m_provinces; }
    std::vector<TerrainObstacle>& GetObstaclesEditable() { return m_obstacles; }
    std::vector<ForeignTerritory>& GetForeignTerritoriesEditable() { return m_foreignTerritories; }

    // For serialization
    void ClearAll();
    void AddFaction(const Faction& f) { m_factions.push_back(f); }
    void AddProvince(const Province& p) { m_provinces.push_back(p); }
    void AddObstacle(const TerrainObstacle& ob) { m_obstacles.push_back(ob); }
    void AddForeignTerritory(const ForeignTerritory& ft) { m_foreignTerritories.push_back(ft); }
    void AddArmy(Army a);
    int GetNextArmyId() { return m_nextArmyId++; }
    int GetNextUnitId() { return m_nextUnitId++; }
    void FinalizeLoad(); // called after loading: sets up ownership, builds nav grid

    // Terrain height at world position (matches shader displacement)
    float GetTerrainHeight(float x, float z) const;
    float GetBaseTerrainHeight(float x, float z) const;
    bool IsInsideMountain(float x, float z) const;
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
    const std::vector<ForeignTerritory>& GetForeignTerritories() const { return m_foreignTerritories; }
    bool IsPointPassable(const glm::vec3&) const;
    bool IsPointOnLand(const glm::vec3&) const;
    const NavGrid& GetNavGrid() const { return m_navGrid; }

    // A* pathfinding on nav grid (returns world-space waypoints)
    std::vector<glm::vec3> FindPathWorld(const glm::vec3& from, glm::vec3 to,
        int movingArmyId = -1, int targetArmyId = -1,
        int targetCityProvId = -1) const;

    // Movement mesh: flood-fill reachable cells from army position
    std::vector<ReachableCell> GetReachableCells(int armyId) const;

    // Battle
    bool HasPendingBattle() const { return m_pendingBattle.has_value(); }
    BattleSetupData GetPendingBattle();
    void ApplyBattleResult(const BattleResult&);

    // AI
    void RunAI();
    void RunAIForFaction(const std::string& factionId);
    std::vector<std::string> GetAIFactionIds() const;
    void DetectBattles(); // only called intentionally now
    void StartBattle(int attackerArmyId, int defenderArmyId);
    void CaptureProvince(int provinceId, const std::string& newOwner);
    void DestroyArmy(int armyId);
    void CheckCityOccupation(Army& army);

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

    // Unit Exchange Modal
    bool IsExchangeOpen() const { return m_exchangeOpen; }
    int GetExchangeArmyA() const { return m_exchangeArmyA; }
    int GetExchangeArmyB() const { return m_exchangeArmyB; }
    const std::vector<bool>& GetExchangeSelA() const { return m_exchangeSelA; }
    const std::vector<bool>& GetExchangeSelB() const { return m_exchangeSelB; }
    void HandleExchangeClick(float screenX, float screenY, float screenW, float screenH);
    void SwapSelectedUnits(); // live swap inside modal
    void ConfirmExchange();
    void CancelExchange();

    int GetCurrentTurn() const { return m_currentTurn; }
    std::string GetCurrentSeason() const;
    std::string GetCurrentYear() const;

    // Turn execution helpers
    int StartNextScheduledArmy(const std::string& factionId);
    bool IsAnyArmyMoving(const std::string& factionId) const;
    void StopAllArmies();

    void MoveArmy(int,int);
    void RecruitUnit(int,UnitType);
    std::vector<Army*> GetArmiesInProvince(int);

    void BuildNavGrid();


    HeightMap& GetHeightMap() { return m_heightMap; }
    const HeightMap& GetHeightMap() const { return m_heightMap; }


private:

    HeightMap m_heightMap;

    void UpdateArmyPositions(float dt);
    void UpdateArmyProvince(Army&);
    void CheckForBattles(); // legacy, no longer auto-called
    void HandleArmyArrival(Army& army);
    void TryGarrison(Army& army, Province* p);  // ← ADD
    void SchedulePathTo(Army& army, glm::vec3 dest, Army::Intent intent,
                        int targetArmy=-1, int targetCity=-1);
    void SetNotification(const std::string& msg);

    std::vector<Province> m_provinces;
    std::vector<Faction> m_factions;
    std::vector<Army> m_armies;
    std::vector<TerrainObstacle> m_obstacles;
    std::vector<ForeignTerritory> m_foreignTerritories;
    NavGrid m_navGrid;

    int m_nextArmyId=1, m_nextUnitId=1, m_currentTurn=1;
    int m_selectedProvinceId=-1, m_selectedArmyId=-1;
    glm::vec3 m_selectionWorldPos={0,0,0};
    std::optional<BattleSetupData> m_pendingBattle;

    // Notifications
    std::string m_notification;
    float m_notifTimer = 0;

    // Unit Exchange Modal
    bool m_exchangeOpen = false;
    int m_exchangeArmyA = -1, m_exchangeArmyB = -1;
    std::vector<bool> m_exchangeSelA, m_exchangeSelB;
    // Backup copies for cancel/revert
    std::vector<Unit> m_backupUnitsA, m_backupUnitsB;
};
