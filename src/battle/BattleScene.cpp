#include "battle/BattleScene.h"
#include "core/InputManager.h"
#include "utils/Logger.h"

BattleScene::BattleScene()  = default;
BattleScene::~BattleScene() = default;

void BattleScene::Setup(const BattleSetupData& data)
{
    m_battleData = data;
    m_finished = false;
    m_resultDisplayTimer = 0.0f;

    // For Phase 3: immediately auto-resolve
    if (data.attacker && data.defender) {
        m_result = m_resolver.Resolve(*data.attacker, *data.defender);
    }

    Logger::Info("Battle scene setup complete (auto-resolved)");
}

void BattleScene::Update(float deltaTime, const InputManager& input)
{
    if (m_finished) return;

    // Show result for a few seconds, then return to campaign
    m_resultDisplayTimer += deltaTime;

    // Click or timer expires -> done
    if (m_resultDisplayTimer >= RESULT_DISPLAY_TIME ||
        input.IsMouseButtonPressed(0) ||
        input.IsKeyPressed(GLFW_KEY_SPACE)) {
        m_finished = true;
    }
}
