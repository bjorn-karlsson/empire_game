#pragma once

struct Province;
class UIRenderer;

// ─── Province Panel ───────────────────────────────────────────
// Shows info about the selected province:
//   - Province name and owner
//   - Population and public order
//   - Income and tax rate slider
//   - Buildings list with upgrade buttons
//   - Recruitment options (if barracks present)
class ProvincePanel {
public:
    void Show(const Province* province);
    void Hide();
    void Render(UIRenderer& renderer);
    bool IsVisible() const { return m_visible; }

private:
    const Province* m_province = nullptr;
    bool m_visible = false;
};
