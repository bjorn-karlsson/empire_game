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
bool CampaignMap::IsPointOnLand(const glm::vec3&pos)const{
    glm::vec2 pt(pos.x,pos.z);
    for(auto&p:m_provinces)if(PtInPoly(pt,p.borderVertices))return true;
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
std::vector<glm::vec3> CampaignMap::FindPathWorld(const glm::vec3&from,const glm::vec3&to)const{
    int sx=m_navGrid.toGX(from.x),sz=m_navGrid.toGZ(from.z);
    int ex=m_navGrid.toGX(to.x),ez=m_navGrid.toGZ(to.z);

    if(!m_navGrid.inBounds(sx,sz)||!m_navGrid.inBounds(ex,ez))return{};
    if(!m_navGrid.passable[ex][ez])return{};
    if(sx==ex&&sz==ez)return{from,to};

    // A* with 8-directional movement
    struct Node{int x,z;float g,f;};
    auto key=[](int x,int z)->int{return x*NavGrid::H+z;};
    auto heur=[&](int x,int z)->float{return std::sqrt((float)((x-ex)*(x-ex)+(z-ez)*(z-ez)))*NavGrid::CELL;};

    auto cmp=[](const Node&a,const Node&b){return a.f>b.f;};
    std::priority_queue<Node,std::vector<Node>,decltype(cmp)> open(cmp);
    std::unordered_map<int,float> bestG;
    std::unordered_map<int,int> parent;

    open.push({sx,sz,0,heur(sx,sz)});
    bestG[key(sx,sz)]=0;
    parent[key(sx,sz)]=-1;

    int dx8[]={-1,0,1,-1,1,-1,0,1};
    int dz8[]={-1,-1,-1,0,0,1,1,1};
    float dcost[]={1.414f,1,1.414f,1,1,1.414f,1,1.414f};

    bool found=false;
    while(!open.empty()){
        Node cur=open.top();open.pop();
        if(cur.x==ex&&cur.z==ez){found=true;break;}

        int ck=key(cur.x,cur.z);
        if(cur.g>bestG[ck]+0.001f)continue; // stale

        for(int d=0;d<8;d++){
            int nx=cur.x+dx8[d],nz=cur.z+dz8[d];
            if(!m_navGrid.inBounds(nx,nz)||!m_navGrid.passable[nx][nz])continue;

            // Check diagonal doesn't cut corner
            if(dx8[d]!=0&&dz8[d]!=0){
                if(!m_navGrid.passable[cur.x+dx8[d]][cur.z]||!m_navGrid.passable[cur.x][cur.z+dz8[d]])
                    continue;
            }

            float ng=cur.g+dcost[d]*NavGrid::CELL;
            int nk=key(nx,nz);
            if(bestG.find(nk)==bestG.end()||ng<bestG[nk]){
                bestG[nk]=ng;
                parent[nk]=ck;
                open.push({nx,nz,ng,ng+heur(nx,nz)});
            }
        }
    }

    if(!found)return{};

    // Reconstruct path
    std::vector<glm::vec3> path;
    int ck=key(ex,ez);
    while(ck!=-1){
        int px=ck/NavGrid::H, pz=ck%NavGrid::H;
        path.push_back(m_navGrid.toWorld(px,pz));
        ck=parent[ck];
    }
    std::reverse(path.begin(),path.end());

    // Smooth: replace first/last with exact positions
    if(!path.empty()){path.front()=glm::vec3(from.x,0,from.z);path.back()=glm::vec3(to.x,0,to.z);}

    // Simplify: remove collinear waypoints
    if(path.size()>2){
        std::vector<glm::vec3> simplified;
        simplified.push_back(path[0]);
        for(int i=1;i<(int)path.size()-1;i++){
            glm::vec2 d1=glm::normalize(glm::vec2(path[i].x-path[i-1].x,path[i].z-path[i-1].z));
            glm::vec2 d2=glm::normalize(glm::vec2(path[i+1].x-path[i].x,path[i+1].z-path[i].z));
            if(glm::dot(d1,d2)<0.98f) simplified.push_back(path[i]); // keep turns
        }
        simplified.push_back(path.back());
        path=simplified;
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

    #define V(x,z) glm::vec3(x,0.0f,z)
    glm::vec3 cBrest=V(-8,-1.5),cStMalo=V(-5.5,-4),cCherb=V(-4,-5),
        cLeHav=V(-2.5,-4.8),cDiep=V(-0.8,-4.5),cCalais=V(0.8,-5),
        cDunk=V(2.2,-4.8),cLille=V(2.8,-3.8),cMetz=V(5,-3.5),
        cStras=V(6.5,-2.5),cBasel=V(6.8,-0.8),cGenev=V(6.5,0.8),
        cAlps=V(7,2),cNice=V(6.5,4),cToulon=V(5.5,5.5),cMars=V(4.5,6.5),
        cMontp=V(2.5,7.5),cNarb=V(1,8.5),cPerp=V(-0.5,9.5),
        cPyrE=V(-1.5,10),cPyrM=V(-3,10),cPyrW=V(-4.5,9.5),
        cBayo=V(-5.5,8.5),cBiarr=V(-6.5,7.5),cBordC=V(-6.5,5.5),
        cRoch=V(-6.5,3),cNant=V(-6.5,0.5),cQuimp=V(-8.5,-0.5);
    glm::vec3 iNB=V(-4.8,-2.5),iNP=V(-1.2,-2.2),iPC=V(1.8,-2.8),
        iCA=V(4.5,-2),iAB=V(5,-0.3),iBD=V(5,1.5),iDP=V(5,3.8),
        iPL=V(2.5,5.8),iLG=V(-2,7.5),iAG=V(-4.5,7.5),
        iAP=V(-5,4.5),iPLo=V(-3.5,1.5),iBP=V(-5,0.5),
        iLP=V(-1.2,0.2),iPCh=V(1.8,-0.3),iLB=V(0.8,1.5),
        iLA=V(-0.8,2.8),iABu=V(2.5,3),iALa=V(0.5,5.5),
        iAPo=V(-2.5,4),iNL=V(-3.2,-0.5);
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

    m_provinces.push_back(mkP(0,"Ile-de-France","Paris",{iNP,iPC,iPCh,iLB,iLA,iLP,iNL},500,false,true));
    m_provinces.push_back(mkP(1,"Normandy","Rouen",{cCherb,cLeHav,cDiep,iNP,iNL,iNB,cStMalo},300,true,false));
    m_provinces.push_back(mkP(2,"Brittany","Rennes",{cStMalo,iNB,iNL,iPLo,iBP,cNant,cQuimp,cBrest},200,true,false,"hills"));
    m_provinces.push_back(mkP(3,"Picardy","Amiens",{cDiep,cCalais,cDunk,cLille,iPC,iNP},280,true,false));
    m_provinces.push_back(mkP(4,"Champagne","Reims",{iPC,cLille,cMetz,iCA,iAB,iPCh},250,false,false,"hills"));
    m_provinces.push_back(mkP(5,"Alsace-Lorraine","Strasbourg",{iCA,cMetz,cStras,cBasel,cGenev,iBD,iAB},220,false,false,"hills"));
    m_provinces.push_back(mkP(6,"Loire Valley","Tours",{iNL,iLP,iLA,iAPo,iPLo},320,false,false));
    m_provinces.push_back(mkP(7,"Burgundy","Dijon",{iPCh,iAB,iBD,iABu,iLB},280,false,false,"hills"));
    m_provinces.push_back(mkP(8,"Poitou","Poitiers",{iBP,iPLo,iAPo,iAP,cRoch,cNant},180,true,false,"marsh"));
    m_provinces.push_back(mkP(9,"Aquitaine","Bordeaux",{iAP,iAPo,iALa,iLG,iAG,cBiarr,cBordC,cRoch},350,true,false));
    m_provinces[9].cityPos={-5,0,5.5f};
    m_provinces.push_back(mkP(10,"Languedoc","Toulouse",{iALa,iPL,cMontp,cNarb,cPerp,cPyrE,iLG},280,true,false));
    m_provinces.push_back(mkP(11,"Provence","Marseille",{iDP,cNice,cToulon,cMars,cMontp,iPL},300,true,false));
    m_provinces.push_back(mkP(12,"Dauphine","Grenoble",{iBD,cGenev,cAlps,cNice,iDP,iABu},180,false,false,"mountains"));
    m_provinces.push_back(mkP(13,"Auvergne","Clermont",{iLA,iLB,iABu,iDP,iPL,iALa,iAPo},150,false,false,"mountains"));
    m_provinces.push_back(mkP(14,"Gascony","Bayonne",{iAG,iLG,cPyrE,cPyrM,cPyrW,cBayo,cBiarr},120,true,false,"mountains"));

    for(auto&p:m_provinces){float t=(float)(p.id%5)*0.025f;p.color.r+=t-0.05f;p.color.g+=t*0.3f;}

    auto adj=[&](int a,int b){m_provinces[a].neighborIds.push_back(b);m_provinces[b].neighborIds.push_back(a);};
    adj(0,1);adj(0,3);adj(0,4);adj(0,6);adj(0,7);adj(0,13);
    adj(1,2);adj(1,3);adj(1,6);adj(2,6);adj(2,8);adj(3,4);
    adj(4,5);adj(4,7);adj(5,7);adj(5,12);adj(6,8);adj(6,13);
    adj(7,12);adj(7,13);adj(8,9);adj(8,13);adj(9,10);adj(9,14);
    adj(10,11);adj(10,13);adj(10,14);adj(11,12);adj(11,13);adj(12,13);

    // Terrain obstacles
    #define V(x,z) glm::vec3(x,0.0f,z)
    m_obstacles.push_back({"Alps","mountain",{V(6.2f,0.5f),V(7.2f,1),V(7.5f,2.5f),V(7,3.5f),V(6.3f,3.2f),V(5.8f,2),V(5.8f,1)},{},{0.55f,0.50f,0.45f}});
    m_obstacles.push_back({"Pyrenees","mountain",{V(-5,9),V(-3.5f,9.5f),V(-1.8f,9.8f),V(-0.8f,9.3f),V(-0.5f,10.2f),V(-1.8f,10.5f),V(-3.5f,10.5f),V(-5.2f,10.2f)},{},{0.50f,0.45f,0.40f}});
    m_obstacles.push_back({"Massif Central","mountain",{V(0.2f,3.5f),V(1.5f,3),V(2.2f,3.5f),V(2,5),V(1,5.2f),V(-0.2f,4.8f),V(-0.3f,4)},{},{0.52f,0.48f,0.42f}});
    m_obstacles.push_back({"Jura","mountain",{V(5.5f,-0.5f),V(6.2f,-0.3f),V(6.5f,0.5f),V(6,0.8f),V(5.3f,0.3f)},{},{0.53f,0.48f,0.43f}});
    m_obstacles.push_back({"Lac Leman","lake",{V(5.8f,0.6f),V(6.5f,0.5f),V(6.8f,0.9f),V(6.3f,1.1f),V(5.7f,0.9f)},{},{0.15f,0.30f,0.55f}});
    m_obstacles.push_back({"Etang de Berre","lake",{V(4,5.8f),V(4.5f,5.6f),V(4.8f,6),V(4.3f,6.3f),V(3.8f,6.1f)},{},{0.12f,0.28f,0.50f}});
    for(auto&ob:m_obstacles)ob.center=Centroid(ob.vertices);
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

    mkA("Duc de Richelieu",{-0.3f,0,-0.8f},4,2,2,1);
    mkA("Comte de Saxe",{1.0f,0,-3.5f},3,1,1,1);
    mkA("Chevalier de Belle-Isle",{5.5f,0,-1.5f},2,1,1,0);
    mkA("Duc de Villars",{4.5f,0,5.5f},2,0,1,0);

    Logger::Info("France: %d provinces, %d obstacles, %d armies",
        (int)m_provinces.size(),(int)m_obstacles.size(),(int)m_armies.size());
}

// ═══════════════════════════════════════════════════════════════
// INPUT
// ═══════════════════════════════════════════════════════════════
void CampaignMap::HandleLeftClick(const glm::vec3&worldPos){
    glm::vec2 pt(worldPos.x,worldPos.z);
    int bestId=-1;float bestD=0.8f;
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
    if(m_selectedArmyId<0)return;
    Army*army=GetArmy(m_selectedArmyId);
    if(!army)return;

    if(!IsPointPassable(worldPos)){Logger::Warning("Impassable!");return;}

    // A* pathfinding around obstacles
    auto path=FindPathWorld(army->worldPosition,worldPos);
    if(path.size()<2){Logger::Warning("No path found!");return;}

    // Calculate total path length
    float totalLen=0;
    for(int i=1;i<(int)path.size();i++)
        totalLen+=glm::distance(glm::vec2(path[i].x,path[i].z),glm::vec2(path[i-1].x,path[i-1].z));

    // Calculate turn breaks
    army->fullPath=path;
    army->currentPathIndex=1; // start at first waypoint after current pos
    army->totalPathLength=totalLen;
    army->distanceTraveled=0;
    army->turnBreaks.clear();

    float remaining=totalLen;
    float accumulated=0;
    float rangeThisTurn=army->movementRange;
    float rangePerTurn=army->movementRangeMax;

    // Turn 1: use remaining movement this turn
    float t1=std::min(rangeThisTurn,remaining);
    army->turnBreaks.push_back(t1);
    accumulated+=t1;remaining-=t1;

    // Subsequent turns
    while(remaining>0.01f){
        float seg=std::min(rangePerTurn,remaining);
        accumulated+=seg;remaining-=seg;
        army->turnBreaks.push_back(accumulated);
    }

    army->isMoving=true;

    int turns=(int)army->turnBreaks.size();
    Logger::Info("Army '%s' path: %.1f units, %d turn%s",
        army->generalName.c_str(),totalLen,turns,turns>1?"s":"");
}

// ═══════════════════════════════════════════════════════════════
// UPDATE
// ═══════════════════════════════════════════════════════════════
void CampaignMap::Update(float dt,const InputManager&){UpdateArmyPositions(dt);}

void CampaignMap::UpdateArmyPositions(float dt){
    for(auto&army:m_armies){
        if(!army.isMoving||army.fullPath.empty())continue;

        // How far can we walk this turn?
        float turnLimit=(army.turnBreaks.empty())?army.movementRange:army.turnBreaks[0];

        // Move toward next waypoint
        if(army.currentPathIndex>=(int)army.fullPath.size()){
            army.isMoving=false;army.fullPath.clear();army.turnBreaks.clear();
            UpdateArmyProvince(army);continue;
        }

        glm::vec3 target=army.fullPath[army.currentPathIndex];
        glm::vec3 dir=target-army.worldPosition;
        float dist=glm::length(glm::vec2(dir.x,dir.z));

        if(dist<0.05f){
            army.worldPosition=target;
            army.currentPathIndex++;
            if(army.currentPathIndex>=(int)army.fullPath.size()){
                army.isMoving=false;army.fullPath.clear();army.turnBreaks.clear();
                army.movementRange=0;
                UpdateArmyProvince(army);
            }
            continue;
        }

        float step=army.moveSpeed*dt;

        // Check if we'd exceed this turn's limit
        float wouldTravel=army.distanceTraveled+step;
        if(wouldTravel>=turnLimit){
            // Stop here for this turn — remaining path continues next turn
            army.isMoving=false; // will resume next turn
            army.movementRange=0;

            Logger::Info("Army '%s' stopped (end of turn movement)",army.generalName.c_str());
            UpdateArmyProvince(army);
            continue;
        }

        if(step>dist)step=dist;
        glm::vec3 moveDir=glm::normalize(dir);
        army.worldPosition+=moveDir*step;
        army.worldPosition.y=0;
        army.distanceTraveled+=step;

        if(army.id==m_selectedArmyId)m_selectionWorldPos=army.worldPosition;
    }
}

void CampaignMap::UpdateArmyProvince(Army&a){
    Province*p=GetProvinceAtWorldPos(a.worldPosition);if(p)a.currentProvinceId=p->id;
    if(a.id==m_selectedArmyId)m_selectionWorldPos=a.worldPosition;
}

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

    // Reset movement and continue scheduled paths
    for(auto&a:m_armies){
        a.movementRange=a.movementRangeMax;
        a.distanceTraveled=0;

        // If army has remaining scheduled path, continue it
        if(!a.fullPath.empty()&&a.currentPathIndex<(int)a.fullPath.size()){
            // Remove the first turn break (it's been used)
            if(!a.turnBreaks.empty()){
                float used=a.turnBreaks[0];
                a.turnBreaks.erase(a.turnBreaks.begin());
                // Shift remaining breaks down
                for(auto&tb:a.turnBreaks)tb-=used;
            }
            a.isMoving=true;
            Logger::Info("Army '%s' continues march (%d waypoints left)",
                a.generalName.c_str(),(int)a.fullPath.size()-a.currentPathIndex);
        }
    }
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
BattleSetupData CampaignMap::GetPendingBattle() { return m_pendingBattle.value(); }
void CampaignMap::ApplyBattleResult(const BattleResult&) { m_pendingBattle.reset(); }