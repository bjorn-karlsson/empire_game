#pragma once
#include <vector>
#include <glm/glm.hpp>

class HeightMap {
public:
    static constexpr int SIZE = 256;
    static constexpr float WORLD_MIN_X = -12.0f;
    static constexpr float WORLD_MIN_Z = -12.0f;
    static constexpr float WORLD_MAX_X = 16.0f;
    static constexpr float WORLD_MAX_Z = 16.0f;

    HeightMap();

    void Paint(float worldX, float worldZ, float radius, float strength);
    void Smooth(float worldX, float worldZ, float radius, float strength);
    void Flatten(float worldX, float worldZ, float radius, float targetH, float strength);
    float Sample(float worldX, float worldZ) const;
    void Clear();

    void UploadToGPU();
    unsigned int GetTextureID() const { return m_textureId; }
    bool IsDirty() const { return m_dirty; }

    std::vector<float>& GetData() { return m_data; }
    const std::vector<float>& GetData() const { return m_data; }

private:
    float toWorldX(int gx) const { return WORLD_MIN_X + (gx + 0.5f) / SIZE * (WORLD_MAX_X - WORLD_MIN_X); }
    float toWorldZ(int gz) const { return WORLD_MIN_Z + (gz + 0.5f) / SIZE * (WORLD_MAX_Z - WORLD_MIN_Z); }

    std::vector<float> m_data;
    unsigned int m_textureId = 0;
    bool m_dirty = true;
};