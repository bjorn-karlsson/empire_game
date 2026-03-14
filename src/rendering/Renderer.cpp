// ============================================================================
// Renderer.cpp — Movement mesh, multi-turn path arrows, obstacles
// ============================================================================
#include <glad/glad.h>
#include "rendering/Renderer.h"
#include "rendering/Camera.h"
#include "rendering/Shader.h"
#include "campaign/CampaignMap.h"
#include "campaign/Province.h"
#include "battle/BattleScene.h"
#include "utils/Logger.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <vector>

Renderer::Renderer(int w,int h):m_width(w),m_height(h){}
Renderer::~Renderer(){
    for(auto&[id,g]:m_provinceGPUs){if(g.VAO)glDeleteVertexArrays(1,&g.VAO);if(g.VBO)glDeleteBuffers(1,&g.VBO);
        if(g.borderVAO)glDeleteVertexArrays(1,&g.borderVAO);if(g.borderVBO)glDeleteBuffers(1,&g.borderVBO);}
    for(auto&g:m_obstacleGPUs){if(g.VAO)glDeleteVertexArrays(1,&g.VAO);if(g.VBO)glDeleteBuffers(1,&g.VBO);}
    if(m_waterVAO)glDeleteVertexArrays(1,&m_waterVAO);if(m_markerVAO)glDeleteVertexArrays(1,&m_markerVAO);
    if(m_cityVAO)glDeleteVertexArrays(1,&m_cityVAO);if(m_circleVAO)glDeleteVertexArrays(1,&m_circleVAO);
    if(m_moveMeshVAO)glDeleteVertexArrays(1,&m_moveMeshVAO);if(m_moveMeshVBO)glDeleteBuffers(1,&m_moveMeshVBO);
    if(m_pathVAO)glDeleteVertexArrays(1,&m_pathVAO);if(m_pathVBO)glDeleteBuffers(1,&m_pathVBO);
}

bool Renderer::Init(){
    glEnable(GL_DEPTH_TEST);glEnable(GL_MULTISAMPLE);glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glEnable(GL_LINE_SMOOTH);
    glEnable(GL_STENCIL_TEST);glClearColor(0.05f,0.08f,0.15f,1);
    m_camera=std::make_unique<Camera>((float)m_width/m_height);
    InitShaders();BuildWaterPlane();BuildArmyMarker();BuildCircle();

    // Create dynamic VAOs (empty, filled each frame)
    glGenVertexArrays(1,&m_moveMeshVAO);glGenBuffers(1,&m_moveMeshVBO);
    glGenVertexArrays(1,&m_pathVAO);glGenBuffers(1,&m_pathVBO);
    return true;
}

void Renderer::BuildMapGeometry(const CampaignMap&map){
    for(const auto&p:map.GetProvinces())BuildProvinceGPU(p);
    for(const auto&ob:map.GetObstacles())BuildObstacleGPU(ob);
}

void Renderer::BuildProvinceGPU(const Province&prov){
    if(prov.borderVertices.size()<3)return;
    ProvinceGPU gpu;std::vector<float>v;
    glm::vec3 c=prov.center;int n=(int)prov.borderVertices.size();
    for(int i=0;i<n;i++){auto&v0=prov.borderVertices[i];auto&v1=prov.borderVertices[(i+1)%n];
        v.insert(v.end(),{c.x,c.y,c.z,0,0,v0.x,v0.y,v0.z,1,0,v1.x,v1.y,v1.z,1,0});}
    gpu.vertexCount=n*3;
    glGenVertexArrays(1,&gpu.VAO);glGenBuffers(1,&gpu.VBO);glBindVertexArray(gpu.VAO);glBindBuffer(GL_ARRAY_BUFFER,gpu.VBO);
    glBufferData(GL_ARRAY_BUFFER,v.size()*sizeof(float),v.data(),GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,5*sizeof(float),(void*)0);glEnableVertexAttribArray(0);
    glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,5*sizeof(float),(void*)(3*sizeof(float)));glEnableVertexAttribArray(1);
    glBindVertexArray(0);
    std::vector<float>bv;
    for(auto&vt:prov.borderVertices)bv.insert(bv.end(),{vt.x,vt.y+0.02f,vt.z});
    bv.insert(bv.end(),{prov.borderVertices[0].x,prov.borderVertices[0].y+0.02f,prov.borderVertices[0].z});
    gpu.borderVertexCount=n+1;
    glGenVertexArrays(1,&gpu.borderVAO);glGenBuffers(1,&gpu.borderVBO);glBindVertexArray(gpu.borderVAO);
    glBindBuffer(GL_ARRAY_BUFFER,gpu.borderVBO);glBufferData(GL_ARRAY_BUFFER,bv.size()*sizeof(float),bv.data(),GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),(void*)0);glEnableVertexAttribArray(0);
    glBindVertexArray(0);m_provinceGPUs[prov.id]=gpu;
}

