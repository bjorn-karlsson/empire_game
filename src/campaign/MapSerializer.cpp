#include "campaign/MapSerializer.h"
#include "campaign/CampaignMap.h"
#include "campaign/Province.h"
#include "utils/Logger.h"
#include <nlohmann/json.hpp>
#include <fstream>

using json = nlohmann::json;

// ── Helper: vec3 <-> JSON ──
static json vec3ToJson(const glm::vec3& v) {
    return { {"x", v.x}, {"z", v.z} };
}
static glm::vec3 jsonToVec3(const json& j) {
    return glm::vec3(j.value("x", 0.0f), 0.0f, j.value("z", 0.0f));
}
static json color3ToJson(const glm::vec3& c) {
    return { {"r", c.r}, {"g", c.g}, {"b", c.b} };
}
static glm::vec3 jsonToColor3(const json& j) {
    return glm::vec3(j.value("r", 0.5f), j.value("g", 0.5f), j.value("b", 0.5f));
}

bool MapSerializer::SaveToFile(const CampaignMap& map, const std::string& path) {
    json root;
    root["version"] = 1;
    root["name"] = "Europe 1700";

    // ── Factions ──
    json factions = json::array();
    for (const auto& f : map.GetFactions()) {
        json fj;
        fj["id"] = f.id;
        fj["name"] = f.name;
        fj["leaderName"] = f.leaderName;
        fj["color"] = color3ToJson(f.color);
        fj["isPlayerControlled"] = f.isPlayerControlled;
        fj["treasury"] = f.treasury;
        json rels = json::array();
        for (const auto& r : f.relations) {
            json rj;
            rj["factionId"] = r.otherFactionId;    // was r.factionId
            rj["status"] = (int)r.status;
            rj["value"] = r.opinion;
            rels.push_back(rj);
        }
        fj["relations"] = rels;
        factions.push_back(fj);
    }
    root["factions"] = factions;

    // ── Provinces ──
    json provinces = json::array();
    for (const auto& p : map.GetProvinces()) {
        json pj;
        pj["id"] = p.id;
        pj["name"] = p.name;
        pj["cityName"] = p.cityName;
        pj["ownerFactionId"] = p.ownerFactionId;
        pj["terrain"] = p.terrain;
        pj["baseIncome"] = p.baseIncome;
        pj["isCoastal"] = p.isCoastal;
        pj["isCapital"] = p.isCapital;
        pj["color"] = color3ToJson(p.color);
        pj["cityPos"] = vec3ToJson(p.cityPos);

        json verts = json::array();
        for (const auto& v : p.borderVertices)
            verts.push_back(vec3ToJson(v));
        pj["vertices"] = verts;

        json neighbors = json::array();
        for (int n : p.neighborIds) neighbors.push_back(n);
        pj["neighbors"] = neighbors;

        json buildings = json::array();
        for (const auto& b : p.buildings) {
            buildings.push_back({
                {"name", b.name}, {"type", b.type}, {"level", b.level},
                {"incomeBonus", b.incomeBonus}, {"recruitSlots", b.recruitSlots}
                });
        }
        pj["buildings"] = buildings;

        provinces.push_back(pj);
    }
    root["provinces"] = provinces;

    // ── Obstacles ──
    json obstacles = json::array();
    for (const auto& ob : map.GetObstacles()) {
        json oj;
        oj["name"] = ob.name;
        oj["type"] = ob.type;
        oj["color"] = color3ToJson(ob.color);
        json verts = json::array();
        for (const auto& v : ob.vertices)
            verts.push_back(vec3ToJson(v));
        oj["vertices"] = verts;
        obstacles.push_back(oj);
    }
    root["obstacles"] = obstacles;


    // ── Armies ──
    json armies = json::array();
    for (const auto& a : map.GetArmies()) {
        json aj;
        aj["factionId"] = a.factionId;
        aj["generalName"] = a.generalName;
        aj["position"] = vec3ToJson(a.worldPosition);
        json units = json::array();
        for (const auto& u : a.units) {
            json uj;
            uj["type"] = (int)u.type;
            uj["name"] = u.name;
            uj["manpower"] = u.stats.manpower;
            uj["maxManpower"] = u.stats.maxManpower;
            uj["attack"] = u.stats.attack;
            uj["defense"] = u.stats.defense;
            uj["morale"] = u.stats.morale;
            uj["upkeep"] = u.stats.upkeep;
            units.push_back(uj);
        }
        aj["units"] = units;
        armies.push_back(aj);
    }
    root["armies"] = armies;

    // Write to file
    std::ofstream file(path);
    if (!file.is_open()) {
        Logger::Error("Failed to save map to: %s", path.c_str());
        return false;
    }
    file << root.dump(2);
    file.close();
    Logger::Info("Map saved to: %s", path.c_str());
    return true;
}

