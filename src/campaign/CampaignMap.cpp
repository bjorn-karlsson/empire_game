// ============================================================================
// CampaignMap.cpp — NavGrid A*, flood-fill movement mesh, multi-turn paths
// ============================================================================
#include "campaign/CampaignMap.h"
#include "core/InputManager.h"
#include "utils/Logger.h"
#include <algorithm>
#include <cmath>
#include <queue>
#include <unordered_set>
#include <unordered_map>

CampaignMap::CampaignMap()=default;
CampaignMap::~CampaignMap()=default;
bool CampaignMap::LoadFromFile(const std::string&){return false;}

static bool PtInPoly(const glm::vec2&pt,const std::vector<glm::vec3>&v){
    bool in=false;int n=(int)v.size();
    for(int i=0,j=n-1;i<n;j=i++){
        if(((v[i].z>pt.y)!=(v[j].z>pt.y))&&(pt.x<(v[j].x-v[i].x)*(pt.y-v[i].z)/(v[j].z-v[i].z)+v[i].x))in=!in;
    }return in;
}
static glm::vec3 Centroid(const std::vector<glm::vec3>&v){glm::vec3 c(0);for(auto&p:v)c+=p;return c/(float)v.size();}

bool CampaignMap::IsPointPassable(const glm::vec3&pos)const{
    glm::vec2 pt(pos.x,pos.z);
    for(auto&ob:m_obstacles)if(PtInPoly(pt,ob.vertices))return false;
    return IsPointOnLand(pos);
}
bool CampaignMap::IsPointOnLand(const glm::vec3& pos)const {
    glm::vec2 pt(pos.x, pos.z);
    for (auto& p : m_provinces)if (PtInPoly(pt, p.borderVertices))return true;
    for (auto& ft : m_foreignTerritories)if (PtInPoly(pt, ft.vertices))return true;
    return false;
}

// ═══════════════════════════════════════════════════════════════
// NAV GRID
// ═══════════════════════════════════════════════════════════════
void CampaignMap::BuildNavGrid(){
    Logger::Info("Building navigation grid (%dx%d, cell=%.2f)...",NavGrid::W,NavGrid::H,NavGrid::CELL);
    for(int gx=0;gx<NavGrid::W;gx++) for(int gz=0;gz<NavGrid::H;gz++){
        glm::vec3 wp = m_navGrid.toWorld(gx,gz);
        m_navGrid.passable[gx][gz] = IsPointPassable(wp);
    }
    int passCount=0;
    for(int gx=0;gx<NavGrid::W;gx++) for(int gz=0;gz<NavGrid::H;gz++) if(m_navGrid.passable[gx][gz]) passCount++;
    Logger::Info("NavGrid: %d passable / %d total cells",passCount,NavGrid::W*NavGrid::H);
}

// ═══════════════════════════════════════════════════════════════
// A* PATHFINDING ON NAV GRID
// ═══════════════════════════════════════════════════════════════
std::vector<glm::vec3> CampaignMap::FindPathWorld(const glm::vec3& from, glm::vec3 to,
    int movingArmyId, int targetArmyId, int targetCityProvId)const
{
    int sx = m_navGrid.toGX(from.x), sz = m_navGrid.toGZ(from.z);
    int ex = m_navGrid.toGX(to.x), ez = m_navGrid.toGZ(to.z);

    if (!m_navGrid.inBounds(sx, sz) || !m_navGrid.inBounds(ex, ez))return{};

    // ── Build dynamic obstacle set ──
    auto key = [](int x, int z)->int {return x * NavGrid::H + z; };
    std::unordered_set<int> dynBlocked;

    // Block cells occupied by OTHER armies (not the one moving, not its target)
    for (const auto& a : m_armies) {
        if (a.id == movingArmyId || a.id == targetArmyId)continue;
        if (a.units.empty() || a.isGarrisoned)continue;
        int ax = m_navGrid.toGX(a.worldPosition.x), az = m_navGrid.toGZ(a.worldPosition.z);
        dynBlocked.insert(key(ax, az));
    }

    // Block city cells (so armies path around, not through)
    // Exclude the target city (so army can enter it)
    for (const auto& p : m_provinces) {
        if (p.id == targetCityProvId)continue;
        int cx = m_navGrid.toGX(p.cityPos.x), cz = m_navGrid.toGZ(p.cityPos.z);
        dynBlocked.insert(key(cx, cz));
    }

    // Never block start or destination
    dynBlocked.erase(key(sx, sz));
    dynBlocked.erase(key(ex, ez));

    // If destination is impassable (terrain), snap to nearest passable+unblocked cell
    if (!m_navGrid.passable[ex][ez]) {
        float bestD = 999; int bx = ex, bz = ez;
        for (int dx = -15; dx <= 15; dx++)for (int dz = -15; dz <= 15; dz++) {
            int cx = ex + dx, cz = ez + dz;
            if (!m_navGrid.inBounds(cx, cz) || !m_navGrid.passable[cx][cz])continue;
            if (dynBlocked.count(key(cx, cz)))continue;
            float d = std::sqrt((float)(dx * dx + dz * dz));
            if (d < bestD) { bestD = d; bx = cx; bz = cz; }
        }
        if (bestD > 998)return{};
        ex = bx; ez = bz;
        to = glm::vec3(m_navGrid.toWX(ex), 0, m_navGrid.toWZ(ez));
    }

    // If destination is dynamically blocked (army/city in the way), also snap
    if (dynBlocked.count(key(ex, ez))) {
        float bestD = 999; int bx = ex, bz = ez;
        for (int dx = -10; dx <= 10; dx++)for (int dz = -10; dz <= 10; dz++) {
            int cx = ex + dx, cz = ez + dz;
            if (!m_navGrid.inBounds(cx, cz) || !m_navGrid.passable[cx][cz])continue;
            if (dynBlocked.count(key(cx, cz)))continue;
            float d = std::sqrt((float)(dx * dx + dz * dz));
            if (d < bestD) { bestD = d; bx = cx; bz = cz; }
        }
        if (bestD > 998)return{};
        ex = bx; ez = bz;
        to = glm::vec3(m_navGrid.toWX(ex), 0, m_navGrid.toWZ(ez));
    }

    if (sx == ex && sz == ez)return{ from,to };

    // A* with 8-directional movement
    struct Node { int x, z; float g, f; };
    auto heur = [&](int x, int z)->float {return std::sqrt((float)((x - ex) * (x - ex) + (z - ez) * (z - ez))) * NavGrid::CELL; };

    auto cmp = [](const Node& a, const Node& b) {return a.f > b.f; };
    std::priority_queue<Node, std::vector<Node>, decltype(cmp)> open(cmp);
    std::unordered_map<int, float> bestG;
    std::unordered_map<int, int> parent;

    open.push({ sx,sz,0,heur(sx,sz) });
    bestG[key(sx, sz)] = 0;
    parent[key(sx, sz)] = -1;

    int dx8[] = { -1,0,1,-1,1,-1,0,1 };
    int dz8[] = { -1,-1,-1,0,0,1,1,1 };
    float dcost[] = { 1.414f,1,1.414f,1,1,1.414f,1,1.414f };

    bool found = false;
    while (!open.empty()) {
        Node cur = open.top(); open.pop();
        if (cur.x == ex && cur.z == ez) { found = true; break; }

        int ck = key(cur.x, cur.z);
        if (cur.g > bestG[ck] + 0.001f)continue;

        for (int d = 0; d < 8; d++) {
            int nx = cur.x + dx8[d], nz = cur.z + dz8[d];
            if (!m_navGrid.inBounds(nx, nz) || !m_navGrid.passable[nx][nz])continue;
            if (dynBlocked.count(key(nx, nz)))continue;

            if (dx8[d] != 0 && dz8[d] != 0) {
                if (!m_navGrid.passable[cur.x + dx8[d]][cur.z] || !m_navGrid.passable[cur.x][cur.z + dz8[d]])
                    continue;
            }

            float ng = cur.g + dcost[d] * NavGrid::CELL;
            int nk = key(nx, nz);
            if (bestG.find(nk) == bestG.end() || ng < bestG[nk]) {
                bestG[nk] = ng;
                parent[nk] = ck;
                open.push({ nx,nz,ng,ng + heur(nx,nz) });
            }
        }
    }

    if (!found)return{};

    // Reconstruct path
    std::vector<glm::vec3> path;
    int ck = key(ex, ez);
    while (ck != -1) {
        int px = ck / NavGrid::H, pz = ck % NavGrid::H;
        path.push_back(m_navGrid.toWorld(px, pz));
        ck = parent[ck];
    }
    std::reverse(path.begin(), path.end());

    if (!path.empty()) { path.front() = glm::vec3(from.x, 0, from.z); path.back() = glm::vec3(to.x, 0, to.z); }

    // Simplify: remove collinear waypoints
    if (path.size() > 2) {
        std::vector<glm::vec3> simplified;
        simplified.push_back(path[0]);
        for (int i = 1; i < (int)path.size() - 1; i++) {
            glm::vec2 d1 = glm::normalize(glm::vec2(path[i].x - path[i - 1].x, path[i].z - path[i - 1].z));
            glm::vec2 d2 = glm::normalize(glm::vec2(path[i + 1].x - path[i].x, path[i + 1].z - path[i].z));
            if (glm::dot(d1, d2) < 0.98f) simplified.push_back(path[i]);
        }
        simplified.push_back(path.back());
        path = simplified;
    }

    return path;
}
// ═══════════════════════════════════════════════════════════════
// FLOOD-FILL REACHABLE CELLS (Dijkstra from army position)
// ═══════════════════════════════════════════════════════════════
std::vector<ReachableCell> CampaignMap::GetReachableCells(int armyId)const{
    const Army*a=GetArmy(armyId);
    if(!a||a->movementRange<0.05f)return{};

    int sx=m_navGrid.toGX(a->worldPosition.x),sz=m_navGrid.toGZ(a->worldPosition.z);
    if(!m_navGrid.inBounds(sx,sz))return{};

    struct Entry{int gx,gz;float dist;bool operator>(const Entry&o)const{return dist>o.dist;}};
    std::priority_queue<Entry,std::vector<Entry>,std::greater<Entry>> pq;
    auto key=[](int x,int z)->int{return x*NavGrid::H+z;};
    std::unordered_map<int,float> best;

    pq.push({sx,sz,0});
    best[key(sx,sz)]=0;

    int dx8[]={-1,0,1,-1,1,-1,0,1};
    int dz8[]={-1,-1,-1,0,0,1,1,1};
    float dcost[]={1.414f,1,1.414f,1,1,1.414f,1,1.414f};

    std::vector<ReachableCell> result;

    while(!pq.empty()){
        auto[gx,gz,dist]=pq.top();pq.pop();
        int k=key(gx,gz);
        if(dist>best[k]+0.001f)continue;
        if(dist>a->movementRange)continue;

        result.push_back({gx,gz,dist});

        for(int d=0;d<8;d++){
            int nx=gx+dx8[d],nz=gz+dz8[d];
            if(!m_navGrid.inBounds(nx,nz)||!m_navGrid.passable[nx][nz])continue;
            if(dx8[d]!=0&&dz8[d]!=0){
                if(!m_navGrid.passable[gx+dx8[d]][gz]||!m_navGrid.passable[gx][gz+dz8[d]])continue;
            }
            float nd=dist+dcost[d]*NavGrid::CELL;
            int nk=key(nx,nz);
            if(nd<=a->movementRange&&(best.find(nk)==best.end()||nd<best[nk])){
                best[nk]=nd;
                pq.push({nx,nz,nd});
            }
        }
    }
    return result;
}

