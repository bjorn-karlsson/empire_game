#pragma once
#include <glm/glm.hpp>
#include <string>
#include <vector>

class CampaignMap;
class Renderer;

class MapEditor {
public:
    bool isActive = false;

    enum class Tool { SELECT, MOVE_VERTEX, ADD_VERTEX, DELETE_VERTEX, MOVE_CITY };
    Tool currentTool = Tool::SELECT;

    int selectedProvinceIdx = -1;
    int selectedVertexIdx = -1;
    bool isDragging = false;

    int selectedObstacleIdx = -1;
    int selectedObsVertexIdx = -1;
    bool isDraggingObstacle = false;

    int draggingCityProvIdx = -1;
    bool isDraggingCity = false;

    int hoverProvinceIdx = -1;
    int hoverVertexIdx = -1;
    int hoverObstacleIdx = -1;
    int hoverObsVertexIdx = -1;

    bool geometryDirty = false;

    void Toggle() { isActive = !isActive; }

    // Call once per frame before any input handling
    void SetScreenInfo(const glm::mat4& vpMatrix, float screenW, float screenH);

    // Input — all picking now uses mousePixel (screen coords) for accuracy
    void HandleLeftClick(const glm::vec3& worldPos, const glm::vec2& mousePixel, CampaignMap& map);
    void HandleLeftRelease(CampaignMap& map);
    void HandleDrag(const glm::vec3& worldPos, CampaignMap& map);
    void HandleRightClick(const glm::vec3& worldPos, const glm::vec2& mousePixel, CampaignMap& map);
    void HandleMouseMove(const glm::vec2& mousePixel, const CampaignMap& map);
    void HandleKeyPress(int key, CampaignMap& map, Renderer& renderer);

    void RebuildGeometry(CampaignMap& map, Renderer& renderer);

    std::string GetToolName() const;
    std::string GetSelectionInfo(const CampaignMap& map) const;

private:
    glm::mat4 m_vpMatrix = glm::mat4(1);
    float m_screenW = 1280, m_screenH = 720;

    glm::vec2 WorldToScreen(const glm::vec3& worldPos) const;

    struct HitResult { int provIdx = -1; int vertIdx = -1; float dist = 999; };
    HitResult FindNearestVertex(const glm::vec2& mousePixel, const CampaignMap& map, float maxPx = 18.0f);

    struct ObsHitResult { int obsIdx = -1; int vertIdx = -1; float dist = 999; };
    ObsHitResult FindNearestObstacleVertex(const glm::vec2& mousePixel, const CampaignMap& map, float maxPx = 18.0f);

    int FindNearestCity(const glm::vec2& mousePixel, const CampaignMap& map, float maxPx = 22.0f);

    void MoveSharedVertices(CampaignMap& map, glm::vec3 oldPos, glm::vec3 newPos);
};