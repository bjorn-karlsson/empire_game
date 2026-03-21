#include "editor/MapEditor.h"
#include "campaign/CampaignMap.h"
#include "campaign/MapSerializer.h"
#include "rendering/Renderer.h"
#include "utils/Logger.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cstdio>

// ═══════════════════════════════════════════════════════════════
// SCREEN PROJECTION
// ═══════════════════════════════════════════════════════════════

void MapEditor::SetScreenInfo(const glm::mat4& vpMatrix, float screenW, float screenH) {
    m_vpMatrix = vpMatrix; m_screenW = screenW; m_screenH = screenH;
}

glm::vec2 MapEditor::WorldToScreen(const glm::vec3& worldPos) const {
    glm::vec4 clip = m_vpMatrix * glm::vec4(worldPos, 1.0f);
    if (clip.w <= 0.001f) return { -9999, -9999 };
    glm::vec3 ndc = glm::vec3(clip) / clip.w;
    return { (ndc.x * 0.5f + 0.5f) * m_screenW, (1.0f - (ndc.y * 0.5f + 0.5f)) * m_screenH };
}

// ═══════════════════════════════════════════════════════════════
// HIT TESTING (screen-space)
// ═══════════════════════════════════════════════════════════════

MapEditor::HitResult MapEditor::FindNearestVertex(const glm::vec2& mousePixel, const CampaignMap& map, float maxPx) {
    HitResult best;
    auto& provs = map.GetProvinces();
    for (int pi = 0; pi < (int)provs.size(); pi++) {
        for (int vi = 0; vi < (int)provs[pi].borderVertices.size(); vi++) {
            auto& v = provs[pi].borderVertices[vi];
            float th = map.GetTerrainHeight(v.x, v.z);
            glm::vec2 sp = WorldToScreen({ v.x, th + 0.2f, v.z });
            float d = glm::distance(mousePixel, sp);
            if (d < best.dist && d < maxPx) best = { pi, vi, d };
        }
    }
    return best;
}

MapEditor::ObsHitResult MapEditor::FindNearestObstacleVertex(const glm::vec2& mousePixel, const CampaignMap& map, float maxPx) {
    ObsHitResult best;
    auto& obs = map.GetObstacles();
    for (int oi = 0; oi < (int)obs.size(); oi++) {
        for (int vi = 0; vi < (int)obs[oi].vertices.size(); vi++) {
            auto& v = obs[oi].vertices[vi];
            float th = map.GetTerrainHeight(v.x, v.z);
            glm::vec2 sp = WorldToScreen({ v.x, th + 0.2f, v.z });
            float d = glm::distance(mousePixel, sp);
            if (d < best.dist && d < maxPx) best = { oi, vi, d };
        }
    }
    return best;
}

int MapEditor::FindNearestCity(const glm::vec2& mousePixel, const CampaignMap& map, float maxPx) {
    float bestD = maxPx; int bestIdx = -1;
    auto& provs = map.GetProvinces();
    for (int i = 0; i < (int)provs.size(); i++) {
        auto& cp = provs[i].cityPos;
        float th = map.GetTerrainHeight(cp.x, cp.z);
        glm::vec2 sp = WorldToScreen({ cp.x, th + 0.2f, cp.z });
        float d = glm::distance(mousePixel, sp);
        if (d < bestD) { bestD = d; bestIdx = i; }
    }
    return bestIdx;
}

// ═══════════════════════════════════════════════════════════════
// SHARED VERTEX MOVEMENT (type-aware)
// ═══════════════════════════════════════════════════════════════

void MapEditor::MoveSharedVertices(CampaignMap& map, glm::vec3 oldPos, glm::vec3 newPos) {
    float threshold = 0.01f;
    glm::vec2 old2d(oldPos.x, oldPos.z);
    bool isProvinceDrag = (selectedProvinceIdx >= 0 && selectedVertexIdx >= 0);
    bool isObstacleDrag = (selectedObstacleIdx >= 0 && selectedObsVertexIdx >= 0);

    if (isProvinceDrag) {
        for (auto& p : map.GetProvincesEditable())
            for (auto& v : p.borderVertices)
                if (glm::distance(glm::vec2(v.x, v.z), old2d) < threshold) v = newPos;
        for (auto& ft : map.GetForeignTerritoriesEditable())
            for (auto& v : ft.vertices)
                if (glm::distance(glm::vec2(v.x, v.z), old2d) < threshold) v = newPos;
    }
    else if (isObstacleDrag) {
        for (auto& ob : map.GetObstaclesEditable())
            for (auto& v : ob.vertices)
                if (glm::distance(glm::vec2(v.x, v.z), old2d) < threshold) v = newPos;
    }
}