// ═══════════════════════════════════════════════════════════════
// MAP GENERATION (same provinces + obstacles as before)
// ═══════════════════════════════════════════════════════════════
void CampaignMap::GenerateTestMap(){
    Logger::Info("Generating France campaign...");

    {Faction f;f.id="france";f.name="France";f.leaderName="Louis XV";
     f.color={0.28f,0.38f,0.75f};f.isPlayerControlled=true;f.treasury=12000;
     m_factions.push_back(f);}

    // ═══════════════════════════════════════════════════════════
    // FRANCE MAP — Geographically accurate province outlines
    // Coordinate system: X=west(-)/east(+), Z=north(-)/south(+)
    // Coastline vertices (c=coast), Internal borders (i=internal)
    // ═══════════════════════════════════════════════════════════
    #define V(x,z) glm::vec3(x,0.0f,z)
    // ── Coastline (clockwise from Dunkirk) ──
    glm::vec3
        // North coast (Channel)
        cDunk=V(2.5,-5.5), cCalais=V(1.2,-5.8), cBoul=V(0.2,-5.3),
        cDiep=V(-1.0,-4.8), cLeHav=V(-2.2,-4.5), cCherb=V(-3.8,-5.2),
        // Brittany peninsula
        cStMalo=V(-4.8,-3.8), cStBri=V(-6.5,-3.5), cBrest=V(-8.2,-2.5),
        cQuimp=V(-7.8,-1.2), cLori=V(-6.8,-0.5),
        // West coast (Atlantic)
        cNant=V(-5.8,0.5), cRoch=V(-5.5,2.5), cBordW=V(-5.0,4.0),
        cBordS=V(-4.5,5.5),
        // Southwest coast (Bay of Biscay)
        cBiarr=V(-4.2,6.8), cBayo=V(-3.8,7.3),
        // Pyrenees border (south)
        cPyrW=V(-3.0,7.8), cPyrM=V(-1.0,8.2), cPyrE=V(0.8,8.0),
        cPerp=V(1.5,7.5),
        // Mediterranean coast
        cNarb=V(2.5,7.2), cMontp=V(3.5,6.8), cMars=V(4.8,6.5),
        cToulon=V(5.5,6.0), cNice=V(6.8,5.0),
        // Alps/East border (going north)
        cAlpS=V(7.0,3.5), cGenev=V(6.5,1.5), cJura=V(6.2,0.5),
        cBasel=V(6.5,-0.5), cStras=V(6.5,-2.0),
        cMetz=V(5.5,-3.5), cLille=V(3.0,-4.5);
    // ── Internal border junctions ──
    glm::vec3
        // Row 1: north tier
        iNrmBrt=V(-4.5,-2.5),  // Normandy-Brittany
        iNrmIdF=V(-1.5,-2.5),  // Normandy-IleDF
        iPicChm=V(2.0,-3.2),   // Picardy-Champagne
        // Row 2: mid-north
        iIdFLoi=V(-2.0,-0.5),  // IleDF-Loire
        iIdFBur=V(0.5,-0.5),   // IleDF-Burgundy
        iChmAlsW=V(4.2,-2.0),  // Champagne-Alsace west
        iAlsBurN=V(4.8,-0.5),  // Alsace-Burgundy north
        // Row 3: center
        iBrtPoi=V(-5.0,1.0),   // Brittany-Poitou
        iLoiPoi=V(-3.0,1.5),   // Loire-Poitou
        iLoiAuv=V(-0.5,1.5),   // Loire-Auvergne
        iBurDau=V(3.5,1.0),    // Burgundy-Dauphine
        iDauAlp=V(5.5,1.5),    // Dauphine-Alps junction
        // Row 4: south-center
        iPoiAqu=V(-3.5,3.5),   // Poitou-Aquitaine
        iAuvLan=V(1.0,4.5),    // Auvergne-Languedoc
        iAuvPro=V(3.0,4.0),    // Auvergne-Provence
        iDauPro=V(5.0,3.5),    // Dauphine-Provence
        // Row 5: south
        iAquGas=V(-3.0,5.5),   // Aquitaine-Gascony
        iAquLan=V(-0.5,6.0),   // Aquitaine-Languedoc
        iLanPro=V(3.5,5.5);    // Languedoc-Provence
    #undef V

    auto mkP=[&](int id,const std::string&nm,const std::string&cn,
        std::vector<glm::vec3>bv,int inc,bool co,bool cap,const std::string&ter="plains"){
        Province p;p.id=id;p.name=nm;p.ownerFactionId="france";
        p.borderVertices=bv;p.center=Centroid(bv);p.cityName=cn;p.cityPos=p.center;
        p.baseIncome=inc;p.isCoastal=co;p.isCapital=cap;p.terrain=ter;
        p.population=inc*80+5000;
        glm::vec3 bc={0.28f,0.38f,0.75f};
        if(ter=="forest")bc={0.22f,0.40f,0.60f};if(ter=="hills")bc={0.30f,0.36f,0.68f};
        if(ter=="mountains")bc={0.32f,0.34f,0.62f};if(ter=="marsh")bc={0.25f,0.42f,0.65f};
        p.color=bc;return p;
    };

    // Provinces (ordered to tile France from north to south)
    // 0: Île-de-France (Paris) — center-north
    m_provinces.push_back(mkP(0,"Ile-de-France","Paris",
        {iNrmIdF,iPicChm,iIdFBur,iLoiAuv,iIdFLoi},500,false,true));
    // 1: Normandy — northwest coast
    m_provinces.push_back(mkP(1,"Normandy","Rouen",
        {cCherb,cLeHav,cDiep,iNrmIdF,iIdFLoi,iNrmBrt,cStMalo},300,true,false));
    // 2: Brittany — far west peninsula
    m_provinces.push_back(mkP(2,"Brittany","Rennes",
        {cStMalo,iNrmBrt,iIdFLoi,iBrtPoi,cNant,cLori,cQuimp,cBrest,cStBri},200,true,false,"hills"));
    // 3: Picardy — northeast coast
    m_provinces.push_back(mkP(3,"Picardy","Amiens",
        {cDiep,cBoul,cCalais,cDunk,cLille,iPicChm,iNrmIdF},280,true,false));
    // 4: Champagne — north-center-east
    m_provinces.push_back(mkP(4,"Champagne","Reims",
        {iPicChm,cLille,cMetz,iChmAlsW,iAlsBurN,iIdFBur},250,false,false));
    // 5: Alsace-Lorraine — far east
    m_provinces.push_back(mkP(5,"Alsace-Lorraine","Strasbourg",
        {iChmAlsW,cMetz,cStras,cBasel,cJura,iDauAlp,iBurDau,iAlsBurN},220,false,false,"hills"));
    // 6: Loire Valley — center-west
    m_provinces.push_back(mkP(6,"Loire Valley","Tours",
        {iIdFLoi,iLoiAuv,iLoiPoi},320,false,false));
    m_provinces[6].cityPos={-1.5f,0,0.5f};
    // 7: Burgundy — center-east
    m_provinces.push_back(mkP(7,"Burgundy","Dijon",
        {iIdFBur,iAlsBurN,iBurDau,iAuvPro,iLoiAuv},280,false,false,"hills"));
    // 8: Poitou — west coast
    m_provinces.push_back(mkP(8,"Poitou","Poitiers",
        {iBrtPoi,iLoiPoi,iPoiAqu,cBordW,cRoch,cNant},180,true,false,"marsh"));
    // 9: Aquitaine — southwest
    m_provinces.push_back(mkP(9,"Aquitaine","Bordeaux",
        {iPoiAqu,iAquLan,iAquGas,cBordS,cBordW},350,true,false));
    m_provinces[9].cityPos={-3.8f,0,4.5f};
    // 10: Languedoc — south-center
    m_provinces.push_back(mkP(10,"Languedoc","Toulouse",
        {iAquLan,iAuvLan,iLanPro,cMontp,cNarb,cPerp,cPyrE,cPyrM,cPyrW,iAquGas},280,true,false));
    // 11: Provence — southeast coast
    m_provinces.push_back(mkP(11,"Provence","Marseille",
        {iLanPro,iAuvPro,iDauPro,cNice,cToulon,cMars,cMontp},300,true,false));
    // 12: Dauphiné — east mountains
    m_provinces.push_back(mkP(12,"Dauphine","Grenoble",
        {iBurDau,iDauAlp,cGenev,cAlpS,cNice,iDauPro,iAuvPro},180,false,false,"mountains"));
    // 13: Auvergne — center mountains
    m_provinces.push_back(mkP(13,"Auvergne","Clermont",
        {iLoiAuv,iAuvPro,iLanPro,iAuvLan,iAquLan,iPoiAqu,iLoiPoi},150,false,false,"mountains"));
    // 14: Gascony — far south (Pyrenees)
    m_provinces.push_back(mkP(14,"Gascony","Bayonne",
        {iAquGas,cPyrW,cBayo,cBiarr,cBordS},120,true,false,"mountains"));

    for(auto&p:m_provinces){float t=(float)(p.id%5)*0.025f;p.color.r+=t-0.05f;p.color.g+=t*0.3f;}

    auto adj=[&](int a,int b){m_provinces[a].neighborIds.push_back(b);m_provinces[b].neighborIds.push_back(a);};
    adj(0,1);adj(0,3);adj(0,4);adj(0,6);adj(0,7);adj(0,13);
    adj(1,2);adj(1,3);adj(1,6);adj(2,6);adj(2,8);adj(3,4);
    adj(4,5);adj(4,7);adj(5,7);adj(5,12);adj(6,8);adj(6,13);
    adj(7,12);adj(7,13);adj(8,9);adj(8,13);adj(9,10);adj(9,14);
    adj(10,11);adj(10,13);adj(10,14);adj(11,12);adj(11,13);adj(12,13);

    // Terrain obstacles (repositioned to match new map)
    #define V(x,z) glm::vec3(x,0.0f,z)
    m_obstacles.push_back({"Alps","mountain",
        {V(6.2f,2.0f),V(7.2f,2.5f),V(7.5f,3.8f),V(7.0f,4.8f),V(6.5f,4.5f),V(6.0f,3.0f)},
        {},{0.55f,0.50f,0.45f}});
    m_obstacles.push_back({"Pyrenees","mountain",
        {V(-3.5f,7.5f),V(-1.5f,8.0f),V(0.5f,8.2f),V(1.2f,7.8f),V(0.8f,8.5f),V(-1.0f,8.8f),V(-3.2f,8.5f),V(-3.8f,8.0f)},
        {},{0.50f,0.45f,0.40f}});
    m_obstacles.push_back({"Massif Central","mountain",
        {V(-0.2f,3.0f),V(1.2f,2.8f),V(1.8f,3.5f),V(1.5f,4.8f),V(0.5f,5.0f),V(-0.5f,4.2f)},
        {},{0.52f,0.48f,0.42f}});
    m_obstacles.push_back({"Jura","mountain",
        {V(5.8f,0.0f),V(6.4f,0.2f),V(6.6f,1.0f),V(6.2f,1.3f),V(5.6f,0.8f)},
        {},{0.53f,0.48f,0.43f}});
    m_obstacles.push_back({"Lac Leman","lake",
        {V(6.0f,1.2f),V(6.6f,1.0f),V(6.8f,1.5f),V(6.4f,1.8f),V(5.8f,1.5f)},
        {},{0.15f,0.30f,0.55f}});
    for(auto&ob:m_obstacles)ob.center=Centroid(ob.vertices);
    #undef V

    // ═══════════════════════════════════════════════════════════
    // FOREIGN TERRITORIES — Surrounding countries (18th century)
    // ═══════════════════════════════════════════════════════════
    #define V(x,z) glm::vec3(x,0.0f,z)
    // Austrian Netherlands (Belgium) — north of France
    m_foreignTerritories.push_back({"Austrian Netherlands",
        {V(0.2f,-5.8f),V(2.5f,-5.5f),V(3.0f,-4.5f),V(4.5f,-4.5f),V(5.5f,-5.0f),
         V(5.5f,-7.0f),V(3.0f,-7.5f),V(0.5f,-7.5f),V(-0.5f,-7.0f),V(-0.5f,-6.0f)},
        {},{0.60f,0.50f,0.35f}}); // brown/tan

    // United Provinces (Dutch Republic) — far north
    m_foreignTerritories.push_back({"United Provinces",
        {V(2.5f,-7.5f),V(5.0f,-7.5f),V(5.5f,-7.0f),V(5.5f,-9.0f),V(3.5f,-9.5f),
         V(1.5f,-9.0f),V(1.5f,-8.0f)},
        {},{0.75f,0.50f,0.20f}}); // orange

    // Holy Roman Empire (German states) — east
    m_foreignTerritories.push_back({"Holy Roman Empire",
        {V(5.5f,-5.0f),V(6.5f,-2.0f),V(6.5f,-0.5f),V(6.2f,0.5f),V(7.0f,0.5f),
         V(8.5f,0.0f),V(9.5f,-1.5f),V(9.5f,-4.0f),V(8.0f,-6.0f),V(5.5f,-7.0f)},
        {},{0.80f,0.75f,0.30f}}); // yellow

    // Swiss Confederation — small, east
    m_foreignTerritories.push_back({"Swiss Confederation",
        {V(6.5f,0.5f),V(7.0f,0.5f),V(8.2f,0.8f),V(8.5f,1.5f),V(7.5f,2.0f),
         V(6.5f,1.5f)},
        {},{0.80f,0.30f,0.25f}}); // red

    // Kingdom of Sardinia (Savoy-Piedmont) — southeast
    m_foreignTerritories.push_back({"Kingdom of Sardinia",
        {V(6.8f,2.0f),V(7.5f,2.0f),V(8.5f,2.5f),V(9.0f,4.0f),V(8.5f,5.5f),
         V(7.5f,5.5f),V(6.8f,5.0f),V(7.0f,3.5f)},
        {},{0.30f,0.60f,0.45f}}); // teal

    // Kingdom of Spain — south of Pyrenees
    m_foreignTerritories.push_back({"Kingdom of Spain",
        {V(-4.2f,7.3f),V(-3.0f,7.8f),V(-1.0f,8.2f),V(0.8f,8.0f),V(1.5f,7.5f),
         V(2.5f,8.0f),V(3.0f,9.5f),V(1.0f,11.0f),V(-2.0f,11.5f),V(-4.5f,11.0f),
         V(-6.0f,9.5f),V(-5.5f,8.0f),V(-4.5f,7.5f)},
        {},{0.70f,0.50f,0.20f}}); // warm brown/orange

    // England (small strip across Channel)
    m_foreignTerritories.push_back({"Kingdom of England",
        {V(-5.0f,-7.5f),V(-2.0f,-8.0f),V(0.5f,-7.5f),V(1.5f,-8.0f),
         V(2.0f,-9.5f),V(-1.0f,-10.0f),V(-4.0f,-9.5f),V(-6.0f,-8.5f)},
        {},{0.75f,0.25f,0.25f}}); // red

    for(auto&ft:m_foreignTerritories)ft.center=Centroid(ft.vertices);
    #undef V

    // Buildings
    m_provinces[0].buildings.push_back({"Royal Palace","government",3,200,0});
    m_provinces[0].buildings.push_back({"Paris Barracks","barracks",2,0,3});
    m_provinces[11].buildings.push_back({"Toulon Naval Base","port",3,100,2});
    m_provinces[5].buildings.push_back({"Strasbourg Fortress","fort",2,0,2});

    Faction*fr=GetFaction("france");
    for(auto&p:m_provinces){fr->ownedProvinces.push_back(p.id);if(p.isCapital)fr->capitalProvinceId=p.id;}

    // Build nav grid AFTER provinces and obstacles are set up
    BuildNavGrid();

    // Armies
    auto mkA=[&](const std::string&gen,glm::vec3 pos,int li,int gr,int cv,int ar){
        Army a;a.id=m_nextArmyId++;a.factionId="france";a.generalName=gen;
        a.worldPosition=pos;
        Province*p=GetProvinceAtWorldPos(pos);a.currentProvinceId=p?p->id:0;
        auto add=[&](UnitType t,const std::string&un,int mp,int at,int df,int mo,int up){
            Unit u;u.id=m_nextUnitId++;u.type=t;u.name=un;
            u.stats={mp,mp,at,df,mo,5,0,up,0.0f};a.units.push_back(u);
        };
        for(int i=0;i<li;i++)add(UnitType::LINE_INFANTRY,"Ligne #"+std::to_string(i+1),120,12,10,60,50);
        for(int i=0;i<gr;i++)add(UnitType::GRENADIERS,"Grenadiers #"+std::to_string(i+1),80,16,14,75,80);
        for(int i=0;i<cv;i++)add(UnitType::DRAGOONS,"Dragons #"+std::to_string(i+1),60,14,8,65,70);
        for(int i=0;i<ar;i++)add(UnitType::CANNON_12PDR,"Artillerie #"+std::to_string(i+1),30,22,4,55,70);
        int aid=a.id;m_armies.push_back(std::move(a));fr->armyIds.push_back(aid);
    };

    mkA("Duc de Richelieu",{-0.5f,0,-1.5f},4,2,2,1);   // near Paris
    mkA("Comte de Saxe",{-1.5f,0,-3.5f},3,1,1,1);      // Normandy
    mkA("Chevalier de Belle-Isle",{5.0f,0,-1.0f},2,1,1,0); // Alsace
    mkA("Duc de Villars",{4.5f,0,5.5f},2,0,1,0);         // Provence

    // ═══════════════════════════════════════════════════════════
    // ENEMY FACTIONS — Surrounding powers at war with France
    // ═══════════════════════════════════════════════════════════

    // Spain — south, across the Pyrenees
    {Faction f;f.id="spain";f.name="Spain";f.leaderName="Ferdinand VI";
     f.color={0.85f,0.55f,0.15f};f.isPlayerControlled=false;f.treasury=8000;
     f.relations.push_back({"france",DiplomaticStatus::WAR,  -80});
     m_factions.push_back(f);}
    // Add war from France side too
    GetFaction("france")->relations.push_back({"spain",DiplomaticStatus::WAR,-80});

    // Great Britain — north, across the Channel
    {Faction f;f.id="britain";f.name="Great Britain";f.leaderName="George II";
     f.color={0.8f,0.2f,0.2f};f.isPlayerControlled=false;f.treasury=10000;
     f.relations.push_back({"france",DiplomaticStatus::WAR,-90});
     m_factions.push_back(f);}
    GetFaction("france")->relations.push_back({"britain",DiplomaticStatus::WAR,-90});

    // Holy Roman Empire — east
    {Faction f;f.id="hre";f.name="Holy Roman Empire";f.leaderName="Charles VII";
     f.color={0.9f,0.85f,0.3f};f.isPlayerControlled=false;f.treasury=7000;
     f.relations.push_back({"france",DiplomaticStatus::WAR,-60});
     m_factions.push_back(f);}
    GetFaction("france")->relations.push_back({"hre",DiplomaticStatus::WAR,-60});

    // Enemy army creator (armies positioned OUTSIDE French borders, threatening)
    auto mkEnemy=[&](const std::string& faction, const std::string& gen,
                     glm::vec3 pos, int li, int gr, int cv, int ar){
        Army a;a.id=m_nextArmyId++;a.factionId=faction;a.generalName=gen;
        a.worldPosition=pos;a.currentProvinceId=-1; // outside French provinces
        auto add=[&](UnitType t,const std::string&un,int mp,int at,int df,int mo,int up){
            Unit u;u.id=m_nextUnitId++;u.type=t;u.name=un;
            u.stats={mp,mp,at,df,mo,5,0,up,0.0f};a.units.push_back(u);
        };
        for(int i=0;i<li;i++)add(UnitType::LINE_INFANTRY,"Infantry #"+std::to_string(i+1),120,11,9,55,50);
        for(int i=0;i<gr;i++)add(UnitType::GRENADIERS,"Grenadiers #"+std::to_string(i+1),80,15,13,70,80);
        for(int i=0;i<cv;i++)add(UnitType::HUSSARS,"Hussars #"+std::to_string(i+1),60,13,7,60,65);
        for(int i=0;i<ar;i++)add(UnitType::CANNON_6PDR,"Artillery #"+std::to_string(i+1),30,18,4,50,55);
        int aid=a.id;m_armies.push_back(std::move(a));
        GetFaction(faction)->armyIds.push_back(aid);
    };

    // British armies — north of the Channel
    mkEnemy("britain","Duke of Cumberland",{0.5f,0,-6.5f},4,1,1,1);
    mkEnemy("britain","Lord Ligonier",{2.5f,0,-6.5f},3,1,1,0);

    // Spanish armies — south of the Pyrenees
    mkEnemy("spain","Duke of Montemar",{-2.0f,0,9.5f},3,1,1,1);
    mkEnemy("spain","Marquis de la Mina",{0.5f,0,9.5f},2,1,0,1);

    // HRE armies — east of the Rhine
    mkEnemy("hre","Prince Charles",{7.8f,0,-1.0f},3,2,1,1);
    mkEnemy("hre","Count Browne",{7.8f,0,2.5f},2,1,1,0);

    Logger::Info("Campaign: %d provinces, %d factions, %d armies, %d obstacles",
        (int)m_provinces.size(),(int)m_factions.size(),(int)m_armies.size(),(int)m_obstacles.size());
}

