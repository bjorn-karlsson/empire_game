#pragma once

#include <string>
#include <unordered_map>
#include <memory>

class Shader;

// ─── Resource Manager ─────────────────────────────────────────
// Central place for loading and caching assets: shaders,
// textures, models, sounds. Prevents duplicate loads and
// provides a clean shutdown path.
class ResourceManager {
public:
    static ResourceManager& Get() {
        static ResourceManager instance;
        return instance;
    }

    // Shaders
    Shader* LoadShader(const std::string& name,
                       const std::string& vertPath,
                       const std::string& fragPath);
    Shader* GetShader(const std::string& name);

    // TODO: Textures, Models, Sounds

    void ClearAll();

private:
    ResourceManager() = default;
    std::unordered_map<std::string, std::unique_ptr<Shader>> m_shaders;
};
