// ============================================================================
// Renderer.cpp — Movement mesh, multi-turn path arrows, obstacles
// ============================================================================
#include <glad/glad.h>
#include "rendering/Renderer.h"
#include "rendering/Camera.h"
#include "rendering/Shader.h"
#include "rendering/BitmapFont.h"
#include "campaign/CampaignMap.h"
#include "campaign/Province.h"
#include "battle/BattleScene.h"
#include "utils/Logger.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <vector>
#include <string>

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
    glEnable(GL_STENCIL_TEST); glClearColor(0.03f, 0.06f, 0.12f, 1);
    m_camera=std::make_unique<Camera>((float)m_width/m_height);
    InitShaders();BuildWaterPlane();BuildArmyMarker();BuildCircle();

    // Create dynamic VAOs (empty, filled each frame)
    glGenVertexArrays(1,&m_moveMeshVAO);glGenBuffers(1,&m_moveMeshVBO);
    glGenVertexArrays(1,&m_pathVAO);glGenBuffers(1,&m_pathVBO);
    glGenVertexArrays(1,&m_textVAO);glGenBuffers(1,&m_textVBO);
    BuildFontTexture();
    return true;
}

void Renderer::BuildMapGeometry(const CampaignMap&map){
    for(const auto&p:map.GetProvinces())BuildProvinceGPU(p);
    for(const auto&ob:map.GetObstacles())BuildObstacleGPU(ob);
    for(const auto&ft:map.GetForeignTerritories())BuildForeignGPU(ft);
}
void Renderer::RebuildProvinceColors(const CampaignMap&){
    // Colors are sent as uniforms each frame — no GPU rebuild needed
}

void Renderer::BuildProvinceGPU(const Province& prov) {
    if (prov.borderVertices.size() < 3)return;
    ProvinceGPU gpu; std::vector<float>v;
    glm::vec3 c = prov.center; int n = (int)prov.borderVertices.size();
    const int SUBS = 4; // subdivisions per edge
    for (int i = 0; i < n; i++) {
        glm::vec3 v0 = prov.borderVertices[i];
        glm::vec3 v1 = prov.borderVertices[(i + 1) % n];
        for (int s = 0; s < SUBS; s++) {
            float t0 = (float)s / SUBS;
            float t1 = (float)(s + 1) / SUBS;
            glm::vec3 a = glm::mix(v0, v1, t0);
            glm::vec3 b = glm::mix(v0, v1, t1);
            // edge attribute: 0=center, 1=border
            // interpolate edge: sub-vertices are all on the border (edge=1)
            v.insert(v.end(), { c.x,c.y,c.z,0,0, a.x,a.y,a.z,1,0, b.x,b.y,b.z,1,0 });
        }
    }
    gpu.vertexCount = n * SUBS * 3;
    glGenVertexArrays(1, &gpu.VAO); glGenBuffers(1, &gpu.VBO); glBindVertexArray(gpu.VAO);
    glBindBuffer(GL_ARRAY_BUFFER, gpu.VBO);
    glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(float), v.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float))); glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    // Border line strip (also subdivided)
    std::vector<float>bv;
    for (int i = 0; i < n; i++) {
        glm::vec3 v0 = prov.borderVertices[i];
        glm::vec3 v1 = prov.borderVertices[(i + 1) % n];
        for (int s = 0; s < SUBS; s++) {
            float t = (float)s / SUBS;
            glm::vec3 p = glm::mix(v0, v1, t);
            bv.insert(bv.end(), { p.x,p.y + 0.02f,p.z });
        }
    }
    // close the loop
    bv.insert(bv.end(), { prov.borderVertices[0].x,prov.borderVertices[0].y + 0.02f,prov.borderVertices[0].z });
    gpu.borderVertexCount = n * SUBS + 1;
    glGenVertexArrays(1, &gpu.borderVAO); glGenBuffers(1, &gpu.borderVBO); glBindVertexArray(gpu.borderVAO);
    glBindBuffer(GL_ARRAY_BUFFER, gpu.borderVBO);
    glBufferData(GL_ARRAY_BUFFER, bv.size() * sizeof(float), bv.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0); glEnableVertexAttribArray(0);
    glBindVertexArray(0); m_provinceGPUs[prov.id] = gpu;
}
// ── BuildObstacleGPU ──────────────────────────────────────────
void Renderer::BuildObstacleGPU(const TerrainObstacle& ob) {
    if (ob.vertices.size() < 3)return; ObstacleGPU gpu; std::vector<float>v;
    glm::vec3 c = ob.center; int n = (int)ob.vertices.size();
    const int SUBS = 4;
    for (int i = 0; i < n; i++) {
        glm::vec3 v0 = ob.vertices[i];
        glm::vec3 v1 = ob.vertices[(i + 1) % n];
        for (int s = 0; s < SUBS; s++) {
            float t0 = (float)s / SUBS;
            float t1 = (float)(s + 1) / SUBS;
            glm::vec3 a = glm::mix(v0, v1, t0);
            glm::vec3 b = glm::mix(v0, v1, t1);
            // edge attribute: 0=center, 1=border
            v.insert(v.end(), { c.x,0,c.z,0, a.x,0,a.z,1, b.x,0,b.z,1 });
        }
    }
    gpu.vertexCount = n * SUBS * 3;
    glGenVertexArrays(1, &gpu.VAO); glGenBuffers(1, &gpu.VBO); glBindVertexArray(gpu.VAO);
    glBindBuffer(GL_ARRAY_BUFFER, gpu.VBO); glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(float), v.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(3 * sizeof(float))); glEnableVertexAttribArray(1);
    glBindVertexArray(0); m_obstacleGPUs.push_back(gpu);
}
// ── BuildForeignGPU ───────────────────────────────────────────
void Renderer::BuildForeignGPU(const ForeignTerritory& ft) {
    if (ft.vertices.size() < 3)return; ObstacleGPU gpu; std::vector<float>v;
    glm::vec3 c = ft.center; int n = (int)ft.vertices.size();
    const int SUBS = 4;
    for (int i = 0; i < n; i++) {
        glm::vec3 v0 = ft.vertices[i];
        glm::vec3 v1 = ft.vertices[(i + 1) % n];
        for (int s = 0; s < SUBS; s++) {
            float t0 = (float)s / SUBS;
            float t1 = (float)(s + 1) / SUBS;
            glm::vec3 a = glm::mix(v0, v1, t0);
            glm::vec3 b = glm::mix(v0, v1, t1);
            v.insert(v.end(), { c.x,0,c.z,0,0, a.x,0,a.z,1,0, b.x,0,b.z,1,0 });
        }
    }
    gpu.vertexCount = n * SUBS * 3;
    glGenVertexArrays(1, &gpu.VAO); glGenBuffers(1, &gpu.VBO); glBindVertexArray(gpu.VAO);
    glBindBuffer(GL_ARRAY_BUFFER, gpu.VBO);
    glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(float), v.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float))); glEnableVertexAttribArray(1);
    glBindVertexArray(0); m_foreignGPUs.push_back(gpu);
}
void Renderer::BuildFontTexture(){
    // Create 128x64 texture atlas: 16 chars per row, 6 rows, each char 8x8 pixels
    const int atlasW=128,atlasH=64;
    std::vector<unsigned char>pixels(atlasW*atlasH,0);
    for(int ch=0;ch<96;ch++){
        int cx=(ch%16)*8, cy=(ch/16)*8;
        for(int row=0;row<8;row++){
            uint8_t bits=FONT_8X8[ch][row];
            for(int col=0;col<8;col++){
                if(bits&(0x80>>col))
                    pixels[(cy+row)*atlasW+(cx+col)]=255;
            }
        }
    }
    glGenTextures(1,&m_fontTexture);
    glBindTexture(GL_TEXTURE_2D,m_fontTexture);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RED,atlasW,atlasH,0,GL_RED,GL_UNSIGNED_BYTE,pixels.data());
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    Logger::Info("Font texture atlas created (%dx%d)",atlasW,atlasH);
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
// ═══════════════════════════════════════════════════════════════
// Shaders
// ═══════════════════════════════════════════════════════════════
// ── InitShaders ───────────────────────────────────────────────
void Renderer::InitShaders() {

    // ── PROVINCE SHADER ──
    m_provinceShader = std::make_unique<Shader>();
    m_provinceShader->LoadFromSource(
        R"(#version 330 core
layout(location=0)in vec3 aPos;
layout(location=1)in vec2 aEdge;
uniform mat4 u_VP;
out vec3 v_W;
out float v_E;

float hash(vec2 p){ return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453); }
float noise(vec2 p){
    vec2 i=floor(p); vec2 f=fract(p);
    f=f*f*(3.0-2.0*f);
    return mix(mix(hash(i),hash(i+vec2(1,0)),f.x),
               mix(hash(i+vec2(0,1)),hash(i+vec2(1,1)),f.x),f.y);
}
float fbm(vec2 p){
    float v=0.0, a=0.5;
    for(int i=0;i<4;i++){ v+=a*noise(p); p*=2.1; a*=0.5; }
    return v;
}
void main(){
    vec3 pos = aPos;
    float h = fbm(pos.xz * 1.5) * 0.35;
    h += fbm(pos.xz * 4.0 + 50.0) * 0.12;
    h += noise(pos.xz * 10.0) * 0.03;
    h *= (1.0 - aEdge.x * 0.4);
    pos.y += h;
    v_W = pos;
    v_E = aEdge.x;
    gl_Position = u_VP * vec4(pos, 1.0);
})",
R"(#version 330 core
in vec3 v_W;
in float v_E;
uniform vec3 u_Color;
out vec4 FC;
 
