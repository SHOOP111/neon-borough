#include "neon/presentation.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace neon {
namespace {
constexpr Color ink{9, 13, 22, 255}, panel{14, 22, 34, 238}, cyan{69, 234, 224, 255}, pink{255, 78, 147, 255},
    gold{255, 199, 95, 255}, muted{119, 141, 160, 255}, paper{222, 237, 238, 255};
constexpr Color carColors[]{{65, 162, 181, 255},  {211, 104, 103, 255}, {215, 185, 112, 255},
                            {108, 130, 160, 255}, {179, 191, 198, 255}, {108, 82, 159, 255},
                            {238, 72, 130, 255},  {45, 71, 85, 255}};
Vector2 rv(Vec2 p) {
    return {p.x, p.y};
}
Rectangle rr(Rect r) {
    return {r.x, r.y, r.w, r.h};
}
Color alpha(Color c, float a) {
    c.a = static_cast<unsigned char>(std::clamp(a, 0.0f, 1.0f) * 255);
    return c;
}
Color brighten(Color c, int n) {
    return {static_cast<unsigned char>(std::clamp(static_cast<int>(c.r) + n, 0, 255)),
            static_cast<unsigned char>(std::clamp(static_cast<int>(c.g) + n, 0, 255)),
            static_cast<unsigned char>(std::clamp(static_cast<int>(c.b) + n, 0, 255)), c.a};
}
void text(const std::string& value, float x, float y, float size, Color color = paper) {
    DrawTextEx(GetFontDefault(), value.c_str(), {x, y}, size, 1, color);
}
void box(Rect r, Color color) {
    DrawRectangleRec(rr(r), color);
}
void outline(Rect r, Color color, float thickness = 1) {
    DrawRectangleLinesEx(rr(r), thickness, color);
}
void line(Vec2 a, Vec2 b, float width, Color color) {
    DrawLineEx(rv(a), rv(b), width, color);
}
void glow(Vec2 p, float radius, Color color, float intensity = 0.22f) {
    DrawCircleGradient(static_cast<int>(p.x), static_cast<int>(p.y), radius, alpha(color, intensity),
                       alpha(color, 0));
}
void bar(float x, float y, float w, float value, Color color) {
    box({x, y, w, 5}, {35, 47, 59, 255});
    box({x, y, w * std::clamp(value / 100, 0.0f, 1.0f), 5}, color);
}
void diamond(Vec2 p, float size, Color color) {
    line({p.x, p.y - size}, {p.x + size, p.y}, 2, color);
    line({p.x + size, p.y}, {p.x, p.y + size}, 2, color);
    line({p.x, p.y + size}, {p.x - size, p.y}, 2, color);
    line({p.x - size, p.y}, {p.x, p.y - size}, 2, color);
}
void button(Rect r, const std::string& label, bool active = false) {
    bool hover = CheckCollisionPointRec(GetMousePosition(), rr(r));
    box(r, hover ? Color{32, 65, 72, 250} : Color{19, 32, 45, 240});
    outline(r, hover || active ? cyan : Color{50, 69, 83, 255});
    if (hover)
        box({r.x, r.y, 3, r.h}, cyan);
    float fontSize = 18;
    while (fontSize > 12 && MeasureTextEx(GetFontDefault(), label.c_str(), fontSize, 1).x > r.w - 36)
        fontSize -= 0.5f;
    text(label, r.x + 18, r.y + (r.h - fontSize) * 0.5f, fontSize, hover ? cyan : paper);
}
void carRect(const Vehicle& car, Vec2 offset, Vec2 size, Color color) {
    Vec2 f = direction(car.heading), s{-f.y, f.x};
    Vec2 center = car.position + f * offset.x + s * offset.y;
    DrawRectanglePro({center.x, center.y, size.x, size.y}, {size.x * 0.5f, size.y * 0.5f},
                     car.heading * 180 / pi, color);
}
std::string cash(int value) {
    return "$" + std::to_string(value);
}
const char* shaderSource = R"GLSL(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec2 resolution;
uniform float clock;
uniform float strength;
out vec4 finalColor;
void main() {
    vec2 uv=fragTexCoord;
    vec3 c=texture(texture0,uv).rgb;
    vec3 bloom=vec3(0.0);
    vec2 px=2.8/resolution;
    for(int x=-1;x<=1;x++) for(int y=-1;y<=1;y++) {
        vec3 s=texture(texture0,uv+vec2(x,y)*px).rgb;
        bloom+=max(s-vec3(0.52),vec3(0.0));
    }
    c+=bloom*(0.11*strength);
    vec2 q=uv*2.0-1.0;
    float vignette=1.0-0.19*dot(q,q)*strength;
    float grain=fract(sin(dot(uv*resolution,vec2(12.9898,78.233))+floor(clock*30.0))*43758.5453)-0.5;
    c=c*vignette+grain*0.012*strength;
    finalColor=vec4(c,1.0)*colDiffuse*fragColor;
}
)GLSL";
} // namespace
Rect mapBounds(float width, float height, bool full) {
    float size = full ? std::min(height - 140, width - 180) : 184;
    return full ? Rect{(width - size) / 2, (height - size) / 2, size, size}
                : Rect{24, height - 224, size, size};
}
std::optional<Vec2> navigationTarget(const Simulation& sim, const ViewState& view) {
    if (sim.mission.active)
        return sim.mission.target;
    if (view.waypointSet)
        return view.waypoint;
    return std::nullopt;
}
Presentation::Presentation() {
    camera_.target = {176, 100};
    camera_.zoom = 1;
    post_ = LoadShaderFromMemory(nullptr, shaderSource);
    resolutionLocation_ = GetShaderLocation(post_, "resolution");
    timeLocation_ = GetShaderLocation(post_, "clock");
    strengthLocation_ = GetShaderLocation(post_, "strength");
    shaderReady_ =
        IsShaderValid(post_) && resolutionLocation_ >= 0 && timeLocation_ >= 0 && strengthLocation_ >= 0;
    resize();
    particles_.reserve(1600);
}
Presentation::~Presentation() {
    UnloadRenderTexture(scene_);
    UnloadShader(post_);
}
void Presentation::resize() {
    int w = std::max(1, GetScreenWidth()), h = std::max(1, GetScreenHeight());
    if (scene_.texture.width == w && scene_.texture.height == h)
        return;
    if (scene_.id)
        UnloadRenderTexture(scene_);
    scene_ = LoadRenderTexture(w, h);
    SetTextureFilter(scene_.texture, TEXTURE_FILTER_BILINEAR);
}
Vec2 Presentation::worldMouse() const {
    Vector2 p = GetScreenToWorld2D(GetMousePosition(), camera_);
    return {p.x, p.y};
}
void Presentation::message(const std::string& value, bool reward) {
    if (toasts_.size() >= 4)
        toasts_.pop_front();
    toasts_.push_back({value, reward ? 6.0f : 4.5f, reward});
}
void Presentation::ingest(const std::vector<Event>& events) {
    for (const auto& e : events) {
        if (!e.text.empty())
            message(e.text, e.kind == EventKind::Reward);
        if (e.kind == EventKind::Skid) {
            Vec2 forward = direction(e.value), side{-forward.y, forward.x};
            for (float offset : {-10.0f, 10.0f}) {
                if (skids_.size() >= 1200)
                    skids_.pop_front();
                Vec2 at = e.position - forward * 12 + side * offset;
                skids_.push_back({at - forward * 6, at, 10});
            }
        }
        int count = e.kind == EventKind::Explosion ? 65
                    : e.kind == EventKind::Impact  ? 10
                    : e.kind == EventKind::Shot    ? 4
                    : e.kind == EventKind::Reward  ? 35
                                                   : 0;
        for (int i = 0; i < count && particles_.size() < 1600; ++i) {
            float life = rng_.range(0.15f, e.kind == EventKind::Explosion ? 1.5f : 0.55f);
            Color c = e.kind == EventKind::Reward      ? cyan
                      : e.kind == EventKind::Explosion ? (i % 3 ? gold : pink)
                                                       : gold;
            particles_.push_back({e.position,
                                  direction(rng_.range(-pi, pi)) *
                                      rng_.range(25, e.kind == EventKind::Explosion ? 160.0f : 90.0f),
                                  life, life, rng_.range(1, 4), c});
        }
    }
}
void Presentation::drawCity(const Simulation& sim, const ViewState& view, Rect visible) {
    const City& city = sim.city;
    box({0, 0, citySize, citySize}, {26, 36, 47, 255});
    // Street ribbons and raised sidewalk curbs.
    for (int b = 0; b < cityBlocks; ++b) {
        float c = static_cast<float>(b * blockSize);
        box({c, 0, streetWidth, citySize}, {19, 28, 37, 255});
        box({0, c, citySize, streetWidth}, {19, 28, 37, 255});
        line({c + streetWidth, 0}, {c + streetWidth, citySize}, 3, {73, 90, 98, 255});
        line({0, c + streetWidth}, {citySize, c + streetWidth}, 3, {73, 90, 98, 255});
        line({c + streetWidth + 7, 0}, {c + streetWidth + 7, citySize}, 1, {43, 58, 68, 255});
        line({0, c + streetWidth + 7}, {citySize, c + streetWidth + 7}, 1, {43, 58, 68, 255});
        for (int segment = 0; segment < cityBlocks; ++segment) {
            float start = static_cast<float>(segment * blockSize) + 128;
            for (int dash = 0; dash < 8; ++dash) {
                float p = start + static_cast<float>(dash * 32) + 5;
                if (visible.overlaps({c, p, 128, 24})) {
                    box({c + 63, p, 2, 18}, {127, 117, 81, 210});
                    box({c + 31, p, 1, 16}, {74, 86, 91, 255});
                    box({c + 95, p, 1, 16}, {74, 86, 91, 255});
                }
                if (visible.overlaps({p, c, 24, 128})) {
                    box({p, c + 63, 18, 2}, {127, 117, 81, 210});
                    box({p, c + 31, 16, 1}, {74, 86, 91, 255});
                    box({p, c + 95, 16, 1}, {74, 86, 91, 255});
                }
            }
        }
    }
    for (int y = 0; y < cityBlocks; ++y)
        for (int x = 0; x < cityBlocks; ++x) {
            float px = static_cast<float>(x * blockSize), py = static_cast<float>(y * blockSize);
            if (!visible.overlaps({px - 25, py - 25, 180, 180}))
                continue;
            for (int d = 0; d < 6; ++d) {
                float step = static_cast<float>(d * 13) + 28;
                box({px + step, py + 113, 7, 12}, {118, 129, 132, 255});
                box({px + 113, py + step, 12, 7}, {118, 129, 132, 255});
            }
            bool green = static_cast<int>(std::fmod(sim.time / 7, 2.0f)) == 0;
            box({px + 125, py + 118, 4, 13}, {7, 12, 18, 255});
            DrawCircle(static_cast<int>(px + 127), static_cast<int>(py + 120), 2, green ? pink : cyan);
            glow({px + 132, py + 132}, 55, gold, 0.1f);
            DrawCircle(static_cast<int>(px + 132), static_cast<int>(py + 132), 3, gold);
            box({px + 132, py + 140, 3, 10}, {18, 24, 33, 255});
        }
    if (auto destination = navigationTarget(sim, view)) {
        auto route = city.route(sim.playerPosition(), *destination);
        for (std::size_t i = 1; i < route.size(); ++i) {
            Vec2 a = city.node(route[i - 1]), b = city.node(route[i]);
            line(a, b, 3, alpha(cyan, 0.25f));
            Vec2 dir = normalized(b - a);
            for (float d = 20; d < distance(a, b); d += 45)
                DrawCircleV(rv(a + dir * d), 2, alpha(cyan, 0.6f));
        }
    }
    for (const auto& park : city.parks) {
        if (!visible.overlaps(park.bounds))
            continue;
        box(park.bounds, {34, 62, 55, 255});
        outline(park.bounds, {64, 93, 78, 255}, 2);
        box({park.bounds.x, park.bounds.y + 105, park.bounds.w, 13}, {77, 85, 74, 255});
        box({park.bounds.x + 104, park.bounds.y, 13, park.bounds.h}, {77, 85, 74, 255});
        for (Vec2 p : park.trees) {
            DrawCircleV(rv(p + Vec2{6, 8}), 17, {10, 24, 27, 130});
            DrawCircleV(rv(p), 17, {24, 65, 54, 255});
            DrawCircleV(rv(p + Vec2{-4, -4}), 12, {41, 92, 65, 255});
            DrawCircleV(rv(p + Vec2{-6, -6}), 5, {61, 112, 76, 255});
        }
        box({park.bounds.x + 70, park.bounds.y + 96, 26, 5}, {136, 113, 80, 255});
    }
    constexpr Color roofs[]{{48, 70, 84, 255}, {55, 62, 90, 255}, {72, 65, 62, 255}, {68, 47, 74, 255}};
    constexpr Color lights[]{cyan, {141, 169, 255, 255}, gold, pink};
    for (const auto& building : city.buildings) {
        Rect r = building.bounds;
        if (!visible.overlaps({r.x - 10, r.y - 10, r.w + 40, r.h + 40}))
            continue;
        float depth = 5 + static_cast<float>(building.floors) * 1.4f;
        box({r.x + depth, r.y + depth, r.w, r.h}, {6, 12, 21, 180});
        Color roof = roofs[building.palette], light = lights[building.palette];
        box({r.x, r.y, r.w, r.h}, brighten(roof, -18));
        box({r.x + 3, r.y + 3, r.w - 6, r.h - 9}, roof);
        outline({r.x + 5, r.y + 5, r.w - 10, r.h - 14}, brighten(roof, 13));
        box({r.x + 7, r.y + 7, r.w - 14, 2}, brighten(roof, 26));
        // Roof mechanicals are derived from the city seed, never frame-randomized.
        float ventX = r.x + 16 + static_cast<float>(building.detail % 20);
        box({ventX + 3, r.y + 21, 23, 19}, brighten(roof, -24));
        box({ventX, r.y + 18, 23, 18}, brighten(roof, 26));
        DrawCircle(static_cast<int>(ventX + 11), static_cast<int>(r.y + 27), 6, brighten(roof, -12));
        line({ventX + 5, r.y + 27}, {ventX + 17, r.y + 27}, 1, muted);
        line({ventX + 11, r.y + 21}, {ventX + 11, r.y + 33}, 1, muted);
        box({r.x + r.w - 29, r.y + 14, 14, 24}, brighten(roof, -15));
        for (int k = 0; k < 4; ++k)
            box({r.x + r.w - 27, r.y + 17 + static_cast<float>(k * 5), 10, 2}, brighten(roof, 17));
        for (int window = 0; window < 5; ++window) {
            Color color = ((building.detail >> window) & 1U) ? alpha(light, 0.85f) : Color{29, 44, 58, 255};
            box({r.x + 10 + static_cast<float>(window) * ((r.w - 22) / 5), r.y + r.h - 6, 9, 3}, color);
            box({r.x + r.w - 4, r.y + 47 + static_cast<float>(window * 8), 3, 4}, color);
        }
        if (building.kind == BuildingKind::Club) {
            box({r.x + 12, r.y + r.h - 23, r.w - 24, 10}, {19, 22, 35, 255});
            const char* names[]{"DUSK", "SYNTH", "VINYL", "VOID"};
            text(names[building.detail % 4], r.x + 18, r.y + r.h - 23, 10, light);
            line({r.x + 10, r.y + r.h - 11}, {r.x + r.w - 10, r.y + r.h - 11}, 2, light);
            glow({r.x + r.w / 2, r.y + r.h}, 50, light, 0.14f);
        } else if (building.kind == BuildingKind::Warehouse) {
            for (int j = 0; j < 4; ++j)
                line({r.x + 10, r.y + 50 + static_cast<float>(j * 7)},
                     {r.x + r.w - 10, r.y + 50 + static_cast<float>(j * 7)}, 1, brighten(roof, -5));
        } else if (building.kind == BuildingKind::Offices) {
            box({r.x + 12, r.y + 46, r.w - 45, 25}, {24, 44, 61, 255});
            for (int j = 0; j < 3; ++j)
                line({r.x + 15 + static_cast<float>(j * 12), r.y + 46},
                     {r.x + 15 + static_cast<float>(j * 12), r.y + 71}, 1, brighten(roof, 15));
        }
    }
    for (const auto& service : city.services) {
        Vec2 p = service.position;
        if (!visible.contains(p))
            continue;
        Color c = service.kind == ServiceKind::Contact  ? cyan
                  : service.kind == ServiceKind::Garage ? Color{109, 234, 155, 255}
                                                        : Color{188, 137, 255, 255};
        float pulse = 0.65f + 0.2f * std::sin(clock_ * 3);
        glow(p, 44, c, 0.24f);
        DrawCircleLinesV(rv(p), 17, alpha(c, pulse));
        diamond(p, 8, c);
        if (distance(p, sim.playerPosition()) < 110) {
            text(service.name, p.x - 45, p.y - 43, 12, c);
            text("[ F ]", p.x - 15, p.y + 23, 12, paper);
        }
    }
    if (!sim.mission.active && view.waypointSet) {
        glow(view.waypoint, 55, cyan);
        diamond(view.waypoint, 16, cyan);
        text("WAYPOINT", view.waypoint.x - 30, view.waypoint.y - 40, 12, cyan);
    }
    if (sim.mission.active) {
        Vec2 p = sim.mission.target;
        glow(p, 72, gold, 0.25f);
        DrawCircleLinesV(rv(p), 32 + 3 * std::sin(clock_ * 4), gold);
        diamond(p, 18, gold);
        text("OBJECTIVE", p.x - 32, p.y - 50, 12, gold);
        if (sim.mission.hold > 0)
            DrawCircleSectorLines(rv(p), 27, -90, -90 + sim.mission.hold / 3 * 360, 24, cyan);
    }
}
void Presentation::drawActors(const Simulation& sim, Rect visible) {
    for (const auto& p : sim.pedestrians) {
        if (!visible.contains(p.position))
            continue;
        if (p.health <= 0) {
            DrawEllipse(static_cast<int>(p.position.x), static_cast<int>(p.position.y), 8, 4,
                        {55, 48, 55, 220});
            continue;
        }
        Vec2 d = direction(p.heading), s{-d.y, d.x};
        float walk = std::sin(sim.time * (p.panic > 0 ? 16 : 8) + p.position.x) * 2;
        DrawCircleV(rv(p.position + Vec2{3, 4}), 6, {0, 0, 0, 90});
        line(p.position + s * 3, p.position + s * 3 + d * walk, 3, {24, 33, 47, 255});
        line(p.position - s * 3, p.position - s * 3 - d * walk, 3, {24, 33, 47, 255});
        DrawCircleV(rv(p.position), 5, carColors[p.color]);
        DrawCircleV(rv(p.position + d * 2), 3, {203, 168, 138, 255});
    }
    for (const auto& car : sim.vehicles) {
        if (!car.active || !visible.overlaps({car.position.x - 100, car.position.y - 100, 200, 200}))
            continue;
        Vec2 f = direction(car.heading), side{-f.y, f.x};
        if (car.health <= 0) {
            carRect(car, {3, 5}, {43, 23}, {5, 9, 15, 150});
            carRect(car, {0, 0}, {41, 21}, {32, 34, 40, 255});
            carRect(car, {3, 0}, {18, 14}, {13, 22, 30, 255});
            glow(car.position, 28, {249, 115, 47, 255}, 0.22f);
            continue;
        }
        // Soft headlight cones are layered triangles, with a bright center core.
        for (float sy : {-7.0f, 7.0f}) {
            Vec2 origin = car.position + f * 21 + side * sy, end = origin + f * 100;
            DrawTriangle(rv(origin), rv(end - side * 28), rv(end + side * 28), {184, 215, 192, 15});
            DrawTriangle(rv(origin), rv(end - side * 12), rv(end + side * 12), {224, 245, 205, 13});
        }
        float carLength = car.kind == VehicleKind::Van ? 46.0f : 40.0f;
        Vehicle shadow = car;
        shadow.position += Vec2{4, 5};
        carRect(shadow, {0, 0}, {carLength + 3, 25}, {2, 7, 14, 140});
        carRect(car, {-12, -11}, {9, 4}, {5, 11, 17, 255});
        carRect(car, {12, -11}, {9, 4}, {5, 11, 17, 255});
        carRect(car, {-12, 11}, {9, 4}, {5, 11, 17, 255});
        carRect(car, {12, 11}, {9, 4}, {5, 11, 17, 255});
        Color body = car.kind == VehicleKind::Police ? Color{201, 213, 216, 255} : carColors[car.color];
        carRect(car, {0, 0}, {carLength, 21}, brighten(body, -22));
        carRect(car, {0, -1}, {carLength - 2, 18}, body);
        carRect(car, {-3, 0}, {20, 16}, brighten(body, -6));
        carRect(car, {7, 0}, {7, 15}, {31, 54, 68, 255});
        carRect(car, {-11, 0}, {5, 14}, {27, 44, 56, 255});
        carRect(car, {0, -9}, {30, 1}, brighten(body, 43));
        carRect(car, {15, 0}, {4, 14}, brighten(body, 15));
        carRect(car, {20, -7}, {2, 4}, {255, 244, 191, 255});
        carRect(car, {20, 7}, {2, 4}, {255, 244, 191, 255});
        carRect(car, {-20, -7}, {2, 4}, pink);
        carRect(car, {-20, 7}, {2, 4}, pink);
        if (car.kind == VehicleKind::Sport) {
            carRect(car, {-17, 0}, {3, 25}, brighten(body, -10));
            carRect(car, {13, 0}, {10, 3}, brighten(body, -20));
        }
        if (car.kind == VehicleKind::Police) {
            carRect(car, {0, 0}, {7, 18}, {21, 30, 44, 255});
            Color a =
                std::sin(sim.time * 15 + static_cast<float>(car.id)) > 0 ? pink : Color{47, 70, 108, 255};
            Color b = a.r == pink.r ? Color{34, 64, 79, 255} : cyan;
            carRect(car, {0, -5}, {4, 6}, a);
            carRect(car, {0, 5}, {4, 6}, b);
            if (sim.wanted() > 0) {
                glow(car.position - side * 5, 38, a, 0.18f);
                glow(car.position + side * 5, 38, b, 0.18f);
            }
        }
        if (car.health < 45) {
            glow(car.position + f * 15, 14, gold, 0.08f);
            for (int s = 0; s < 3; ++s) {
                float age = std::fmod(clock_ * 0.7f + static_cast<float>(s) / 3, 1.0f);
                DrawCircleV(rv(car.position + f * 15 + Vec2{age * 9, -age * 25}), 2 + age * 5,
                            alpha(muted, (1 - age) * 0.35f));
            }
        }
        if (car.id == sim.player.vehicle) {
            DrawCircleV(rv(car.position), 3, paper);
            DrawCircleLinesV(rv(car.position), 27, alpha(cyan, 0.48f));
        }
    }
    if (sim.player.vehicle == -1) {
        Vec2 p = sim.player.position, d = sim.player.aim, s{-d.y, d.x};
        if (sim.player.invulnerable == 0 || static_cast<int>(sim.time * 12) % 2 == 0) {
            DrawCircleV(rv(p + Vec2{3, 5}), 9, {2, 8, 15, 120});
            DrawCircleV(rv(p), 8, {29, 132, 142, 255});
            line(p - s * 6, p + d * 11 - s * 4, 4, {32, 91, 111, 255});
            line(p + s * 6, p + d * 12 + s * 3, 4, {32, 91, 111, 255});
            line(p + d * 8, p + d * 18, 4, {169, 181, 188, 255});
            DrawCircleV(rv(p - d * 1), 5, {225, 184, 148, 255});
            DrawCircleV(rv(p - d * 3), 4, {27, 34, 45, 255});
        }
    }
    for (const auto& bullet : sim.bullets) {
        Color c = bullet.hostile ? pink : gold;
        line(bullet.position - normalized(bullet.velocity) * 16, bullet.position, 2, c);
        glow(bullet.position, 9, c, 0.25f);
    }
    for (const auto& p : particles_) {
        float a = p.life / p.maximum;
        DrawCircleV(rv(p.position), p.size * a, alpha(p.color, a));
    }
}
void Presentation::drawMap(const Simulation& sim, const ViewState& view, bool full) {
    float w = static_cast<float>(GetScreenWidth()), h = static_cast<float>(GetScreenHeight());
    Rect r = mapBounds(w, h, full);
    float size = r.w;
    if (full) {
        box({0, 0, w, h}, {6, 11, 20, 225});
        text("CITY NETWORK / M TO CLOSE", r.x, r.y - 36, 22, cyan);
    }
    box({r.x - 6, r.y - 6, r.w + 12, r.h + 12}, panel);
    box(r, {24, 36, 48, 255});
    float scale = size / citySize;
    auto point = [r, scale](Vec2 p) { return Vec2{r.x + p.x * scale, r.y + p.y * scale}; };
    for (int b = 0; b < cityBlocks; ++b) {
        float p = static_cast<float>(b * blockSize) * scale;
        box({r.x + p, r.y, streetWidth * scale, size}, {49, 66, 77, 255});
        box({r.x, r.y + p, size, streetWidth * scale}, {49, 66, 77, 255});
    }
    for (const auto& p : sim.city.parks)
        box({r.x + p.bounds.x * scale, r.y + p.bounds.y * scale, p.bounds.w * scale, p.bounds.h * scale},
            {47, 92, 69, 255});
    for (const auto& s : sim.city.services) {
        Vec2 p = point(s.position);
        Color c = s.kind == ServiceKind::Contact  ? cyan
                  : s.kind == ServiceKind::Garage ? Color{101, 219, 142, 255}
                                                  : Color{188, 137, 255, 255};
        DrawCircleV(rv(p), full ? 4.0f : 2.2f, c);
        if (full)
            text(s.name, p.x + 8, p.y - 6, 10, c);
    }
    for (const auto& v : sim.vehicles)
        if (v.active && v.driver == 1 && v.health > 0)
            DrawCircleV(rv(point(v.position)), full ? 4.0f : 2.5f, pink);
    if (auto destination = navigationTarget(sim, view)) {
        auto route = sim.city.route(sim.playerPosition(), *destination);
        for (std::size_t i = 1; i < route.size(); ++i)
            line(point(sim.city.node(route[i - 1])), point(sim.city.node(route[i])), full ? 3.0f : 1.5f,
                 cyan);
        diamond(point(*destination), full ? 10.0f : 5.0f, sim.mission.active ? gold : cyan);
    }
    Vec2 player = point(sim.playerPosition());
    DrawCircleV(rv(player), full ? 6.0f : 3.5f, paper);
    DrawCircleLinesV(rv(player), full ? 12.0f : 7.0f, cyan);
    outline(r, {64, 96, 111, 255});
    if (full) {
        text("CYAN contacts     GREEN garages     VIOLET safehouse     RED police", r.x, r.y + size + 16, 14,
             muted);
        text("Paused. Click a service / road to route; right-click clears the waypoint.", r.x,
             r.y + size + 38, 14, paper);
    } else
        text("M / NETWORK", r.x, r.y + size + 10, 12, muted);
}
void Presentation::drawHud(const Simulation& sim, const ViewState& view) {
    float w = static_cast<float>(GetScreenWidth()), h = static_cast<float>(GetScreenHeight());
    box({24, 24, 260, 120}, panel);
    box({24, 24, 3, 120}, cyan);
    text("NEON / BOROUGH", 40, 36, 20, cyan);
    text(cash(sim.player.cash), 40, 65, 27, paper);
    text("VITAL", 40, 106, 10, muted);
    bar(82, 109, 80, sim.player.health, pink);
    text("ARMOR", 173, 106, 10, muted);
    bar(220, 109, 46, sim.player.armor, cyan);
    bar(40, 131, 224, sim.player.stamina, gold);
    std::string district =
        sim.city.districts[static_cast<std::size_t>(sim.city.districtAt(sim.playerPosition()))].name;
    text(district, 24, 155, 14, muted);
    float wantedX = w / 2 - 80;
    for (int i = 0; i < 5; ++i) {
        diamond({wantedX + static_cast<float>(i * 38), 40}, 10,
                i < sim.wanted() ? pink : Color{62, 75, 86, 255});
    }
    if (sim.wanted() > 0)
        text(sim.unseen > 2 ? "SEARCHING / STAY HIDDEN" : "PURSUIT / BREAK LINE OF SIGHT", w / 2 - 133, 61,
             12, sim.unseen > 2 ? gold : pink);
    if (sim.mission.active) {
        Rect card{w - 372, 24, 348, 160};
        box(card, panel);
        box({card.x, card.y, card.w, 3}, gold);
        text("CONTRACT / " + sim.mission.title, card.x + 15, card.y + 17, 16, gold);
        int sec = static_cast<int>(std::ceil(sim.mission.timeLeft));
        char timer[32];
        std::snprintf(timer, sizeof(timer), "%02d:%02d", sec / 60, sec % 60);
        text(timer, card.x + 15, card.y + 46, 29, sim.mission.timeLeft < 30 ? pink : paper);
        text(std::to_string(static_cast<int>(distance(sim.playerPosition(), sim.mission.target))) + " m",
             card.x + 220, card.y + 53, 20, cyan);
        std::string stage = sim.mission.kind == MissionKind::Courier
                                ? "DROPS " + std::to_string(sim.mission.deliveries) + " / 3"
                            : sim.mission.stage == 0 ? "ACQUIRE TARGET"
                                                     : "RETURN / ZERO WANTED";
        text(stage, card.x + 15, card.y + 86, 14, paper);
        // Explicitly wrapped, human-authored objective lines fit the HUD.
        const char* hint = sim.mission.kind == MissionKind::Courier ? "Reach gold markers while driving."
                           : sim.mission.kind == MissionKind::Acquisition
                               ? (sim.mission.stage == 0 ? "E: enter the marked pink sports car."
                                                         : "Deliver target car after losing police.")
                           : sim.mission.stage == 0 ? "Hold at the gold marker for 3 seconds."
                                                    : "Return to the loft after losing police.";
        text(hint, card.x + 15, card.y + 116, 13, muted);
        text("PAYOUT " + cash(sim.mission.reward) + "  /  CHAIN " + std::to_string(sim.chain), card.x + 15,
             card.y + 139, 12, gold);
    } else {
        box({w - 334, 24, 310, 92}, panel);
        text(view.waypointSet ? "WAYPOINT ACTIVE / FOLLOW CYAN ROUTE" : "FREE ROAM / FIND A CONTACT", w - 318,
             39, 15, cyan);
        text("Cyan diamonds offer paid contracts.", w - 318, 66, 13, muted);
        text("F: interact   M: city map   ESC: pause", w - 318, 88, 12, paper);
    }
    drawMap(sim, view, false);
    Rect weapon{w - 234, h - 109, 210, 85};
    box(weapon, panel);
    text("SIDEARM / 9MM", weapon.x + 14, weapon.y + 12, 13, muted);
    text(sim.player.reloadTimer > 0
             ? "RELOADING"
             : std::to_string(sim.player.ammo) + " / " + std::to_string(sim.player.reserve),
         weapon.x + 14, weapon.y + 34, 25, paper);
    text("LMB fire  /  R reload", weapon.x + 14, weapon.y + 65, 12, muted);
    if (const auto* car = sim.vehicle(sim.player.vehicle)) {
        float speed = length(car->velocity) * 0.42f;
        box({w - 234, h - 204, 210, 83}, panel);
        text("DRIVE / " + std::string(car->kind == VehicleKind::Sport    ? "APEX GT"
                                      : car->kind == VehicleKind::Police ? "PATROL"
                                      : car->kind == VehicleKind::Van    ? "CARGO"
                                                                         : "SEDAN"),
             w - 220, h - 191, 13, cyan);
        text(std::to_string(static_cast<int>(speed)) + " KM/H", w - 220, h - 169, 24, paper);
        bar(w - 220, h - 133, 182, car->health, gold);
    }
    float y = h - 64;
    for (auto it = toasts_.rbegin(); it != toasts_.rend(); ++it) {
        float size = 14;
        float tw = MeasureTextEx(GetFontDefault(), it->text.c_str(), size, 1).x;
        tw = std::min(tw, w - 510);
        Rect toast{(w - tw) / 2 - 13, y - 6, tw + 26, 30};
        box(toast, alpha(panel, std::min(it->life, 1.0f)));
        BeginScissorMode(static_cast<int>(toast.x + 6), static_cast<int>(toast.y),
                         static_cast<int>(toast.w - 12), 30);
        text(it->text, toast.x + 13, y, size, alpha(it->reward ? gold : paper, std::min(it->life, 1.0f)));
        EndScissorMode();
        y -= 37;
    }
    if (view.showHelp) {
        box({w / 2 - 231, h - 154, 462, 68}, panel);
        text("WASD move / drive   SHIFT sprint   SPACE handbrake", w / 2 - 216, h - 140, 13, paper);
        text("E enter / exit   F interact   LMB shoot   F1 help", w / 2 - 216, h - 116, 13, muted);
    }
    if (sim.garageOpen) {
        Rect r{w / 2 - 230, h / 2 - 132, 460, 264};
        box(r, {12, 26, 32, 248});
        outline(r, cyan, 2);
        text("PITSTOP / VEHICLE SERVICES", r.x + 24, r.y + 22, 23, cyan);
        text("Press a number to purchase. F to close.", r.x + 24, r.y + 57, 14, muted);
        text("1   FULL VEHICLE REPAIR            $150", r.x + 24, r.y + 96, 17, paper);
        text("2   BODY ARMOR                     $250", r.x + 24, r.y + 132, 17, paper);
        text("3   ENGINE " + std::to_string(sim.progress.engineLevel) + " / 3               " +
                 (sim.progress.engineLevel >= maxEngineLevel ? "MAXED"
                                                             : cash(650 + sim.progress.engineLevel * 350)),
             r.x + 24, r.y + 168, 17, paper);
        text("4   64 ROUNDS                      $120", r.x + 24, r.y + 204, 17, paper);
        text("Tuning applies to every vehicle you drive.", r.x + 24, r.y + 239, 12, gold);
    }
    if (view.map)
        drawMap(sim, view, true);
    if (!view.map && !sim.garageOpen) {
        Vector2 mouse = GetMousePosition();
        Color c = sim.player.reloadTimer > 0 ? muted : paper;
        DrawCircleLines(static_cast<int>(mouse.x), static_cast<int>(mouse.y), 7, alpha(c, 0.8f));
        DrawLine(static_cast<int>(mouse.x) - 12, static_cast<int>(mouse.y), static_cast<int>(mouse.x) - 9,
                 static_cast<int>(mouse.y), c);
        DrawLine(static_cast<int>(mouse.x) + 9, static_cast<int>(mouse.y), static_cast<int>(mouse.x) + 12,
                 static_cast<int>(mouse.y), c);
    }
}
void Presentation::drawMenu(const Simulation& sim, const ViewState& view) {
    float w = static_cast<float>(GetScreenWidth()), h = static_cast<float>(GetScreenHeight());
    box({0, 0, w, h}, {5, 11, 21, 201});
    if (!toasts_.empty()) {
        box({24, 24, w - 48, 36}, panel);
        BeginScissorMode(36, 24, static_cast<int>(w - 72), 36);
        text(toasts_.back().text, 36, 34, 14, paper);
        EndScissorMode();
    }
    for (int i = 0; i < 10; ++i)
        line({w * 0.52f + static_cast<float>(i * 70), 0}, {w * 0.3f + static_cast<float>(i * 70), h}, 1,
             {36, 80, 90, 25});
    if (view.screen == Screen::Settings) {
        float x = w / 2 - 220, y = h * 0.36f;
        text("SIGNAL / SETTINGS", x, y - 84, 36, cyan);
        button({x, y, 440, 46}, std::string("R / RAIN                  ") + (view.rain ? "ON" : "OFF"));
        button({x, y + 58, 440, 46},
               std::string("V / POST EFFECTS          ") + (view.effects ? "ON" : "OFF"));
        button({x, y + 116, 440, 46},
               std::string("N / AUDIO                 ") + (view.audio ? "ON" : "OFF"));
        button({x, y + 174, 440, 46},
               "VOLUME   <  " + std::to_string(static_cast<int>(view.volume * 100)) + "%  >");
        button({x, y + 232, 440, 46}, "BACK / ESC");
        text("F11 / fullscreen     +/- / volume", x, y + 296, 14, muted);
        return;
    }
    float y = h * 0.55f;
    text("AN ORIGINAL TOP-DOWN URBAN SANDBOX", 70, 94, 16, cyan);
    text("NEON", 65, 134, 96, paper);
    text("BOROUGH", 65, 222, 78, paper);
    box({70, 316, 130, 4}, pink);
    text(view.screen == Screen::Paused ? "SIGNAL PAUSED" : "THE STREETS DON'T SLEEP.", 70, 344, 20, muted);
    button({70, y, 300, 46}, view.screen == Screen::Paused ? "RESUME / ENTER" : "ENTER THE BOROUGH / ENTER",
           view.menuSelection == 0);
    button({70, y + 60, 300, 46}, "SETTINGS", view.menuSelection == 1);
    button({70, y + 120, 300, 46}, view.screen == Screen::Paused ? "RETURN TO TITLE" : "QUIT",
           view.menuSelection == 2);
    text("UP / DOWN / TAB select   ENTER confirm   Q quit / title", 70, y + 178, 12, muted);
    float x = std::max(600.0f, w - 510);
    box({x, 156, 420, 355}, {14, 27, 39, 230});
    box({x, 156, 3, 355}, cyan);
    text("CITY SYSTEMS / ONLINE", x + 24, 180, 22, cyan);
    text("01 / DRIVE", x + 24, 232, 17, gold);
    text("Grip, drift, traffic, damage, garages.", x + 24, 258, 14, muted);
    text("02 / DISAPPEAR", x + 24, 305, 17, pink);
    text("Five wanted levels. Occlusion-based escape.", x + 24, 331, 14, muted);
    text("03 / GET PAID", x + 24, 378, 17, cyan);
    text("Courier, acquisition, informant extraction.", x + 24, 404, 14, muted);
    text("COMPLETED " + std::to_string(sim.progress.completed) + "  /  BEST CHAIN " +
             std::to_string(sim.progress.bestChain),
         x + 24, 464, 14, paper);
    text("C++20 / RAYLIB 5.5 / PROCEDURAL EVERYTHING", 70, h - 56, 12, muted);
    text("Original code and artwork. Not affiliated with Rockstar Games.", 70, h - 34, 12, muted);
}
void Presentation::render(const Simulation& sim, const ViewState& view, float dt) {
    resize();
    dt = std::isfinite(dt) ? std::clamp(dt, 0.0f, 0.1f) : 0.0f;
    clock_ = std::fmod(clock_ + dt, 3600.0f);
    float effectDt = view.screen == Screen::Playing && !view.map ? dt : 0.0f;
    for (auto& p : particles_) {
        p.life -= effectDt;
        p.position += p.velocity * effectDt;
        p.velocity *= std::exp(-3 * effectDt);
    }
    std::erase_if(particles_, [](const Particle& p) { return p.life <= 0; });
    for (auto& mark : skids_)
        mark.life -= effectDt;
    std::erase_if(skids_, [](const SkidMark& mark) { return mark.life <= 0; });
    for (auto& t : toasts_)
        t.life -= effectDt;
    std::erase_if(toasts_, [](const Toast& t) { return t.life <= 0; });
    float w = static_cast<float>(GetScreenWidth()), h = static_cast<float>(GetScreenHeight());
    Vec2 target = sim.playerPosition();
    float speed = 0;
    if (const auto* v = sim.vehicle(sim.player.vehicle)) {
        target += v->velocity * 0.42f;
        speed = length(v->velocity);
    } else
        target += sim.player.aim * 30;
    if (view.screen == Screen::Title)
        target = {650 + 120 * std::sin(clock_ * 0.12f), 640 + 95 * std::cos(clock_ * 0.11f)};
    camera_.target = rv(lerp({camera_.target.x, camera_.target.y}, target, 1 - std::exp(-5 * dt)));
    zoom_ = lerp(zoom_, speed > 160 ? 0.88f : 1.08f, 1 - std::exp(-2 * dt));
    camera_.zoom = zoom_;
    float halfWidth = w / (2 * zoom_), halfHeight = h / (2 * zoom_);
    // Keep the camera inside the world, including the initial safehouse view.
    camera_.target.x = std::clamp(camera_.target.x, std::min(halfWidth, citySize / 2),
                                  std::max(citySize - halfWidth, citySize / 2));
    camera_.target.y = std::clamp(camera_.target.y, std::min(halfHeight, citySize / 2),
                                  std::max(citySize - halfHeight, citySize / 2));
    camera_.offset = {w / 2, h / 2};
    if (view.effects) {
        float shake = sim.trauma * sim.trauma * 9;
        camera_.offset.x += std::sin(clock_ * 63) * shake;
        camera_.offset.y += std::cos(clock_ * 71) * shake;
    }
    Rect visible{camera_.target.x - w / (2 * zoom_) - 100, camera_.target.y - h / (2 * zoom_) - 100,
                 w / zoom_ + 200, h / zoom_ + 200};
    BeginTextureMode(scene_);
    ClearBackground(ink);
    BeginMode2D(camera_);
    drawCity(sim, view, visible);
    for (const auto& mark : skids_)
        line(mark.a, mark.b, 3, alpha(ink, std::min(0.65f, mark.life / 5)));
    drawActors(sim, visible);
    EndMode2D();
    if (view.rain) {
        for (int i = 0; i < 130; ++i) {
            float x = std::fmod(static_cast<float>(i * 137) + clock_ * 85, w + 40) - 20;
            float y = std::fmod(static_cast<float>(i * 211) + clock_ * 420, h + 50) - 25;
            line({x, y}, {x - 4, y + 13}, 1, {141, 185, 204, 38});
        }
    }
    EndTextureMode();
    float resolution[]{w, h}, strength = view.effects ? 1.0f : 0.0f;
    SetShaderValue(post_, resolutionLocation_, resolution, SHADER_UNIFORM_VEC2);
    SetShaderValue(post_, timeLocation_, &clock_, SHADER_UNIFORM_FLOAT);
    SetShaderValue(post_, strengthLocation_, &strength, SHADER_UNIFORM_FLOAT);
    BeginDrawing();
    ClearBackground(ink);
    if (shaderReady_)
        BeginShaderMode(post_);
    DrawTexturePro(scene_.texture, {0, 0, w, -h}, {0, 0, w, h}, {0, 0}, 0, WHITE);
    if (shaderReady_)
        EndShaderMode();
    if (view.screen == Screen::Playing)
        drawHud(sim, view);
    else
        drawMenu(sim, view);
    EndDrawing();
}
} // namespace neon
