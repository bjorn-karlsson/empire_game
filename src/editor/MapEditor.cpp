#include "editor/MapEditor.h"
#include "campaign/CampaignMap.h"
#include "campaign/MapSerializer.h"
#include "rendering/Renderer.h"
#include "utils/Logger.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cstdio>

void MapEditor::SetScreenInfo(const glm::mat4& vp, float sw, float sh) { m_vpMatrix = vp; m_screenW = sw; m_screenH = sh; }
glm::vec2 MapEditor::WorldToScreen(const glm::vec3& wp) const {
    glm::vec4 c = m_vpMatrix * glm::vec4(wp, 1.0f);
    if (c.w <= 0.001f) return { -9999, -9999 };
    glm::vec3 n = glm::vec3(c) / c.w;
    return { (n.x * 0.5f + 0.5f) * m_screenW, (1.0f - (n.y * 0.5f + 0.5f)) * m_screenH };
}

MapEditor::HitResult MapEditor::FindNearestVertex(const glm::vec2& mp, const CampaignMap& map, float maxPx) {
    HitResult best; auto& ps = map.GetProvinces();
    for (int pi = 0; pi < (int)ps.size(); pi++) for (int vi = 0; vi < (int)ps[pi].borderVertices.size(); vi++) {
        auto& v = ps[pi].borderVertices[vi]; float th = map.GetTerrainHeight(v.x, v.z);
        float d = glm::distance(mp, WorldToScreen({ v.x, th + 0.2f, v.z }));
        if (d < best.dist && d < maxPx) best = { pi, vi, d };
    } return best;
}
MapEditor::ObsHitResult MapEditor::FindNearestObstacleVertex(const glm::vec2& mp, const CampaignMap& map, float maxPx) {
    ObsHitResult best; auto& os = map.GetObstacles();
    for (int oi = 0; oi < (int)os.size(); oi++) for (int vi = 0; vi < (int)os[oi].vertices.size(); vi++) {
        auto& v = os[oi].vertices[vi]; float th = map.GetTerrainHeight(v.x, v.z);
        float d = glm::distance(mp, WorldToScreen({ v.x, th + 0.2f, v.z }));
        if (d < best.dist && d < maxPx) best = { oi, vi, d };
    } return best;
}
int MapEditor::FindNearestCity(const glm::vec2& mp, const CampaignMap& map, float maxPx) {
    float bd = maxPx; int bi = -1;
    for (int i = 0; i < (int)map.GetProvinces().size(); i++) {
        auto& cp = map.GetProvinces()[i].cityPos; float th = map.GetTerrainHeight(cp.x, cp.z);
        float d = glm::distance(mp, WorldToScreen({ cp.x, th + 0.2f, cp.z }));
        if (d < bd) { bd = d; bi = i; }
    } return bi;
}

void MapEditor::MoveSharedVertices(CampaignMap& map, glm::vec3 op, glm::vec3 np) {
    float t = 0.01f; glm::vec2 o2(op.x, op.z);
    bool isProv = (selectedProvinceIdx >= 0 && selectedVertexIdx >= 0);
    if (isProv) {
        for (auto& p : map.GetProvincesEditable()) for (auto& v : p.borderVertices)
            if (glm::distance(glm::vec2(v.x, v.z), o2) < t) v = np;
    }
    else {
        for (auto& ob : map.GetObstaclesEditable()) for (auto& v : ob.vertices)
            if (glm::distance(glm::vec2(v.x, v.z), o2) < t) v = np;
    }
}