// ═══════════════════════════════════════════════════════════════
// UNDO / REDO
// ═══════════════════════════════════════════════════════════════

MapEditor::MapSnapshot MapEditor::CaptureSnapshot(const CampaignMap& map) {
    MapSnapshot snap;
    for (auto& p : map.GetProvinces())
        snap.provinces.push_back({ p.borderVertices, p.cityPos });
    for (auto& ob : map.GetObstacles())
        snap.obstacles.push_back({ ob.vertices });
    for (auto& ft : map.GetForeignTerritories())
        snap.foreigns.push_back({ ft.vertices });
    snap.heightData = map.GetHeightMap().GetData();
    return snap;
}

void MapEditor::ApplySnapshot(const MapSnapshot& snap, CampaignMap& map) {
    auto& provs = map.GetProvincesEditable();
    for (int i = 0; i < (int)snap.provinces.size() && i < (int)provs.size(); i++) {
        provs[i].borderVertices = snap.provinces[i].verts;
        provs[i].cityPos = snap.provinces[i].cityPos;
        glm::vec3 c(0);
        for (auto& v : provs[i].borderVertices) c += v;
        if (!provs[i].borderVertices.empty()) provs[i].center = c / (float)provs[i].borderVertices.size();
    }
    auto& obs = map.GetObstaclesEditable();
    for (int i = 0; i < (int)snap.obstacles.size() && i < (int)obs.size(); i++) {
        obs[i].vertices = snap.obstacles[i].verts;
        glm::vec3 c(0);
        for (auto& v : obs[i].vertices) c += v;
        if (!obs[i].vertices.empty()) obs[i].center = c / (float)obs[i].vertices.size();
    }
    auto& fts = map.GetForeignTerritoriesEditable();
    for (int i = 0; i < (int)snap.foreigns.size() && i < (int)fts.size(); i++) {
        fts[i].vertices = snap.foreigns[i].verts;
        glm::vec3 c(0);
        for (auto& v : fts[i].vertices) c += v;
        if (!fts[i].vertices.empty()) fts[i].center = c / (float)fts[i].vertices.size();
    }
    if (!snap.heightData.empty()) {
        map.GetHeightMap().GetData() = snap.heightData;
        map.GetHeightMap().UploadToGPU();
    }
}

void MapEditor::SaveUndoState(const CampaignMap& map) {
    m_undoStack.push_back(CaptureSnapshot(map));
    if ((int)m_undoStack.size() > MAX_UNDO) m_undoStack.pop_front();
    m_redoStack.clear();
}

void MapEditor::Undo(CampaignMap& map, Renderer& renderer) {
    if (m_undoStack.empty()) { Logger::Info("Editor: Nothing to undo"); return; }
    m_redoStack.push_back(CaptureSnapshot(map));
    ApplySnapshot(m_undoStack.back(), map);
    m_undoStack.pop_back();
    RebuildGeometry(map, renderer);
    Logger::Info("Editor: Undo (%d left)", (int)m_undoStack.size());
}

void MapEditor::Redo(CampaignMap& map, Renderer& renderer) {
    if (m_redoStack.empty()) { Logger::Info("Editor: Nothing to redo"); return; }
    m_undoStack.push_back(CaptureSnapshot(map));
    ApplySnapshot(m_redoStack.back(), map);
    m_redoStack.pop_back();
    RebuildGeometry(map, renderer);
    Logger::Info("Editor: Redo (%d left)", (int)m_redoStack.size());
}

// ═══════════════════════════════════════════════════════════════
// HOVER
// ═══════════════════════════════════════════════════════════════

void MapEditor::HandleMouseMove(const glm::vec2& mousePixel, const CampaignMap& map) {
    if (!isActive || isDragging || isDraggingCity || isPainting) return;
    auto hit = FindNearestVertex(mousePixel, map, 18.0f);
    hoverProvinceIdx = hit.provIdx; hoverVertexIdx = hit.vertIdx;
    auto obsHit = FindNearestObstacleVertex(mousePixel, map, 18.0f);
    hoverObstacleIdx = obsHit.obsIdx; hoverObsVertexIdx = obsHit.vertIdx;
}

