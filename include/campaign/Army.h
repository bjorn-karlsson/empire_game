#pragma once

#include <string>
#include <vector>
#include <glm/glm.hpp>

// ─── Unit Types (18th Century) ────────────────────────────────
// These map roughly to Empire Total War's unit roster
enum class UnitType {
    // Infantry
    LINE_INFANTRY,      // Standard musket troops
    LIGHT_INFANTRY,     // Skirmishers, faster, less firepower
    GRENADIERS,         // Elite heavy infantry
    MILITIA,            // Cheap, low morale

    // Cavalry
    DRAGOONS,           // Versatile mounted troops
    HUSSARS,            // Light cavalry, fast flankers
    CUIRASSIERS,        // Heavy cavalry, devastating charge

    // Artillery
    CANNON_6PDR,        // Light field guns
    CANNON_12PDR,       // Medium field guns
    HOWITZER,           // Siege weapons, good vs buildings

    // Special
    GENERAL,            // Army commander, morale aura
};

// ─── Unit ─────────────────────────────────────────────────────
// A single regiment/battery within an army
struct UnitStats {
    int     manpower    = 100;   // current soldiers
    int     maxManpower = 100;
    int     attack      = 10;
    int     defense     = 10;
    int     morale      = 60;    // 0-100, breaks and routes when low
    int     speed       = 5;     // movement points
    int     range       = 0;     // 0 = melee only, >0 = ranged
    int     upkeep      = 50;    // gold per turn
    float   experience  = 0.0f;  // 0-1, improves stats
};

struct UnitTemplate {
    UnitType    type;
    std::string name;       // "42nd Highland Regiment"
    UnitStats   baseStats;
    int         recruitCost = 500;
    int         recruitTurns = 1;
};

struct Unit {
    int         id = -1;
    UnitType    type;
    std::string name;
    UnitStats   stats;
    bool        isRouted = false;
};

// ─── Army ─────────────────────────────────────────────────────
// A stack of units that moves as one on the campaign map.
// Like Total War, armies have a general and a unit cap.
struct Army {
    int          id = -1;
    std::string  factionId;
    std::string  generalName;

    std::vector<Unit> units;        // Up to ~20 units per army
    static constexpr int MAX_UNITS = 20;

    // Position
    int   currentProvinceId = -1;
    glm::vec3 worldPosition = {0, 0, 0};
    bool  isMoving          = false;
    bool  isGarrisoned      = false;  // inside a city

    // Movement per turn
    float movementRange     = 5.0f;   // remaining this turn
    float movementRangeMax  = 5.0f;   // max per turn
    float moveSpeed         = 3.5f;   // visual speed (units/sec)

    // Intent: what to do when arriving at destination
    enum class Intent { NONE, MOVE, ATTACK, MERGE, ENTER_CITY };
    Intent intent = Intent::NONE;
    int targetArmyId = -1;    // for ATTACK/MERGE — tracks a moving army
    int targetCityProvId = -1; // for ENTER_CITY

    // Full path (A* result, world-space waypoints around obstacles)
    std::vector<glm::vec3> fullPath;
    int currentPathIndex = 0;

    // Multi-turn scheduling
    std::vector<float> turnBreaks;
    float totalPathLength = 0.0f;
    float distanceTraveled = 0.0f;

    void ClearPath() {
        fullPath.clear(); turnBreaks.clear(); currentPathIndex=0;
        totalPathLength=0; distanceTraveled=0; isMoving=false;
        intent=Intent::NONE; targetArmyId=-1; targetCityProvId=-1;
    }

    // ─── Methods ──────────────────────────────────────────────
    int GetTotalManpower() const {
        int total = 0;
        for (const auto& u : units) total += u.stats.manpower;
        return total;
    }

    int GetTotalUpkeep() const {
        int total = 0;
        for (const auto& u : units) total += u.stats.upkeep;
        return total;
    }

    float GetAverageMorale() const {
        if (units.empty()) return 0;
        float sum = 0;
        for (const auto& u : units) sum += u.stats.morale;
        return sum / units.size();
    }

    bool CanAddUnit() const {
        return static_cast<int>(units.size()) < MAX_UNITS;
    }

    bool HasMovementLeft() const {
        return movementRange > 0.1f;
    }

    void ResetMovement() {
        movementRange = movementRangeMax;
    }

    void RemoveDestroyedUnits() {
        units.erase(
            std::remove_if(units.begin(), units.end(),
                [](const Unit& u) { return u.stats.manpower <= 0; }),
            units.end()
        );
    }
};