// ── Undo/Redo ──
MapEditor::MapSnapshot MapEditor::CaptureSnapshot(const CampaignMap& map) {
    MapSnapshot s;
    for (auto& p : map.GetProvinces()) s.provinces.push_back({ p.borderVertices, p.cityPos });
    for (auto& o : map.GetObstacles()) s.obstacles.push_back({ o.vertices });
    s.heightData = map.GetHeightMap().GetData();
    return s;
}
void MapEditor::ApplySnapshot(const MapSnapshot& s, CampaignMap& map) {
    auto& ps = map.GetProvincesEditable();
    for (int i = 0; i < (int)s.provinces.size() && i < (int)ps.size(); i++) {
        ps[i].borderVertices = s.provinces[i].verts; ps[i].cityPos = s.provinces[i].cityPos;
        glm::vec3 c(0); for (auto& v : ps[i].borderVertices) c += v;
        if (!ps[i].borderVertices.empty()) ps[i].center = c / (float)ps[i].borderVertices.size();
    }
    auto& os = map.GetObstaclesEditable();
    for (int i = 0; i < (int)s.obstacles.size() && i < (int)os.size(); i++) {
        os[i].vertices = s.obstacles[i].verts; glm::vec3 c(0);
        for (auto& v : os[i].vertices) c += v;
        if (!os[i].vertices.empty()) os[i].center = c / (float)os[i].vertices.size();
    }
    if (!s.heightData.empty()) { map.GetHeightMap().GetData() = s.heightData; map.GetHeightMap().UploadToGPU(); }
}
void MapEditor::SaveUndoState(const CampaignMap& m) {
    m_undoStack.push_back(CaptureSnapshot(m));
    if ((int)m_undoStack.size() > MAX_UNDO) m_undoStack.pop_front(); m_redoStack.clear();
}
void MapEditor::Undo(CampaignMap& m, Renderer& r) {
    if (m_undoStack.empty()) return; m_redoStack.push_back(CaptureSnapshot(m));
    ApplySnapshot(m_undoStack.back(), m); m_undoStack.pop_back(); RebuildGeometry(m, r);
}
void MapEditor::Redo(CampaignMap& m, Renderer& r) {
    if (m_redoStack.empty()) return; m_undoStack.push_back(CaptureSnapshot(m));
    ApplySnapshot(m_redoStack.back(), m); m_redoStack.pop_back(); RebuildGeometry(m, r);
}

void MapEditor::HandleMouseMove(const glm::vec2& mp, const CampaignMap& map) {
    if (!isActive || isDragging || isDraggingCity || isPainting) return;
    auto h = FindNearestVertex(mp, map, 18.0f); hoverProvinceIdx = h.provIdx; hoverVertexIdx = h.vertIdx;
    auto oh = FindNearestObstacleVertex(mp, map, 18.0f); hoverObstacleIdx = oh.obsIdx; hoverObsVertexIdx = oh.vertIdx;
}

void MapEditor::PaintHeight(const glm::vec3& wp, CampaignMap& map, bool raise) {
    auto& hm = map.GetHeightMap(); float s = brushStrength * (raise ? 1.0f : -1.0f);
    if (brushMode == BrushMode::RAISE || brushMode == BrushMode::LOWER) hm.Paint(wp.x, wp.z, brushRadius, s);
    else if (brushMode == BrushMode::SMOOTH) hm.Smooth(wp.x, wp.z, brushRadius, brushStrength * 0.5f);
    else hm.Flatten(wp.x, wp.z, brushRadius, 0.0f, brushStrength);
}
void MapEditor::HandleScrollInEditor(float sc) {
    if (currentTool == Tool::HEIGHT_BRUSH) { brushRadius += sc * 0.15f; brushRadius = glm::clamp(brushRadius, 0.2f, 5.0f); }
}

