#include "editor/MapEditor.h"
#include "campaign/CampaignMap.h"
#include "campaign/MapSerializer.h"
#include "rendering/Renderer.h"
#include "utils/Logger.h"
#include <GLFW/glfw3.h>
#include <algorithm>

// ═══════════════════════════════════════════════════════════════
// HIT TESTING
// ═══════════════════════════════════════════════════════════════

MapEditor::HitResult MapEditor::FindNearestVertex(const glm::vec3& worldPos, const CampaignMap& map, float maxDist) {
    HitResult best;
    glm::vec2 click(worldPos.x, worldPos.z);
    auto& provs = map.GetProvinces();
    for (int pi = 0; pi < (int)provs.size(); pi++) {
        for (int vi = 0; vi < (int)provs[pi].borderVertices.size(); vi++) {
            glm::vec2 vp(provs[pi].borderVertices[vi].x, provs[pi].borderVertices[vi].z);
            float d = glm::distance(click, vp);
            if (d < best.dist && d < maxDist) {
                best = { pi, vi, d };
            }
        }
    }
    return best;
}

MapEditor::ObsHitResult MapEditor::FindNearestObstacleVertex(const glm::vec3& worldPos, const CampaignMap& map, float maxDist) {
    ObsHitResult best;
    glm::vec2 click(worldPos.x, worldPos.z);
    auto& obs = map.GetObstacles();
    for (int oi = 0; oi < (int)obs.size(); oi++) {
        for (int vi = 0; vi < (int)obs[oi].vertices.size(); vi++) {
            glm::vec2 vp(obs[oi].vertices[vi].x, obs[oi].vertices[vi].z);
            float d = glm::distance(click, vp);
            if (d < best.dist && d < maxDist) {
                best = { oi, vi, d };
            }
        }
    }
    return best;
}

int MapEditor::FindNearestCity(const glm::vec3& worldPos, const CampaignMap& map, float maxDist) {
    glm::vec2 click(worldPos.x, worldPos.z);
    float bestD = maxDist;
    int bestIdx = -1;
    auto& provs = map.GetProvinces();
    for (int i = 0; i < (int)provs.size(); i++) {
        float d = glm::distance(click, glm::vec2(provs[i].cityPos.x, provs[i].cityPos.z));
        if (d < bestD) { bestD = d; bestIdx = i; }
    }
    return bestIdx;
}

// ═══════════════════════════════════════════════════════════════
// SHARED VERTEX MOVEMENT
// ═══════════════════════════════════════════════════════════════

void MapEditor::MoveSharedVertices(CampaignMap& map, glm::vec3 oldPos, glm::vec3 newPos) {
    float threshold = 0.01f;
    glm::vec2 old2d(oldPos.x, oldPos.z);

    for (auto& p : map.GetProvincesEditable()) {
        for (auto& v : p.borderVertices) {
            if (glm::distance(glm::vec2(v.x, v.z), old2d) < threshold) {
                v = newPos;
            }
        }
    }
    for (auto& ob : map.GetObstaclesEditable()) {
        for (auto& v : ob.vertices) {
            if (glm::distance(glm::vec2(v.x, v.z), old2d) < threshold) {
                v = newPos;
            }
        }
    }
    for (auto& ft : map.GetForeignTerritoriesEditable()) {
        for (auto& v : ft.vertices) {
            if (glm::distance(glm::vec2(v.x, v.z), old2d) < threshold) {
                v = newPos;
            }
        }
    }
}

// ═══════════════════════════════════════════════════════════════
// MOUSE MOVE (hover feedback)
// ═══════════════════════════════════════════════════════════════

void MapEditor::HandleMouseMove(const glm::vec3& worldPos, const CampaignMap& map) {
    if (!isActive || isDragging || isDraggingObstacle || isDraggingCity) return;

    auto hit = FindNearestVertex(worldPos, map, 0.4f);
    hoverProvinceIdx = hit.provIdx;
    hoverVertexIdx = hit.vertIdx;

    auto obsHit = FindNearestObstacleVertex(worldPos, map, 0.4f);
    hoverObstacleIdx = obsHit.obsIdx;
    hoverObsVertexIdx = obsHit.vertIdx;
}

