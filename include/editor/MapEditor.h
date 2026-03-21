#pragma once
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <deque>

class CampaignMap;
class Renderer;

class MapEditor {
public:
    bool isActive = false;
    enum class Tool { SELECT, ADD_VERTEX, MOVE_CITY, LINK_VERTEX, HEIGHT_BRUSH };
    Tool currentTool = Tool::SELECT;

    int selectedProvinceIdx = -1, selectedVertexIdx = -1;
    int selectedObstacleIdx = -1, selectedObsVertexIdx = -1;
    bool isDragging = false, isDraggingCity = false;
    int draggingCityProvIdx = -1;
    int hoverProvinceIdx = -1, hoverVertexIdx = -1;
    int hoverObstacleIdx = -1, hoverObsVertexIdx = -1;
    int linkSourceProvIdx = -1, linkSourceVertIdx = -1;
    int linkSourceObsIdx = -1, linkSourceObsVertIdx = -1;

    float brushRadius = 0.8f, brushStrength = 0.015f;
    enum class BrushMode { RAISE, LOWER, SMOOTH, FLATTEN };
    BrushMode brushMode = BrushMode::RAISE;
    bool isPainting = false, geometryDirty = false;

    void Toggle() { isActive = !isActive; }
    void SetScreenInfo(const glm::mat4& vp, float sw, float sh);
    void HandleLeftClick(const glm::vec3& wp, const glm::vec2& mp, CampaignMap& map);
    void HandleRightPress(const glm::vec3& wp, const glm::vec2& mp, CampaignMap& map);
    void HandleRightDrag(const glm::vec3& wp, CampaignMap& map);
    void HandleRightRelease(CampaignMap& map);
    void HandleMouseMove(const glm::vec2& mp, const CampaignMap& map);
    void HandleKeyPress(int key, bool ctrl, CampaignMap& map, Renderer& renderer);
    void PaintHeight(const glm::vec3& wp, CampaignMap& map, bool raise);
    void HandleScrollInEditor(float scroll);
    void RebuildGeometry(CampaignMap& map, Renderer& renderer);
    void SaveUndoState(const CampaignMap& map);
    void Undo(CampaignMap& map, Renderer& renderer);
    void Redo(CampaignMap& map, Renderer& renderer);
    std::string GetToolName() const;
    std::string GetSelectionInfo(const CampaignMap& map) const;

private:
    glm::mat4 m_vpMatrix = glm::mat4(1); float m_screenW = 1280, m_screenH = 720;
    glm::vec2 WorldToScreen(const glm::vec3& wp) const;
    struct HitResult { int provIdx = -1; int vertIdx = -1; float dist = 999; };
    HitResult FindNearestVertex(const glm::vec2& mp, const CampaignMap& map, float maxPx = 18.0f);
    struct ObsHitResult { int obsIdx = -1; int vertIdx = -1; float dist = 999; };
    ObsHitResult FindNearestObstacleVertex(const glm::vec2& mp, const CampaignMap& map, float maxPx = 18.0f);
    int FindNearestCity(const glm::vec2& mp, const CampaignMap& map, float maxPx = 22.0f);
    void MoveSharedVertices(CampaignMap& map, glm::vec3 oldP, glm::vec3 newP);
    void DeleteSelectedVertex(CampaignMap& map);
    struct MapSnapshot {
        struct ProvSnap { std::vector<glm::vec3> verts; glm::vec3 cityPos; };
        struct ObsSnap { std::vector<glm::vec3> verts; };
        std::vector<ProvSnap> provinces;
        std::vector<ObsSnap> obstacles;
        std::vector<float> heightData;
    };
    MapSnapshot CaptureSnapshot(const CampaignMap& map);
    void ApplySnapshot(const MapSnapshot& snap, CampaignMap& map);
    std::deque<MapSnapshot> m_undoStack, m_redoStack;
    static constexpr int MAX_UNDO = 30;
};