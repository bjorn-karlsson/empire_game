#pragma once

#include <string>
#include <vector>
#include <glm/glm.hpp>

// ─── Province ─────────────────────────────────────────────────
// A single region on the campaign map. Provinces are the atomic
// unit of territory — factions own provinces, armies move between
// them, and economies are calculated per-province.
//
// In Empire Total War, provinces contain buildings, populations,
// and produce income/units. We model this simply here.

struct Building {
    std::string name;
    std::string type;       // "barracks", "farm", "port", "fort", "university"
    int         level = 1;
    int         incomeBonus = 0;
    int         recruitSlots = 0;
};

struct Province {
    // Identity
    int          id = -1;
    std::string  name;
    std::string  ownerFactionId;  // which faction controls this

    // Map geometry (for rendering the province shape)
    std::vector<glm::vec3> borderVertices;  // polygon outline in world space
    glm::vec3              center = {0, 0, 0}; // centroid for label/icon placement
    glm::vec3              color  = {0.5f, 0.5f, 0.5f}; // faction color

    // City / Capital position (where the settlement icon sits)
    glm::vec3              cityPos = {0, 0, 0};
    std::string            cityName;   // e.g. "Paris", "Lyon"

    // Terrain
    std::string            terrain = "plains"; // plains, forest, hills, mountains, marsh

    // Economy
    int baseIncome     = 100;   // gold per turn from population
    int taxRate        = 50;    // 0-100%, affects income and public order
    int population     = 10000;
    float growthRate   = 0.02f; // per turn

    // Public order (like Empire TW's unrest system)
    int publicOrder    = 70;    // 0-100, below 30 = rebellion risk

    // Infrastructure
    std::vector<Building> buildings;

    // Adjacency (which provinces border this one)
    std::vector<int> neighborIds;

    // Gameplay state
    bool isCapital     = false;
    bool isCoastal     = false;
    bool isUnderSiege  = false;

    // ─── Computed values ──────────────────────────────────────
    int GetTotalIncome() const {
        int income = static_cast<int>(baseIncome * (taxRate / 100.0f));
        for (const auto& b : buildings)
            income += b.incomeBonus;
        return income;
    }

    int GetRecruitmentSlots() const {
        int slots = 0;
        for (const auto& b : buildings)
            slots += b.recruitSlots;
        return slots;
    }

    void UpdatePopulation() {
        population = static_cast<int>(population * (1.0f + growthRate));
    }

    void UpdatePublicOrder() {
        // High taxes reduce order, buildings can improve it
        int taxPenalty = (taxRate > 50) ? (taxRate - 50) / 5 : 0;
        publicOrder = glm::clamp(publicOrder - taxPenalty + 2, 0, 100);
    }
};