// ═══════════════════════════════════════════════════════════════
// LEFT CLICK
// ═══════════════════════════════════════════════════════════════

void MapEditor::HandleLeftClick(const glm::vec3& worldPos, CampaignMap& map) {
    if (!isActive) return;

    // ── MOVE CITY tool ──
    if (currentTool == Tool::MOVE_CITY) {
        int cityIdx = FindNearestCity(worldPos, map, 0.5f);
        if (cityIdx >= 0) {
            draggingCityProvIdx = cityIdx;
            isDraggingCity = true;
            Logger::Info("Editor: Dragging city of %s", map.GetProvinces()[cityIdx].name.c_str());
        }
        return;
    }

    // ── ADD VERTEX tool ──
    if (currentTool == Tool::ADD_VERTEX && selectedProvinceIdx >= 0) {
        auto& provs = map.GetProvincesEditable();
        if (selectedProvinceIdx < (int)provs.size()) {
            Province& p = provs[selectedProvinceIdx];
            // Find the edge closest to the click point, insert between those two vertices
            glm::vec2 click(worldPos.x, worldPos.z);
            float bestDist = 999;
            int bestEdge = -1;
            int n = (int)p.borderVertices.size();
            for (int i = 0; i < n; i++) {
                glm::vec2 a(p.borderVertices[i].x, p.borderVertices[i].z);
                glm::vec2 b(p.borderVertices[(i + 1) % n].x, p.borderVertices[(i + 1) % n].z);
                // Distance from click to line segment
                glm::vec2 ab = b - a;
                float t = glm::clamp(glm::dot(click - a, ab) / glm::dot(ab, ab), 0.0f, 1.0f);
                glm::vec2 proj = a + t * ab;
                float d = glm::distance(click, proj);
                if (d < bestDist) { bestDist = d; bestEdge = i; }
            }
            int insertAt = (bestEdge >= 0) ? bestEdge + 1 : (int)p.borderVertices.size();
            p.borderVertices.insert(p.borderVertices.begin() + insertAt,
                glm::vec3(worldPos.x, 0, worldPos.z));
            selectedVertexIdx = insertAt;

            // Recompute center
            glm::vec3 c(0);
            for (auto& v : p.borderVertices) c += v;
            p.center = c / (float)p.borderVertices.size();

            geometryDirty = true;
            Logger::Info("Editor: Added vertex %d to %s (%.1f, %.1f)",
                insertAt, p.name.c_str(), worldPos.x, worldPos.z);
        }
        return;
    }

    // ── SELECT / MOVE VERTEX tool ──
    // Try province vertices first
    auto hit = FindNearestVertex(worldPos, map, 0.4f);
    // Also try obstacle vertices
    auto obsHit = FindNearestObstacleVertex(worldPos, map, 0.4f);

    // Pick whichever is closer
    if (hit.dist < obsHit.dist && hit.provIdx >= 0) {
        selectedProvinceIdx = hit.provIdx;
        selectedVertexIdx = hit.vertIdx;
        selectedObstacleIdx = -1;
        selectedObsVertexIdx = -1;
        if (currentTool == Tool::MOVE_VERTEX || currentTool == Tool::SELECT) {
            isDragging = true;
        }
    }
    else if (obsHit.obsIdx >= 0) {
        selectedObstacleIdx = obsHit.obsIdx;
        selectedObsVertexIdx = obsHit.vertIdx;
        selectedProvinceIdx = -1;
        selectedVertexIdx = -1;
        if (currentTool == Tool::MOVE_VERTEX || currentTool == Tool::SELECT) {
            isDraggingObstacle = true;
        }
    }
    else {
        // Click on empty space — try to select a province by clicking inside it
        auto* prov = map.GetProvinceAtWorldPos(worldPos);
        if (prov) {
            // Find this province's index
            auto& provs = map.GetProvinces();
            for (int i = 0; i < (int)provs.size(); i++) {
                if (provs[i].id == prov->id) { selectedProvinceIdx = i; break; }
            }
            selectedVertexIdx = -1;
        }
        else {
            selectedProvinceIdx = -1;
            selectedVertexIdx = -1;
        }
        selectedObstacleIdx = -1;
        selectedObsVertexIdx = -1;
    }
}

