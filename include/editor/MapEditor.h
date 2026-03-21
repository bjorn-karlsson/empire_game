#pragma once
#include <glm/glm.hpp>
#include <string>

class CampaignMap;
class Renderer;

class MapEditor {
public:
    bool isActive = false;

    // Current selection
    int selectedProvinceIdx = -1;  // which province
    int selectedVertexIdx = -1;    // which vertex in that province
    bool isDragging = false;

    // Editor state
    enum class Tool { SELECT, ADD_VERTEX, DELETE_VERTEX };
    Tool currentTool = Tool::SELECT;

    void Toggle() { isActive = !isActive; }

    // Input handlers (call from Game.cpp)
    void HandleLeftClick(const glm::vec3& worldPos, CampaignMap& map);
    void HandleLeftRelease(CampaignMap& map);
    void HandleDrag(const glm::vec3& worldPos, CampaignMap& map);
    void HandleRightClick(const glm::vec3& worldPos, CampaignMap& map);
    void HandleKeyPress(int key, CampaignMap& map, Renderer& renderer);

    // Call after any vertex change to rebuild GPU data
    void RebuildGeometry(CampaignMap& map, Renderer& renderer);

    // Get info string for HUD
    std::string GetStatusText() const;
};