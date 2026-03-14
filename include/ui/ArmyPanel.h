#pragma once

struct Army;
class UIRenderer;

// ─── Army Panel ───────────────────────────────────────────────
// Shows info about the selected army:
//   - General name and army strength
//   - Unit list with manpower bars
//   - Movement points remaining
//   - Merge/split army buttons
class ArmyPanel {
public:
    void Show(const Army* army);
    void Hide();
    void Render(UIRenderer& renderer);
    bool IsVisible() const { return m_visible; }

private:
    const Army* m_army = nullptr;
    bool m_visible = false;
};