// ═══════════════════════════════════════════════════════════════
// HEIGHT BRUSH
// ═══════════════════════════════════════════════════════════════

void MapEditor::PaintHeight(const glm::vec3& worldPos, CampaignMap& map, bool raise) {
    auto& hm = map.GetHeightMap();
    float str = brushStrength * (raise ? 1.0f : -1.0f);
    switch (brushMode) {
    case BrushMode::RAISE:
    case BrushMode::LOWER:
        hm.Paint(worldPos.x, worldPos.z, brushRadius, str);
        break;
    case BrushMode::SMOOTH:
        hm.Smooth(worldPos.x, worldPos.z, brushRadius, brushStrength * 0.5f);
        break;
    case BrushMode::FLATTEN:
        hm.Flatten(worldPos.x, worldPos.z, brushRadius, 0.0f, brushStrength);
        break;
    }
}

void MapEditor::HandleScrollInEditor(float scroll) {
    if (currentTool == Tool::HEIGHT_BRUSH) {
        brushRadius += scroll * 0.15f;
        brushRadius = glm::clamp(brushRadius, 0.2f, 5.0f);
    }
}

// ═══════════════════════════════════════════════════════════════
// LEFT CLICK = SELECT / ADD VERTEX / LINK
// ═══════════════════════════════════════════════════════════════

void MapEditor::HandleLeftClick(const glm::vec3& worldPos, const glm::vec2& mousePixel, CampaignMap& map) {
    if (!isActive) return;

    // ── LINK VERTEX ──
    if (currentTool == Tool::LINK_VERTEX) {
        auto hit = FindNearestVertex(mousePixel, map, 25.0f);
        auto obsHit = FindNearestObstacleVertex(mousePixel, map, 25.0f);
        bool isProv = (hit.dist < obsHit.dist && hit.provIdx >= 0);
        bool isObs = (!isProv && obsHit.obsIdx >= 0);

        if (isProv || isObs) {
            // No source yet — set it
            if (linkSourceProvIdx < 0 && linkSourceObsIdx < 0) {
                if (isProv) {
                    linkSourceProvIdx = hit.provIdx; linkSourceVertIdx = hit.vertIdx;
                    linkSourceObsIdx = -1;
                    selectedProvinceIdx = hit.provIdx; selectedVertexIdx = hit.vertIdx;
                    Logger::Info("Editor: Link source set — now click target vertex");
                }
                else {
                    linkSourceObsIdx = obsHit.obsIdx; linkSourceObsVertIdx = obsHit.vertIdx;
                    linkSourceProvIdx = -1;
                    selectedObstacleIdx = obsHit.obsIdx; selectedObsVertexIdx = obsHit.vertIdx;
                    Logger::Info("Editor: Link source set (obstacle) — now click target");
                }
            }
            else {
                // Have source — snap target to source position
                SaveUndoState(map);
                glm::vec3 sourcePos;
                if (linkSourceProvIdx >= 0)
                    sourcePos = map.GetProvinces()[linkSourceProvIdx].borderVertices[linkSourceVertIdx];
                else
                    sourcePos = map.GetObstacles()[linkSourceObsIdx].vertices[linkSourceObsVertIdx];

                if (isProv) {
                    auto& provs = map.GetProvincesEditable();
                    provs[hit.provIdx].borderVertices[hit.vertIdx] = sourcePos;
                    glm::vec3 c(0);
                    for (auto& v : provs[hit.provIdx].borderVertices) c += v;
                    provs[hit.provIdx].center = c / (float)provs[hit.provIdx].borderVertices.size();
                }
                else {
                    auto& obs = map.GetObstaclesEditable();
                    obs[obsHit.obsIdx].vertices[obsHit.vertIdx] = sourcePos;
                }
                Logger::Info("Editor: Vertices linked!");
                geometryDirty = true;
                linkSourceProvIdx = -1; linkSourceVertIdx = -1;
                linkSourceObsIdx = -1; linkSourceObsVertIdx = -1;
            }
        }
        return;
    }

    // ── ADD VERTEX ──
    if (currentTool == Tool::ADD_VERTEX && selectedProvinceIdx >= 0) {
        auto& provs = map.GetProvincesEditable();
        if (selectedProvinceIdx < (int)provs.size()) {
            SaveUndoState(map);
            Province& p = provs[selectedProvinceIdx];
            glm::vec2 click(worldPos.x, worldPos.z);
            float bestDist = 999; int bestEdge = -1;
            int n = (int)p.borderVertices.size();
            for (int i = 0; i < n; i++) {
                glm::vec2 a(p.borderVertices[i].x, p.borderVertices[i].z);
                glm::vec2 b(p.borderVertices[(i + 1) % n].x, p.borderVertices[(i + 1) % n].z);
                glm::vec2 ab = b - a;
                float len2 = glm::dot(ab, ab);
                if (len2 < 0.0001f) continue;
                float t = glm::clamp(glm::dot(click - a, ab) / len2, 0.0f, 1.0f);
                float d = glm::distance(click, a + t * ab);
                if (d < bestDist) { bestDist = d; bestEdge = i; }
            }
            int insertAt = (bestEdge >= 0) ? bestEdge + 1 : (int)p.borderVertices.size();
            p.borderVertices.insert(p.borderVertices.begin() + insertAt, glm::vec3(worldPos.x, 0, worldPos.z));
            selectedVertexIdx = insertAt;
            glm::vec3 c(0);
            for (auto& v : p.borderVertices) c += v;
            p.center = c / (float)p.borderVertices.size();
            geometryDirty = true;
            Logger::Info("Editor: Added vertex %d to %s", insertAt, p.name.c_str());
        }
        return;
    }

    // ── HEIGHT BRUSH: left click does nothing special (painting handled in Game.cpp) ──
    if (currentTool == Tool::HEIGHT_BRUSH) return;

    // ── SELECT ──
    auto hit = FindNearestVertex(mousePixel, map, 18.0f);
    auto obsHit = FindNearestObstacleVertex(mousePixel, map, 18.0f);

    if (hit.dist < obsHit.dist && hit.provIdx >= 0) {
        selectedProvinceIdx = hit.provIdx; selectedVertexIdx = hit.vertIdx;
        selectedObstacleIdx = -1; selectedObsVertexIdx = -1;
    }
    else if (obsHit.obsIdx >= 0) {
        selectedObstacleIdx = obsHit.obsIdx; selectedObsVertexIdx = obsHit.vertIdx;
        selectedProvinceIdx = -1; selectedVertexIdx = -1;
    }
    else {
        auto* prov = map.GetProvinceAtWorldPos(worldPos);
        if (prov) {
            auto& provs = map.GetProvinces();
            for (int i = 0; i < (int)provs.size(); i++)
                if (provs[i].id == prov->id) { selectedProvinceIdx = i; break; }
            selectedVertexIdx = -1;
        }
        else {
            selectedProvinceIdx = -1; selectedVertexIdx = -1;
        }
        selectedObstacleIdx = -1; selectedObsVertexIdx = -1;
    }
}

