#include "neon/world.hpp"
#include <queue>

namespace neon {
City::City(std::uint64_t seedValue) : seed(seedValue) {
    Rng rng(seed);
    districts = {{{"HARBOR / 01", {768,768},0}, {"CHROME / 02", {2304,768},1},
                  {"OLD TOWN / 03", {768,2304},2}, {"MIDNIGHT / 04", {2304,2304},3}}};
    for (int by=0; by<cityBlocks; ++by) for (int bx=0; bx<cityBlocks; ++bx) {
        float x = static_cast<float>(bx*blockSize)+streetWidth;
        float y = static_cast<float>(by*blockSize)+streetWidth;
        if ((bx+by*3)%9==4) {
            Park park{{x+12,y+12,228,228},{}};
            for (int t=0;t<14;++t) park.trees.push_back({x+rng.range(24,226),y+rng.range(24,226)});
            parks.push_back(std::move(park));
        } else {
            int district = (bx>=4 ? 1:0)+(by>=4 ? 2:0);
            // Separated footprints leave real alleys and navigable sidewalks.
            for (int sy=0;sy<2;++sy) for (int sx=0;sx<2;++sx) {
                float w=rng.range(84,105), h=rng.range(84,105);
                Rect r{x+16+static_cast<float>(sx*116),y+16+static_cast<float>(sy*116),w,h};
                BuildingKind kind=static_cast<BuildingKind>(rng.integer(0,3));
                buildings.push_back({r,kind,district,rng.integer(2,8),rng.next()});
            }
        }
    }
    services = {
        {{176,516},ServiceKind::Safehouse,"THE LOFT",0},
        {{560,132},ServiceKind::Contact,"RELAY / courier",0},
        {{1712,132},ServiceKind::Contact,"VECTOR / acquisition",1},
        {{2480,900},ServiceKind::Contact,"STATIC / extraction",1},
        {{560,2052},ServiceKind::Contact,"GHOST / courier",2},
        {{2096,2052},ServiceKind::Contact,"ONYX / acquisition",3},
        {{2864,2820},ServiceKind::Contact,"ECHO / extraction",3},
        {{944,900},ServiceKind::Garage,"PITSTOP NORTH",0},
        {{1712,2436},ServiceKind::Garage,"PITSTOP SOUTH",3}
    };
}
bool City::isStreet(Vec2 p) const {
    if (p.x<0 || p.y<0 || p.x>=citySize || p.y>=citySize) return false;
    return std::fmod(p.x,static_cast<float>(blockSize))<streetWidth ||
           std::fmod(p.y,static_cast<float>(blockSize))<streetWidth;
}
bool City::blocked(Vec2 p, float radius) const {
    if (p.x<radius || p.y<radius || p.x>citySize-radius || p.y>citySize-radius) return true;
    for (const auto& b:buildings) if (circleRect(p,radius+0.01f,b.bounds)) return true;
    return false;
}
bool City::lineOfSight(Vec2 a, Vec2 b) const {
    for (const auto& building:buildings) if (segmentRect(a,b,building.bounds)) return false;
    return true;
}
Vec2 City::moveCircle(Vec2 p, Vec2 delta, float radius, bool* collided) const {
    int steps=std::max(1, static_cast<int>(std::ceil(length(delta)/6.0f)));
    Vec2 step=delta/static_cast<float>(steps);
    bool hit=false;
    for (int i=0;i<steps;++i) {
        Vec2 nx{p.x+step.x,p.y};
        if (!blocked(nx,radius)) p=nx; else hit=true;
        Vec2 ny{p.x,p.y+step.y};
        if (!blocked(ny,radius)) p=ny; else hit=true;
    }
    if (collided) *collided=hit;
    return p;
}
Vec2 City::node(int index) const {
    index=std::clamp(index,0,cityBlocks*cityBlocks-1);
    return {static_cast<float>((index%cityBlocks)*blockSize)+64,
            static_cast<float>((index/cityBlocks)*blockSize)+64};
}
int City::nearestNode(Vec2 p) const {
    int x=std::clamp(static_cast<int>(std::lround((p.x-64)/blockSize)),0,cityBlocks-1);
    int y=std::clamp(static_cast<int>(std::lround((p.y-64)/blockSize)),0,cityBlocks-1);
    return y*cityBlocks+x;
}
std::vector<int> City::neighbors(int index) const {
    std::vector<int> out;
    int x=index%cityBlocks,y=index/cityBlocks;
    if(x>0) out.push_back(index-1);
    if(x<cityBlocks-1) out.push_back(index+1);
    if(y>0) out.push_back(index-cityBlocks);
    if(y<cityBlocks-1) out.push_back(index+cityBlocks);
    return out;
}
std::vector<int> City::route(Vec2 start, Vec2 end) const {
    int from=nearestNode(start),to=nearestNode(end);
    std::array<int,cityBlocks*cityBlocks> previous;
    previous.fill(-1); previous[static_cast<std::size_t>(from)]=from;
    std::queue<int> queue; queue.push(from);
    while (!queue.empty()) {
        int at=queue.front(); queue.pop();
        if(at==to) break;
        for(int n:neighbors(at)) if(previous[static_cast<std::size_t>(n)]==-1) {
            previous[static_cast<std::size_t>(n)]=at; queue.push(n);
        }
    }
    std::vector<int> path;
    for(int n=to;n!=from;n=previous[static_cast<std::size_t>(n)]) path.push_back(n);
    path.push_back(from); std::reverse(path.begin(),path.end());
    return path;
}
int City::districtAt(Vec2 p) const { return (p.x>=citySize/2 ? 1:0)+(p.y>=citySize/2 ? 2:0); }
Vec2 City::spawn() const { return {176,484}; }
}