void Renderer::BuildObstacleGPU(const TerrainObstacle&ob){
    if(ob.vertices.size()<3)return;ObstacleGPU gpu;std::vector<float>v;
    glm::vec3 c=ob.center;int n=(int)ob.vertices.size();float y=0.01f;
    for(int i=0;i<n;i++){auto&v0=ob.vertices[i];auto&v1=ob.vertices[(i+1)%n];
        v.insert(v.end(),{c.x,y,c.z,v0.x,y,v0.z,v1.x,y,v1.z});}
    gpu.vertexCount=n*3;
    glGenVertexArrays(1,&gpu.VAO);glGenBuffers(1,&gpu.VBO);glBindVertexArray(gpu.VAO);
    glBindBuffer(GL_ARRAY_BUFFER,gpu.VBO);glBufferData(GL_ARRAY_BUFFER,v.size()*sizeof(float),v.data(),GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),(void*)0);glEnableVertexAttribArray(0);
    glBindVertexArray(0);m_obstacleGPUs.push_back(gpu);
}

void Renderer::BuildWaterPlane(){
    float s=40;std::vector<float>wv;int gs=40;float st=s*2.f/gs;
    for(int gz=0;gz<gs;gz++)for(int gx=0;gx<gs;gx++){
        float x0=-s+gx*st,z0=-s+gz*st,x1=x0+st,z1=z0+st;
        wv.insert(wv.end(),{x0,-0.08f,z0,x1,-0.08f,z0,x1,-0.08f,z1,x0,-0.08f,z0,x1,-0.08f,z1,x0,-0.08f,z1});}
    glGenVertexArrays(1,&m_waterVAO);glGenBuffers(1,&m_waterVBO);glBindVertexArray(m_waterVAO);
    glBindBuffer(GL_ARRAY_BUFFER,m_waterVBO);glBufferData(GL_ARRAY_BUFFER,wv.size()*sizeof(float),wv.data(),GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),(void*)0);glEnableVertexAttribArray(0);glBindVertexArray(0);
}

void Renderer::BuildCircle(){
    std::vector<float>cv;int segs=48;
    for(int i=0;i<=segs;i++){float a=glm::radians(360.f*i/segs);cv.insert(cv.end(),{cos(a),0.05f,sin(a)});}
    m_circleVerts=segs+1;
    glGenVertexArrays(1,&m_circleVAO);glGenBuffers(1,&m_circleVBO);glBindVertexArray(m_circleVAO);
    glBindBuffer(GL_ARRAY_BUFFER,m_circleVBO);glBufferData(GL_ARRAY_BUFFER,cv.size()*sizeof(float),cv.data(),GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),(void*)0);glEnableVertexAttribArray(0);glBindVertexArray(0);
}