// ═══════════════════════════════════════════════════════════════
// INPUT
// ═══════════════════════════════════════════════════════════════
void CampaignMap::HandleLeftClick(const glm::vec3&worldPos){
    glm::vec2 pt(worldPos.x,worldPos.z);
    int bestId=-1;float bestD=0.45f; // smaller hitbox
    for(const auto&a:m_armies){
        float d=glm::distance(pt,glm::vec2(a.worldPosition.x,a.worldPosition.z));
        if(d<bestD){bestD=d;bestId=a.id;}
    }
    if(bestId>=0){
        m_selectedArmyId=(m_selectedArmyId==bestId)?-1:bestId;
        m_selectedProvinceId=-1;
        if(m_selectedArmyId>=0){Army*a=GetArmy(m_selectedArmyId);m_selectionWorldPos=a->worldPosition;
            Logger::Info("Selected: %s (%d units, %d men, range:%.1f)",
                a->generalName.c_str(),(int)a->units.size(),a->GetTotalManpower(),a->movementRange);}
        return;
    }
    float bestC=0.5f;int bestCity=-1;
    for(const auto&p:m_provinces){float d=glm::distance(pt,glm::vec2(p.cityPos.x,p.cityPos.z));if(d<bestC){bestC=d;bestCity=p.id;}}
    if(bestCity>=0){m_selectedProvinceId=bestCity;m_selectedArmyId=-1;Province*p=GetProvince(bestCity);m_selectionWorldPos=p->cityPos;
        Logger::Info("City: %s",p->cityName.c_str());return;}
    m_selectedArmyId=-1;m_selectedProvinceId=-1;
}

