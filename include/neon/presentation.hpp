#pragma once
#include "neon/simulation.hpp"
#include "raylib.h"
#include <array>
#include <deque>

namespace neon {
enum class Screen { Title, Playing, Paused, Settings };
struct ViewState {
    Screen screen{Screen::Title};
    bool map{}, rain{true}, effects{true}, audio{true}, fullscreen{}, showHelp{true};
    float volume{0.65f};
};
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
    void resize();
    void drawCity(const Simulation& sim, Rect visible);
    void drawActors(const Simulation& sim, Rect visible);
    void drawHud(const Simulation& sim, const ViewState& view);
    void drawMap(const Simulation& sim, bool full);
    void drawMenu(const Simulation& sim, const ViewState& view);

  public:
    Presentation();
    ~Presentation();
    Presentation(const Presentation&) = delete;
    Presentation& operator=(const Presentation&) = delete;
    Vec2 worldMouse() const;
    void ingest(const std::vector<Event>& events);
    void message(const std::string& text, bool reward = false);
    void render(const Simulation& sim, const ViewState& view, float dt);
};
class Audio {
    bool ready_{};
    std::array<Sound, 6> sounds_{};
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
    void ingest(const std::vector<Event>& events, float volume);
    void update(const Simulation& sim, const ViewState& view, float dt);
};
} // namespace neon