float hash(vec2 p){ return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float noise(vec2 p){
    vec2 i = floor(p); vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i),hash(i+vec2(1,0)),f.x), mix(hash(i+vec2(0,1)),hash(i+vec2(1,1)),f.x), f.y);
}
float fbm(vec2 p){
    float v = 0.0, a = 0.5;
    for(int i = 0; i < 5; i++){ v += a * noise(p); p *= 2.1; a *= 0.5; }
    return v;
}
void main(){
    vec3 grassGreen = vec3(0.35, 0.52, 0.22);
    vec3 dryGrass   = vec3(0.55, 0.50, 0.30);
    vec3 darkEarth  = vec3(0.30, 0.25, 0.18);
    float n1 = fbm(v_W.xz * 3.0);
    float n2 = fbm(v_W.xz * 8.0 + 42.0);
    float n3 = noise(v_W.xz * 25.0);
    vec3 terrain = mix(grassGreen, dryGrass, n1 * 0.7);
    terrain = mix(terrain, darkEarth, n2 * 0.15);
    vec3 col = mix(terrain, u_Color, 0.25);
    col += (n3 - 0.5) * 0.04;
    float edge = smoothstep(0.5, 0.95, v_E);
    col *= (1.0 - edge * 0.35);
    vec3 lightDir = normalize(vec3(-0.5, 1.0, -0.3));
    float eps = 0.1;
    float hL = fbm((v_W.xz + vec2(-eps, 0)) * 3.0);
    float hR = fbm((v_W.xz + vec2(eps, 0)) * 3.0);
    float hD = fbm((v_W.xz + vec2(0, -eps)) * 3.0);
    float hU = fbm((v_W.xz + vec2(0, eps)) * 3.0);
    vec3 normal = normalize(vec3(hL - hR, 0.3, hD - hU));
    float NdotL = max(dot(normal, lightDir), 0.0);
    col *= 0.55 + 0.45 * NdotL;
    float dist = length(v_W.xz);
    float fog = smoothstep(8.0, 18.0, dist);
    col = mix(col, vec3(0.45, 0.52, 0.62), fog * 0.25);
    FC = vec4(col, 1.0);
})");

    // ── BORDER / MOUNTAIN SHADER ──
    m_borderShader = std::make_unique<Shader>();
    m_borderShader->LoadFromSource(
        R"(#version 330 core
layout(location=0)in vec3 aPos;
layout(location=1)in float aEdge;
uniform mat4 u_VP;
out vec3 v_W;
 
float hash(vec2 p){ return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453); }
float noise(vec2 p){
    vec2 i=floor(p); vec2 f=fract(p);
    f=f*f*(3.0-2.0*f);
    return mix(mix(hash(i),hash(i+vec2(1,0)),f.x),
               mix(hash(i+vec2(0,1)),hash(i+vec2(1,1)),f.x),f.y);
}
float fbm(vec2 p){
    float v=0.0, a=0.5;
    for(int i=0;i<4;i++){ v+=a*noise(p); p*=2.1; a*=0.5; }
    return v;
}
void main(){
    vec3 pos = aPos;
    float terrainH = fbm(pos.xz * 1.5) * 0.25;
    terrainH += fbm(pos.xz * 4.0 + 50.0) * 0.08;
    terrainH += noise(pos.xz * 10.0) * 0.02;
    float mountainH = fbm(pos.xz * 2.0) * 0.6;
    mountainH += fbm(pos.xz * 5.0 + 30.0) * 0.2;
    mountainH += noise(pos.xz * 12.0) * 0.08;
    mountainH += 0.1;
    float edgeFade = 1.0 - aEdge;
    pos.y = terrainH + mountainH * edgeFade;
    v_W = pos;
    gl_Position = u_VP * vec4(pos, 1.0);
})",
R"(#version 330 core
in vec3 v_W;
uniform vec3 u_Color;
out vec4 FC;
 
float hash(vec2 p){ return fract(sin(dot(p, vec2(127.1,311.7)))*43758.5453); }
float noise(vec2 p){
    vec2 i=floor(p); vec2 f=fract(p);
    f=f*f*(3.0-2.0*f);
    return mix(mix(hash(i),hash(i+vec2(1,0)),f.x),
               mix(hash(i+vec2(0,1)),hash(i+vec2(1,1)),f.x),f.y);
}
float fbm(vec2 p){
    float v=0.0, a=0.5;
    for(int i=0;i<4;i++){ v+=a*noise(p); p*=2.1; a*=0.5; }
    return v;
}
void main(){
    float n = fbm(v_W.xz * 5.0);
    float n2 = noise(v_W.xz * 20.0);
    float height = v_W.y;
    vec3 grassBase = vec3(0.30, 0.42, 0.20);
    vec3 rockMid   = vec3(0.45, 0.40, 0.35);
    vec3 greyPeak  = vec3(0.60, 0.58, 0.55);
    vec3 snowCap   = vec3(0.92, 0.93, 0.95);
    vec3 col;
    if(height < 0.2)       col = mix(grassBase, rockMid, height / 0.2);
    else if(height < 0.45) col = mix(rockMid, greyPeak, (height - 0.2) / 0.25);
    else                   col = mix(greyPeak, snowCap, clamp((height - 0.45) / 0.25, 0.0, 1.0));
    col += (n - 0.5) * 0.12;
    col += (n2 - 0.5) * 0.04;
    vec3 lightDir = normalize(vec3(-0.5, 1.0, -0.3));
    float eps = 0.08;
    float hL = fbm((v_W.xz+vec2(-eps,0))*5.0);
    float hR = fbm((v_W.xz+vec2(eps,0))*5.0);
    float hD = fbm((v_W.xz+vec2(0,-eps))*5.0);
    float hU = fbm((v_W.xz+vec2(0,eps))*5.0);
    vec3 normal = normalize(vec3(hL-hR, 0.25, hD-hU));
    float lit = 0.5 + 0.5 * max(dot(normal, lightDir), 0.0);
    col *= lit;
    FC = vec4(col, 1.0);
})");

    // ── WATER SHADER ──
    m_waterShader = std::make_unique<Shader>();
    m_waterShader->LoadFromSource(
        R"(#version 330 core
layout(location=0)in vec3 aPos;
uniform mat4 u_VP;
uniform float u_Time;
out vec3 v_W;
void main(){
    vec3 p = aPos;
    float wave1 = sin(p.x * 1.5 + u_Time * 0.8) * cos(p.z * 1.2 + u_Time * 0.6) * 0.04;
    float wave2 = sin(p.x * 3.0 - u_Time * 1.2) * sin(p.z * 2.5 + u_Time * 0.9) * 0.02;
    p.y += wave1 + wave2;
    v_W = p;
    gl_Position = u_VP * vec4(p, 1.0);
})",
R"(#version 330 core
in vec3 v_W;
uniform float u_Time;
out vec4 FC;
float hash(vec2 p){ return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453); }
float noise(vec2 p){
    vec2 i=floor(p); vec2 f=fract(p);
    f=f*f*(3.0-2.0*f);
    return mix(mix(hash(i),hash(i+vec2(1,0)),f.x),mix(hash(i+vec2(0,1)),hash(i+vec2(1,1)),f.x),f.y);
}
void main(){
    vec2 uv = v_W.xz;
    vec3 deepWater = vec3(0.05, 0.12, 0.22);
    vec3 shallowWater = vec3(0.10, 0.25, 0.38);
    vec3 highlight = vec3(0.20, 0.40, 0.55);
    float wave1 = sin(uv.x * 2.5 + u_Time * 0.7) * cos(uv.y * 2.0 + u_Time * 0.5);
    float wave2 = sin(uv.x * 5.0 - u_Time * 1.1 + uv.y * 3.0) * 0.5;
    float wave3 = noise(uv * 4.0 + u_Time * 0.3) * 0.4;
    float waves = wave1 * 0.3 + wave2 * 0.2 + wave3;
    float depth = noise(uv * 0.5) * 0.5 + 0.5;
    vec3 col = mix(deepWater, shallowWater, depth * 0.6);
    float spec = smoothstep(0.3, 0.8, waves);
    col = mix(col, highlight, spec * 0.4);
    float sparkle = noise(uv * 15.0 + u_Time * vec2(0.5, 0.3));
    sparkle = pow(sparkle, 4.0) * 0.3;
    col += vec3(sparkle * 0.5, sparkle * 0.7, sparkle);
    float dist = length(uv);
    col = mix(col, vec3(0.03, 0.06, 0.12), smoothstep(5.0, 25.0, dist) * 0.5);
    FC = vec4(col, 0.92);
})");

    // ── ARMY MARKER SHADER ──
    m_armyShader = std::make_unique<Shader>();
    m_armyShader->LoadFromSource(
        R"(#version 330 core
layout(location=0)in vec3 aPos;
uniform mat4 u_VP;
uniform mat4 u_Model;
out vec3 v_Pos;
out vec3 v_Normal;
void main(){
    vec4 worldPos = u_Model * vec4(aPos, 1.0);
    v_Pos = worldPos.xyz;
    v_Normal = normalize(mat3(u_Model) * aPos);
    gl_Position = u_VP * worldPos;
})",
R"(#version 330 core
in vec3 v_Pos;
in vec3 v_Normal;
uniform vec3 u_Color;
uniform float u_Selected;
out vec4 FC;
void main(){
    vec3 lightDir = normalize(vec3(-0.4, 0.8, -0.3));
    float NdotL = max(dot(normalize(v_Normal), lightDir), 0.0);
    vec3 col = u_Color * (0.4 + 0.6 * NdotL);
    if(u_Selected > 0.5) col += vec3(0.15, 0.3, 0.1) * (0.5 + 0.5 * sin(v_Pos.y * 10.0));
    FC = vec4(col, 1.0);
})");

    // ── OVERLAY SHADER ──
    m_overlayShader = std::make_unique<Shader>();
    m_overlayShader->LoadFromSource(
        R"(#version 330 core
layout(location=0)in vec3 aPos;
uniform mat4 u_VP;
uniform mat4 u_Model;
void main(){gl_Position=u_VP*u_Model*vec4(aPos,1);})",
R"(#version 330 core
uniform vec4 u_Color;
out vec4 FC;
void main(){FC=u_Color;})");

    // ── SCREEN QUAD SHADER ──
    m_screenShader = std::make_unique<Shader>();
    m_screenShader->LoadFromSource(
        R"(#version 330 core
layout(location=0)in vec2 aPos;
uniform vec4 u_Rect;
uniform vec4 u_Screen;
void main(){
    vec2 p = u_Rect.xy + aPos * u_Rect.zw;
    vec2 ndc = (p / u_Screen.xy) * 2.0 - 1.0;
    ndc.y = -ndc.y;
    gl_Position = vec4(ndc, 0.0, 1.0);
})",
R"(#version 330 core
uniform vec4 u_Color;
out vec4 FC;
void main(){FC=u_Color;})");

    // ── TEXT SHADER ──
    m_textShader = std::make_unique<Shader>();
    m_textShader->LoadFromSource(
        R"(#version 330 core
layout(location=0)in vec2 aPos;
layout(location=1)in vec2 aUV;
uniform vec4 u_Screen;
out vec2 v_UV;
void main(){
    v_UV = aUV;
    vec2 ndc = (aPos / u_Screen.xy) * 2.0 - 1.0;
    ndc.y = -ndc.y;
    gl_Position = vec4(ndc, 0.0, 1.0);
})",
R"(#version 330 core
in vec2 v_UV;
uniform sampler2D u_Font;
uniform vec4 u_Color;
out vec4 FC;
void main(){
    float a = texture(u_Font, v_UV).r;
    if(a < 0.5) discard;
    FC = u_Color;
})");
}



