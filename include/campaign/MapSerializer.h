#pragma once
#include <string>

class CampaignMap;

class MapSerializer {
public:
    static bool SaveToFile(const CampaignMap& map, const std::string& path);
    static bool LoadFromFile(CampaignMap& map, const std::string& path);

    // Export the current hardcoded map to JSON (call once to bootstrap)
    static bool ExportCurrentMap(const CampaignMap& map, const std::string& path);
};