void CampaignMap::HandleRightClick(const glm::vec3&worldPos){
    if(m_exchangeOpen)return;
    if(m_selectedArmyId<0)return;
    Army*army=GetArmy(m_selectedArmyId);
    if(!army)return;

    Faction*player=GetPlayerFaction();
    if(!player||army->factionId!=player->id){
        Logger::Warning("Cannot control enemy armies!");return;
    }

    glm::vec2 clickPt(worldPos.x,worldPos.z);

    // ── Check if right-clicking an army ──
    for(auto&other:m_armies){
        if(other.id==army->id)continue;
        float d=glm::distance(clickPt,glm::vec2(other.worldPosition.x,other.worldPosition.z));
        if(d<0.5f){
            float armyDist=glm::distance(
                glm::vec2(army->worldPosition.x,army->worldPosition.z),
                glm::vec2(other.worldPosition.x,other.worldPosition.z));

            if(other.factionId==player->id){
                // FRIENDLY: merge/exchange
                if(armyDist<1.5f){
                    // Close enough — merge now
                    int total=(int)army->units.size()+(int)other.units.size();
                    if(total<=Army::MAX_UNITS){
                        for(auto&u:other.units)army->units.push_back(std::move(u));
                        other.units.clear();
                        SetNotification("Armies merged! ("+std::to_string((int)army->units.size())+" units)");
                        DestroyArmy(other.id);
                    } else {
                        m_exchangeOpen=true;m_exchangeArmyA=m_selectedArmyId;m_exchangeArmyB=other.id;
                        m_exchangeSelA.assign(army->units.size(),false);m_exchangeSelB.assign(other.units.size(),false);
                        m_backupUnitsA=army->units;m_backupUnitsB=other.units;
                    }
                } else {
                    // Too far — schedule movement toward friendly army (tracking)
                    SchedulePathTo(*army,other.worldPosition,Army::Intent::MERGE,other.id);
                    SetNotification("Moving to merge with "+other.generalName);
                }
            } else {
                // ENEMY: attack
                if(armyDist<1.5f){
                    // Close enough — start battle now
                    StartBattle(army->id,other.id);
                } else {
                    // Schedule attack movement (tracking)
                    SchedulePathTo(*army,other.worldPosition,Army::Intent::ATTACK,other.id);
                    SetNotification("Attacking "+other.generalName+"!");
                }
            }
            return;
        }
    }

    // ── Check if right-clicking a city ──
    for(const auto&p:m_provinces){
        float d=glm::distance(clickPt,glm::vec2(p.cityPos.x,p.cityPos.z));
        if(d<0.5f){
            SchedulePathTo(*army,p.cityPos,Army::Intent::ENTER_CITY,-1,p.id);
            return;
        }
    }

    // ── Empty ground — just move ──
    if(!IsPointPassable(worldPos)){Logger::Warning("Impassable!");return;}
    SchedulePathTo(*army,worldPos,Army::Intent::MOVE);
}

