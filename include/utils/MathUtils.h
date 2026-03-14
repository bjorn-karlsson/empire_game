#pragma once

#include <glm/glm.hpp>
#include <vector>

// ─── Math Utilities ───────────────────────────────────────────
namespace MathUtils {

    // Point-in-polygon test (for clicking on provinces)
    bool PointInPolygon(const glm::vec2& point,
                        const std::vector<glm::vec2>& polygon);

    // Line segment intersection (for pathfinding / borders)
    bool LineSegmentIntersect(const glm::vec2& p1, const glm::vec2& p2,
                              const glm::vec2& p3, const glm::vec2& p4);

    // Distance from point to line segment
    float PointToSegmentDistance(const glm::vec2& point,
                                 const glm::vec2& segA,
                                 const glm::vec2& segB);

    // Random float in range
    float RandomFloat(float min, float max);

    // Random int in range (inclusive)
    int RandomInt(int min, int max);

    // Smooth interpolation (ease in/out)
    float SmoothStep(float edge0, float edge1, float x);
}
