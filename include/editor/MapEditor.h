#pragma once
#include <glm/glm.hpp>
#include <string>
#include <vector>

class CampaignMap;
class Renderer;

class MapEditor {
public:
    bool isActive = false;

    // Tools
    enum class Tool { SELECT, MOVE_VERTEX, ADD_VERTEX, DELETE_VERTEX, MOVE_CITY };
    Tool currentTool = Tool::SELECT;

    // Province vertex selection
    int selectedProvinceIdx = -1;
    int selectedVertexIdx = -1;
    bool isDragging = false;

    // Obstacle vertex selection
    int selectedObstacleIdx = -1;
    int selectedObsVertexIdx = -1;
    bool isDraggingObstacle = false;

    // City dragging
    int draggingCityProvIdx = -1;
    bool isDraggingCity = false;

    // Hover state (for visual feedback)
    int hoverProvinceIdx = -1;
    int hoverVertexIdx = -1;
    int hoverObstacleIdx = -1;
    int hoverObsVertexIdx = -1;

    // Dirty flag — geometry needs rebuild
    bool geometryDirty = false;

    void Toggle() { isActive = !isActive; }

    // Input handlers
    void HandleLeftClick(const glm::vec3& worldPos, CampaignMap& map);
    void HandleLeftRelease(CampaignMap& map);
    void HandleDrag(const glm::vec3& worldPos, CampaignMap& map);
    void HandleRightClick(const glm::vec3& worldPos, CampaignMap& map);
    void HandleMouseMove(const glm::vec3& worldPos, const CampaignMap& map);
    void HandleKeyPress(int key, CampaignMap& map, Renderer& renderer);

    void RebuildGeometry(CampaignMap& map, Renderer& renderer);

    // Info for HUD
    std::string GetToolName() const;
    std::string GetSelectionInfo(const CampaignMap& map) const;
    int GetSelectedProvinceId(const CampaignMap& map) const;

private:
    // Find nearest vertex to a world position
    struct HitResult { int provIdx = -1; int vertIdx = -1; float dist = 999; };
    HitResult FindNearestVertex(const glm::vec3& worldPos, const CampaignMap& map, float maxDist = 0.4f);

    struct ObsHitResult { int obsIdx = -1; int vertIdx = -1; float dist = 999; };
    ObsHitResult FindNearestObstacleVertex(const glm::vec3& worldPos, const CampaignMap& map, float maxDist = 0.4f);

    int FindNearestCity(const glm::vec3& worldPos, const CampaignMap& map, float maxDist = 0.5f);

    // Move shared vertices across all provinces/obstacles/foreign territories
    void MoveSharedVertices(CampaignMap& map, glm::vec3 oldPos, glm::vec3 newPos);
};