// ═══════════════════════════════════════════════════════════════
// RIGHT MOUSE = DRAG selected vertex
// ═══════════════════════════════════════════════════════════════

void MapEditor::HandleRightPress(const glm::vec3& worldPos, const glm::vec2& mousePixel, CampaignMap& map) {
    if (!isActive) return;
    if (currentTool == Tool::MOVE_CITY) {
        int cityIdx = FindNearestCity(mousePixel, map, 22.0f);
        if (cityIdx >= 0) {
            SaveUndoState(map);
            draggingCityProvIdx = cityIdx; isDraggingCity = true;
        }
        return;
    }
    if (selectedProvinceIdx >= 0 && selectedVertexIdx >= 0) {
        SaveUndoState(map); isDragging = true; return;
    }
    if (selectedObstacleIdx >= 0 && selectedObsVertexIdx >= 0) {
        SaveUndoState(map); isDragging = true; return;
    }
}

void MapEditor::HandleRightDrag(const glm::vec3& worldPos, CampaignMap& map) {
    if (!isActive) return;
    glm::vec3 newPos(worldPos.x, 0, worldPos.z);
    if (isDragging) {
        if (selectedProvinceIdx >= 0 && selectedVertexIdx >= 0) {
            auto& provs = map.GetProvincesEditable();
            if (selectedProvinceIdx < (int)provs.size()) {
                Province& p = provs[selectedProvinceIdx];
                if (selectedVertexIdx < (int)p.borderVertices.size()) {
                    MoveSharedVertices(map, p.borderVertices[selectedVertexIdx], newPos);
                }
            }
        }
        else if (selectedObstacleIdx >= 0 && selectedObsVertexIdx >= 0) {
            auto& obs = map.GetObstaclesEditable();
            if (selectedObstacleIdx < (int)obs.size()) {
                auto& ob = obs[selectedObstacleIdx];
                if (selectedObsVertexIdx < (int)ob.vertices.size()) {
                    MoveSharedVertices(map, ob.vertices[selectedObsVertexIdx], newPos);
                }
            }
        }
    }
    if (isDraggingCity && draggingCityProvIdx >= 0) {
        auto& provs = map.GetProvincesEditable();
        if (draggingCityProvIdx < (int)provs.size())
            provs[draggingCityProvIdx].cityPos = newPos;
    }
}