// ═══════════════════════════════════════════════════════════════
// LEFT RELEASE
// ═══════════════════════════════════════════════════════════════

void MapEditor::HandleLeftRelease(CampaignMap& map) {
    if (isDragging && selectedProvinceIdx >= 0) {
        auto& provs = map.GetProvincesEditable();
        Province& p = provs[selectedProvinceIdx];
        glm::vec3 c(0);
        for (auto& v : p.borderVertices) c += v;
        p.center = c / (float)p.borderVertices.size();
        geometryDirty = true;
    }
    if (isDraggingObstacle && selectedObstacleIdx >= 0) {
        auto& obs = map.GetObstaclesEditable();
        auto& ob = obs[selectedObstacleIdx];
        glm::vec3 c(0);
        for (auto& v : ob.vertices) c += v;
        ob.center = c / (float)ob.vertices.size();
        geometryDirty = true;
    }
    if (isDraggingCity) {
        geometryDirty = true;
    }
    isDragging = false;
    isDraggingObstacle = false;
    isDraggingCity = false;
}

// ═══════════════════════════════════════════════════════════════
// DRAG
// ═══════════════════════════════════════════════════════════════

void MapEditor::HandleDrag(const glm::vec3& worldPos, CampaignMap& map) {
    if (!isActive) return;
    glm::vec3 newPos(worldPos.x, 0, worldPos.z);

    // Province vertex drag
    if (isDragging && selectedProvinceIdx >= 0 && selectedVertexIdx >= 0) {
        auto& provs = map.GetProvincesEditable();
        if (selectedProvinceIdx < (int)provs.size()) {
            Province& p = provs[selectedProvinceIdx];
            if (selectedVertexIdx < (int)p.borderVertices.size()) {
                glm::vec3 oldPos = p.borderVertices[selectedVertexIdx];
                MoveSharedVertices(map, oldPos, newPos);
            }
        }
    }

    // Obstacle vertex drag
    if (isDraggingObstacle && selectedObstacleIdx >= 0 && selectedObsVertexIdx >= 0) {
        auto& obs = map.GetObstaclesEditable();
        if (selectedObstacleIdx < (int)obs.size()) {
            auto& ob = obs[selectedObstacleIdx];
            if (selectedObsVertexIdx < (int)ob.vertices.size()) {
                glm::vec3 oldPos = ob.vertices[selectedObsVertexIdx];
                MoveSharedVertices(map, oldPos, newPos);
            }
        }
    }

    // City drag
    if (isDraggingCity && draggingCityProvIdx >= 0) {
        auto& provs = map.GetProvincesEditable();
        if (draggingCityProvIdx < (int)provs.size()) {
            provs[draggingCityProvIdx].cityPos = newPos;
        }
    }
}

// ═══════════════════════════════════════════════════════════════
// RIGHT CLICK
// ═══════════════════════════════════════════════════════════════

void MapEditor::HandleRightClick(const glm::vec3& worldPos, CampaignMap& map) {
    if (!isActive) return;

    if (currentTool == Tool::DELETE_VERTEX) {
        // Try province vertex
        auto hit = FindNearestVertex(worldPos, map, 0.4f);
        if (hit.provIdx >= 0) {
            auto& provs = map.GetProvincesEditable();
            Province& p = provs[hit.provIdx];
            if (p.borderVertices.size() > 3 && hit.vertIdx < (int)p.borderVertices.size()) {
                Logger::Info("Editor: Deleted vertex %d from %s", hit.vertIdx, p.name.c_str());
                p.borderVertices.erase(p.borderVertices.begin() + hit.vertIdx);
                glm::vec3 c(0);
                for (auto& v : p.borderVertices) c += v;
                p.center = c / (float)p.borderVertices.size();
                selectedVertexIdx = -1;
                geometryDirty = true;
            }
            return;
        }
        // Try obstacle vertex
        auto obsHit = FindNearestObstacleVertex(worldPos, map, 0.4f);
        if (obsHit.obsIdx >= 0) {
            auto& obs = map.GetObstaclesEditable();
            auto& ob = obs[obsHit.obsIdx];
            if (ob.vertices.size() > 3 && obsHit.vertIdx < (int)ob.vertices.size()) {
                Logger::Info("Editor: Deleted obstacle vertex %d from %s", obsHit.vertIdx, ob.name.c_str());
                ob.vertices.erase(ob.vertices.begin() + obsHit.vertIdx);
                glm::vec3 c(0);
                for (auto& v : ob.vertices) c += v;
                ob.center = c / (float)ob.vertices.size();
                selectedObsVertexIdx = -1;
                geometryDirty = true;
            }
        }
    }
}