// ═══════════════════════════════════════════════════════════════
// RENDER
// ═══════════════════════════════════════════════════════════════
void Renderer::BeginFrame(){glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);m_time+=0.016f;}
void Renderer::EndFrame(){}

// ── RenderCampaignMap ─────────────────────────────────────────
void Renderer::RenderCampaignMap(const CampaignMap& map) {
    glStencilFunc(GL_ALWAYS, 1, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    glStencilMask(0xFF);
    RenderProvinces(map);
    RenderForeignTerritories(map);

    glStencilFunc(GL_EQUAL, 0, 0xFF);
    glStencilMask(0x00);
    RenderWater();

    glStencilFunc(GL_ALWAYS, 0, 0xFF);
    glStencilMask(0x00);
    RenderObstacles(map);
    RenderBorders(map);
    RenderMovementMesh(map);
    RenderPathArrows(map);
    RenderCities(map);
    RenderArmies(map);
    RenderSelectionCircle(map);
    glStencilMask(0xFF);

    RenderMapLabels(map);
    RenderHUD(map);
    RenderNotification(map);
    RenderExchangeModal(map);
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
// ── RenderObstacles ───────────────────────────────────────────
void Renderer::RenderObstacles(const CampaignMap& map) {
    auto& obs = map.GetObstacles();

    // Mountains (with height shader)
    m_borderShader->Use();
    m_borderShader->SetMat4("u_VP", m_camera->GetViewProjectionMatrix());
    for (int i = 0; i < (int)obs.size() && i < (int)m_obstacleGPUs.size(); i++) {
        if (obs[i].type != "mountain")continue;
        m_borderShader->SetVec3("u_Color", obs[i].color);
        glBindVertexArray(m_obstacleGPUs[i].VAO);
        glDrawArrays(GL_TRIANGLES, 0, m_obstacleGPUs[i].vertexCount);
    }
    glBindVertexArray(0);

    // Lakes (flat, blue, slight transparency)
    m_overlayShader->Use();
    m_overlayShader->SetMat4("u_VP", m_camera->GetViewProjectionMatrix());
    m_overlayShader->SetMat4("u_Model", glm::mat4(1.0f));
    for (int i = 0; i < (int)obs.size() && i < (int)m_obstacleGPUs.size(); i++) {
        if (obs[i].type != "lake")continue;
        m_overlayShader->SetVec4("u_Color", { 0.08f,0.22f,0.42f,0.80f });
        glBindVertexArray(m_obstacleGPUs[i].VAO);
        glDrawArrays(GL_TRIANGLES, 0, m_obstacleGPUs[i].vertexCount);
    }

    // Rivers (flat, blue, more transparent, thinner look)
    for (int i = 0; i < (int)obs.size() && i < (int)m_obstacleGPUs.size(); i++) {
        if (obs[i].type != "river")continue;
        m_overlayShader->SetVec4("u_Color", { 0.10f,0.25f,0.48f,0.70f });
        glBindVertexArray(m_obstacleGPUs[i].VAO);
        glDrawArrays(GL_TRIANGLES, 0, m_obstacleGPUs[i].vertexCount);
    }
    glBindVertexArray(0);
}
// ── RenderBorders ─────────────────────────────────────────────
void Renderer::RenderBorders(const CampaignMap& map) {
    m_overlayShader->Use();
    m_overlayShader->SetMat4("u_VP", m_camera->GetViewProjectionMatrix());
    m_overlayShader->SetMat4("u_Model", glm::mat4(1.0f));

    const Faction* player = map.GetPlayerFaction();
    if (!player)return;

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    float insetAmount = 0.03f;

    auto isOnLand = [&](glm::vec2 pt)->bool {
        for (const auto& p : map.GetProvinces()) {
            bool inside = false;
            int n = (int)p.borderVertices.size();
            for (int i = 0, j = n - 1; i < n; j = i++) {
                float zi = p.borderVertices[i].z, zj = p.borderVertices[j].z;
                float xi = p.borderVertices[i].x, xj = p.borderVertices[j].x;
                if (((zi > pt.y) != (zj > pt.y)) && (pt.x < (xj - xi) * (pt.y - zi) / (zj - zi) + xi))
                    inside = !inside;
            }
            if (inside)return true;
        }
        for (const auto& ft : map.GetForeignTerritories()) {
            bool inside = false;
            int n = (int)ft.vertices.size();
            for (int i = 0, j = n - 1; i < n; j = i++) {
                float zi = ft.vertices[i].z, zj = ft.vertices[j].z;
                float xi = ft.vertices[i].x, xj = ft.vertices[j].x;
                if (((zi > pt.y) != (zj > pt.y)) && (pt.x < (xj - xi) * (pt.y - zi) / (zj - zi) + xi))
                    inside = !inside;
            }
            if (inside)return true;
        }
        return false;
        };

    auto getDiploColor = [&](const Province& p)->glm::vec4 {
        if (p.ownerFactionId == player->id)
            return{ 0.2f,0.7f,0.2f,1.0f };
        const Faction* f = map.GetFaction(p.ownerFactionId);
        if (!f)return{ 0.8f,0.8f,0.8f,0.7f };
        DiplomaticStatus status = f->GetRelationWith(player->id);
        switch (status) {
        case DiplomaticStatus::WAR:            return{ 0.85f,0.15f,0.1f,1.0f };
        case DiplomaticStatus::ALLIANCE:       return{ 0.2f,0.35f,0.85f,1.0f };
        case DiplomaticStatus::TRADE_AGREEMENT:return{ 0.3f,0.6f,0.85f,0.9f };
        default:                               return{ 0.8f,0.8f,0.75f,0.7f };
        }
        };

    for (const auto& p : map.GetProvinces()) {
        glm::vec4 col = getDiploColor(p);
        int n = (int)p.borderVertices.size();
        if (n < 3)continue;

        glm::vec2 center2D(p.center.x, p.center.z);
        std::vector<float>verts;
        int drawn = 0;

        for (int i = 0; i < n; i++) {
            glm::vec2 v0(p.borderVertices[i].x, p.borderVertices[i].z);
            glm::vec2 v1(p.borderVertices[(i + 1) % n].x, p.borderVertices[(i + 1) % n].z);

            glm::vec2 mid = (v0 + v1) * 0.5f;
            glm::vec2 toCenter = glm::normalize(center2D - mid);
            glm::vec2 testPt = mid - toCenter * 0.15f;
            if (!isOnLand(testPt))continue;

            // Skip edges where midpoint is inside a mountain
            if (map.IsInsideMountain(mid.x, mid.y))continue;

            glm::vec2 inV0 = v0 + glm::normalize(center2D - v0) * insetAmount;
            glm::vec2 inV1 = v1 + glm::normalize(center2D - v1) * insetAmount;

            // Use BASE terrain height only (no mountain)
            float y0 = map.GetBaseTerrainHeight(inV0.x, inV0.y) + 0.02f;
            float y1 = map.GetBaseTerrainHeight(inV1.x, inV1.y) + 0.02f;

            verts.insert(verts.end(), { inV0.x,y0,inV0.y });
            verts.insert(verts.end(), { inV1.x,y1,inV1.y });
            drawn++;
        }

        if (drawn < 1)continue;

        glBindVertexArray(m_pathVAO); glBindBuffer(GL_ARRAY_BUFFER, m_pathVBO);
        glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_DYNAMIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        bool isOwn = (p.ownerFactionId == player->id);

        // Pass 1: Black outline (slightly wider)
        m_overlayShader->SetVec4("u_Color", { 0.0f,0.0f,0.0f,col.a * 0.8f });
        glLineWidth(isOwn ? 4.5f : 3.5f);
        glDrawArrays(GL_LINES, 0, (int)verts.size() / 3);

        // Pass 2: Colored line on top (thinner)
        m_overlayShader->SetVec4("u_Color", col);
        glLineWidth(isOwn ? 2.5f : 1.5f);
        glDrawArrays(GL_LINES, 0, (int)verts.size() / 3);
    }

    glBindVertexArray(0);
    glLineWidth(1);
    glDepthMask(GL_TRUE);
    //glEnable(GL_DEPTH_TEST);
}
// ── Movement mesh: green overlay from flood-fill cells ────────
// ── Screen-space text using bitmap font ───────────────────────
void Renderer::DrawScreenText(const std::string&text,float x,float y,float scale,glm::vec4 color){
    if(text.empty()||!m_fontTexture)return;
    float charW=8*scale,charH=8*scale;
    // Build quads: 6 verts per char (pos.xy + uv.xy)
    std::vector<float>verts;
    float cx=x;
    for(char c:text){
        int idx=c-32;
        if(idx<0||idx>=96){cx+=charW;continue;}
        float u0=(float)(idx%16)*8.0f/128.0f;
        float v0=(float)(idx/16)*8.0f/64.0f;
        float u1=u0+8.0f/128.0f;
        float v1=v0+8.0f/64.0f;
        // 2 triangles: TL,TR,BL + TR,BR,BL (in screen coords via shader)
        // We pass raw pixel coords; shader maps via u_Rect={0,0,1,1}
        verts.insert(verts.end(),{cx,y,u0,v0, cx+charW,y,u1,v0, cx,y+charH,u0,v1,
            cx+charW,y,u1,v0, cx+charW,y+charH,u1,v1, cx,y+charH,u0,v1});
        cx+=charW;
    }
    if(verts.empty())return;

    glBindVertexArray(m_textVAO);glBindBuffer(GL_ARRAY_BUFFER,m_textVBO);
    glBufferData(GL_ARRAY_BUFFER,verts.size()*sizeof(float),verts.data(),GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,4*sizeof(float),(void*)0);glEnableVertexAttribArray(0);
    glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,4*sizeof(float),(void*)(2*sizeof(float)));glEnableVertexAttribArray(1);

    m_textShader->Use();
    m_textShader->SetVec4("u_Rect",{0,0,1,1}); // identity — coords already in pixels
    m_textShader->SetVec4("u_Screen",{(float)m_width,(float)m_height,0,0});
    m_textShader->SetVec4("u_Color",color);
    glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,m_fontTexture);

    glDrawArrays(GL_TRIANGLES,0,(int)verts.size()/4);
    glBindVertexArray(0);
}