void Renderer::BuildArmyMarker(){
    float h=0.9f,fw=0.4f,fh=0.25f;
    std::vector<float>av={-0.03f,0,0,0.03f,0,0,0.03f,h,0,-0.03f,0,0,0.03f,h,0,-0.03f,h,0,
        0.03f,h,0,0.03f+fw,h,0,0.03f+fw,h-fh,0,0.03f,h,0,0.03f+fw,h-fh,0,0.03f,h-fh,0,
        0.03f+fw,h,0,0.03f,h,0,0.03f,h-fh,0,0.03f+fw,h,0,0.03f,h-fh,0,0.03f+fw,h-fh,0,
        -0.18f,0.01f,-0.1f,0.18f,0.01f,-0.1f,0.18f,0.01f,0.1f,-0.18f,0.01f,-0.1f,0.18f,0.01f,0.1f,-0.18f,0.01f,0.1f};
    glGenVertexArrays(1,&m_markerVAO);glGenBuffers(1,&m_markerVBO);glBindVertexArray(m_markerVAO);
    glBindBuffer(GL_ARRAY_BUFFER,m_markerVBO);glBufferData(GL_ARRAY_BUFFER,av.size()*sizeof(float),av.data(),GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),(void*)0);glEnableVertexAttribArray(0);glBindVertexArray(0);
    float cs=0.12f;float cv[]={-cs,0,-cs,cs,0,-cs,cs,cs*2,-cs,-cs,0,-cs,cs,cs*2,-cs,-cs,cs*2,-cs,
        -cs,0,cs,cs,0,cs,cs,cs*2,cs,-cs,0,cs,cs,cs*2,cs,-cs,cs*2,cs,cs,0,-cs,cs,0,cs,cs,cs*2,cs,cs,0,-cs,cs,cs*2,cs,cs,cs*2,-cs,
        -cs,0,cs,-cs,0,-cs,-cs,cs*2,-cs,-cs,0,cs,-cs,cs*2,-cs,-cs,cs*2,cs,
        -cs,cs*2,-cs,cs,cs*2,-cs,cs,cs*2,cs,-cs,cs*2,-cs,cs,cs*2,cs,-cs,cs*2,cs};
    glGenVertexArrays(1,&m_cityVAO);glGenBuffers(1,&m_cityVBO);glBindVertexArray(m_cityVAO);
    glBindBuffer(GL_ARRAY_BUFFER,m_cityVBO);glBufferData(GL_ARRAY_BUFFER,sizeof(cv),cv,GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),(void*)0);glEnableVertexAttribArray(0);glBindVertexArray(0);
}

// ═══════════════════════════════════════════════════════════════
void Renderer::InitShaders(){
    m_provinceShader=std::make_unique<Shader>();
    m_provinceShader->LoadFromSource(
    R"(#version 330 core
    layout(location=0)in vec3 aPos;layout(location=1)in vec2 aEdge;uniform mat4 u_VP;out vec3 v_W;out float v_E;
    void main(){v_W=aPos;v_E=aEdge.x;gl_Position=u_VP*vec4(aPos,1);})",
    R"(#version 330 core
    in vec3 v_W;in float v_E;uniform vec3 u_Color;out vec4 FC;
    void main(){vec3 p=vec3(0.82,0.75,0.62);vec3 c=mix(u_Color,p,0.3);
    float e=smoothstep(0.55,0.95,v_E);c*=(1.0-e*0.3);c+=sin(v_W.x*4.1)*cos(v_W.z*3.3)*0.03;FC=vec4(c,1);})");

    m_borderShader=std::make_unique<Shader>();
    m_borderShader->LoadFromSource(
    R"(#version 330 core
    layout(location=0)in vec3 aPos;uniform mat4 u_VP;void main(){gl_Position=u_VP*vec4(aPos,1);})",
    R"(#version 330 core
    uniform vec3 u_Color;out vec4 FC;void main(){FC=vec4(u_Color,1);})");

    m_waterShader=std::make_unique<Shader>();
    m_waterShader->LoadFromSource(
    R"(#version 330 core
    layout(location=0)in vec3 aPos;uniform mat4 u_VP;uniform float u_Time;out vec3 v_W;
    void main(){vec3 p=aPos;p.y+=sin(p.x*0.4+u_Time*0.7)*cos(p.z*0.3+u_Time*0.5)*0.06+sin(p.x*0.8-u_Time*0.4)*0.03;
    v_W=p;gl_Position=u_VP*vec4(p,1);})",
    R"(#version 330 core
    in vec3 v_W;uniform float u_Time;out vec4 FC;
    void main(){vec3 d=vec3(0.03,0.06,0.14),m=vec3(0.06,0.12,0.25),b=vec3(0.10,0.18,0.35);
    float w1=sin(v_W.x*0.8+u_Time*0.6)*cos(v_W.z*0.6+u_Time*0.4);
    float w2=sin(v_W.x*1.5-u_Time*0.3+v_W.z*0.9)*0.5+0.5;float w3=sin(v_W.x*2.2+u_Time*1.1)*cos(v_W.z*1.8-u_Time*0.7)*0.5+0.5;
    float w=w1*0.4+w2*0.35+w3*0.25;w=w*0.5+0.5;vec3 c=mix(d,m,w*0.6);c=mix(c,b,w3*0.3);
    float s=pow(max(w,0.0),6.0)*0.2;c+=vec3(s*0.6,s*0.7,s);FC=vec4(c,1);})");

    m_armyShader=std::make_unique<Shader>();
    m_armyShader->LoadFromSource(
    R"(#version 330 core
    layout(location=0)in vec3 aPos;uniform mat4 u_VP;uniform mat4 u_Model;out vec3 v_L;
    void main(){v_L=aPos;gl_Position=u_VP*u_Model*vec4(aPos,1);})",
    R"(#version 330 core
    in vec3 v_L;uniform vec3 u_Color;uniform float u_Selected;out vec4 FC;
    void main(){float s=0.5+0.5*clamp(v_L.y*2.5,0,1);vec3 c=u_Color*s;
    if(u_Selected>0.5)c=mix(c,vec3(1,0.95,0.4),0.6);FC=vec4(c,1);})");

    m_overlayShader=std::make_unique<Shader>();
    m_overlayShader->LoadFromSource(
    R"(#version 330 core
    layout(location=0)in vec3 aPos;uniform mat4 u_VP;uniform mat4 u_Model;
    void main(){gl_Position=u_VP*u_Model*vec4(aPos,1);})",
    R"(#version 330 core
    uniform vec4 u_Color;out vec4 FC;void main(){FC=u_Color;})");
}

