#pragma once
#include "neon/simulation.hpp"
#include "raylib.h"
#include <array>
#include <deque>
#include <optional>

namespace neon {
enum class Screen { Title, Playing, Paused, Settings };
struct ViewState {
    Screen screen{Screen::Title};
    bool map{}, rain{true}, effects{true}, audio{true}, fullscreen{}, showHelp{true}, waypointSet{};
    Vec2 waypoint{};
    float volume{0.65f};
};
Rect mapBounds(float width, float height, bool full);
std::optional<Vec2> navigationTarget(const Simulation& sim, const ViewState& view);
struct Particle {
    Vec2 position{}, velocity{};
    float life{}, maximum{}, size{};
    Color color{};
};
struct Toast {
    std::string text;
    float life{};
    bool reward{};
};
struct SkidMark {
    Vec2 a{}, b{};
    float life{10};
};
class Presentation {
    Camera2D camera_{};
    RenderTexture2D scene_{};
    Shader post_{};
    int resolutionLocation_{-1}, timeLocation_{-1}, strengthLocation_{-1};
    Rng rng_{47};
    std::vector<Particle> particles_;
    std::deque<Toast> toasts_;
    std::deque<SkidMark> skids_;
    float clock_{}, zoom_{1};
    bool shaderReady_{};
    void resize();
    void drawCity(const Simulation& sim, const ViewState& view, Rect visible);
    void drawActors(const Simulation& sim, Rect visible);
    void drawHud(const Simulation& sim, const ViewState& view);
    void drawMap(const Simulation& sim, const ViewState& view, bool full);
    void drawMenu(const Simulation& sim, const ViewState& view);

  public:
    Presentation();
    ~Presentation();
    Presentation(const Presentation&) = delete;
    Presentation& operator=(const Presentation&) = delete;
    bool ready() const { return scene_.id != 0 && shaderReady_; }
    Vec2 worldMouse() const;
    void ingest(const std::vector<Event>& events);
    void message(const std::string& text, bool reward = false);
    void render(const Simulation& sim, const ViewState& view, float dt);
};
class Audio {
    bool ready_{}, deviceReady_{};
    std::array<Sound, 5> sounds_{};
    AudioStream music_{};
    double musicClock_{};
    float enginePhase_{}, lastShot_{};
    std::array<float, 1024> samples_{};
    Sound makeSound(int kind);

  public:
    Audio();
    ~Audio();
    Audio(const Audio&) = delete;
    Audio& operator=(const Audio&) = delete;
    bool ready() const { return ready_; }
    void ingest(const std::vector<Event>& events);
    void update(const Simulation& sim, const ViewState& view, float dt);
};
} // namespace neon
