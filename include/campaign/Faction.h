#pragma once

#include <string>
#include <vector>
#include <glm/glm.hpp>

// ─── Diplomacy Relations ──────────────────────────────────────
enum class DiplomaticStatus {
    NEUTRAL,
    ALLIANCE,
    TRADE_AGREEMENT,
    WAR,
    CEASEFIRE
};

struct DiplomaticRelation {
    std::string otherFactionId;
    DiplomaticStatus status = DiplomaticStatus::NEUTRAL;
    int opinion = 0; // -100 to +100, affects AI decisions
};

// ─── Faction ──────────────────────────────────────────────────
// Represents a nation/empire on the campaign map.
// In 18th century Europe: Great Britain, France, Prussia,
// Austria, Russia, Spain, Ottoman Empire, etc.
struct Faction {
    std::string id;         // "great_britain", "france", etc.
    std::string name;       // "Great Britain"
    std::string leaderName; // "King George II"

    glm::vec3   color = {0.5f, 0.5f, 0.5f}; // map color for provinces
    bool        isPlayerControlled = false;
    bool        isEliminated = false;

    // Economy
    int treasury     = 5000;    // current gold
    int incomePerTurn = 0;      // computed from provinces
    int expensesPerTurn = 0;    // army upkeep, building costs

    // Owned territory (province IDs)
    std::vector<int> ownedProvinces;
    int capitalProvinceId = -1;

    // Armies (army IDs)
    std::vector<int> armyIds;

    // Diplomacy
    std::vector<DiplomaticRelation> relations;

    // Research / tech (simplified)
    int researchPoints = 0;
    int techLevel = 1;          // 1-5, unlocks better units

    // ─── Methods ──────────────────────────────────────────────
    DiplomaticStatus GetRelationWith(const std::string& factionId) const {
        for (const auto& rel : relations) {
            if (rel.otherFactionId == factionId)
                return rel.status;
        }
        return DiplomaticStatus::NEUTRAL;
    }

    bool IsAtWarWith(const std::string& factionId) const {
        return GetRelationWith(factionId) == DiplomaticStatus::WAR;
    }

    void UpdateEconomy(int income, int expenses) {
        incomePerTurn   = income;
        expensesPerTurn = expenses;
        treasury += (income - expenses);
    }
};
