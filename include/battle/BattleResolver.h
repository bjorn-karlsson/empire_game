#pragma once

#include "campaign/Army.h"
#include "campaign/CampaignMap.h"
#include <random>

// ─── Battle Resolver ──────────────────────────────────────────
// Phase 3 auto-resolve: calculates battle outcomes without
// real-time combat. Uses a simple but satisfying model:
//
// Each round:
//   1. Both sides inflict casualties based on attack vs defense
//   2. Morale damage is applied based on casualties
//   3. Units with low morale route
//   4. Battle ends when one side is eliminated or fully routed
//
// This gives you meaningful army composition decisions even
// before the real-time battle system exists.

class BattleResolver {
public:
    BattleResolver();

    BattleResult Resolve(Army& attacker, Army& defender);

private:
    struct RoundResult {
        int attackerDamage = 0;
        int defenderDamage = 0;
        int attackerMoraleLoss = 0;
        int defenderMoraleLoss = 0;
    };

    RoundResult SimulateRound(Army& attacker, Army& defender);
    void ApplyDamage(Army& army, int totalDamage);
    void ApplyMorale(Army& army, int moraleLoss);
    bool IsArmyDefeated(const Army& army) const;
    int  CalculateArmyCombatPower(const Army& army) const;

    std::mt19937 m_rng;
    static constexpr int MAX_ROUNDS = 20;
};