// ── World-space text (projects 3D position to screen) ─────────
void Renderer::DrawWorldText(const std::string&text,glm::vec3 worldPos,float scale,glm::vec4 color){
    glm::mat4 vp=m_camera->GetViewProjectionMatrix();
    glm::vec4 clip=vp*glm::vec4(worldPos,1.0f);
    if(clip.w<=0)return; // behind camera
    glm::vec3 ndc=glm::vec3(clip)/clip.w;
    float sx=(ndc.x*0.5f+0.5f)*m_width;
    float sy=(1.0f-(ndc.y*0.5f+0.5f))*m_height;
    // Center text
    float textW=text.size()*8*scale;
    DrawScreenText(text,sx-textW*0.5f,sy,scale,color);
}

// ── RenderForeignTerritories ──────────────────────────────────
void Renderer::RenderForeignTerritories(const CampaignMap& map) {
    m_provinceShader->Use();
    m_provinceShader->SetMat4("u_VP", m_camera->GetViewProjectionMatrix());
    auto& fts = map.GetForeignTerritories();
    for (int i = 0; i < (int)fts.size() && i < (int)m_foreignGPUs.size(); i++) {
        glm::vec3 c = fts[i].color * 0.55f;
        m_provinceShader->SetVec3("u_Color", c);
        glBindVertexArray(m_foreignGPUs[i].VAO);
        glDrawArrays(GL_TRIANGLES, 0, m_foreignGPUs[i].vertexCount);
    }
    glBindVertexArray(0);
}


// ── Map labels (province names, city names, army names, foreign countries) ──
// ── RenderMapLabels ───────────────────────────────────────────
void Renderer::RenderMapLabels(const CampaignMap& map) {
    glDisable(GL_DEPTH_TEST); glDisable(GL_STENCIL_TEST);

    for (const auto& p : map.GetProvinces()) {
        glm::vec4 col = { 0.9f,0.88f,0.75f,0.85f };
        float th = map.GetBaseTerrainHeight(p.center.x, p.center.z);
        DrawWorldText(p.name, { p.center.x,th + 0.3f,p.center.z }, 1.2f, col);
    }

    for (const auto& p : map.GetProvinces()) {
        glm::vec4 col = p.isCapital ? glm::vec4(1.0f, 0.9f, 0.5f, 0.95f) : glm::vec4(0.8f, 0.75f, 0.6f, 0.8f);
        float scale = p.isCapital ? 1.3f : 1.0f;
        float th = map.GetBaseTerrainHeight(p.cityPos.x, p.cityPos.z);
        DrawWorldText(p.cityName, { p.cityPos.x,th + 0.25f,p.cityPos.z + 0.35f }, scale, col);
    }

    for (const auto& a : map.GetArmies()) {
        if (a.isGarrisoned)continue;
        const Faction* f = map.GetFaction(a.factionId);
        glm::vec3 fc = f ? f->color : glm::vec3(0.5f);
        glm::vec4 col = { fc.r * 0.5f + 0.5f,fc.g * 0.5f + 0.5f,fc.b * 0.5f + 0.5f,0.95f };
        float th = map.GetTerrainHeight(a.worldPosition.x, a.worldPosition.z);
        DrawWorldText(a.generalName, { a.worldPosition.x,th + 1.1f,a.worldPosition.z }, 1.0f, col);
        std::string info = std::to_string(a.GetTotalManpower()) + " men";
        DrawWorldText(info, { a.worldPosition.x,th + 1.0f,a.worldPosition.z + 0.15f }, 0.8f, { 0.8f,0.8f,0.75f,0.75f });
    }

    for (const auto& ft : map.GetForeignTerritories()) {
        float th = map.GetBaseTerrainHeight(ft.center.x, ft.center.z);
        DrawWorldText(ft.name, { ft.center.x,th + 0.2f,ft.center.z }, 1.5f, { 0.6f,0.55f,0.45f,0.7f });
    }

    glEnable(GL_DEPTH_TEST); glEnable(GL_STENCIL_TEST);
}
// ── RenderMovementMesh ────────────────────────────────────────
void Renderer::RenderMovementMesh(const CampaignMap& map) {
    int sel = map.GetSelectedArmyId(); if (sel < 0)return;
    const Army* a = map.GetArmy(sel); if (!a || a->movementRange < 0.05f)return;
    if (a->isMoving)return;

    auto cells = map.GetReachableCells(sel);
    if (cells.empty())return;

    const auto& ng = map.GetNavGrid();
    float cs = NavGrid::CELL;

    std::vector<float>verts;
    for (const auto& c : cells) {
        float x = ng.toWX(c.gx), z = ng.toWZ(c.gz);
        float h = cs * 0.5f;
        float th = map.GetTerrainHeight(x, z) + 0.05f;
        verts.insert(verts.end(), { x - h,th,z - h, x + h,th,z - h, x + h,th,z + h, x - h,th,z - h, x + h,th,z + h, x - h,th,z + h });
    }
    m_moveMeshVerts = (int)verts.size() / 3;

    glBindVertexArray(m_moveMeshVAO); glBindBuffer(GL_ARRAY_BUFFER, m_moveMeshVBO);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0); glEnableVertexAttribArray(0);

    glDepthMask(GL_FALSE);
    m_overlayShader->Use();
    m_overlayShader->SetMat4("u_VP", m_camera->GetViewProjectionMatrix());
    m_overlayShader->SetMat4("u_Model", glm::mat4(1.0f));
    m_overlayShader->SetVec4("u_Color", { 0.15f,0.6f,0.25f,0.25f });
    glDrawArrays(GL_TRIANGLES, 0, m_moveMeshVerts);
    glBindVertexArray(0);
    glDepthMask(GL_TRUE);
}

// ── Path arrows with multi-turn colors ────────────────────────
// Green=this turn, Red=turn2, Blue=turn3, Yellow=turn4, Purple=turn5+
// ── RenderPathArrows ──────────────────────────────────────────
void Renderer::RenderPathArrows(const CampaignMap& map) {
    int sel = map.GetSelectedArmyId(); if (sel < 0)return;
    const Army* a = map.GetArmy(sel); if (!a || a->fullPath.size() < 2)return;

    glm::vec4 turnColors[] = {
        {0.2f,0.85f,0.3f,1.0f},
        {0.9f,0.2f,0.2f,1.0f},
        {0.2f,0.5f,0.95f,1.0f},
        {0.95f,0.85f,0.2f,1.0f},
        {0.7f,0.3f,0.9f,1.0f},
    };
    int numColors = 5;

    m_overlayShader->Use();
    m_overlayShader->SetMat4("u_VP", m_camera->GetViewProjectionMatrix());
    m_overlayShader->SetMat4("u_Model", glm::mat4(1.0f));

    int pathLen = (int)a->fullPath.size();
    std::vector<float>cumDist(pathLen, 0);
    for (int i = 1; i < pathLen; i++)
        cumDist[i] = cumDist[i - 1] + glm::distance(
            glm::vec2(a->fullPath[i].x, a->fullPath[i].z),
            glm::vec2(a->fullPath[i - 1].x, a->fullPath[i - 1].z));

    auto interpAtDist = [&](float d)->glm::vec3 {
        if (d <= 0)return a->fullPath[0];
        if (d >= cumDist.back())return a->fullPath.back();
        for (int i = 1; i < pathLen; i++) {
            if (cumDist[i] >= d) {
                float segLen = cumDist[i] - cumDist[i - 1];
                float t = (segLen > 0.001f) ? (d - cumDist[i - 1]) / segLen : 0;
                return glm::mix(a->fullPath[i - 1], a->fullPath[i], t);
            }
        }
        return a->fullPath.back();
        };

    for (int turn = 0; turn < (int)a->turnBreaks.size(); turn++) {
        float segStart = a->pathStartOffset + ((turn == 0) ? a->distanceTraveled : a->turnBreaks[std::max(0, turn - 1)]);
        float segEnd = a->pathStartOffset + a->turnBreaks[turn];
        if (segEnd <= segStart + 0.01f)continue;

        std::vector<float>segVerts;

        glm::vec3 startPt = interpAtDist(segStart);
        startPt.y = map.GetTerrainHeight(startPt.x, startPt.z) + 0.07f;
        segVerts.insert(segVerts.end(), { startPt.x,startPt.y,startPt.z });

        for (int i = 0; i < pathLen; i++) {
            if (cumDist[i] > segStart + 0.01f && cumDist[i] < segEnd - 0.01f) {
                float py = map.GetTerrainHeight(a->fullPath[i].x, a->fullPath[i].z) + 0.07f;
                segVerts.insert(segVerts.end(), { a->fullPath[i].x,py,a->fullPath[i].z });
            }
        }

        glm::vec3 endPt = interpAtDist(segEnd);
        endPt.y = map.GetTerrainHeight(endPt.x, endPt.z) + 0.07f;
        segVerts.insert(segVerts.end(), { endPt.x,endPt.y,endPt.z });

        if (segVerts.size() < 6)continue;

        glBindVertexArray(m_pathVAO); glBindBuffer(GL_ARRAY_BUFFER, m_pathVBO);
        glBufferData(GL_ARRAY_BUFFER, segVerts.size() * sizeof(float), segVerts.data(), GL_DYNAMIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0); glEnableVertexAttribArray(0);

        int ci = std::min(turn, numColors - 1);
        m_overlayShader->SetVec4("u_Color", turnColors[ci]);
        glLineWidth(turn == 0 ? 5.0f : 3.5f);
        glDrawArrays(GL_LINE_STRIP, 0, (int)segVerts.size() / 3);
    }
    glBindVertexArray(0); glLineWidth(1);
}
// ── RenderCities ──────────────────────────────────────────────
void Renderer::RenderCities(const CampaignMap& map) {
    m_armyShader->Use();
    m_armyShader->SetMat4("u_VP", m_camera->GetViewProjectionMatrix());
    glBindVertexArray(m_cityVAO);
    for (const auto& p : map.GetProvinces()) {
        // Use base terrain (no mountain) so cities are always reachable
        float th = map.GetBaseTerrainHeight(p.cityPos.x, p.cityPos.z) + 0.02f;
        glm::vec3 pos = { p.cityPos.x,th,p.cityPos.z };
        glm::mat4 m = glm::translate(glm::mat4(1), pos);
        float sc = p.isCapital ? 1.8f : 0.9f;
        m = glm::scale(m, glm::vec3(sc));
        m_armyShader->SetMat4("u_Model", m);
        m_armyShader->SetVec3("u_Color", p.isCapital ? glm::vec3(0.85f, 0.75f, 0.50f) : glm::vec3(0.55f, 0.48f, 0.38f));
        m_armyShader->SetFloat("u_Selected", (p.id == map.GetSelectedProvinceId()) ? 1.f : 0.f);
        glDrawArrays(GL_TRIANGLES, 0, 30);
    }
    glBindVertexArray(0);
}
// ── RenderArmies ──────────────────────────────────────────────
void Renderer::RenderArmies(const CampaignMap& map) {
    m_armyShader->Use();
    m_armyShader->SetMat4("u_VP", m_camera->GetViewProjectionMatrix());
    glBindVertexArray(m_markerVAO);
    for (const auto& a : map.GetArmies()) {
        const Faction* f = map.GetFaction(a.factionId);
        glm::vec3 col = f ? f->color : glm::vec3(0.5f);
        float th = map.GetTerrainHeight(a.worldPosition.x, a.worldPosition.z) + 0.02f;
        glm::vec3 pos = { a.worldPosition.x,th,a.worldPosition.z };
        glm::mat4 m = glm::translate(glm::mat4(1), pos);
        m_armyShader->SetMat4("u_Model", m);
        m_armyShader->SetVec3("u_Color", col);
        m_armyShader->SetFloat("u_Selected", (a.id == map.GetSelectedArmyId()) ? 1.f : 0.f);
        glDrawArrays(GL_TRIANGLES, 0, 24);
    }
    glBindVertexArray(0);
}