void MapEditor::HandleRightRelease(CampaignMap& map) {
    if (isDragging) {
        if (selectedProvinceIdx >= 0) {
            auto& p = map.GetProvincesEditable()[selectedProvinceIdx];
            glm::vec3 c(0);
            for (auto& v : p.borderVertices) c += v;
            p.center = c / (float)p.borderVertices.size();
        }
        if (selectedObstacleIdx >= 0) {
            auto& ob = map.GetObstaclesEditable()[selectedObstacleIdx];
            glm::vec3 c(0);
            for (auto& v : ob.vertices) c += v;
            ob.center = c / (float)ob.vertices.size();
        }
        geometryDirty = true;
    }
    if (isDraggingCity) geometryDirty = true;
    isDragging = false; isDraggingCity = false;
}

// ═══════════════════════════════════════════════════════════════
// DELETE SELECTED VERTEX
// ═══════════════════════════════════════════════════════════════

void MapEditor::DeleteSelectedVertex(CampaignMap& map) {
    if (selectedProvinceIdx >= 0 && selectedVertexIdx >= 0) {
        auto& provs = map.GetProvincesEditable();
        Province& p = provs[selectedProvinceIdx];
        if (p.borderVertices.size() > 3 && selectedVertexIdx < (int)p.borderVertices.size()) {
            SaveUndoState(map);
            p.borderVertices.erase(p.borderVertices.begin() + selectedVertexIdx);
            glm::vec3 c(0); for (auto& v : p.borderVertices) c += v;
            p.center = c / (float)p.borderVertices.size();
            selectedVertexIdx = -1; geometryDirty = true;
        }
        return;
    }
    if (selectedObstacleIdx >= 0 && selectedObsVertexIdx >= 0) {
        auto& obs = map.GetObstaclesEditable();
        auto& ob = obs[selectedObstacleIdx];
        if (ob.vertices.size() > 3 && selectedObsVertexIdx < (int)ob.vertices.size()) {
            SaveUndoState(map);
            ob.vertices.erase(ob.vertices.begin() + selectedObsVertexIdx);
            glm::vec3 c(0); for (auto& v : ob.vertices) c += v;
            ob.center = c / (float)ob.vertices.size();
            selectedObsVertexIdx = -1; geometryDirty = true;
        }
    }
}

// ═══════════════════════════════════════════════════════════════
// KEY PRESS
// ═══════════════════════════════════════════════════════════════

void MapEditor::HandleKeyPress(int key, bool ctrlHeld, CampaignMap& map, Renderer& renderer) {
    if (!isActive) return;

    // Tools
    if (!ctrlHeld) {
        if (key == GLFW_KEY_1) { currentTool = Tool::SELECT; linkSourceProvIdx = -1; linkSourceObsIdx = -1; Logger::Info("Editor: SELECT"); }
        if (key == GLFW_KEY_2) { currentTool = Tool::ADD_VERTEX; Logger::Info("Editor: ADD VERTEX"); }
        if (key == GLFW_KEY_3) { currentTool = Tool::LINK_VERTEX; linkSourceProvIdx = -1; linkSourceObsIdx = -1; Logger::Info("Editor: LINK VERTEX — click source, then target"); }
        if (key == GLFW_KEY_4) { currentTool = Tool::HEIGHT_BRUSH; Logger::Info("Editor: HEIGHT BRUSH"); }
        if (key == GLFW_KEY_5) { currentTool = Tool::MOVE_CITY; Logger::Info("Editor: MOVE CITY"); }

        if (key == GLFW_KEY_DELETE) {
            DeleteSelectedVertex(map);
            if (geometryDirty) RebuildGeometry(map, renderer);
        }
        if (key == GLFW_KEY_R) { RebuildGeometry(map, renderer); Logger::Info("Editor: Geometry rebuilt"); }

        // Height brush sub-modes
        if (currentTool == Tool::HEIGHT_BRUSH) {
            if (key == GLFW_KEY_Q) { brushMode = BrushMode::RAISE; Logger::Info("Brush: Raise"); }
            if (key == GLFW_KEY_F) { brushMode = BrushMode::FLATTEN; Logger::Info("Brush: Flatten"); }
            if (key == GLFW_KEY_G) { brushMode = BrushMode::SMOOTH; Logger::Info("Brush: Smooth"); }
        }
    }

    // Ctrl combos
    if (ctrlHeld) {
        if (key == GLFW_KEY_Z) Undo(map, renderer);
        if (key == GLFW_KEY_Y) Redo(map, renderer);
        if (key == GLFW_KEY_S) {
            if (geometryDirty) RebuildGeometry(map, renderer);
            MapSerializer::SaveToFile(map, "maps/europe_1700.json");
        }
        if (key == GLFW_KEY_L) {
            if (MapSerializer::LoadFromFile(map, "maps/europe_1700.json")) {
                RebuildGeometry(map, renderer);
                selectedProvinceIdx = -1; selectedVertexIdx = -1;
                selectedObstacleIdx = -1; selectedObsVertexIdx = -1;
                m_undoStack.clear(); m_redoStack.clear();
            }
        }
    }
}