// ─── Schedule a path with turn breaks ─────────────────────────
void CampaignMap::SchedulePathTo(Army& army, glm::vec3 dest,
    Army::Intent intent, int targetArmy, int targetCity)
{
    auto path = FindPathWorld(army.worldPosition, dest, army.id, targetArmy, targetCity);
    if(path.size()<2){Logger::Warning("No path found!");return;}

    float totalLen=0;
    for(int i=1;i<(int)path.size();i++)
        totalLen+=glm::distance(glm::vec2(path[i].x,path[i].z),glm::vec2(path[i-1].x,path[i-1].z));

    army.fullPath=path;
    army.currentPathIndex=1;
    army.totalPathLength=totalLen;
    army.distanceTraveled=0;
    army.pathStartOffset = 0;
    army.turnBreaks.clear();
    army.intent=intent;
    army.targetArmyId=targetArmy;
    army.targetCityProvId=targetCity;

    float remaining=totalLen;
    float accumulated=0;
    float rangeThisTurn=army.movementRange;
    float rangePerTurn=army.movementRangeMax;

    float t1=std::min(rangeThisTurn,remaining);
    army.turnBreaks.push_back(t1);accumulated+=t1;remaining-=t1;
    while(remaining>0.01f){
        float seg=std::min(rangePerTurn,remaining);
        accumulated+=seg;remaining-=seg;
        army.turnBreaks.push_back(accumulated);
    }

    army.isMoving=true;
    army.isGarrisoned=false;

    int turns=(int)army.turnBreaks.size();
    Logger::Info("Army '%s' → %s (%.1f, %d turn%s)",
        army.generalName.c_str(),
        intent==Army::Intent::ATTACK?"ATTACK":
        intent==Army::Intent::MERGE?"MERGE":
        intent==Army::Intent::ENTER_CITY?"CITY":"MOVE",
        totalLen,turns,turns>1?"s":"");
}

// ─── Start a battle between two armies ────────────────────────
void CampaignMap::StartBattle(int attackerId,int defenderId){
    Army*atk=GetArmy(attackerId);Army*def=GetArmy(defenderId);
    if(!atk||!def)return;

    // Stop both armies
    atk->ClearPath();def->ClearPath();

    BattleSetupData battle;
    battle.attacker=atk;battle.defender=def;
    Province*p=GetProvinceAtWorldPos(atk->worldPosition);
    battle.provinceId=p?p->id:-1;
    m_pendingBattle=battle;

    SetNotification("BATTLE! "+atk->generalName+" vs "+def->generalName);
    Logger::Info("*** BATTLE: %s vs %s ***",atk->generalName.c_str(),def->generalName.c_str());
}

// ═══════════════════════════════════════════════════════════════
// UPDATE
// ═══════════════════════════════════════════════════════════════
void CampaignMap::Update(float dt,const InputManager&){
    UpdateArmyPositions(dt);
    // NO auto-battle detection here — battles are intent-based only
    if(m_notifTimer>0)m_notifTimer-=dt;
}

void CampaignMap::UpdateArmyPositions(float dt){
    for(auto&army:m_armies){
        // ── Update tracking targets — recalc path if target moved significantly ──
        if(army.targetArmyId>=0 && !army.fullPath.empty()){
            Army*target=GetArmy(army.targetArmyId);
            if(target && !target->units.empty()){
                glm::vec3 lastPt=army.fullPath.back();
                float drift=glm::distance(glm::vec2(lastPt.x,lastPt.z),
                    glm::vec2(target->worldPosition.x,target->worldPosition.z));
                if(drift>0.5f){
                    // Target moved significantly — recalculate entire path from current pos
                    auto newPath = FindPathWorld(army.worldPosition, target->worldPosition, army.id, army.targetArmyId, army.targetCityProvId);
                    if(newPath.size()>=2){
                        army.fullPath=newPath;
                        army.currentPathIndex=1;
                        // Recalculate turn breaks from current range
                        float totalLen=0;
                        for(int i=1;i<(int)newPath.size();i++)
                            totalLen+=glm::distance(glm::vec2(newPath[i].x,newPath[i].z),
                                glm::vec2(newPath[i-1].x,newPath[i-1].z));
                        army.totalPathLength=totalLen;
                        army.distanceTraveled=0;
                        army.pathStartOffset = 0;
                        army.turnBreaks.clear();
                        float rem=totalLen,acc=0;
                        float rt=army.movementRange;
                        float t1=std::min(rt,rem);
                        army.turnBreaks.push_back(t1);acc+=t1;rem-=t1;
                        while(rem>0.01f){float s=std::min(army.movementRangeMax,rem);acc+=s;rem-=s;army.turnBreaks.push_back(acc);}
                    }
                }
            } else if(!target || target->units.empty()){
                // Target destroyed — stop
                army.ClearPath();continue;
            }
            // ── City proximity: enter city early if close enough ──
            if (army.intent == Army::Intent::ENTER_CITY && army.targetCityProvId >= 0) {
                Province* p = GetProvince(army.targetCityProvId);
                if (p) {
                    float d = glm::distance(glm::vec2(army.worldPosition.x, army.worldPosition.z),
                        glm::vec2(p->cityPos.x, p->cityPos.z));
                    if (d < 1.0f) {
                        army.isMoving = false;
                        TryGarrison(army, p);
                        continue;
                    }
                }
            }
        }

        if(!army.isMoving||army.fullPath.empty())continue;

        // This turn's movement limit
        float turnLimit=army.turnBreaks.empty()?army.movementRange:army.turnBreaks[0];

        if(army.currentPathIndex>=(int)army.fullPath.size()){
            HandleArmyArrival(army);continue;
        }

        glm::vec3 target=army.fullPath[army.currentPathIndex];
        glm::vec3 dir=target-army.worldPosition;
        float dist=glm::length(glm::vec2(dir.x,dir.z));

        if(dist<0.05f){
            army.worldPosition=target;
            army.currentPathIndex++;
            if(army.currentPathIndex>=(int)army.fullPath.size()){
                HandleArmyArrival(army);
            }
            continue;
        }

        float step=army.moveSpeed*dt;

        // Don't exceed this turn's limit
        if(army.distanceTraveled+step>=turnLimit){
            army.isMoving=false;
            army.movementRange=0;
            UpdateArmyProvince(army);
            continue;
        }

        // Check if army has range left
        if(army.movementRange<=0.01f){
            army.isMoving=false;
            UpdateArmyProvince(army);
            continue;
        }

        if(step>dist)step=dist;
        glm::vec3 moveDir=glm::normalize(dir);
        army.worldPosition+=moveDir*step;
        army.worldPosition.y=0;
        army.distanceTraveled+=step;
        // ── KEY FIX: deduct movement range as we walk ──
        army.movementRange-=step;
        if(army.movementRange<0)army.movementRange=0;

        if(army.id==m_selectedArmyId)m_selectionWorldPos=army.worldPosition;
    }
}