bool MapSerializer::LoadFromFile(CampaignMap& map, const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        Logger::Error("Failed to open map file: %s", path.c_str());
        return false;
    }

    json root;
    try {
        file >> root;
    }
    catch (const std::exception& e) {
        Logger::Error("JSON parse error: %s", e.what());
        return false;
    }
    file.close();

    // Clear existing data
    map.ClearAll();

    // ── Factions ──
    if (root.contains("factions")) {
        for (const auto& fj : root["factions"]) {
            Faction f;
            f.id = fj.value("id", "");
            f.name = fj.value("name", "");
            f.leaderName = fj.value("leaderName", "");
            f.color = jsonToColor3(fj["color"]);
            f.isPlayerControlled = fj.value("isPlayerControlled", false);
            f.treasury = fj.value("treasury", 5000);
            if (fj.contains("relations")) {
                for (const auto& rj : fj["relations"]) {
                    DiplomaticRelation r;
                    r.otherFactionId = rj.value("factionId", "");
                    r.status = (DiplomaticStatus)rj.value("status", 0);
                    r.opinion = rj.value("value", 0);
                    f.relations.push_back(r);
                }
            }
            map.AddFaction(f);
        }
    }

    // ── Provinces ──
    if (root.contains("provinces")) {
        for (const auto& pj : root["provinces"]) {
            Province p;
            p.id = pj.value("id", 0);
            p.name = pj.value("name", "");
            p.cityName = pj.value("cityName", "");
            p.ownerFactionId = pj.value("ownerFactionId", "");
            p.terrain = pj.value("terrain", "plains");
            p.baseIncome = pj.value("baseIncome", 100);
            p.isCoastal = pj.value("isCoastal", false);
            p.isCapital = pj.value("isCapital", false);
            p.color = jsonToColor3(pj["color"]);
            p.cityPos = jsonToVec3(pj["cityPos"]);
            p.population = p.baseIncome * 80 + 5000;

            for (const auto& vj : pj["vertices"])
                p.borderVertices.push_back(jsonToVec3(vj));

            // Compute center
            glm::vec3 c(0);
            for (auto& v : p.borderVertices) c += v;
            if (!p.borderVertices.empty()) c /= (float)p.borderVertices.size();
            p.center = c;

            for (const auto& n : pj["neighbors"])
                p.neighborIds.push_back(n.get<int>());

            if (pj.contains("buildings")) {
                for (const auto& bj : pj["buildings"]) {
                    Building b;
                    b.name = bj.value("name", "");
                    b.type = bj.value("type", "");
                    b.level = bj.value("level", 1);
                    b.incomeBonus = bj.value("incomeBonus", 0);
                    b.recruitSlots = bj.value("recruitSlots", 0);
                    p.buildings.push_back(b);
                }
            }

            map.AddProvince(p);
        }
    }

    // ── Obstacles ──
    if (root.contains("obstacles")) {
        for (const auto& oj : root["obstacles"]) {
            TerrainObstacle ob;
            ob.name = oj.value("name", "");
            ob.type = oj.value("type", "mountain");
            ob.color = jsonToColor3(oj["color"]);
            for (const auto& vj : oj["vertices"])
                ob.vertices.push_back(jsonToVec3(vj));
            glm::vec3 c(0);
            for (auto& v : ob.vertices) c += v;
            if (!ob.vertices.empty()) c /= (float)ob.vertices.size();
            ob.center = c;
            map.AddObstacle(ob);
        }
    }


    // ── Armies ──
    if (root.contains("armies")) {
        for (const auto& aj : root["armies"]) {
            std::string faction = aj.value("factionId", "");
            std::string general = aj.value("generalName", "");
            glm::vec3 pos = jsonToVec3(aj["position"]);

            Army a;
            a.id = map.GetNextArmyId();
            a.factionId = faction;
            a.generalName = general;
            a.worldPosition = pos;

            if (aj.contains("units")) {
                for (const auto& uj : aj["units"]) {
                    Unit u;
                    u.id = map.GetNextUnitId();
                    u.type = (UnitType)uj.value("type", 0);
                    u.name = uj.value("name", "");
                    u.stats.manpower = uj.value("manpower", 100);
                    u.stats.maxManpower = uj.value("maxManpower", 100);
                    u.stats.attack = uj.value("attack", 10);
                    u.stats.defense = uj.value("defense", 10);
                    u.stats.morale = uj.value("morale", 50);
                    u.stats.upkeep = uj.value("upkeep", 50);
                    a.units.push_back(u);
                }
            }

            map.AddArmy(a);
        }
    }

    // Setup ownership + nav grid
    map.FinalizeLoad();

    Logger::Info("Map loaded: %s (%d provinces, %d obstacles, %d armies)",
        path.c_str(),
        (int)map.GetProvinces().size(),
        (int)map.GetObstacles().size(),
        (int)map.GetArmies().size());
    return true;
}

bool MapSerializer::ExportCurrentMap(const CampaignMap& map, const std::string& path) {
    return SaveToFile(map, path);
}