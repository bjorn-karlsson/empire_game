#include "editor/MapEditor.h"
#include "campaign/CampaignMap.h"
#include "campaign/MapSerializer.h"
#include "rendering/Renderer.h"
#include "utils/Logger.h"
#include <GLFW/glfw3.h>

void MapEditor::HandleLeftClick(const glm::vec3& worldPos, CampaignMap& map) {
    if (!isActive) return;
    glm::vec2 click(worldPos.x, worldPos.z);

    if (currentTool == Tool::ADD_VERTEX && selectedProvinceIdx >= 0) {
        // Add a new vertex to the selected province
        auto& provs = map.GetProvincesEditable();
        if (selectedProvinceIdx < (int)provs.size()) {
            Province& p = provs[selectedProvinceIdx];
            // Insert after selectedVertexIdx (or at end)
            int insertAt = (selectedVertexIdx >= 0) ? selectedVertexIdx + 1 : (int)p.borderVertices.size();
            p.borderVertices.insert(p.borderVertices.begin() + insertAt,
                glm::vec3(worldPos.x, 0, worldPos.z));
            selectedVertexIdx = insertAt;

            // Recompute center
            glm::vec3 c(0);
            for (auto& v : p.borderVertices) c += v;
            p.center = c / (float)p.borderVertices.size();

            Logger::Info("Added vertex %d to %s at (%.1f, %.1f)",
                insertAt, p.name.c_str(), worldPos.x, worldPos.z);
        }
        return;
    }

    // Tool::SELECT — find nearest vertex across all provinces
    float bestDist = 0.5f; // max click distance
    int bestProv = -1, bestVert = -1;

    auto& provs = map.GetProvinces();
    for (int pi = 0; pi < (int)provs.size(); pi++) {
        for (int vi = 0; vi < (int)provs[pi].borderVertices.size(); vi++) {
            glm::vec2 vp(provs[pi].borderVertices[vi].x, provs[pi].borderVertices[vi].z);
            float d = glm::distance(click, vp);
            if (d < bestDist) {
                bestDist = d;
                bestProv = pi;
                bestVert = vi;
            }
        }
    }

    selectedProvinceIdx = bestProv;
    selectedVertexIdx = bestVert;
    isDragging = (bestProv >= 0);

    if (bestProv >= 0) {
        Logger::Info("Selected: %s vertex %d (%.1f, %.1f)",
            provs[bestProv].name.c_str(), bestVert,
            provs[bestProv].borderVertices[bestVert].x,
            provs[bestProv].borderVertices[bestVert].z);
    }
}

void MapEditor::HandleLeftRelease(CampaignMap& map) {
    if (isDragging) {
        isDragging = false;
        // Vertex was moved — center needs recomputing
        if (selectedProvinceIdx >= 0) {
            auto& provs = map.GetProvincesEditable();
            Province& p = provs[selectedProvinceIdx];
            glm::vec3 c(0);
            for (auto& v : p.borderVertices) c += v;
            p.center = c / (float)p.borderVertices.size();
        }
    }
}

void MapEditor::HandleDrag(const glm::vec3& worldPos, CampaignMap& map) {
    if (!isActive || !isDragging) return;
    if (selectedProvinceIdx < 0 || selectedVertexIdx < 0) return;

    auto& provs = map.GetProvincesEditable();
    if (selectedProvinceIdx >= (int)provs.size()) return;
    Province& p = provs[selectedProvinceIdx];
    if (selectedVertexIdx >= (int)p.borderVertices.size()) return;

    glm::vec3 oldPos = p.borderVertices[selectedVertexIdx];
    glm::vec3 newPos(worldPos.x, 0, worldPos.z);

    // Also move matching vertices in neighboring provinces (shared border points)
    for (auto& other : provs) {
        if (other.id == p.id) continue;
        for (auto& v : other.borderVertices) {
            if (glm::distance(glm::vec2(v.x, v.z), glm::vec2(oldPos.x, oldPos.z)) < 0.01f) {
                v = newPos;
            }
        }
    }

    // Move in obstacles too
    for (auto& ob : map.GetObstaclesEditable()) {
        for (auto& v : ob.vertices) {
            if (glm::distance(glm::vec2(v.x, v.z), glm::vec2(oldPos.x, oldPos.z)) < 0.01f) {
                v = newPos;
            }
        }
    }

    // Move in foreign territories
    for (auto& ft : map.GetForeignTerritoriesEditable()) {
        for (auto& v : ft.vertices) {
            if (glm::distance(glm::vec2(v.x, v.z), glm::vec2(oldPos.x, oldPos.z)) < 0.01f) {
                v = newPos;
            }
        }
    }

    p.borderVertices[selectedVertexIdx] = newPos;
}

void MapEditor::HandleRightClick(const glm::vec3& worldPos, CampaignMap& map) {
    if (!isActive) return;

    if (currentTool == Tool::DELETE_VERTEX && selectedProvinceIdx >= 0 && selectedVertexIdx >= 0) {
        auto& provs = map.GetProvincesEditable();
        if (selectedProvinceIdx < (int)provs.size()) {
            Province& p = provs[selectedProvinceIdx];
            if (p.borderVertices.size() > 3 && selectedVertexIdx < (int)p.borderVertices.size()) {
                Logger::Info("Deleted vertex %d from %s", selectedVertexIdx, p.name.c_str());
                p.borderVertices.erase(p.borderVertices.begin() + selectedVertexIdx);
                selectedVertexIdx = -1;

                glm::vec3 c(0);
                for (auto& v : p.borderVertices) c += v;
                p.center = c / (float)p.borderVertices.size();
            }
        }
    }
}

void MapEditor::HandleKeyPress(int key, CampaignMap& map, Renderer& renderer) {
    if (!isActive) return;

    if (key == GLFW_KEY_1) { currentTool = Tool::SELECT; Logger::Info("Editor: SELECT tool"); }
    if (key == GLFW_KEY_2) { currentTool = Tool::ADD_VERTEX; Logger::Info("Editor: ADD VERTEX tool"); }
    if (key == GLFW_KEY_3) { currentTool = Tool::DELETE_VERTEX; Logger::Info("Editor: DELETE VERTEX tool"); }

    // Ctrl+S = save
    if (key == GLFW_KEY_S) {
        MapSerializer::SaveToFile(map, "maps/europe_1700.json");
    }

    // Ctrl+L = load
    if (key == GLFW_KEY_L) {
        if (MapSerializer::LoadFromFile(map, "maps/europe_1700.json")) {
            RebuildGeometry(map, renderer);
        }
    }

    // R = rebuild GPU geometry (after dragging vertices)
    if (key == GLFW_KEY_R) {
        RebuildGeometry(map, renderer);
        Logger::Info("Editor: Geometry rebuilt");
    }
}

void MapEditor::RebuildGeometry(CampaignMap& map, Renderer& renderer) {
    renderer.ClearMapGeometry();
    renderer.BuildMapGeometry(map);
    map.BuildNavGrid();
}

std::string MapEditor::GetStatusText() const {
    std::string tool = "SELECT";
    if (currentTool == Tool::ADD_VERTEX) tool = "ADD VERTEX";
    if (currentTool == Tool::DELETE_VERTEX) tool = "DELETE";
    std::string sel = (selectedProvinceIdx >= 0) ?
        "P" + std::to_string(selectedProvinceIdx) + " V" + std::to_string(selectedVertexIdx) : "none";
    return "EDITOR [" + tool + "] Sel:" + sel + " | 1/2/3=tool S=save L=load R=rebuild";
}