// ─── Try to garrison an army in a city (auto-merge if occupied) ──
void CampaignMap::TryGarrison(Army& army, Province* p) {
    if (!p) { army.ClearPath(); return; }

    // Check if another army is already garrisoned here
    Army* existing = nullptr;
    for (auto& other : m_armies) {
        if (other.id == army.id)continue;
        if (other.isGarrisoned && other.currentProvinceId == p->id) {
            existing = &other; break;
        }
    }

    if (existing) {
        // Auto-merge as many units as possible
        while (!army.units.empty() && existing->CanAddUnit()) {
            existing->units.push_back(std::move(army.units.back()));
            army.units.pop_back();
        }
        if (army.units.empty()) {
            SetNotification("Merged into garrison! (" + std::to_string((int)existing->units.size()) + " units)");
            Logger::Info("Army '%s' merged into '%s' in %s",
                army.generalName.c_str(), existing->generalName.c_str(), p->cityName.c_str());
            army.ClearPath();
            DestroyArmy(army.id);
        }
        else {
            // Overflow stays outside
            army.ClearPath();
            army.isGarrisoned = false;
            glm::vec2 dir(army.worldPosition.x - p->cityPos.x, army.worldPosition.z - p->cityPos.z);
            if (glm::length(dir) < 0.01f)dir = { 1,0 };
            dir = glm::normalize(dir);
            army.worldPosition = { p->cityPos.x + dir.x * 1.2f, 0, p->cityPos.z + dir.y * 1.2f };
            UpdateArmyProvince(army);
            SetNotification("City full! " + std::to_string((int)army.units.size()) + " units outside");
        }
    }
    else {
        army.isGarrisoned = true;
        army.worldPosition = p->cityPos;
        army.ClearPath();
        UpdateArmyProvince(army);
        Logger::Info("Army '%s' garrisoned in %s", army.generalName.c_str(), p->cityName.c_str());
        CheckCityOccupation(army);
    }
}
// ─── Handle what happens when an army reaches its destination ──
void CampaignMap::HandleArmyArrival(Army& army){
    army.isMoving=false;
    army.movementRange=std::max(0.0f,army.movementRange-army.distanceTraveled);
    UpdateArmyProvince(army);

    if(army.id==m_selectedArmyId)m_selectionWorldPos=army.worldPosition;

    switch(army.intent){
        case Army::Intent::ATTACK:{
            Army*target=GetArmy(army.targetArmyId);
            if(target){
                float d=glm::distance(glm::vec2(army.worldPosition.x,army.worldPosition.z),
                    glm::vec2(target->worldPosition.x,target->worldPosition.z));
                if(d<1.5f){
                    StartBattle(army.id,target->id);
                }
            }
            army.ClearPath();
            break;
        }
        case Army::Intent::MERGE:{
            Army*target=GetArmy(army.targetArmyId);
            if(target){
                float d=glm::distance(glm::vec2(army.worldPosition.x,army.worldPosition.z),
                    glm::vec2(target->worldPosition.x,target->worldPosition.z));
                if(d<1.5f){
                    int total=(int)army.units.size()+(int)target->units.size();
                    if(total<=Army::MAX_UNITS){
                        for(auto&u:target->units)army.units.push_back(std::move(u));
                        target->units.clear();
                        SetNotification("Armies merged!");
                        DestroyArmy(target->id);
                    } else {
                        // Open exchange
                        m_exchangeOpen=true;m_exchangeArmyA=army.id;m_exchangeArmyB=target->id;
                        m_exchangeSelA.assign(army.units.size(),false);
                        m_exchangeSelB.assign(target->units.size(),false);
                        m_backupUnitsA=army.units;m_backupUnitsB=target->units;
                    }
                }
            }
            army.ClearPath();
            break;
        }
        case Army::Intent::ENTER_CITY: {
            Province* p = GetProvince(army.targetCityProvId);
            TryGarrison(army, p); // handles merge, overflow, or normal garrison
            break;
        }
        default:
            army.ClearPath();
            break;
    }
}

// ─── Check if army occupies a hostile city ────────────────────
void CampaignMap::CheckCityOccupation(Army& army){
    Province*p=GetProvince(army.currentProvinceId);
    if(!p)return;
    if(p->ownerFactionId!=army.factionId){
        CaptureProvince(p->id,army.factionId);
    }
}

void CampaignMap::UpdateArmyProvince(Army&a){
    Province*p=GetProvinceAtWorldPos(a.worldPosition);if(p)a.currentProvinceId=p->id;
    if(a.id==m_selectedArmyId)m_selectionWorldPos=a.worldPosition;
}

// Called AFTER all execution is complete (end of turn cycle)
void CampaignMap::ProcessTurn(){
    m_currentTurn++;
    Logger::Info("=== Turn %d (%s %s) ===",m_currentTurn,GetCurrentSeason().c_str(),GetCurrentYear().c_str());
    for(auto&p:m_provinces){p.UpdatePopulation();p.UpdatePublicOrder();}
    for(auto&f:m_factions){
        if(f.isEliminated)continue;int inc=0,exp=0;
        for(int pid:f.ownedProvinces){auto*p=GetProvince(pid);if(p)inc+=p->GetTotalIncome();}
        for(int aid:f.armyIds){auto*a=GetArmy(aid);if(a)exp+=a->GetTotalUpkeep();}
        f.UpdateEconomy(inc,exp);
    }


    // Restore movement and shift turn breaks — but DON'T start moving
    for(auto&a:m_armies){

        a.movementRange=a.movementRangeMax;
        a.distanceTraveled=0;

        if(!a.fullPath.empty()&&a.currentPathIndex<(int)a.fullPath.size()){
            if (!a.turnBreaks.empty()) {
                float used = a.turnBreaks[0];
                a.pathStartOffset += used;   // ← accumulate distance already traveled
                a.turnBreaks.erase(a.turnBreaks.begin());
                for (auto& tb : a.turnBreaks)tb -= used;
            }
            // NOT setting isMoving — armies wait for explicit activation
            a.isMoving=false;
            Logger::Info("Army '%s' has scheduled path (%d waypoints left)",
                a.generalName.c_str(),(int)a.fullPath.size()-a.currentPathIndex);
        }

    }
}

// Start the next scheduled army that has fatigue remaining. Returns army ID or -1.
int CampaignMap::StartNextScheduledArmy(const std::string& factionId){

    for(auto&a:m_armies){
        if(a.factionId!=factionId)continue;
        if(a.isMoving)continue;
        if(a.fullPath.empty()||a.currentPathIndex>=(int)a.fullPath.size())continue;
        if(a.turnBreaks.empty())continue;
        // KEY: only start if army has movement range left
        if(a.movementRange<0.1f){
            Logger::Info("Army '%s' fatigued (%.1f range) — skipping",a.generalName.c_str(),a.movementRange);
            continue;
        }
        // Don't start if the first segment is 0 (already exhausted this turn)
        if(a.turnBreaks[0]<0.1f){
            Logger::Info("Army '%s' has zero-length segment — skipping",a.generalName.c_str());
            continue;
        }

        a.isMoving=true;
        a.distanceTraveled=0;
        Logger::Info("Starting army '%s' (%.1f range, %.1f segment)",
            a.generalName.c_str(),a.movementRange,a.turnBreaks[0]);
        return a.id;
    }
    return -1;
}

// Check if any army of a faction is currently moving
bool CampaignMap::IsAnyArmyMoving(const std::string& factionId)const{
    for(const auto&a:m_armies)
        if(a.factionId==factionId&&a.isMoving)return true;
    return false;
}