// ═══════════════════════════════════════════════════════════════
// KEY PRESS
// ═══════════════════════════════════════════════════════════════

void MapEditor::HandleKeyPress(int key, CampaignMap& map, Renderer& renderer) {
    if (!isActive) return;

    if (key == GLFW_KEY_1) { currentTool = Tool::SELECT; Logger::Info("Editor: SELECT"); }
    if (key == GLFW_KEY_2) { currentTool = Tool::MOVE_VERTEX; Logger::Info("Editor: MOVE VERTEX"); }
    if (key == GLFW_KEY_3) { currentTool = Tool::ADD_VERTEX; Logger::Info("Editor: ADD VERTEX"); }
    if (key == GLFW_KEY_4) { currentTool = Tool::DELETE_VERTEX; Logger::Info("Editor: DELETE VERTEX"); }
    if (key == GLFW_KEY_5) { currentTool = Tool::MOVE_CITY; Logger::Info("Editor: MOVE CITY"); }

    // Save
    if (key == GLFW_KEY_F5) {
        if (geometryDirty) RebuildGeometry(map, renderer);
        MapSerializer::SaveToFile(map, "maps/europe_1700.json");
    }

    // Load
    if (key == GLFW_KEY_F8) {
        if (MapSerializer::LoadFromFile(map, "maps/europe_1700.json")) {
            RebuildGeometry(map, renderer);
            selectedProvinceIdx = -1;
            selectedVertexIdx = -1;
            selectedObstacleIdx = -1;
            selectedObsVertexIdx = -1;
        }
    }

    // R = rebuild geometry
    if (key == GLFW_KEY_R) {
        RebuildGeometry(map, renderer);
        geometryDirty = false;
        Logger::Info("Editor: Geometry rebuilt");
    }
}

void MapEditor::RebuildGeometry(CampaignMap& map, Renderer& renderer) {
    // Recompute all centers
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
    case Tool::MOVE_VERTEX: return "Move Vertex";
    case Tool::ADD_VERTEX: return "Add Vertex";
    case Tool::DELETE_VERTEX: return "Delete Vertex";
    case Tool::MOVE_CITY: return "Move City";
    }
    return "?";
}

std::string MapEditor::GetSelectionInfo(const CampaignMap& map) const {
    if (selectedProvinceIdx >= 0 && selectedProvinceIdx < (int)map.GetProvinces().size()) {
        auto& p = map.GetProvinces()[selectedProvinceIdx];
        std::string info = p.name + " (" + std::to_string(p.borderVertices.size()) + " verts)";
        if (selectedVertexIdx >= 0 && selectedVertexIdx < (int)p.borderVertices.size()) {
            auto& v = p.borderVertices[selectedVertexIdx];
            char buf[64];
            snprintf(buf, sizeof(buf), " V%d (%.1f, %.1f)", selectedVertexIdx, v.x, v.z);
            info += buf;
        }
        return info;
    }
    if (selectedObstacleIdx >= 0 && selectedObstacleIdx < (int)map.GetObstacles().size()) {
        auto& ob = map.GetObstacles()[selectedObstacleIdx];
        std::string info = ob.name + " [" + ob.type + "] (" + std::to_string(ob.vertices.size()) + " verts)";
        if (selectedObsVertexIdx >= 0) {
            auto& v = ob.vertices[selectedObsVertexIdx];
            char buf[64];
            snprintf(buf, sizeof(buf), " V%d (%.1f, %.1f)", selectedObsVertexIdx, v.x, v.z);
            info += buf;
        }
        return info;
    }
    return "nothing";
}

int MapEditor::GetSelectedProvinceId(const CampaignMap& map) const {
    if (selectedProvinceIdx >= 0 && selectedProvinceIdx < (int)map.GetProvinces().size())
        return map.GetProvinces()[selectedProvinceIdx].id;
    return -1;
}