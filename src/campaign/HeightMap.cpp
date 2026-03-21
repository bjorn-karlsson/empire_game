#include <glad/glad.h>
#include "campaign/HeightMap.h"
#include <cmath>
#include <algorithm>

HeightMap::HeightMap() : m_data(SIZE* SIZE, 0.0f) {}

void HeightMap::Paint(float worldX, float worldZ, float radius, float strength) {
    float cellW = (WORLD_MAX_X - WORLD_MIN_X) / SIZE;
    int gridRadius = (int)(radius / cellW) + 1;
    int cx = glm::clamp((int)((worldX - WORLD_MIN_X) / cellW), 0, SIZE - 1);
    int cz = glm::clamp((int)((worldZ - WORLD_MIN_Z) / cellW), 0, SIZE - 1);

    for (int gz = cz - gridRadius; gz <= cz + gridRadius; gz++) {
        for (int gx = cx - gridRadius; gx <= cx + gridRadius; gx++) {
            if (gx < 0 || gx >= SIZE || gz < 0 || gz >= SIZE) continue;
            float wx = toWorldX(gx), wz = toWorldZ(gz);
            float dist = std::sqrt((wx - worldX) * (wx - worldX) + (wz - worldZ) * (wz - worldZ));
            if (dist > radius) continue;
            float falloff = 1.0f - (dist / radius);
            falloff = falloff * falloff;
            m_data[gz * SIZE + gx] += strength * falloff;
            m_data[gz * SIZE + gx] = glm::clamp(m_data[gz * SIZE + gx], -2.0f, 3.0f);
        }
    }
    m_dirty = true;
}

void HeightMap::Smooth(float worldX, float worldZ, float radius, float strength) {
    float cellW = (WORLD_MAX_X - WORLD_MIN_X) / SIZE;
    int gridRadius = (int)(radius / cellW) + 1;
    int cx = glm::clamp((int)((worldX - WORLD_MIN_X) / cellW), 0, SIZE - 1);
    int cz = glm::clamp((int)((worldZ - WORLD_MIN_Z) / cellW), 0, SIZE - 1);

    std::vector<float> temp = m_data;
    for (int gz = cz - gridRadius; gz <= cz + gridRadius; gz++) {
        for (int gx = cx - gridRadius; gx <= cx + gridRadius; gx++) {
            if (gx < 1 || gx >= SIZE - 1 || gz < 1 || gz >= SIZE - 1) continue;
            float wx = toWorldX(gx), wz = toWorldZ(gz);
            float dist = std::sqrt((wx - worldX) * (wx - worldX) + (wz - worldZ) * (wz - worldZ));
            if (dist > radius) continue;
            float falloff = 1.0f - (dist / radius);
            float avg = (temp[(gz - 1) * SIZE + gx] + temp[(gz + 1) * SIZE + gx] +
                temp[gz * SIZE + gx - 1] + temp[gz * SIZE + gx + 1]) * 0.25f;
            m_data[gz * SIZE + gx] = glm::mix(m_data[gz * SIZE + gx], avg, strength * falloff);
        }
    }
    m_dirty = true;
}

void HeightMap::Flatten(float worldX, float worldZ, float radius, float targetH, float strength) {
    float cellW = (WORLD_MAX_X - WORLD_MIN_X) / SIZE;
    int gridRadius = (int)(radius / cellW) + 1;
    int cx = glm::clamp((int)((worldX - WORLD_MIN_X) / cellW), 0, SIZE - 1);
    int cz = glm::clamp((int)((worldZ - WORLD_MIN_Z) / cellW), 0, SIZE - 1);

    for (int gz = cz - gridRadius; gz <= cz + gridRadius; gz++) {
        for (int gx = cx - gridRadius; gx <= cx + gridRadius; gx++) {
            if (gx < 0 || gx >= SIZE || gz < 0 || gz >= SIZE) continue;
            float wx = toWorldX(gx), wz = toWorldZ(gz);
            float dist = std::sqrt((wx - worldX) * (wx - worldX) + (wz - worldZ) * (wz - worldZ));
            if (dist > radius) continue;
            float falloff = 1.0f - (dist / radius);
            falloff = falloff * falloff;
            m_data[gz * SIZE + gx] = glm::mix(m_data[gz * SIZE + gx], targetH, strength * falloff);
        }
    }
    m_dirty = true;
}

float HeightMap::Sample(float worldX, float worldZ) const {
    float u = (worldX - WORLD_MIN_X) / (WORLD_MAX_X - WORLD_MIN_X) * SIZE;
    float v = (worldZ - WORLD_MIN_Z) / (WORLD_MAX_Z - WORLD_MIN_Z) * SIZE;
    int x0 = glm::clamp((int)u, 0, SIZE - 2);
    int z0 = glm::clamp((int)v, 0, SIZE - 2);
    float fx = u - x0, fz = v - z0;
    float a = m_data[z0 * SIZE + x0];
    float b = m_data[z0 * SIZE + x0 + 1];
    float c = m_data[(z0 + 1) * SIZE + x0];
    float d = m_data[(z0 + 1) * SIZE + x0 + 1];
    return glm::mix(glm::mix(a, b, fx), glm::mix(c, d, fx), fz);
}

void HeightMap::Clear() {
    std::fill(m_data.begin(), m_data.end(), 0.0f);
    m_dirty = true;
}

void HeightMap::UploadToGPU() {
    if (!m_textureId) {
        glGenTextures(1, &m_textureId);
        glBindTexture(GL_TEXTURE_2D, m_textureId);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    glBindTexture(GL_TEXTURE_2D, m_textureId);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, SIZE, SIZE, 0, GL_RED, GL_FLOAT, m_data.data());
    m_dirty = false;
}