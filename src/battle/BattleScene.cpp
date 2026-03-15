#include "battle/BattleScene.h"
#include "core/InputManager.h"
#include "utils/Logger.h"

BattleScene::BattleScene()=default;
BattleScene::~BattleScene()=default;

static ArmySnapshot SnapArmy(const Army*a,const CampaignMap&map){
    ArmySnapshot s;if(!a)return s;
    s.generalName=a->generalName;s.factionId=a->factionId;
    const Faction*f=map.GetFaction(a->factionId);
    if(f)s.factionColor=f->color;
    s.totalManpower=a->GetTotalManpower();s.unitCount=(int)a->units.size();
    for(const auto&u:a->units)s.units.push_back({u.type,u.stats.manpower,u.stats.manpower,false});
    return s;
}

void BattleScene::Setup(const BattleSetupData&data,const CampaignMap&map){
    m_battleData=data;m_phase=BattlePhase::PRE_BATTLE;m_retreated=false;m_timer=0;
    m_attackerSnap=SnapArmy(data.attacker,map);
    m_defenderSnap=SnapArmy(data.defender,map);

    // Battle position (midpoint between armies)
    if(data.attacker&&data.defender)
        m_battlePos=(data.attacker->worldPosition+data.defender->worldPosition)*0.5f;

    // Predict outcome based on combat power ratio
    float atkPower=0,defPower=0;
    if(data.attacker)for(const auto&u:data.attacker->units)
        atkPower+=u.stats.attack*(u.stats.manpower/10.0f)*(u.stats.morale/100.0f);
    if(data.defender)for(const auto&u:data.defender->units)
        defPower+=u.stats.attack*(u.stats.manpower/10.0f)*(u.stats.morale/100.0f);
    float total=atkPower+defPower;
    m_predictedOutcome=(total>0)?atkPower/total:0.5f;

    Logger::Info("Pre-battle: %s (%d) vs %s (%d) — prediction: %.0f%%",
        m_attackerSnap.generalName.c_str(),m_attackerSnap.totalManpower,
        m_defenderSnap.generalName.c_str(),m_defenderSnap.totalManpower,
        m_predictedOutcome*100);
}

void BattleScene::Update(float dt,const InputManager&input){
    m_timer+=dt;
    // Clicks are handled by HandleClick from Game.cpp
}

void BattleScene::HandleClick(float sx,float sy,float sw,float sh){
    if(m_phase==BattlePhase::PRE_BATTLE){
        // Button layout (must match renderer)
        float panW=sw*0.5f,panH=120;
        float px=(sw-panW)/2,py=sh-panH-40;
        float btnW=panW/3-20,btnH=35;
        float btnY=py+panH-50;

        // Button 1: Fight (disabled — skip for now)
        // float btn1X=px+15;

        // Button 2: Auto-resolve (center)
        float btn2X=px+panW/3+5;
        if(sx>=btn2X&&sx<=btn2X+btnW&&sy>=btnY&&sy<=btnY+btnH){
            DoAutoResolve();return;
        }

        // Button 3: Retreat (right)
        float btn3X=px+2*panW/3-5;
        if(sx>=btn3X&&sx<=btn3X+btnW&&sy>=btnY&&sy<=btnY+btnH){
            DoRetreat();return;
        }
    }
    else if(m_phase==BattlePhase::POST_BATTLE){
        // Click anywhere to dismiss (after short delay)
        if(m_timer>0.5f) m_phase=BattlePhase::DONE;
    }
}

void BattleScene::DoAutoResolve(){
    if(!m_battleData.attacker||!m_battleData.defender)return;

    m_result=m_resolver.Resolve(*m_battleData.attacker,*m_battleData.defender);
    m_result.attackerId = m_battleData.attacker->id;    // ← ADD
    m_result.defenderId = m_battleData.defender->id;    // ← ADD

    // Update snapshots with post-battle
    for(int i=0;i<(int)m_battleData.attacker->units.size()&&i<(int)m_attackerSnap.units.size();i++){
        m_attackerSnap.units[i].manpowerAfter=m_battleData.attacker->units[i].stats.manpower;
        m_attackerSnap.units[i].destroyed=(m_battleData.attacker->units[i].stats.manpower<=0);
    }
    for(int i=0;i<(int)m_battleData.defender->units.size()&&i<(int)m_defenderSnap.units.size();i++){
        m_defenderSnap.units[i].manpowerAfter=m_battleData.defender->units[i].stats.manpower;
        m_defenderSnap.units[i].destroyed=(m_battleData.defender->units[i].stats.manpower<=0);
    }

    m_phase=BattlePhase::POST_BATTLE;m_timer=0;
    Logger::Info("Auto-resolved: %s wins!",m_result.attackerWon?"Attacker":"Defender");
}

void BattleScene::DoRetreat() {
    m_retreated = true;
    m_result.attackerWon = false;
    m_result.isRetreat = true;
    m_result.attackerCasualties = 0;
    m_result.defenderCasualties = 0;
    m_result.attackerId = m_battleData.attacker ? m_battleData.attacker->id : -1;
    m_result.defenderId = m_battleData.defender ? m_battleData.defender->id : -1;

    m_phase = BattlePhase::DONE;
    Logger::Info("Army retreated! (no casualties)");
}
