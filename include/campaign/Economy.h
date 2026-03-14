#pragma once

#include <vector>

struct Province;
struct Faction;
struct Army;

// ─── Economy System ───────────────────────────────────────────
// Calculates per-turn income and expenses for each faction.
// Separated from Faction/Province so the logic is testable
// and modifiable without touching data structures.
namespace Economy {

    // Calculate total income for a faction from its provinces
    int CalculateFactionIncome(const Faction& faction,
                               const std::vector<Province>& provinces);

    // Calculate total expenses (army upkeep + building maintenance)
    int CalculateFactionExpenses(const Faction& faction,
                                 const std::vector<Army>& armies);

    // Calculate trade income between two allied/trading factions
    int CalculateTradeIncome(const Faction& factionA,
                             const Faction& factionB,
                             const std::vector<Province>& provinces);

    // Check if a faction can afford a purchase
    bool CanAfford(const Faction& faction, int cost);
}
