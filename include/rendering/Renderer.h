#pragma once
#include <memory>
#include <vector>
#include <unordered_map>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class CampaignMap;class BattleScene;class Camera;class Shader;
struct Province;struct TerrainObstacle;

struct ProvinceGPU{unsigned int VAO=0,VBO=0;int vertexCount=0;unsigned int borderVAO=0,borderVBO=0;int borderVertexCount=0;};
struct ObstacleGPU{unsigned int VAO=0,VBO=0;int vertexCount=0;};

class Renderer{
public:
    Renderer(int w,int h);~Renderer();
    bool Init();
    void BuildMapGeometry(const CampaignMap&);
    void RebuildProvinceColors(const CampaignMap&); // call after province capture
    void BeginFrame();void EndFrame();
    void RenderCampaignMap(const CampaignMap&);
    void RenderBattle(const BattleScene&);
    Camera*GetCamera(){return m_camera.get();}
    void OnResize(int,int);

private:
    void InitShaders();
    void BuildProvinceGPU(const Province&);
    void BuildObstacleGPU(const TerrainObstacle&);
    void BuildWaterPlane();void BuildArmyMarker();void BuildCircle();

    void RenderWater();
    void RenderProvinces(const CampaignMap&);
    void RenderObstacles(const CampaignMap&);
    void RenderBorders(const CampaignMap&);
    void RenderMovementMesh(const CampaignMap&);
    void RenderPathArrows(const CampaignMap&);
    void RenderCities(const CampaignMap&);
    void RenderArmies(const CampaignMap&);
    void RenderSelectionCircle(const CampaignMap&);
    void RenderHUD(const CampaignMap&);
    void RenderNotification(const CampaignMap&);
    void RenderExchangeModal(const CampaignMap&);

    void DrawScreenQuad(float x,float y,float w,float h,glm::vec4 color);

    int m_width,m_height;
    std::unique_ptr<Camera> m_camera;
    std::unique_ptr<Shader> m_provinceShader,m_borderShader,m_waterShader,m_armyShader,m_overlayShader;
    std::unique_ptr<Shader> m_screenShader; // 2D screen-space HUD
    std::unordered_map<int,ProvinceGPU> m_provinceGPUs;
    std::vector<ObstacleGPU> m_obstacleGPUs;
    unsigned int m_waterVAO=0,m_waterVBO=0;
    unsigned int m_markerVAO=0,m_markerVBO=0;
    unsigned int m_cityVAO=0,m_cityVBO=0;
    unsigned int m_circleVAO=0,m_circleVBO=0;int m_circleVerts=0;

    // Dynamic movement mesh (rebuilt each frame when army selected)
    unsigned int m_moveMeshVAO=0,m_moveMeshVBO=0;int m_moveMeshVerts=0;

    // Dynamic path lines (rebuilt each frame)
    unsigned int m_pathVAO=0,m_pathVBO=0;int m_pathVerts=0;

    float m_time=0;
};