// ═══════════════════════════════════════════════════════════════
// RENDER
// ═══════════════════════════════════════════════════════════════
void Renderer::BeginFrame(){glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);m_time+=0.016f;}
void Renderer::EndFrame(){}

void Renderer::RenderCampaignMap(const CampaignMap&map){
    glStencilFunc(GL_ALWAYS,1,0xFF);glStencilOp(GL_KEEP,GL_KEEP,GL_REPLACE);glStencilMask(0xFF);
    RenderProvinces(map);
    glStencilFunc(GL_EQUAL,0,0xFF);glStencilMask(0x00);
    RenderWater();
    glStencilFunc(GL_ALWAYS,0,0xFF);glStencilMask(0x00);
    RenderObstacles(map);RenderBorders(map);
    RenderMovementMesh(map);RenderPathArrows(map);
    RenderCities(map);RenderArmies(map);RenderSelectionCircle(map);
    glStencilMask(0xFF);
}

void Renderer::RenderWater(){
    m_waterShader->Use();m_waterShader->SetMat4("u_VP",m_camera->GetViewProjectionMatrix());
    m_waterShader->SetFloat("u_Time",m_time);
    glBindVertexArray(m_waterVAO);glDrawArrays(GL_TRIANGLES,0,40*40*6);glBindVertexArray(0);
}

void Renderer::RenderProvinces(const CampaignMap&map){
    m_provinceShader->Use();m_provinceShader->SetMat4("u_VP",m_camera->GetViewProjectionMatrix());
    for(const auto&p:map.GetProvinces()){auto it=m_provinceGPUs.find(p.id);if(it==m_provinceGPUs.end())continue;
        m_provinceShader->SetVec3("u_Color",p.color);glBindVertexArray(it->second.VAO);
        glDrawArrays(GL_TRIANGLES,0,it->second.vertexCount);}glBindVertexArray(0);
}

void Renderer::RenderObstacles(const CampaignMap&map){
    m_borderShader->Use();m_borderShader->SetMat4("u_VP",m_camera->GetViewProjectionMatrix());
    auto&obs=map.GetObstacles();
    for(int i=0;i<(int)obs.size()&&i<(int)m_obstacleGPUs.size();i++){
        m_borderShader->SetVec3("u_Color",obs[i].color);
        glBindVertexArray(m_obstacleGPUs[i].VAO);glDrawArrays(GL_TRIANGLES,0,m_obstacleGPUs[i].vertexCount);}
    glBindVertexArray(0);
}