// ── RenderSelectionCircle ─────────────────────────────────────
void Renderer::RenderSelectionCircle(const CampaignMap& map) {
    bool has = (map.GetSelectedArmyId() >= 0 || map.GetSelectedProvinceId() >= 0);
    if (!has)return;
    glm::vec3 pos = map.GetSelectionWorldPos();
    float th = map.GetTerrainHeight(pos.x, pos.z);
    pos.y = th + 0.05f;
    m_overlayShader->Use();
    m_overlayShader->SetMat4("u_VP", m_camera->GetViewProjectionMatrix());
    float r = (map.GetSelectedArmyId() >= 0) ? 0.5f : 0.35f;
    glm::mat4 model = glm::translate(glm::mat4(1), pos);
    model = glm::scale(model, glm::vec3(r));
    m_overlayShader->SetMat4("u_Model", model);
    m_overlayShader->SetVec4("u_Color", { 0.2f,0.9f,0.3f,1.0f });
    glLineWidth(3);
    glBindVertexArray(m_circleVAO);
    glDrawArrays(GL_LINE_STRIP, 0, m_circleVerts);
    glBindVertexArray(0);
    glLineWidth(1);
}

// ── Draw a 2D screen-space quad ───────────────────────────────
void Renderer::DrawScreenQuad(float x,float y,float w,float h,glm::vec4 color){
    float quad[]={0,0, 1,0, 1,1, 0,0, 1,1, 0,1};

    unsigned int vao,vbo;
    glGenVertexArrays(1,&vao);glGenBuffers(1,&vbo);
    glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,vbo);
    glBufferData(GL_ARRAY_BUFFER,sizeof(quad),quad,GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,2*sizeof(float),(void*)0);
    glEnableVertexAttribArray(0);

    m_screenShader->Use();
    m_screenShader->SetVec4("u_Rect",{x,y,w,h});
    m_screenShader->SetVec4("u_Color",color);
    m_screenShader->SetVec4("u_Screen",{(float)m_width,(float)m_height,0,0});
    // u_Screen is vec2 but SetVec4 works (extra components ignored in shader)

    glDrawArrays(GL_TRIANGLES,0,6);
    glBindVertexArray(0);
    glDeleteVertexArrays(1,&vao);glDeleteBuffers(1,&vbo);
}

// ── HUD ───────────────────────────────────────────────────────
void Renderer::RenderHUD(const CampaignMap&map){
    glDisable(GL_DEPTH_TEST);glDisable(GL_STENCIL_TEST);

    float sw=(float)m_width, sh=(float)m_height;

    // Top bar background
    DrawScreenQuad(0,0,sw,36,{0.05f,0.05f,0.1f,0.85f});

    // Turn indicator: colored blocks for season
    // Spring=green, Summer=yellow, Autumn=orange, Winter=white
    glm::vec4 seasonColors[]={{0.3f,0.7f,0.3f,1},{0.9f,0.8f,0.2f,1},{0.8f,0.5f,0.2f,1},{0.8f,0.85f,0.9f,1}};
    int season=(map.GetCurrentTurn()-1)%4;
    DrawScreenQuad(10,6,24,24,seasonColors[season]);

    // Season + Year text
    std::string seasonName[]={"Spring","Summer","Autumn","Winter"};
    DrawScreenText(seasonName[season]+" "+map.GetCurrentYear(),44,10,1.5f,{0.85f,0.82f,0.7f,0.95f});

    // Treasury bar (gold)
    const Faction*player=map.GetPlayerFaction();
    if(player){
        float maxTreasury=20000.0f;
        float treasuryRatio=glm::clamp((float)player->treasury/maxTreasury,0.0f,1.0f);
        float barX=sw-310;
        DrawScreenText("Treasury: "+std::to_string(player->treasury),barX,4,1.2f,{0.9f,0.8f,0.4f,0.95f});
        DrawScreenQuad(barX,20,200,12,{0.15f,0.12f,0.08f,0.8f});
        DrawScreenQuad(barX,20,200*treasuryRatio,12,{0.85f,0.7f,0.15f,0.9f});

        // Income indicator (green/red bar below treasury)
        int net=player->incomePerTurn-player->expensesPerTurn;
        float netRatio=glm::clamp(std::abs((float)net)/500.0f,0.0f,1.0f);
        glm::vec4 netColor=net>=0?glm::vec4(0.2f,0.7f,0.2f,0.8f):glm::vec4(0.8f,0.2f,0.2f,0.8f);
        DrawScreenQuad(barX,30,200*netRatio,4,netColor);
    }

    // End Turn button (bottom right)
    float btnW=120,btnH=32;
    float btnX=sw-btnW-10,btnY=sh-btnH-10;
    DrawScreenQuad(btnX,btnY,btnW,btnH,{0.15f,0.4f,0.15f,0.85f});
    DrawScreenQuad(btnX+2,btnY+2,btnW-4,btnH-4,{0.2f,0.55f,0.2f,0.9f});
    DrawScreenText("End Turn",btnX+20,btnY+8,1.5f,{0.95f,0.92f,0.8f,1});

    // Selected army info (bottom left panel)
    int selArmy=map.GetSelectedArmyId();
    if(selArmy>=0){
        const Army*a=map.GetArmy(selArmy);
        if(a){
            float panelW=300,panelH=100;
            DrawScreenQuad(0,sh-panelH,panelW,panelH,{0.05f,0.05f,0.1f,0.85f});

            const Faction*f=map.GetFaction(a->factionId);
            glm::vec3 fc=f?f->color:glm::vec3(0.5f);
            DrawScreenQuad(0,sh-panelH,6,panelH,{fc.r,fc.g,fc.b,1});

            // General name + faction
            DrawScreenText(a->generalName,14,sh-panelH+4,1.3f,{0.95f,0.9f,0.75f,1});
            DrawScreenText(std::to_string(a->GetTotalManpower())+" men  "+
                std::to_string((int)a->units.size())+" units",14,sh-panelH+18,1.0f,{0.75f,0.72f,0.6f,0.9f});

            // Unit count bars
            for(int i=0;i<(int)a->units.size();i++){
                float ux=14+i*13;
                float uy=sh-panelH+34;
                // Bar height based on manpower ratio
                float hpRatio=(float)a->units[i].stats.manpower/a->units[i].stats.maxManpower;
                float barH=40*hpRatio;
                glm::vec4 unitColor={0.5f,0.5f,0.6f,0.9f};
                if(a->units[i].type==UnitType::LINE_INFANTRY) unitColor={0.3f,0.4f,0.8f,0.9f};
                if(a->units[i].type==UnitType::GRENADIERS) unitColor={0.8f,0.3f,0.3f,0.9f};
                if(a->units[i].type==UnitType::DRAGOONS||a->units[i].type==UnitType::HUSSARS) unitColor={0.3f,0.7f,0.3f,0.9f};
                if(a->units[i].type==UnitType::CANNON_6PDR||a->units[i].type==UnitType::CANNON_12PDR) unitColor={0.7f,0.6f,0.3f,0.9f};

                DrawScreenQuad(ux,uy,10,40,{0.1f,0.1f,0.15f,0.5f});
                DrawScreenQuad(ux,uy+40-barH,10,barH,unitColor);
            }

            // Movement range bar
            float moveRatio=a->movementRange/a->movementRangeMax;
            DrawScreenQuad(14,sh-22,270,10,{0.1f,0.1f,0.15f,0.6f});
            DrawScreenQuad(14,sh-22,270*moveRatio,10,{0.2f,0.8f,0.3f,0.9f});
            DrawScreenText("Move",14,sh-34,0.9f,{0.6f,0.8f,0.6f,0.8f});
        }
    }

    // Selected city info (bottom left, when city selected)
    int selProv=map.GetSelectedProvinceId();
    if(selProv>=0&&selArmy<0){
        const Province*p=map.GetProvince(selProv);
        if(p){
            float panelW=250,panelH=80;
            DrawScreenQuad(0,sh-panelH,panelW,panelH,{0.05f,0.05f,0.1f,0.85f});
            DrawScreenText(p->cityName,10,sh-panelH+4,1.4f,{0.95f,0.9f,0.7f,1});
            DrawScreenText(p->name,10,sh-panelH+20,1.0f,{0.7f,0.65f,0.55f,0.85f});

            // Income bar
            float incRatio=glm::clamp(p->GetTotalIncome()/500.0f,0.0f,1.0f);
            DrawScreenText("Income: "+std::to_string(p->GetTotalIncome()),10,sh-panelH+34,1.0f,{0.85f,0.75f,0.4f,0.9f});
            DrawScreenQuad(10,sh-panelH+48,220,8,{0.1f,0.1f,0.15f,0.5f});
            DrawScreenQuad(10,sh-panelH+48,220*incRatio,8,{0.85f,0.7f,0.15f,0.9f});

            // Public order bar
            float orderRatio=p->publicOrder/100.0f;
            glm::vec4 orderColor=orderRatio>0.5f?glm::vec4(0.2f,0.7f,0.2f,0.9f):glm::vec4(0.8f,0.3f,0.2f,0.9f);
            DrawScreenText("Order: "+std::to_string((int)p->publicOrder)+"%",10,sh-panelH+58,1.0f,orderColor);
            DrawScreenQuad(10,sh-panelH+70,220,8,{0.1f,0.1f,0.15f,0.5f});
            DrawScreenQuad(10,sh-panelH+70,220*orderRatio,8,orderColor);
        }
    }

    // Faction indicators (top right — flag-style boxes with status)
    float factionX = sw - 10;
    const Faction* playerF = map.GetPlayerFaction();
    for (int fi = (int)map.GetFactions().size() - 1; fi >= 0; fi--) {
        const auto& f = map.GetFactions()[fi];
        if (f.isPlayerControlled)continue;

        float boxW = 90, boxH = 28;
        factionX -= boxW + 4;
        float fy = 4;

        // Background
        float alpha = f.isEliminated ? 0.25f : 0.85f;
        DrawScreenQuad(factionX, fy, boxW, boxH, { 0.08f,0.08f,0.12f,alpha });

        // Faction color stripe on left
        DrawScreenQuad(factionX, fy, 4, boxH, { f.color.r,f.color.g,f.color.b,alpha });

        // Faction name
        DrawScreenText(f.name, factionX + 8, fy + 3, 0.85f,
            { 0.9f,0.85f,0.7f,f.isEliminated ? 0.4f : 1.0f });

        // War/peace status
        if (f.isEliminated) {
            DrawScreenText("DEFEATED", factionX + 8, fy + 15, 0.7f, { 0.5f,0.3f,0.3f,0.6f });
        }
        else if (playerF && f.IsAtWarWith(playerF->id)) {
            DrawScreenText("AT WAR", factionX + 8, fy + 15, 0.7f, { 0.9f,0.25f,0.2f,0.9f });
        }
        else {
            DrawScreenText("Neutral", factionX + 8, fy + 15, 0.7f, { 0.5f,0.6f,0.5f,0.7f });
        }
    }

    glEnable(GL_DEPTH_TEST);glEnable(GL_STENCIL_TEST);
}