// ── Left Click ──
void MapEditor::HandleLeftClick(const glm::vec3& wp, const glm::vec2& mp, CampaignMap& map) {
    if (!isActive) return;

    // LINK VERTEX
    if (currentTool == Tool::LINK_VERTEX) {
        auto h = FindNearestVertex(mp, map, 25.0f);
        auto oh = FindNearestObstacleVertex(mp, map, 25.0f);
        bool ip = (h.dist < oh.dist && h.provIdx >= 0);
        bool io = (!ip && oh.obsIdx >= 0);
        if (ip || io) {
            if (linkSourceProvIdx < 0 && linkSourceObsIdx < 0) {
                if (ip) {
                    linkSourceProvIdx = h.provIdx; linkSourceVertIdx = h.vertIdx; linkSourceObsIdx = -1;
                    selectedProvinceIdx = h.provIdx; selectedVertexIdx = h.vertIdx;
                }
                else {
                    linkSourceObsIdx = oh.obsIdx; linkSourceObsVertIdx = oh.vertIdx; linkSourceProvIdx = -1;
                    selectedObstacleIdx = oh.obsIdx; selectedObsVertexIdx = oh.vertIdx;
                }
                Logger::Info("Editor: Link source set");
            }
            else {
                SaveUndoState(map);
                glm::vec3 src = (linkSourceProvIdx >= 0) ?
                    map.GetProvinces()[linkSourceProvIdx].borderVertices[linkSourceVertIdx] :
                    map.GetObstacles()[linkSourceObsIdx].vertices[linkSourceObsVertIdx];
                if (ip) {
                    auto& ps = map.GetProvincesEditable(); ps[h.provIdx].borderVertices[h.vertIdx] = src;
                    glm::vec3 c(0); for (auto& v : ps[h.provIdx].borderVertices)c += v;
                    ps[h.provIdx].center = c / (float)ps[h.provIdx].borderVertices.size();
                }
                else { map.GetObstaclesEditable()[oh.obsIdx].vertices[oh.vertIdx] = src; }
                geometryDirty = true;
                linkSourceProvIdx = -1; linkSourceVertIdx = -1; linkSourceObsIdx = -1; linkSourceObsVertIdx = -1;
                Logger::Info("Editor: Vertices linked!");
            }
        }
        return;
    }

    // ADD VERTEX
    if (currentTool == Tool::ADD_VERTEX && selectedProvinceIdx >= 0) {
        auto& ps = map.GetProvincesEditable();
        if (selectedProvinceIdx < (int)ps.size()) {
            SaveUndoState(map);
            Province& p = ps[selectedProvinceIdx]; glm::vec2 ck(wp.x, wp.z);
            float bd = 999; int be = -1; int n = (int)p.borderVertices.size();
            for (int i = 0; i < n; i++) {
                glm::vec2 a(p.borderVertices[i].x, p.borderVertices[i].z);
                glm::vec2 b(p.borderVertices[(i + 1) % n].x, p.borderVertices[(i + 1) % n].z);
                glm::vec2 ab = b - a; float l2 = glm::dot(ab, ab);
                if (l2 < 0.0001f) continue;
                float t = glm::clamp(glm::dot(ck - a, ab) / l2, 0.f, 1.f);
                float d = glm::distance(ck, a + t * ab);
                if (d < bd) { bd = d; be = i; }
            }
            int at = (be >= 0) ? be + 1 : n;
            p.borderVertices.insert(p.borderVertices.begin() + at, glm::vec3(wp.x, 0, wp.z));
            selectedVertexIdx = at;
            glm::vec3 c(0); for (auto& v : p.borderVertices) c += v;
            p.center = c / (float)p.borderVertices.size();
            geometryDirty = true;
        }
        return;
    }

    if (currentTool == Tool::HEIGHT_BRUSH) return;

    // SELECT
    auto h = FindNearestVertex(mp, map, 18.0f);
    auto oh = FindNearestObstacleVertex(mp, map, 18.0f);
    if (h.dist < oh.dist && h.provIdx >= 0) {
        selectedProvinceIdx = h.provIdx; selectedVertexIdx = h.vertIdx;
        selectedObstacleIdx = -1; selectedObsVertexIdx = -1;
    }
    else if (oh.obsIdx >= 0) {
        selectedObstacleIdx = oh.obsIdx; selectedObsVertexIdx = oh.vertIdx;
        selectedProvinceIdx = -1; selectedVertexIdx = -1;
    }
    else {
        auto* pv = map.GetProvinceAtWorldPos(wp);
        if (pv) {
            for (int i = 0; i < (int)map.GetProvinces().size(); i++)
                if (map.GetProvinces()[i].id == pv->id) { selectedProvinceIdx = i; break; }
            selectedVertexIdx = -1;
        }
        else { selectedProvinceIdx = -1; selectedVertexIdx = -1; }
        selectedObstacleIdx = -1; selectedObsVertexIdx = -1;
    }
}

// ── Right mouse: drag ──
void MapEditor::HandleRightPress(const glm::vec3& wp, const glm::vec2& mp, CampaignMap& map) {
    if (!isActive) return;
    if (currentTool == Tool::MOVE_CITY) {
        int ci = FindNearestCity(mp, map, 22.0f);
        if (ci >= 0) { SaveUndoState(map); draggingCityProvIdx = ci; isDraggingCity = true; }
        return;
    }
    if ((selectedProvinceIdx >= 0 && selectedVertexIdx >= 0) ||
        (selectedObstacleIdx >= 0 && selectedObsVertexIdx >= 0)) {
        SaveUndoState(map); isDragging = true;
    }
}
void MapEditor::HandleRightDrag(const glm::vec3& wp, CampaignMap& map) {
    if (!isActive) return;
    glm::vec3 np(wp.x, 0, wp.z);
    if (isDragging) {
        if (selectedProvinceIdx >= 0 && selectedVertexIdx >= 0) {
            auto& p = map.GetProvincesEditable()[selectedProvinceIdx];
            if (selectedVertexIdx < (int)p.borderVertices.size())
                MoveSharedVertices(map, p.borderVertices[selectedVertexIdx], np);
        }
        else if (selectedObstacleIdx >= 0 && selectedObsVertexIdx >= 0) {
            auto& ob = map.GetObstaclesEditable()[selectedObstacleIdx];
            if (selectedObsVertexIdx < (int)ob.vertices.size())
                MoveSharedVertices(map, ob.vertices[selectedObsVertexIdx], np);
        }
    }
    if (isDraggingCity && draggingCityProvIdx >= 0)
        map.GetProvincesEditable()[draggingCityProvIdx].cityPos = np;
}
void MapEditor::HandleRightRelease(CampaignMap& map) {
    if (isDragging) {
        if (selectedProvinceIdx >= 0) {
            auto& p = map.GetProvincesEditable()[selectedProvinceIdx];
            glm::vec3 c(0); for (auto& v : p.borderVertices) c += v;
            p.center = c / (float)p.borderVertices.size();
        }
        if (selectedObstacleIdx >= 0) {
            auto& ob = map.GetObstaclesEditable()[selectedObstacleIdx];
            glm::vec3 c(0); for (auto& v : ob.vertices) c += v;
            ob.center = c / (float)ob.vertices.size();
        }
        geometryDirty = true;
    }
    if (isDraggingCity) geometryDirty = true;
    isDragging = false; isDraggingCity = false;
}