void Renderer::RenderBorders(const CampaignMap&map){
    m_borderShader->Use();m_borderShader->SetMat4("u_VP",m_camera->GetViewProjectionMatrix());
    const Faction*pl=map.GetPlayerFaction();
    for(const auto&p:map.GetProvinces()){auto it=m_provinceGPUs.find(p.id);if(it==m_provinceGPUs.end())continue;
        bool own=(pl&&p.ownerFactionId==pl->id);
        if(own){m_borderShader->SetVec3("u_Color",{0.2f,0.55f,0.2f});glLineWidth(2);}
        else{m_borderShader->SetVec3("u_Color",{0.15f,0.12f,0.08f});glLineWidth(1.5f);}
        glBindVertexArray(it->second.borderVAO);glDrawArrays(GL_LINE_STRIP,0,it->second.borderVertexCount);}
    glBindVertexArray(0);glLineWidth(1);
}

// ── Movement mesh: green overlay from flood-fill cells ────────
void Renderer::RenderMovementMesh(const CampaignMap&map){
    int sel=map.GetSelectedArmyId();if(sel<0)return;
    const Army*a=map.GetArmy(sel);if(!a||a->movementRange<0.05f)return;
    if(a->isMoving)return; // don't show mesh while moving

    auto cells=map.GetReachableCells(sel);
    if(cells.empty())return;

    const auto&ng=map.GetNavGrid();
    float cs=NavGrid::CELL;float y=0.04f;

    // Build quad mesh from cells
    std::vector<float>verts;
    for(const auto&c:cells){
        float x=ng.toWX(c.gx),z=ng.toWZ(c.gz);
        float h=cs*0.5f;
        verts.insert(verts.end(),{x-h,y,z-h, x+h,y,z-h, x+h,y,z+h, x-h,y,z-h, x+h,y,z+h, x-h,y,z+h});
    }
    m_moveMeshVerts=(int)verts.size()/3;

    glBindVertexArray(m_moveMeshVAO);glBindBuffer(GL_ARRAY_BUFFER,m_moveMeshVBO);
    glBufferData(GL_ARRAY_BUFFER,verts.size()*sizeof(float),verts.data(),GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),(void*)0);glEnableVertexAttribArray(0);

    glDepthMask(GL_FALSE);
    m_overlayShader->Use();m_overlayShader->SetMat4("u_VP",m_camera->GetViewProjectionMatrix());
    m_overlayShader->SetMat4("u_Model",glm::mat4(1.0f));
    m_overlayShader->SetVec4("u_Color",{0.15f,0.6f,0.25f,0.25f});
    glDrawArrays(GL_TRIANGLES,0,m_moveMeshVerts);
    glBindVertexArray(0);
    glDepthMask(GL_TRUE);
}

// ── Path arrows with multi-turn colors ────────────────────────
// Green=this turn, Red=turn2, Blue=turn3, Yellow=turn4, Purple=turn5+
void Renderer::RenderPathArrows(const CampaignMap&map){
    int sel=map.GetSelectedArmyId();if(sel<0)return;
    const Army*a=map.GetArmy(sel);if(!a||a->fullPath.size()<2)return;

    // Turn colors: green, red, blue, yellow, purple
    glm::vec4 turnColors[]={
        {0.2f,0.85f,0.3f,1.0f},  // green - this turn
        {0.9f,0.2f,0.2f,1.0f},   // red - turn 2
        {0.2f,0.5f,0.95f,1.0f},  // blue - turn 3
        {0.95f,0.85f,0.2f,1.0f}, // yellow - turn 4
        {0.7f,0.3f,0.9f,1.0f},   // purple - turn 5+
    };

    m_overlayShader->Use();m_overlayShader->SetMat4("u_VP",m_camera->GetViewProjectionMatrix());
    m_overlayShader->SetMat4("u_Model",glm::mat4(1.0f));

    // Calculate cumulative distances along path
    std::vector<float>cumDist(a->fullPath.size(),0);
    for(int i=1;i<(int)a->fullPath.size();i++){
        cumDist[i]=cumDist[i-1]+glm::distance(
            glm::vec2(a->fullPath[i].x,a->fullPath[i].z),
            glm::vec2(a->fullPath[i-1].x,a->fullPath[i-1].z));
    }

    // Draw path segments colored by turn
    float pathY=0.06f;
    for(int turn=0;turn<(int)a->turnBreaks.size();turn++){
        float segStart=(turn==0)?a->distanceTraveled:a->turnBreaks[turn-1];
        float segEnd=a->turnBreaks[turn];

        // Collect points in this turn's segment
        std::vector<float>segVerts;
        for(int i=0;i<(int)a->fullPath.size();i++){
            if(cumDist[i]>=segStart&&cumDist[i]<=segEnd+0.01f){
                segVerts.insert(segVerts.end(),{a->fullPath[i].x,pathY,a->fullPath[i].z});
            }
        }
        if(segVerts.size()<6)continue; // need at least 2 points

        glBindVertexArray(m_pathVAO);glBindBuffer(GL_ARRAY_BUFFER,m_pathVBO);
        glBufferData(GL_ARRAY_BUFFER,segVerts.size()*sizeof(float),segVerts.data(),GL_DYNAMIC_DRAW);
        glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),(void*)0);glEnableVertexAttribArray(0);

        int colorIdx=std::min(turn,(int)(sizeof(turnColors)/sizeof(turnColors[0]))-1);
        m_overlayShader->SetVec4("u_Color",turnColors[colorIdx]);
        glLineWidth(turn==0?4.0f:3.0f);
        glDrawArrays(GL_LINE_STRIP,0,(int)segVerts.size()/3);
    }
    glBindVertexArray(0);glLineWidth(1);
}