// Stop all armies (used before turn transition)
void CampaignMap::StopAllArmies(){
    for(auto&a:m_armies)a.isMoving=false;
}

std::string CampaignMap::GetCurrentSeason()const{const char*s[]={"Spring","Summer","Autumn","Winter"};return s[(m_currentTurn-1)%4];}
std::string CampaignMap::GetCurrentYear()const{return std::to_string(1700+(m_currentTurn-1)/4);}
Province* CampaignMap::GetProvince(int id){for(auto&p:m_provinces)if(p.id==id)return&p;return nullptr;}
const Province* CampaignMap::GetProvince(int id)const{for(const auto&p:m_provinces)if(p.id==id)return&p;return nullptr;}
Faction* CampaignMap::GetFaction(const std::string&id){for(auto&f:m_factions)if(f.id==id)return&f;return nullptr;}
const Faction* CampaignMap::GetFaction(const std::string&id)const{for(const auto&f:m_factions)if(f.id==id)return&f;return nullptr;}
Faction* CampaignMap::GetPlayerFaction(){for(auto&f:m_factions)if(f.isPlayerControlled)return&f;return nullptr;}
const Faction* CampaignMap::GetPlayerFaction()const{for(const auto&f:m_factions)if(f.isPlayerControlled)return&f;return nullptr;}
Army* CampaignMap::GetArmy(int id){for(auto&a:m_armies)if(a.id==id)return&a;return nullptr;}
const Army* CampaignMap::GetArmy(int id)const{for(const auto&a:m_armies)if(a.id==id)return&a;return nullptr;}
std::vector<Army*> CampaignMap::GetArmiesInProvince(int pid){std::vector<Army*>r;for(auto&a:m_armies)if(a.currentProvinceId==pid)r.push_back(&a);return r;}
Province* CampaignMap::GetProvinceAtWorldPos(const glm::vec3&w){glm::vec2 pt(w.x,w.z);for(auto&p:m_provinces)if(PtInPoly(pt,p.borderVertices))return&p;return nullptr;}
void CampaignMap::MoveArmy(int id,int tgt){Province*p=GetProvince(tgt);if(p){m_selectedArmyId=id;HandleRightClick(p->cityPos);}}
void CampaignMap::RecruitUnit(int,UnitType){}

// ═══════════════════════════════════════════════════════════════
// UNIT EXCHANGE MODAL — Live swap inside, Accept/Cancel to commit/revert
// ═══════════════════════════════════════════════════════════════
void CampaignMap::HandleExchangeClick(float sx, float sy, float sw, float sh){
    if(!m_exchangeOpen)return;
    Army*a=GetArmy(m_exchangeArmyA);
    Army*b=GetArmy(m_exchangeArmyB);
    if(!a||!b){CancelExchange();return;}

    // Modal layout (must match renderer)
    float panW=sw*0.65f, panH=sh*0.7f;
    float px=(sw-panW)/2, py=(sh-panH)/2;
    float halfW=(panW-30)/2;
    float cardW=34, cardH=50, cardGap=4;
    float unitY=py+80;
    float leftX=px+10, rightX=px+halfW+20;
    float cx=sw/2;

    // Buttons layout (3 buttons: Swap center, Accept left, Cancel right)
    float btnW=70, btnH=30;
    float swapX=cx-btnW/2, swapY=py+panH/2-btnH/2; // center swap button
    float acceptX=px+panW/2-btnW-40, acceptY=py+panH-45;
    float cancelX=px+panW/2+40, cancelY=acceptY;

    // Swap button (center)
    if(sx>=swapX&&sx<=swapX+btnW&&sy>=swapY&&sy<=swapY+btnH){
        SwapSelectedUnits();return;
    }
    // Accept button
    if(sx>=acceptX&&sx<=acceptX+btnW&&sy>=acceptY&&sy<=acceptY+btnH){
        ConfirmExchange();return;
    }
    // Cancel button
    if(sx>=cancelX&&sx<=cancelX+btnW&&sy>=cancelY&&sy<=cancelY+btnH){
        CancelExchange();return;
    }

    // Check left side unit clicks (Army A)
    for(int i=0;i<(int)a->units.size();i++){
        float ux=leftX+(cardW+cardGap)*(i%6);
        float uy=unitY+(cardH+cardGap)*(i/6);
        if(sx>=ux&&sx<=ux+cardW&&sy>=uy&&sy<=uy+cardH){
            if(i<(int)m_exchangeSelA.size()) m_exchangeSelA[i]=!m_exchangeSelA[i];
            return;
        }
    }

    // Check right side unit clicks (Army B)
    for(int i=0;i<(int)b->units.size();i++){
        float ux=rightX+(cardW+cardGap)*(i%6);
        float uy=unitY+(cardH+cardGap)*(i/6);
        if(sx>=ux&&sx<=ux+cardW&&sy>=uy&&sy<=uy+cardH){
            if(i<(int)m_exchangeSelB.size()) m_exchangeSelB[i]=!m_exchangeSelB[i];
            return;
        }
    }
}

// Live swap: moves selected units between armies inside the modal
void CampaignMap::SwapSelectedUnits(){
    Army*a=GetArmy(m_exchangeArmyA);
    Army*b=GetArmy(m_exchangeArmyB);
    if(!a||!b)return;

    // Gather selected from A
    std::vector<Unit> fromA;
    for(int i=(int)m_exchangeSelA.size()-1;i>=0;i--){
        if(i<(int)a->units.size()&&m_exchangeSelA[i]){
            fromA.push_back(std::move(a->units[i]));
            a->units.erase(a->units.begin()+i);
        }
    }
    // Gather selected from B
    std::vector<Unit> fromB;
    for(int i=(int)m_exchangeSelB.size()-1;i>=0;i--){
        if(i<(int)b->units.size()&&m_exchangeSelB[i]){
            fromB.push_back(std::move(b->units[i]));
            b->units.erase(b->units.begin()+i);
        }
    }

    // Check validity
    int newA=(int)a->units.size()+(int)fromB.size();
    int newB=(int)b->units.size()+(int)fromA.size();
    if(newA>Army::MAX_UNITS||newB>Army::MAX_UNITS){
        // Revert
        for(auto&u:fromA)a->units.push_back(std::move(u));
        for(auto&u:fromB)b->units.push_back(std::move(u));
        SetNotification("Cannot swap — would exceed 20 units!");
        return;
    }

    // Execute swap
    for(auto&u:fromB)a->units.push_back(std::move(u));
    for(auto&u:fromA)b->units.push_back(std::move(u));

    // Reset selections for new state
    m_exchangeSelA.assign(a->units.size(),false);
    m_exchangeSelB.assign(b->units.size(),false);

    Logger::Info("Swapped! A=%d, B=%d",(int)a->units.size(),(int)b->units.size());
}

void CampaignMap::ConfirmExchange(){
    if(!m_exchangeOpen)return;
    // Changes are already live from SwapSelectedUnits — just close
    Army*a=GetArmy(m_exchangeArmyA);
    Army*b=GetArmy(m_exchangeArmyB);

    // Clean up empty armies
    if(a&&a->units.empty())DestroyArmy(m_exchangeArmyA);
    if(b&&b->units.empty())DestroyArmy(m_exchangeArmyB);

    m_backupUnitsA.clear();m_backupUnitsB.clear();
    m_exchangeOpen=false;
    SetNotification("Exchange confirmed!");
    Logger::Info("Exchange confirmed");
}

