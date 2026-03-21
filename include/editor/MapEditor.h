#pragma once
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <deque>

class CampaignMap;
class Renderer;
struct Province;

class MapEditor {
public:
    bool isActive = false;

    // Tools: simplified — vertex moving is always right-drag
    enum class Tool { SELECT, ADD_VERTEX, MOVE_CITY };
    Tool currentTool = Tool::SELECT;

    // Selection
    int selectedProvinceIdx = -1;
    int selectedVertexIdx = -1;
    int selectedObstacleIdx = -1;
    int selectedObsVertexIdx = -1;

    // Dragging (right mouse button)
    bool isDragging = false;
    bool isDraggingCity = false;
    int draggingCityProvIdx = -1;

    // Hover
    int hoverProvinceIdx = -1;
    int hoverVertexIdx = -1;
    int hoverObstacleIdx = -1;
    int hoverObsVertexIdx = -1;

    bool geometryDirty = false;

    void Toggle() { isActive = !isActive; }

    void SetScreenInfo(const glm::mat4& vpMatrix, float screenW, float screenH);

    // Input
    void HandleLeftClick(const glm::vec3& worldPos, const glm::vec2& mousePixel, CampaignMap& map);
    void HandleRightPress(const glm::vec3& worldPos, const glm::vec2& mousePixel, CampaignMap& map);
    void HandleRightDrag(const glm::vec3& worldPos, CampaignMap& map);
    void HandleRightRelease(CampaignMap& map);
    void HandleMouseMove(const glm::vec2& mousePixel, const CampaignMap& map);
    void HandleKeyPress(int key, bool ctrlHeld, CampaignMap& map, Renderer& renderer);

    void RebuildGeometry(CampaignMap& map, Renderer& renderer);

    // Undo/redo
    void SaveUndoState(const CampaignMap& map);
    void Undo(CampaignMap& map, Renderer& renderer);
    void Redo(CampaignMap& map, Renderer& renderer);

    std::string GetToolName() const;
    std::string GetSelectionInfo(const CampaignMap& map) const;
    int GetUndoCount() const { return (int)m_undoStack.size(); }
    int GetRedoCount() const { return (int)m_redoStack.size(); }

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
    void DeleteSelectedVertex(CampaignMap& map);

    // Undo/redo snapshot
    struct MapSnapshot {
        struct ProvSnap { std::vector<glm::vec3> verts; glm::vec3 cityPos; };
        struct ObsSnap { std::vector<glm::vec3> verts; };
        struct FtSnap { std::vector<glm::vec3> verts; };
        std::vector<ProvSnap> provinces;
        std::vector<ObsSnap>  obstacles;
        std::vector<FtSnap>   foreigns;
    };

    MapSnapshot CaptureSnapshot(const CampaignMap& map);
    void ApplySnapshot(const MapSnapshot& snap, CampaignMap& map);

    std::deque<MapSnapshot> m_undoStack;
    std::deque<MapSnapshot> m_redoStack;
    static constexpr int MAX_UNDO = 50;
};