#pragma once
#include "campaign/CampaignMap.h"
#include "battle/BattleResolver.h"
#include <string>
#include <vector>

class InputManager;

struct ArmySnapshot {
    std::string generalName;
    std::string factionId;
    glm::vec3 factionColor={0.5f,0.5f,0.5f};
    int totalManpower=0;
    int unitCount=0;
    struct UnitSnap{UnitType type;int manpowerBefore;int manpowerAfter;bool destroyed;};
    std::vector<UnitSnap> units;
};

enum class BattlePhase { PRE_BATTLE, POST_BATTLE, DONE };

class BattleScene {
public:
    BattleScene();~BattleScene();

    void Setup(const BattleSetupData& data,const CampaignMap& map);
    void Update(float dt,const InputManager& input);
    void HandleClick(float sx,float sy,float sw,float sh);

    bool IsFinished()const{return m_phase==BattlePhase::DONE;}
    bool IsRetreated()const{return m_retreated;}
    BattleResult GetResult()const{return m_result;}
    BattlePhase GetPhase()const{return m_phase;}

    const ArmySnapshot&GetAttackerSnap()const{return m_attackerSnap;}
    const ArmySnapshot&GetDefenderSnap()const{return m_defenderSnap;}
    bool AttackerWon()const{return m_result.attackerWon;}
    glm::vec3 GetBattleWorldPos()const{return m_battlePos;}

    // Predicted outcome (0=attacker certain loss, 0.5=even, 1=attacker certain win)
    float GetPredictedOutcome()const{return m_predictedOutcome;}

private:
    void DoAutoResolve();
    void DoRetreat();

    BattleResolver m_resolver;
    BattleResult m_result;
    BattleSetupData m_battleData;
    BattlePhase m_phase=BattlePhase::DONE;

    ArmySnapshot m_attackerSnap,m_defenderSnap;
    glm::vec3 m_battlePos={0,0,0};
    float m_predictedOutcome=0.5f;
    bool m_retreated=false;
    float m_timer=0;
};