void CampaignMap::CancelExchange(){
    if(!m_exchangeOpen){m_exchangeOpen=false;return;}
    // Restore from backups
    Army*a=GetArmy(m_exchangeArmyA);
    Army*b=GetArmy(m_exchangeArmyB);
    if(a&&!m_backupUnitsA.empty())a->units=std::move(m_backupUnitsA);
    if(b&&!m_backupUnitsB.empty())b->units=std::move(m_backupUnitsB);
    m_backupUnitsA.clear();m_backupUnitsB.clear();
    m_exchangeOpen=false;m_exchangeArmyA=-1;m_exchangeArmyB=-1;
    m_exchangeSelA.clear();m_exchangeSelB.clear();
    Logger::Info("Exchange cancelled — reverted");
}
BattleSetupData CampaignMap::GetPendingBattle(){return m_pendingBattle.value();}
void CampaignMap::ApplyBattleResult(const BattleResult& r) {
    m_pendingBattle.reset();

    // Clean up dead units from combat
    for (auto& a : m_armies)a.RemoveDestroyedUnits();

    // Determine winner and loser from stored IDs
    int winnerId = -1, loserId = -1;
    if (r.attackerWon) {
        winnerId = r.attackerId;
        loserId = r.defenderId;
    }
    else {
        winnerId = r.defenderId;
        loserId = r.attackerId;
    }

    // Destroy armies with no units left
    std::vector<int> toDestroy;
    for (auto& a : m_armies)
        if (a.units.empty())toDestroy.push_back(a.id);

    // Retreat the loser (if still alive with units)
    Army* loser = GetArmy(loserId);
    if (loser && !loser->units.empty()) {
        // Find nearest friendly city to retreat toward
        float bestDist = 999; glm::vec3 retreatDest = loser->worldPosition;
        for (const auto& p : m_provinces) {
            if (p.ownerFactionId == loser->factionId) {
                float d = glm::distance(glm::vec2(loser->worldPosition.x, loser->worldPosition.z),
                    glm::vec2(p.cityPos.x, p.cityPos.z));
                if (d > 0.5f && d < bestDist) { bestDist = d; retreatDest = p.cityPos; }
            }
        }

        // No friendly city found — flee in the opposite direction from the winner
        if (bestDist > 998) {
            Army* w = GetArmy(winnerId);
            if (w) {
                glm::vec2 away = glm::normalize(glm::vec2(
                    loser->worldPosition.x - w->worldPosition.x,
                    loser->worldPosition.z - w->worldPosition.z));
                retreatDest = { loser->worldPosition.x + away.x * 4.0f,0,
                             loser->worldPosition.z + away.y * 4.0f };
            }
        }

        // Give full movement for retreat — ignore fatigue
        // Give full movement for retreat flee
        loser->movementRange = loser->movementRangeMax;
        loser->distanceTraveled = 0;
        loser->pathStartOffset = 0;

        // Check if retreating to a friendly city → use ENTER_CITY for TryGarrison
        int retreatCityId = -1;
        for (const auto& p : m_provinces) {
            if (p.ownerFactionId == loser->factionId) {
                float d = glm::distance(glm::vec2(retreatDest.x, retreatDest.z),
                    glm::vec2(p.cityPos.x, p.cityPos.z));
                if (d < 0.5f) { retreatCityId = p.id; break; }
            }
        }

        if (retreatCityId >= 0) {
            SchedulePathTo(*loser, retreatDest, Army::Intent::ENTER_CITY, -1, retreatCityId);
        }
        else {
            SchedulePathTo(*loser, retreatDest, Army::Intent::MOVE);
        }

        Logger::Info("Army '%s' retreating toward (%.1f,%.1f)!",
            loser->generalName.c_str(), retreatDest.x, retreatDest.z);
    }

    // Province capture by winner
    Army* winner = GetArmy(winnerId);
    if (winner && !winner->units.empty()) {
        Province* p = GetProvinceAtWorldPos(winner->worldPosition);
        if (p && p->ownerFactionId != winner->factionId)
            CaptureProvince(p->id, winner->factionId);
    }

    // Destroy empty armies last (after retreat logic has run)
    for (int aid : toDestroy)DestroyArmy(aid);
}
void CampaignMap::SetNotification(const std::string&msg){
    m_notification=msg;m_notifTimer=4.0f;
    Logger::Info("NOTIFICATION: %s",msg.c_str());
}

// ═══════════════════════════════════════════════════════════════
// AI — Intent-based aggressive behavior
// ═══════════════════════════════════════════════════════════════
void CampaignMap::RunAI() {
    // Bulk version — runs all AI factions at once (used by TurnManager fallback)
    for (const auto& fid : GetAIFactionIds())
        RunAIForFaction(fid);
}

void CampaignMap::RunAIForFaction(const std::string& factionId) {
    Faction* faction = GetFaction(factionId);
    Faction* player = GetPlayerFaction();
    if (!faction || !player || faction->isPlayerControlled || faction->isEliminated)return;
    if (!faction->IsAtWarWith(player->id))return;

    Logger::Info("AI turn: %s", faction->name.c_str());

    for (int aid : faction->armyIds) {
        Army* army = GetArmy(aid);
        if (!army || army->isMoving || army->units.empty())continue;
        if (!army->fullPath.empty())continue; // already has a scheduled path

        // Find nearest player army to attack
        float bestArmyDist = 999; int bestArmyTarget = -1;
        for (const auto& pa : m_armies) {
            if (pa.factionId != player->id || pa.units.empty())continue;
            float d = glm::distance(glm::vec2(army->worldPosition.x, army->worldPosition.z),
                glm::vec2(pa.worldPosition.x, pa.worldPosition.z));
            if (d < bestArmyDist) { bestArmyDist = d; bestArmyTarget = pa.id; }
        }

        // Find nearest player city to capture
        float bestCityDist = 999; int bestCityProv = -1;
        for (const auto& p : m_provinces) {
            if (p.ownerFactionId != player->id)continue;
            float d = glm::distance(glm::vec2(army->worldPosition.x, army->worldPosition.z),
                glm::vec2(p.cityPos.x, p.cityPos.z));
            if (d < bestCityDist) { bestCityDist = d; bestCityProv = p.id; }
        }

        if (bestArmyDist < bestCityDist + 3.0f && bestArmyTarget >= 0) {
            Army* target = GetArmy(bestArmyTarget);
            if (target) {
                float d = glm::distance(glm::vec2(army->worldPosition.x, army->worldPosition.z),
                    glm::vec2(target->worldPosition.x, target->worldPosition.z));
                if (d < 1.5f) {
                    StartBattle(army->id, target->id);
                }
                else {
                    SchedulePathTo(*army, target->worldPosition, Army::Intent::ATTACK, target->id);
                }
            }
        }
        else if (bestCityProv >= 0) {
            Province* p = GetProvince(bestCityProv);
            if (p) SchedulePathTo(*army, p->cityPos, Army::Intent::ENTER_CITY, -1, p->id);
        }
    }

    // Stop all armies so Game.cpp can activate them one at a time
    for (auto& a : m_armies) {
        if (a.factionId == factionId && a.isMoving && !a.fullPath.empty()) {
            a.isMoving = false;
        }
    }
}

std::vector<std::string> CampaignMap::GetAIFactionIds()const {
    std::vector<std::string> ids;
    for (const auto& f : m_factions)
        if (!f.isPlayerControlled && !f.isEliminated)
            ids.push_back(f.id);
    return ids;
}
// ═══════════════════════════════════════════════════════════════
// BATTLE DETECTION — when hostile armies are close
// ═══════════════════════════════════════════════════════════════
void CampaignMap::DetectBattles(){
    // Intentionally empty — battles are now triggered by intent (ATTACK)
    // not by proximity detection. See HandleArmyArrival and AI.
}
void CampaignMap::CheckForBattles(){
    // Legacy — no longer used
}

// ═══════════════════════════════════════════════════════════════
// PROVINCE CAPTURE
// ═══════════════════════════════════════════════════════════════
void CampaignMap::CaptureProvince(int provinceId,const std::string&newOwner){
    Province*prov=GetProvince(provinceId);if(!prov)return;
    std::string oldOwner=prov->ownerFactionId;
    if(oldOwner==newOwner)return;

    // Remove from old faction
    Faction*oldF=GetFaction(oldOwner);
    if(oldF){
        auto&op=oldF->ownedProvinces;
        op.erase(std::remove(op.begin(),op.end(),provinceId),op.end());
    }

    // Add to new faction
    Faction*newF=GetFaction(newOwner);
    if(newF){
        newF->ownedProvinces.push_back(provinceId);
        prov->ownerFactionId=newOwner;
        prov->color=newF->color;
        // Apply per-province tint
        float t=(float)(prov->id%5)*0.025f;
        prov->color.r+=t-0.05f;prov->color.g+=t*0.3f;
    }

    SetNotification(prov->name+" captured by "+(newF?newF->name:newOwner)+"!");
    Logger::Info("Province %s captured by %s!",prov->name.c_str(),newOwner.c_str());

    // Check if old faction is eliminated (no provinces left)
    if(oldF&&oldF->ownedProvinces.empty()){
        oldF->isEliminated=true;
        SetNotification(oldF->name+" has been eliminated!");
        Logger::Info("%s ELIMINATED!",oldF->name.c_str());
    }
}

void CampaignMap::DestroyArmy(int armyId){
    // Remove from faction's army list
    for(auto&f:m_factions){
        auto&ids=f.armyIds;
        ids.erase(std::remove(ids.begin(),ids.end(),armyId),ids.end());
    }
    // Remove from army vector
    m_armies.erase(std::remove_if(m_armies.begin(),m_armies.end(),
        [armyId](const Army&a){return a.id==armyId;}),m_armies.end());

    if(m_selectedArmyId==armyId){m_selectedArmyId=-1;m_selectedProvinceId=-1;}
}
