#pragma once

#include "campaign/CampaignMap.h"
#include "battle/BattleResolver.h"

class InputManager;

// ─── Battle Scene ─────────────────────────────────────────────
// Phase 3: Auto-resolve only (shows a result screen)
// Phase 4: Full real-time 3D battles with formations
//
// For now, this immediately auto-resolves when Setup() is called
// and presents the results. Later, this becomes the real-time
// battle engine.
class BattleScene {
public:
    BattleScene();
    ~BattleScene();

    void Setup(const BattleSetupData& data);
    void Update(float deltaTime, const InputManager& input);
    bool IsFinished() const { return m_finished; }
    BattleResult GetResult() const { return m_result; }

private:
    BattleResolver m_resolver;
    BattleResult   m_result;
    BattleSetupData m_battleData;

    bool  m_finished = true;
    float m_resultDisplayTimer = 0.0f;
    static constexpr float RESULT_DISPLAY_TIME = 3.0f;
};