// ── Notification banner ───────────────────────────────────────
void Renderer::RenderNotification(const CampaignMap&map){
    if(map.GetNotificationTimer()<=0)return;

    glDisable(GL_DEPTH_TEST);glDisable(GL_STENCIL_TEST);
    float sw=(float)m_width;
    float alpha=glm::clamp(map.GetNotificationTimer(),0.0f,1.0f);

    // Banner across top-center
    float bannerW=500,bannerH=40;
    float bx=(sw-bannerW)/2,by=50;
    DrawScreenQuad(bx,by,bannerW,bannerH,{0.6f,0.15f,0.15f,0.85f*alpha});
    DrawScreenQuad(bx+2,by+2,bannerW-4,bannerH-4,{0.75f,0.2f,0.2f,0.8f*alpha});
    std::string notif=map.GetNotification();
    float textX=bx+(bannerW-notif.size()*10)/2;
    DrawScreenText(notif,textX,by+10,1.4f,{1,0.95f,0.85f,alpha});

    glEnable(GL_DEPTH_TEST);glEnable(GL_STENCIL_TEST);
}

// ── Unit Exchange Modal ───────────────────────────────────────
void Renderer::RenderExchangeModal(const CampaignMap&map){
    if(!map.IsExchangeOpen())return;
    const Army*a=map.GetArmy(map.GetExchangeArmyA());
    const Army*b=map.GetArmy(map.GetExchangeArmyB());
    if(!a||!b)return;
    const auto&selA=map.GetExchangeSelA();
    const auto&selB=map.GetExchangeSelB();

    glDisable(GL_DEPTH_TEST);glDisable(GL_STENCIL_TEST);
    float sw=(float)m_width,sh=(float)m_height;

    // Dim background
    DrawScreenQuad(0,0,sw,sh,{0,0,0,0.5f});

    // Panel
    float panW=sw*0.65f,panH=sh*0.7f;
    float px=(sw-panW)/2,py=(sh-panH)/2;
    DrawScreenQuad(px,py,panW,panH,{0.18f,0.14f,0.10f,0.95f});
    DrawScreenQuad(px+3,py+3,panW-6,panH-6,{0.35f,0.28f,0.20f,0.95f});

    // Title bar
    DrawScreenQuad(px+10,py+8,panW-20,30,{0.25f,0.20f,0.15f,0.9f});
    DrawScreenText("Unit Exchange",px+panW/2-65,py+14,1.5f,{0.95f,0.9f,0.75f,1});

    // Divider
    float cx=sw/2;
    DrawScreenQuad(cx-1,py+45,2,panH-65,{0.15f,0.12f,0.08f,0.8f});

    // Swap button
    float btnW=70,btnH=30;
    float swapX=cx-btnW/2, swapY=py+panH/2-btnH/2;
    DrawScreenQuad(swapX,swapY,btnW,btnH,{0.45f,0.35f,0.15f,0.9f});
    DrawScreenQuad(swapX+2,swapY+2,btnW-4,btnH-4,{0.6f,0.5f,0.25f,0.9f});
    DrawScreenText("Swap",swapX+15,swapY+8,1.3f,{0.95f,0.9f,0.7f,1});
    // Arrows on swap button
    DrawScreenQuad(swapX+10,swapY+8,btnW-20,3,{0.9f,0.85f,0.6f,0.0f});
    DrawScreenQuad(swapX+10,swapY+btnH-11,btnW-20,3,{0.9f,0.85f,0.6f,0.0f});

    float halfW=(panW-30)/2;
    float leftX=px+10,rightX=px+halfW+20;
    float unitY=py+80;
    float cardW=34,cardH=50,cardGap=4;

    const Faction*fa=map.GetFaction(a->factionId);
    const Faction*fb=map.GetFaction(b->factionId);
    glm::vec3 colA=fa?fa->color:glm::vec3(0.5f);
    glm::vec3 colB=fb?fb->color:glm::vec3(0.5f);

    // Army A header + unit count bar
    DrawScreenQuad(leftX,py+48,halfW,6,{colA.r,colA.g,colA.b,1});
    DrawScreenText(a->generalName,leftX,py+56,1.1f,{0.9f,0.85f,0.7f,0.95f});
    DrawScreenText(std::to_string((int)a->units.size())+"/20 units",leftX,py+68,0.9f,{0.7f,0.65f,0.55f,0.8f});

    // Army A units
    for(int i=0;i<(int)a->units.size();i++){
        float ux=leftX+(cardW+cardGap)*(i%6);
        float uy=unitY+(cardH+cardGap)*(i/6);
        bool sel=(i<(int)selA.size()&&selA[i]);
        DrawScreenQuad(ux,uy,cardW,cardH,sel?glm::vec4(0.4f,0.7f,0.3f,0.9f):glm::vec4(0.2f,0.18f,0.14f,0.8f));
        glm::vec4 tc={0.4f,0.4f,0.5f,0.9f};
        if(a->units[i].type==UnitType::LINE_INFANTRY)tc={0.3f,0.4f,0.8f,0.9f};
        if(a->units[i].type==UnitType::GRENADIERS)tc={0.8f,0.3f,0.3f,0.9f};
        if(a->units[i].type==UnitType::DRAGOONS||a->units[i].type==UnitType::HUSSARS)tc={0.3f,0.7f,0.3f,0.9f};
        if(a->units[i].type==UnitType::CANNON_6PDR||a->units[i].type==UnitType::CANNON_12PDR)tc={0.7f,0.6f,0.3f,0.9f};
        float hp=(float)a->units[i].stats.manpower/150.0f;
        DrawScreenQuad(ux+3,uy+3+(cardH-6)*(1-hp),cardW-6,(cardH-6)*hp,tc);
    }

    // Army B header
    DrawScreenQuad(rightX,py+48,halfW,6,{colB.r,colB.g,colB.b,1});
    DrawScreenText(b->generalName,rightX,py+56,1.1f,{0.9f,0.85f,0.7f,0.95f});
    DrawScreenText(std::to_string((int)b->units.size())+"/20 units",rightX,py+68,0.9f,{0.7f,0.65f,0.55f,0.8f});

    // Army B units
    for(int i=0;i<(int)b->units.size();i++){
        float ux=rightX+(cardW+cardGap)*(i%6);
        float uy=unitY+(cardH+cardGap)*(i/6);
        bool sel=(i<(int)selB.size()&&selB[i]);
        DrawScreenQuad(ux,uy,cardW,cardH,sel?glm::vec4(0.4f,0.7f,0.3f,0.9f):glm::vec4(0.2f,0.18f,0.14f,0.8f));
        glm::vec4 tc={0.4f,0.4f,0.5f,0.9f};
        if(b->units[i].type==UnitType::LINE_INFANTRY)tc={0.3f,0.4f,0.8f,0.9f};
        if(b->units[i].type==UnitType::GRENADIERS)tc={0.8f,0.3f,0.3f,0.9f};
        if(b->units[i].type==UnitType::DRAGOONS||b->units[i].type==UnitType::HUSSARS)tc={0.3f,0.7f,0.3f,0.9f};
        if(b->units[i].type==UnitType::CANNON_6PDR||b->units[i].type==UnitType::CANNON_12PDR)tc={0.7f,0.6f,0.3f,0.9f};
        float hp=(float)b->units[i].stats.manpower/150.0f;
        DrawScreenQuad(ux+3,uy+3+(cardH-6)*(1-hp),cardW-6,(cardH-6)*hp,tc);
    }

    // Accept button (green, left of center)
    float abtnW=70,abtnH=30;
    float acceptX=px+panW/2-abtnW-40,acceptY=py+panH-45;
    DrawScreenQuad(acceptX,acceptY,abtnW,abtnH,{0.15f,0.45f,0.15f,0.9f});
    DrawScreenQuad(acceptX+2,acceptY+2,abtnW-4,abtnH-4,{0.2f,0.6f,0.2f,0.9f});
    DrawScreenText("Accept",acceptX+8,acceptY+8,1.2f,{0.95f,0.92f,0.8f,1});

    // Cancel button (red, right of center)
    float cancelX=px+panW/2+40;
    DrawScreenQuad(cancelX,acceptY,abtnW,abtnH,{0.5f,0.12f,0.12f,0.9f});
    DrawScreenQuad(cancelX+2,acceptY+2,abtnW-4,abtnH-4,{0.65f,0.18f,0.18f,0.9f});
    DrawScreenText("Cancel",cancelX+8,acceptY+8,1.2f,{0.95f,0.85f,0.8f,1});

    // Selection count text
    int selCountA=0,selCountB=0;
    for(bool s:selA)if(s)selCountA++;
    for(bool s:selB)if(s)selCountB++;
    if(selCountA>0)DrawScreenText(std::to_string(selCountA)+" selected",leftX,py+panH-68,1.0f,{0.5f,0.85f,0.4f,0.9f});
    if(selCountB>0)DrawScreenText(std::to_string(selCountB)+" selected",rightX,py+panH-68,1.0f,{0.5f,0.85f,0.4f,0.9f});

    glEnable(GL_DEPTH_TEST);glEnable(GL_STENCIL_TEST);
}