void Renderer::RenderCities(const CampaignMap&map){
    m_armyShader->Use();m_armyShader->SetMat4("u_VP",m_camera->GetViewProjectionMatrix());
    glBindVertexArray(m_cityVAO);
    for(const auto&p:map.GetProvinces()){
        glm::mat4 m=glm::translate(glm::mat4(1),p.cityPos);float sc=p.isCapital?1.8f:0.9f;m=glm::scale(m,glm::vec3(sc));
        m_armyShader->SetMat4("u_Model",m);
        m_armyShader->SetVec3("u_Color",p.isCapital?glm::vec3(0.9f,0.8f,0.55f):glm::vec3(0.65f,0.58f,0.48f));
        m_armyShader->SetFloat("u_Selected",(p.id==map.GetSelectedProvinceId())?1.f:0.f);
        glDrawArrays(GL_TRIANGLES,0,30);}
    glBindVertexArray(0);
}

void Renderer::RenderArmies(const CampaignMap&map){
    m_armyShader->Use();m_armyShader->SetMat4("u_VP",m_camera->GetViewProjectionMatrix());
    glBindVertexArray(m_markerVAO);
    for(const auto&a:map.GetArmies()){const Faction*f=map.GetFaction(a.factionId);glm::vec3 col=f?f->color:glm::vec3(0.5f);
        glm::mat4 m=glm::translate(glm::mat4(1),a.worldPosition);
        m_armyShader->SetMat4("u_Model",m);m_armyShader->SetVec3("u_Color",col);
        m_armyShader->SetFloat("u_Selected",(a.id==map.GetSelectedArmyId())?1.f:0.f);
        glDrawArrays(GL_TRIANGLES,0,24);}
    glBindVertexArray(0);
}

void Renderer::RenderSelectionCircle(const CampaignMap&map){
    bool has=(map.GetSelectedArmyId()>=0||map.GetSelectedProvinceId()>=0);if(!has)return;
    glm::vec3 pos=map.GetSelectionWorldPos();
    m_overlayShader->Use();m_overlayShader->SetMat4("u_VP",m_camera->GetViewProjectionMatrix());
    float r=(map.GetSelectedArmyId()>=0)?0.5f:0.35f;
    glm::mat4 model=glm::translate(glm::mat4(1),pos);model=glm::scale(model,glm::vec3(r));
    m_overlayShader->SetMat4("u_Model",model);
    m_overlayShader->SetVec4("u_Color",{0.2f,0.9f,0.3f,1.0f});
    glLineWidth(3);glBindVertexArray(m_circleVAO);glDrawArrays(GL_LINE_STRIP,0,m_circleVerts);
    glBindVertexArray(0);glLineWidth(1);
}

void Renderer::RenderBattle(const BattleScene&){glClearColor(0.3f,0.15f,0.1f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);glClearColor(0.05f,0.08f,0.15f,1);}
void Renderer::OnResize(int w,int h){m_width=w;m_height=h;glViewport(0,0,w,h);if(m_camera)m_camera->OnResize((float)w/h);}
