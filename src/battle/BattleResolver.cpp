#include "battle/BattleResolver.h"
#include "utils/Logger.h"
#include <algorithm>
#include <numeric>

BattleResolver::BattleResolver()
    : m_rng(std::random_device{}())
{
}

BattleResult BattleResolver::Resolve(Army& attacker, Army& defender)
{
    Logger::Info("Battle: %s (%d men) vs %s (%d men)",
        attacker.generalName.c_str(), attacker.GetTotalManpower(),
        defender.generalName.c_str(), defender.GetTotalManpower());

    BattleResult result;
    result.provinceId = attacker.currentProvinceId;

    int attackerStartManpower = attacker.GetTotalManpower();
    int defenderStartManpower = defender.GetTotalManpower();

    // Simulate rounds until one side is defeated
    for (int round = 0; round < MAX_ROUNDS; round++) {
        auto roundResult = SimulateRound(attacker, defender);

        // Apply results
        ApplyDamage(defender, roundResult.attackerDamage);
        ApplyDamage(attacker, roundResult.defenderDamage);
        ApplyMorale(attacker, roundResult.attackerMoraleLoss);
        ApplyMorale(defender, roundResult.defenderMoraleLoss);

        // Clean up destroyed units
        attacker.RemoveDestroyedUnits();
        defender.RemoveDestroyedUnits();

        Logger::Info("  Round %d: Attacker %d men, Defender %d men",
            round + 1, attacker.GetTotalManpower(), defender.GetTotalManpower());

        if (IsArmyDefeated(attacker) || IsArmyDefeated(defender))
            break;
    }

    // Determine winner
    result.attackerWon = !IsArmyDefeated(attacker) &&
                         (IsArmyDefeated(defender) ||
                          attacker.GetTotalManpower() > defender.GetTotalManpower());

    result.attackerCasualties = attackerStartManpower - attacker.GetTotalManpower();
    result.defenderCasualties = defenderStartManpower - defender.GetTotalManpower();

    Logger::Info("Battle result: %s wins! (Attacker lost %d, Defender lost %d)",
        result.attackerWon ? "Attacker" : "Defender",
        result.attackerCasualties, result.defenderCasualties);

    return result;
}

BattleResolver::RoundResult BattleResolver::SimulateRound(Army& attacker, Army& defender)
{
    RoundResult result;

    // Calculate attack power for each side
    int atkPower = CalculateArmyCombatPower(attacker);
    int defPower = CalculateArmyCombatPower(defender);

    // Add randomness (±20%)
    std::uniform_real_distribution<float> variance(0.8f, 1.2f);
    float atkRoll = variance(m_rng);
    float defRoll = variance(m_rng);

    // Damage = attacker's power vs defender's average defense (and vice versa)
    float avgDefenderDefense = 0;
    float avgAttackerDefense = 0;
    for (const auto& u : defender.units) avgDefenderDefense += u.stats.defense;
    for (const auto& u : attacker.units) avgAttackerDefense += u.stats.defense;
    if (!defender.units.empty()) avgDefenderDefense /= defender.units.size();
    if (!attacker.units.empty()) avgAttackerDefense /= attacker.units.size();

    result.attackerDamage = static_cast<int>(
        (atkPower * atkRoll - avgDefenderDefense * 0.5f) * 0.3f);
    result.defenderDamage = static_cast<int>(
        (defPower * defRoll - avgAttackerDefense * 0.5f) * 0.3f);

    result.attackerDamage = std::max(result.attackerDamage, 1);
    result.defenderDamage = std::max(result.defenderDamage, 1);

    // Morale damage: proportional to casualties taken relative to army size
    float atkCasualtyRatio = (float)result.defenderDamage / std::max(attacker.GetTotalManpower(), 1);
    float defCasualtyRatio = (float)result.attackerDamage / std::max(defender.GetTotalManpower(), 1);

    result.attackerMoraleLoss = static_cast<int>(atkCasualtyRatio * 30);
    result.defenderMoraleLoss = static_cast<int>(defCasualtyRatio * 30);

    return result;
}

int BattleResolver::CalculateArmyCombatPower(const Army& army) const
{
    int power = 0;
    for (const auto& unit : army.units) {
        if (unit.isRouted) continue;
        // Power = attack * manpower, scaled by morale
        float moraleMultiplier = unit.stats.morale / 100.0f;
        power += static_cast<int>(unit.stats.attack * (unit.stats.manpower / 10.0f)
                                  * moraleMultiplier);
    }
    return power;
}

void BattleResolver::ApplyDamage(Army& army, int totalDamage)
{
    if (army.units.empty() || totalDamage <= 0) return;

    // Distribute damage across units (front-line units take more)
    int damagePerUnit = totalDamage / static_cast<int>(army.units.size());
    int remainder = totalDamage % static_cast<int>(army.units.size());

    for (size_t i = 0; i < army.units.size(); i++) {
        int dmg = damagePerUnit + (i == 0 ? remainder : 0);
        army.units[i].stats.manpower = std::max(0, army.units[i].stats.manpower - dmg);
    }
}

void BattleResolver::ApplyMorale(Army& army, int moraleLoss)
{
    for (auto& unit : army.units) {
        unit.stats.morale = std::max(0, unit.stats.morale - moraleLoss);
        if (unit.stats.morale < 15 && !unit.isRouted) {
            unit.isRouted = true;
            Logger::Info("    %s has routed!", unit.name.c_str());
        }
    }
}

bool BattleResolver::IsArmyDefeated(const Army& army) const
{
    if (army.units.empty()) return true;

    // Defeated if all units routed or destroyed
    for (const auto& u : army.units) {
        if (u.stats.manpower > 0 && !u.isRouted)
            return false;
    }
    return true;
}