void MapEditor::DeleteSelectedVertex(CampaignMap& map) {
    if (selectedProvinceIdx >= 0 && selectedVertexIdx >= 0) {
        auto& p = map.GetProvincesEditable()[selectedProvinceIdx];
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
        auto& ob = map.GetObstaclesEditable()[selectedObstacleIdx];
        if (ob.vertices.size() > 3 && selectedObsVertexIdx < (int)ob.vertices.size()) {
            SaveUndoState(map);
            ob.vertices.erase(ob.vertices.begin() + selectedObsVertexIdx);
            glm::vec3 c(0); for (auto& v : ob.vertices) c += v;
            ob.center = c / (float)ob.vertices.size();
            selectedObsVertexIdx = -1; geometryDirty = true;
        }
    }
}

// ── Keys ──
void MapEditor::HandleKeyPress(int key, bool ctrl, CampaignMap& map, Renderer& rend) {
    if (!isActive) return;

    if (!ctrl) {
        if (key == GLFW_KEY_1) { currentTool = Tool::SELECT; linkSourceProvIdx = -1; linkSourceObsIdx = -1; }
        if (key == GLFW_KEY_2) currentTool = Tool::ADD_VERTEX;
        if (key == GLFW_KEY_3) { currentTool = Tool::LINK_VERTEX; linkSourceProvIdx = -1; linkSourceObsIdx = -1; }
        if (key == GLFW_KEY_4) currentTool = Tool::HEIGHT_BRUSH;
        if (key == GLFW_KEY_5) currentTool = Tool::MOVE_CITY;

        // Del = delete selected vertex only
        if (key == GLFW_KEY_DELETE) {
            DeleteSelectedVertex(map);
            if (geometryDirty) RebuildGeometry(map, rend);
        }

        if (key == GLFW_KEY_R) RebuildGeometry(map, rend);

        // Height brush sub-modes
        if (currentTool == Tool::HEIGHT_BRUSH) {
            if (key == GLFW_KEY_Q) { brushMode = BrushMode::RAISE; Logger::Info("Brush: Raise"); }
            if (key == GLFW_KEY_G) { brushMode = BrushMode::SMOOTH; Logger::Info("Brush: Smooth"); }
            if (key == GLFW_KEY_F) { brushMode = BrushMode::FLATTEN; Logger::Info("Brush: Flatten"); }
        }
    }

    if (ctrl) {
        if (key == GLFW_KEY_Z) Undo(map, rend);
        if (key == GLFW_KEY_Y) Redo(map, rend);
        if (key == GLFW_KEY_S) {
            if (geometryDirty) RebuildGeometry(map, rend);
            MapSerializer::SaveToFile(map, "maps/europe_1700.json");
            Logger::Info("Editor: Map saved!");
        }
        if (key == GLFW_KEY_L) {
            if (MapSerializer::LoadFromFile(map, "maps/europe_1700.json")) {
                RebuildGeometry(map, rend);
                selectedProvinceIdx = -1; selectedVertexIdx = -1;
                selectedObstacleIdx = -1; selectedObsVertexIdx = -1;
                m_undoStack.clear(); m_redoStack.clear();
                Logger::Info("Editor: Map loaded!");
            }
        }

        // Ctrl+N = new province at mouse cursor
        if (key == GLFW_KEY_N) {
            SaveUndoState(map);
            float mx = mouseWorldPos.x, mz = mouseWorldPos.z;
            float sz = 0.8f; // half-size of default square
            std::vector<glm::vec3> vs = {
                {mx - sz, 0, mz - sz}, {mx + sz, 0, mz - sz},
                {mx + sz, 0, mz + sz}, {mx - sz, 0, mz + sz}
            };
            map.CreateProvince("New Province", "New City", "france", vs);
            selectedProvinceIdx = (int)map.GetProvinces().size() - 1;
            selectedVertexIdx = -1;
            selectedObstacleIdx = -1; selectedObsVertexIdx = -1;
            geometryDirty = true;
            RebuildGeometry(map, rend);
            Logger::Info("Editor: Created province at (%.1f, %.1f)", mx, mz);
        }

        // Ctrl+Delete = delete entire selected object (province OR obstacle/river)
        if (key == GLFW_KEY_DELETE) {
            if (selectedProvinceIdx >= 0) {
                SaveUndoState(map);
                Logger::Info("Editor: Deleting province: %s",
                    map.GetProvinces()[selectedProvinceIdx].name.c_str());
                map.DeleteProvince(selectedProvinceIdx);
                selectedProvinceIdx = -1; selectedVertexIdx = -1;
                geometryDirty = true;
                RebuildGeometry(map, rend);
            }
            else if (selectedObstacleIdx >= 0) {
                SaveUndoState(map);
                Logger::Info("Editor: Deleting obstacle: %s",
                    map.GetObstacles()[selectedObstacleIdx].name.c_str());
                map.DeleteObstacle(selectedObstacleIdx);
                selectedObstacleIdx = -1; selectedObsVertexIdx = -1;
                geometryDirty = true;
                RebuildGeometry(map, rend);
            }
        }
    }
}