void MapEditor::RebuildGeometry(CampaignMap& map, Renderer& renderer) {
    for (auto& p : map.GetProvincesEditable()) {
        glm::vec3 c(0);
        for (auto& v : p.borderVertices) c += v;
        if (!p.borderVertices.empty()) p.center = c / (float)p.borderVertices.size();
    }
    for (auto& ob : map.GetObstaclesEditable()) {
        glm::vec3 c(0);
        for (auto& v : ob.vertices) c += v;
        if (!ob.vertices.empty()) ob.center = c / (float)ob.vertices.size();
    }
    renderer.ClearMapGeometry();
    renderer.BuildMapGeometry(map);
    map.BuildNavGrid();
    geometryDirty = false;
}

// ═══════════════════════════════════════════════════════════════
// INFO
// ═══════════════════════════════════════════════════════════════

std::string MapEditor::GetToolName() const {
    switch (currentTool) {
    case Tool::SELECT: return "Select";
    case Tool::ADD_VERTEX: return "Add Vertex";
    case Tool::MOVE_CITY: return "Move City";
    case Tool::LINK_VERTEX: {
        if (linkSourceProvIdx >= 0 || linkSourceObsIdx >= 0) return "Link (click target)";
        return "Link (click source)";
    }
    case Tool::HEIGHT_BRUSH: {
        char buf[64];
        const char* mode = "Raise";
        if (brushMode == BrushMode::SMOOTH) mode = "Smooth";
        if (brushMode == BrushMode::FLATTEN) mode = "Flatten";
        snprintf(buf, sizeof(buf), "Brush [%s] R:%.1f", mode, brushRadius);
        return buf;
    }
    }
    return "?";
}

std::string MapEditor::GetSelectionInfo(const CampaignMap& map) const {
    if (selectedProvinceIdx >= 0 && selectedProvinceIdx < (int)map.GetProvinces().size()) {
        auto& p = map.GetProvinces()[selectedProvinceIdx];
        std::string info = p.name + " (" + std::to_string(p.borderVertices.size()) + "v)";
        if (selectedVertexIdx >= 0 && selectedVertexIdx < (int)p.borderVertices.size()) {
            auto& v = p.borderVertices[selectedVertexIdx];
            char buf[64]; snprintf(buf, sizeof(buf), " V%d (%.1f,%.1f)", selectedVertexIdx, v.x, v.z);
            info += buf;
        }
        return info;
    }
    if (selectedObstacleIdx >= 0 && selectedObstacleIdx < (int)map.GetObstacles().size()) {
        auto& ob = map.GetObstacles()[selectedObstacleIdx];
        std::string info = ob.name + " [" + ob.type + "] (" + std::to_string(ob.vertices.size()) + "v)";
        if (selectedObsVertexIdx >= 0 && selectedObsVertexIdx < (int)ob.vertices.size()) {
            auto& v = ob.vertices[selectedObsVertexIdx];
            char buf[64]; snprintf(buf, sizeof(buf), " V%d (%.1f,%.1f)", selectedObsVertexIdx, v.x, v.z);
            info += buf;
        }
        return info;
    }
    return "nothing";
}