void Renderer::RenderBattle(const BattleScene& battle){
    // Campaign map is already rendered behind us — just overlay the battle UI
    glDisable(GL_DEPTH_TEST);glDisable(GL_STENCIL_TEST);

    float sw=(float)m_width,sh=(float)m_height;
    float cx=sw/2;
    const auto&atk=battle.GetAttackerSnap();
    const auto&def=battle.GetDefenderSnap();

    // Helper lambda for unit type color
    auto unitCol=[](UnitType t)->glm::vec4{
        if(t==UnitType::LINE_INFANTRY)return{0.3f,0.4f,0.8f,0.9f};
        if(t==UnitType::GRENADIERS)return{0.8f,0.3f,0.3f,0.9f};
        if(t==UnitType::DRAGOONS||t==UnitType::HUSSARS)return{0.3f,0.7f,0.3f,0.9f};
        if(t==UnitType::CANNON_6PDR||t==UnitType::CANNON_12PDR)return{0.7f,0.6f,0.3f,0.9f};
        return{0.4f,0.4f,0.5f,0.9f};
    };

    if(battle.GetPhase()==BattlePhase::PRE_BATTLE){
        // ═══════════════════════════════════════════════════════
        // PRE-BATTLE DEPLOYMENT SCREEN
        // ═══════════════════════════════════════════════════════

        // Full-screen dim overlay
        DrawScreenQuad(0,0,sw,sh,{0.05f,0.03f,0.02f,0.7f});

        // Top banner
        float banH=50;
        DrawScreenQuad(0,0,sw,banH,{0.18f,0.14f,0.10f,0.95f});
        DrawScreenQuad(cx-150,5,300,banH-10,{0.25f,0.2f,0.15f,0.9f});
        DrawScreenText("Battle Deployment",cx-100,15,2.0f,{0.95f,0.9f,0.75f,1});

        // ── Left panel: Your Forces ──
        float lpW=sw*0.28f,lpH=sh-banH-160;
        float lpX=10,lpY=banH+10;
        DrawScreenQuad(lpX,lpY,lpW,lpH,{0.12f,0.10f,0.08f,0.9f});
        DrawScreenQuad(lpX+2,lpY+2,lpW-4,lpH-4,{0.3f,0.25f,0.18f,0.9f});

        // Attacker info
        DrawScreenQuad(lpX+5,lpY+5,lpW-10,6,{atk.factionColor.r,atk.factionColor.g,atk.factionColor.b,1});
        DrawScreenText(atk.generalName,lpX+10,lpY+14,1.2f,{0.95f,0.9f,0.7f,1});
        DrawScreenText(std::to_string(atk.totalManpower)+" men",lpX+10,lpY+28,1.0f,{0.8f,0.75f,0.6f,0.9f});

        // Manpower bar
        DrawScreenQuad(lpX+10,lpY+20,lpW-20,10,{0.15f,0.12f,0.08f,0.6f});
        float maxMen=(float)std::max({atk.totalManpower,def.totalManpower,1});
        DrawScreenQuad(lpX+10,lpY+20,(lpW-20)*atk.totalManpower/maxMen,10,
            {atk.factionColor.r,atk.factionColor.g,atk.factionColor.b,0.8f});

        // Unit cards
        float cardW=32,cardH=45,cardGap=3;
        for(int i=0;i<(int)atk.units.size();i++){
            float ux=lpX+8+(cardW+cardGap)*(i%7);
            float uy=lpY+40+(cardH+cardGap)*(i/7);
            DrawScreenQuad(ux,uy,cardW,cardH,{0.15f,0.12f,0.08f,0.7f});
            glm::vec4 tc=unitCol(atk.units[i].type);
            float hp=(float)atk.units[i].manpowerBefore/150.0f;
            DrawScreenQuad(ux+2,uy+2+(cardH-4)*(1-hp),cardW-4,(cardH-4)*hp,tc);
        }

        // ── Right panel: Enemy Forces ──
        float rpX=sw-lpW-10;
        DrawScreenQuad(rpX,lpY,lpW,lpH,{0.12f,0.10f,0.08f,0.9f});
        DrawScreenQuad(rpX+2,lpY+2,lpW-4,lpH-4,{0.3f,0.25f,0.18f,0.9f});

        DrawScreenQuad(rpX+5,lpY+5,lpW-10,6,{def.factionColor.r,def.factionColor.g,def.factionColor.b,1});
        DrawScreenText(def.generalName,rpX+10,lpY+14,1.2f,{0.95f,0.9f,0.7f,1});
        DrawScreenText(std::to_string(def.totalManpower)+" men",rpX+10,lpY+28,1.0f,{0.8f,0.75f,0.6f,0.9f});
        DrawScreenQuad(rpX+10,lpY+40,lpW-20,10,{0.15f,0.12f,0.08f,0.6f});
        DrawScreenQuad(rpX+10,lpY+40,(lpW-20)*def.totalManpower/maxMen,10,
            {def.factionColor.r,def.factionColor.g,def.factionColor.b,0.8f});

        for(int i=0;i<(int)def.units.size();i++){
            float ux=rpX+8+(cardW+cardGap)*(i%7);
            float uy=lpY+55+(cardH+cardGap)*(i/7);
            DrawScreenQuad(ux,uy,cardW,cardH,{0.15f,0.12f,0.08f,0.7f});
            glm::vec4 tc=unitCol(def.units[i].type);
            float hp=(float)def.units[i].manpowerBefore/150.0f;
            DrawScreenQuad(ux+2,uy+2+(cardH-4)*(1-hp),cardW-4,(cardH-4)*hp,tc);
        }

        // ── Center: Battle details panel ──
        float cpW=sw*0.35f,cpH=130;
        float cpX=(sw-cpW)/2,cpY=sh-cpH-40;
        DrawScreenQuad(cpX,cpY,cpW,cpH,{0.18f,0.14f,0.10f,0.95f});
        DrawScreenQuad(cpX+2,cpY+2,cpW-4,cpH-4,{0.3f,0.25f,0.18f,0.95f});

        // Predicted outcome label + bar
        DrawScreenText("Predicted Outcome",cpX+15,cpY+5,1.0f,{0.8f,0.75f,0.6f,0.9f});
        float predW=cpW-30;
        float pred=battle.GetPredictedOutcome();
        DrawScreenQuad(cpX+15,cpY+20,predW,16,{0.15f,0.12f,0.08f,0.6f});
        DrawScreenQuad(cpX+15,cpY+20,predW*pred,16,{0.2f,0.65f,0.2f,0.9f});
        DrawScreenQuad(cpX+15+predW*pred,cpY+20,predW*(1-pred),16,{0.7f,0.15f,0.15f,0.9f});
        DrawScreenQuad(cpX+15+predW*0.5f-1,cpY+18,2,20,{0.9f,0.85f,0.7f,0.9f});
        int pctStr=(int)(pred*100);
        DrawScreenText(std::to_string(pctStr)+"%",cpX+15+predW*pred-10,cpY+38,0.9f,{0.9f,0.9f,0.8f,0.8f});

        // Three buttons
        float btnW=cpW/3-20,btnH=35;
        float btnY2=cpY+cpH-50;

        // Fight button (disabled/grey for now)
        float btn1X=cpX+15;
        DrawScreenQuad(btn1X,btnY2,btnW,btnH,{0.3f,0.25f,0.2f,0.5f});
        DrawScreenQuad(btn1X+2,btnY2+2,btnW-4,btnH-4,{0.4f,0.35f,0.3f,0.4f});
        DrawScreenText("Fight",btn1X+btnW/2-20,btnY2+10,1.2f,{0.6f,0.55f,0.5f,0.5f});

        // Auto-resolve button (gold)
        float btn2X=cpX+cpW/3+5;
        DrawScreenQuad(btn2X,btnY2,btnW,btnH,{0.5f,0.4f,0.1f,0.9f});
        DrawScreenQuad(btn2X+2,btnY2+2,btnW-4,btnH-4,{0.7f,0.55f,0.15f,0.9f});
        DrawScreenText("Resolve",btn2X+btnW/2-28,btnY2+10,1.2f,{0.95f,0.9f,0.7f,1});

        // Retreat button (dark red)
        float btn3X=cpX+2*cpW/3-5;
        DrawScreenQuad(btn3X,btnY2,btnW,btnH,{0.5f,0.12f,0.08f,0.9f});
        DrawScreenQuad(btn3X+2,btnY2+2,btnW-4,btnH-4,{0.65f,0.18f,0.12f,0.9f});
        DrawScreenText("Retreat",btn3X+btnW/2-28,btnY2+10,1.2f,{0.95f,0.85f,0.8f,1});

    } else {
        // ═══════════════════════════════════════════════════════
        // POST-BATTLE RESULTS SCREEN
        // ═══════════════════════════════════════════════════════
        DrawScreenQuad(0,0,sw,sh,{0.05f,0.03f,0.02f,0.6f}); // dim overlay
        bool atkWon=battle.AttackerWon();

        float panW=sw*0.65f,panH=sh*0.75f;
        float px=(sw-panW)/2,py=(sh-panH)/2;
        DrawScreenQuad(px,py,panW,panH,{0.18f,0.14f,0.10f,0.95f});
        DrawScreenQuad(px+3,py+3,panW-6,panH-6,{0.35f,0.28f,0.20f,0.95f});

        // Title: Victory/Defeat
        float titleH=40;
        DrawScreenQuad(px+panW/2-120,py+8,240,titleH,{0.25f,0.2f,0.15f,0.9f});
        if(atkWon){
            DrawScreenQuad(px+panW/2-115,py+12,230,titleH-8,{0.15f,0.45f,0.15f,0.8f});
            DrawScreenText("VICTORY!",px+panW/2-48,py+18,2.0f,{0.95f,0.92f,0.7f,1});
        } else {
            DrawScreenQuad(px+panW/2-115,py+12,230,titleH-8,{0.55f,0.12f,0.12f,0.8f});
            DrawScreenText("DEFEAT",px+panW/2-36,py+18,2.0f,{0.95f,0.85f,0.8f,1});
        }

        // Battle Results header
        float tableY=py+60;
        DrawScreenQuad(px+15,tableY,panW-30,80,{0.25f,0.2f,0.15f,0.8f});
        DrawScreenText("Battle Results",px+20,tableY+2,1.2f,{0.85f,0.8f,0.65f,0.9f});

        // Column headers
        float col1=px+30,col2=px+200,col3=px+310,col4=px+400;
        DrawScreenText("General",col1,tableY+16,1.0f,{0.7f,0.65f,0.55f,0.8f});
        DrawScreenText("Deployed",col2,tableY+16,1.0f,{0.7f,0.65f,0.55f,0.8f});
        DrawScreenText("Lost",col3,tableY+16,1.0f,{0.7f,0.65f,0.55f,0.8f});
        DrawScreenText("Remaining",col4,tableY+16,1.0f,{0.7f,0.65f,0.55f,0.8f});

        // Attacker stats row
        DrawScreenQuad(px+20,tableY+30,8,16,{atk.factionColor.r,atk.factionColor.g,atk.factionColor.b,1});
        DrawScreenText(atk.generalName,col1,tableY+32,1.0f,{0.9f,0.85f,0.7f,1});
        DrawScreenText(std::to_string(atk.totalManpower),col2,tableY+32,1.0f,{0.85f,0.82f,0.7f,0.9f});
        float atkLoss=(float)battle.GetResult().attackerCasualties;
        DrawScreenText(std::to_string((int)atkLoss),col3,tableY+32,1.0f,{0.85f,0.3f,0.25f,0.9f});
        DrawScreenText(std::to_string(atk.totalManpower-(int)atkLoss),col4,tableY+32,1.0f,{0.8f,0.8f,0.7f,0.9f});

        // Defender stats row
        DrawScreenQuad(px+20,tableY+52,8,16,{def.factionColor.r,def.factionColor.g,def.factionColor.b,1});
        DrawScreenText(def.generalName,col1,tableY+54,1.0f,{0.9f,0.85f,0.7f,1});
        DrawScreenText(std::to_string(def.totalManpower),col2,tableY+54,1.0f,{0.85f,0.82f,0.7f,0.9f});
        float defLoss=(float)battle.GetResult().defenderCasualties;
        DrawScreenText(std::to_string((int)defLoss),col3,tableY+54,1.0f,{0.85f,0.3f,0.25f,0.9f});
        DrawScreenText(std::to_string(def.totalManpower-(int)defLoss),col4,tableY+54,1.0f,{0.8f,0.8f,0.7f,0.9f});

        // Unit review header
        DrawScreenQuad(px+15,tableY+85,panW-30,25,{0.25f,0.2f,0.15f,0.9f});
        DrawScreenText("Unit Review",px+20,tableY+90,1.2f,{0.85f,0.8f,0.65f,0.9f});

        // Unit cards (your army review)
        float cardW=34,cardH=55,cardGap=4;
        float unitY=tableY+115;
        int colsPerRow=std::max(1,(int)((panW-40)/(cardW+cardGap)));

        for(int i=0;i<(int)atk.units.size();i++){
            float ux=px+20+(cardW+cardGap)*(i%colsPerRow);
            float uy=unitY+(cardH+cardGap)*(i/colsPerRow);
            DrawScreenQuad(ux,uy,cardW,cardH,{0.15f,0.12f,0.08f,0.7f});
            glm::vec4 tc=unitCol(atk.units[i].type);
            float hpB=(float)atk.units[i].manpowerBefore/150.0f;
            float hpA=(float)atk.units[i].manpowerAfter/150.0f;
            DrawScreenQuad(ux+2,uy+2,cardW-4,(cardH-4)*hpB,{tc.r*0.3f,tc.g*0.3f,tc.b*0.3f,0.4f});
            float aH=(cardH-4)*hpA;
            DrawScreenQuad(ux+2,uy+2+(cardH-4)-aH,cardW-4,aH,tc);
            // Manpower number below card
            DrawScreenText(std::to_string(atk.units[i].manpowerAfter),ux,uy+cardH+1,0.8f,{0.7f,0.7f,0.6f,0.8f});
            if(atk.units[i].destroyed){
                DrawScreenQuad(ux+4,uy+cardH/2-1,cardW-8,3,{0.9f,0.1f,0.1f,0.9f});
                DrawScreenQuad(ux+cardW/2-1,uy+4,3,cardH-8,{0.9f,0.1f,0.1f,0.9f});
            }
        }

        // Click to continue (pulsing)
        float pulse=0.5f+0.5f*sin(m_time*3.0f);
        DrawScreenQuad(cx-60,py+panH-38,120,28,{0.4f,0.35f,0.2f,0.8f*pulse});
        DrawScreenText("Continue",cx-40,py+panH-32,1.3f,{0.95f,0.9f,0.7f,pulse});
    }

    glEnable(GL_DEPTH_TEST);glEnable(GL_STENCIL_TEST);
}
void Renderer::OnResize(int w,int h){m_width=w;m_height=h;glViewport(0,0,w,h);if(m_camera)m_camera->OnResize((float)w/h);}