void MapEditor::RebuildGeometry(CampaignMap& map, Renderer& rend) {
    for (auto& p : map.GetProvincesEditable()) {
        glm::vec3 c(0); for (auto& v : p.borderVertices) c += v;
        if (!p.borderVertices.empty()) p.center = c / (float)p.borderVertices.size();
    }
    for (auto& ob : map.GetObstaclesEditable()) {
        glm::vec3 c(0); for (auto& v : ob.vertices) c += v;
        if (!ob.vertices.empty()) ob.center = c / (float)ob.vertices.size();
    }
    rend.ClearMapGeometry(); rend.BuildMapGeometry(map);
    map.BuildNavGrid(); geometryDirty = false;
}

std::string MapEditor::GetToolName() const {
    switch (currentTool) {
    case Tool::SELECT: return "Select";
    case Tool::ADD_VERTEX: return "Add Vertex";
    case Tool::MOVE_CITY: return "Move City";
    case Tool::LINK_VERTEX:
        return (linkSourceProvIdx >= 0 || linkSourceObsIdx >= 0) ? "Link (click target)" : "Link (click source)";
    case Tool::HEIGHT_BRUSH: {
        char b[64]; const char* m = "Raise";
        if (brushMode == BrushMode::SMOOTH) m = "Smooth";
        if (brushMode == BrushMode::FLATTEN) m = "Flatten";
        snprintf(b, sizeof(b), "Brush [%s] R:%.1f", m, brushRadius); return b;
    }
    } return "?";
}

std::string MapEditor::GetSelectionInfo(const CampaignMap& map) const {
    if (selectedProvinceIdx >= 0 && selectedProvinceIdx < (int)map.GetProvinces().size()) {
        auto& p = map.GetProvinces()[selectedProvinceIdx];
        std::string info = p.name + " [" + p.ownerFactionId + "] (" + std::to_string(p.borderVertices.size()) + "v)";
        if (selectedVertexIdx >= 0 && selectedVertexIdx < (int)p.borderVertices.size()) {
            auto& v = p.borderVertices[selectedVertexIdx];
            char b[64]; snprintf(b, sizeof(b), " V%d(%.1f,%.1f)", selectedVertexIdx, v.x, v.z);
            info += b;
        }
        return info;
    }
    if (selectedObstacleIdx >= 0 && selectedObstacleIdx < (int)map.GetObstacles().size()) {
        auto& ob = map.GetObstacles()[selectedObstacleIdx];
        std::string info = ob.name + " [" + ob.type + "] (" + std::to_string(ob.vertices.size()) + "v)";
        if (selectedObsVertexIdx >= 0 && selectedObsVertexIdx < (int)ob.vertices.size()) {
            auto& v = ob.vertices[selectedObsVertexIdx];
            char b[64]; snprintf(b, sizeof(b), " V%d(%.1f,%.1f)", selectedObsVertexIdx, v.x, v.z);
            info += b;
        }
        return info;
    }
    return "nothing";
}