void Renderer::ClearMapGeometry() {
    for (auto& [id, g] : m_provinceGPUs) {
        if (g.VAO) glDeleteVertexArrays(1, &g.VAO);
        if (g.VBO) glDeleteBuffers(1, &g.VBO);
        if (g.borderVAO) glDeleteVertexArrays(1, &g.borderVAO);
        if (g.borderVBO) glDeleteBuffers(1, &g.borderVBO);
    }
    m_provinceGPUs.clear();
    for (auto& g : m_obstacleGPUs) {
        if (g.VAO) glDeleteVertexArrays(1, &g.VAO);
        if (g.VBO) glDeleteBuffers(1, &g.VBO);
    }
    m_obstacleGPUs.clear();
    for (auto& g : m_foreignGPUs) {
        if (g.VAO) glDeleteVertexArrays(1, &g.VAO);
        if (g.VBO) glDeleteBuffers(1, &g.VBO);
    }
    m_foreignGPUs.clear();
}

void Renderer::RenderEditorOverlay(const CampaignMap& map, int selProvIdx, int selVertIdx) {
    glDisable(GL_DEPTH_TEST);
    m_overlayShader->Use();
    m_overlayShader->SetMat4("u_VP", m_camera->GetViewProjectionMatrix());

    auto& provs = map.GetProvinces();

    // Draw all vertices as small dots
    for (int pi = 0; pi < (int)provs.size(); pi++) {
        for (int vi = 0; vi < (int)provs[pi].borderVertices.size(); vi++) {
            glm::vec3 v = provs[pi].borderVertices[vi];
            float th = map.GetBaseTerrainHeight(v.x, v.z);
            glm::vec3 pos(v.x, th + 0.15f, v.z);

            bool isSelected = (pi == selProvIdx && vi == selVertIdx);
            bool isProvSelected = (pi == selProvIdx);
            float size = isSelected ? 0.12f : 0.06f;

            glm::mat4 model = glm::translate(glm::mat4(1), pos);
            model = glm::scale(model, glm::vec3(size));
            m_overlayShader->SetMat4("u_Model", model);

            if (isSelected)
                m_overlayShader->SetVec4("u_Color", {1.0f, 0.2f, 0.2f, 1.0f});
            else if (isProvSelected)
                m_overlayShader->SetVec4("u_Color", {1.0f, 0.8f, 0.2f, 0.9f});
            else
                m_overlayShader->SetVec4("u_Color", {0.3f, 0.9f, 0.3f, 0.6f});

            glBindVertexArray(m_circleVAO);
            glLineWidth(isSelected ? 4.0f : 2.0f);
            glDrawArrays(GL_LINE_STRIP, 0, m_circleVerts);
        }
    }

    // Draw obstacle vertices too (grey)
    for (const auto& ob : map.GetObstacles()) {
        for (const auto& v : ob.vertices) {
            float th = map.GetTerrainHeight(v.x, v.z);
            glm::vec3 pos(v.x, th + 0.15f, v.z);
            glm::mat4 model = glm::translate(glm::mat4(1), pos);
            model = glm::scale(model, glm::vec3(0.05f));
            m_overlayShader->SetMat4("u_Model", model);
            m_overlayShader->SetVec4("u_Color", {0.7f, 0.7f, 0.7f, 0.5f});
            glBindVertexArray(m_circleVAO);
            glLineWidth(1.5f);
            glDrawArrays(GL_LINE_STRIP, 0, m_circleVerts);
        }
    }

    glBindVertexArray(0);
    glLineWidth(1);
    glEnable(GL_DEPTH_TEST);
}