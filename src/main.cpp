#include "neon/presentation.hpp"
#include "neon/save.hpp"
#include <charconv>
#include <iostream>
#include <memory>
#include <string_view>

namespace {
struct Options {
    std::uint64_t seed{2025};
    int frames{240}, width{1280}, height{800};
    bool smoke{}, smokeAudio{}, smokeMute{}, noAudio{}, noSave{}, seedSet{}, help{};
    std::string smokeView{"gameplay"};
    std::filesystem::path screenshot{"neon-smoke.png"}, savePath = neon::defaultSavePath();
};
template <class T> bool number(std::string_view text, T& value) {
    auto [p, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
    return ec == std::errc{} && p == text.data() + text.size();
}
bool parse(int argc, char** argv, Options& o) {
    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        auto next = [&]() -> std::string_view {
            if (i + 1 < argc)
                return argv[++i];
            return {};
        };
        if (arg == "--smoke")
            o.smoke = true;
        else if (arg == "--smoke-audio" || arg == "--smoke-mute") {
            o.smoke = true;
            o.smokeAudio = true;
            o.smokeMute = arg == "--smoke-mute";
        } else if (arg == "--smoke-view") {
            auto v = next();
            if (v != "gameplay" && v != "title" && v != "map" && v != "garage" && v != "paused" &&
                v != "settings" && v != "waypoint" && v != "lifecycle")
                return false;
            o.smoke = true;
            o.smokeView = v;
        } else if (arg == "--no-audio")
            o.noAudio = true;
        else if (arg == "--no-save")
            o.noSave = true;
        else if (arg == "--help" || arg == "-h")
            o.help = true;
        else if (arg == "--frames") {
            if (!number(next(), o.frames) || o.frames < 1 || o.frames > 20000)
                return false;
        } else if (arg == "--size") {
            auto size = next();
            auto separator = size.find('x');
            if (separator == std::string_view::npos || !number(size.substr(0, separator), o.width) ||
                !number(size.substr(separator + 1), o.height) || o.width < 1100 || o.width > 7680 ||
                o.height < 700 || o.height > 4320)
                return false;
        } else if (arg == "--seed") {
            if (!number(next(), o.seed))
                return false;
            o.seedSet = true;
        } else if (arg == "--screenshot") {
            auto v = next();
            if (v.empty())
                return false;
            o.screenshot = v;
        } else if (arg == "--save-dir") {
            auto v = next();
            if (v.empty())
                return false;
            o.savePath = std::filesystem::path(v) / "progress.sav";
        } else {
            std::cerr << "Unknown argument: " << arg << '\n';
            return false;
        }
    }
    if (o.smoke) {
        o.noSave = true;
        if (!o.smokeAudio)
            o.noAudio = true;
    }
    return true;
}
bool clicked(float x, float y, float w, float h) {
    return IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
           CheckCollisionPointRec(GetMousePosition(), {x, y, w, h});
}
} // namespace
int run(int argc, char** argv) {
    Options options;
    if (!parse(argc, argv, options)) {
        std::cerr << "Invalid arguments. Run --help.\n";
        return 2;
    }
    if (options.help) {
        std::cout
            << "Neon Borough\n--seed N --no-audio --no-save --save-dir PATH --size WIDTHxHEIGHT\n"
               "--smoke --frames N --screenshot PATH (CI-only scripted renderer smoke test)\n"
               "--smoke-view gameplay|title|map|garage|paused|settings|waypoint|lifecycle\n"
               "--smoke-audio --smoke-mute\n";
        return 0;
    }
    std::optional<neon::Progress> saved;
    std::string loadError;
    if (!options.noSave) {
        saved = neon::recoverProgress(options.savePath, &loadError);
        if (saved) {
            if (options.seedSet)
                saved->seed = options.seed; // New city, same hard-earned economy.
            else
                options.seed = saved->seed;
        }
    }
    auto sim = std::make_unique<neon::Simulation>(options.seed);
    if (saved)
        sim->applyProgress(*saved);
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    SetTraceLogLevel(LOG_INFO);
    InitWindow(options.width, options.height, "NEON BOROUGH / the streets don't sleep");
    if (!IsWindowReady()) {
        std::cerr << "Cannot initialize graphics.\n";
        return 3;
    }
    SetWindowMinSize(1100, 700);
    SetExitKey(KEY_NULL);
    SetTargetFPS(options.smoke ? 60 : 120);
    int result = 0;
    {
        neon::Presentation presentation;
        if (!presentation.ready()) {
            std::cerr << "Graphics render target or post-processing shader unavailable.\n";
            result = 3;
        }
        std::unique_ptr<neon::Audio> audio;
        if (!options.noAudio)
            audio = std::make_unique<neon::Audio>();
        if (options.smokeAudio) {
            if (audio && audio->ready())
                std::cout << "AUDIO_READY\n";
            else {
                std::cerr << "Audio device initialization failed.\n";
                result = 5;
            }
        }
        neon::ViewState view;
        view.audio = audio && audio->ready() && !options.smokeMute;
        if (audio && !audio->ready())
            presentation.message("Audio unavailable. Continuing silently.");
        neon::Screen previous = neon::Screen::Title;
        bool quit = result != 0, suppressFire = true;
        float accumulator = 0, saveRetry = 0;
        int frame = 0, lastRevision = sim->saveRevision;
        neon::Input pending;
        if (!loadError.empty() && loadError != "No save found.")
            presentation.message(loadError);
        presentation.ingest(sim->events);
        if (options.smoke) {
            view.screen = neon::Screen::Playing;
            view.showHelp = false;
            sim->player.position = {580, 484};
            int id = sim->spawnVehicle({580, 484}, neon::VehicleKind::Sport, 0);
            sim->player.vehicle = id;
            sim->vehicle(id)->heading = 0;
            sim->vehicle(id)->color = 6;
            sim->startMission(0);
            if (options.smokeView == "title")
                view.screen = neon::Screen::Title;
            if (options.smokeView == "paused")
                view.screen = neon::Screen::Paused;
            if (options.smokeView == "settings")
                view.screen = neon::Screen::Settings;
            if (options.smokeView == "waypoint") {
                sim->mission.active = false;
                view.waypointSet = true;
                view.waypoint = sim->city.services[7].position;
                view.map = true;
            }
            if (options.smokeView == "map")
                view.map = true;
            if (options.smokeView == "garage") {
                sim->vehicle(id)->position = sim->city.services[7].position + neon::Vec2{0, -30};
                sim->garageOpen = true;
            }
        }
        auto save = [&] {
            if (options.noSave)
                return true;
            auto status = neon::saveProgress(options.savePath, sim->snapshot());
            if (!status.ok) {
                std::cerr << status.message << '\n';
                presentation.message(status.message);
            }
            return status.ok;
        };
        while (!quit && !WindowShouldClose()) {
            float dt = options.smoke ? neon::fixedStep : std::min(GetFrameTime(), 0.1f);
            float w = static_cast<float>(GetScreenWidth()), h = static_cast<float>(GetScreenHeight());
            bool wasRunning = view.screen == neon::Screen::Playing && !view.map;
            bool wasGarage = sim->garageOpen;
            float previousSimulationTime = sim->time;
            if (options.smokeView == "lifecycle") {
                if (frame == 0) view.screen = neon::Screen::Title;
                if (frame == 20) view.screen = neon::Screen::Settings;
                if (frame == 40 || frame == 120) {
                    view.screen = neon::Screen::Playing;
                    view.map = false;
                    sim->garageOpen = false;
                }
                if (frame == 60) {
                    auto* car = sim->vehicle(sim->player.vehicle);
                    car->position = sim->city.services[7].position + neon::Vec2{0, -30};
                    car->velocity = {};
                    sim->garageOpen = true;
                    SetWindowSize(1100, 700);
                    view.effects = false;
                }
                if (frame == 80) view.map = true;
                if (frame == 90) SetWindowSize(1600, 900);
                if (frame == 100) { view.map = false; view.screen = neon::Screen::Paused; }
                if (frame == 120) { SetWindowSize(1280, 800); view.effects = true; }
                if (frame % 20 == 0 && frame <= 120)
                    std::cout << "SMOKE_LIFECYCLE frame=" << frame << " simulation=" << sim->time << '\n';
            }
            if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT))
                suppressFire = false;
            if (!options.smoke && wasRunning && !IsWindowFocused())
                view.screen = neon::Screen::Paused;
            if (IsKeyPressed(KEY_F11)) {
                ToggleFullscreen();
                view.fullscreen = IsWindowFullscreen();
            }
            if (view.screen == neon::Screen::Settings) {
                float x = w / 2 - 220, y = h * 0.36f;
                if (IsKeyPressed(KEY_R) || clicked(x, y, 440, 46))
                    view.rain = !view.rain;
                if (IsKeyPressed(KEY_V) || clicked(x, y + 58, 440, 46))
                    view.effects = !view.effects;
                if (IsKeyPressed(KEY_N) || clicked(x, y + 116, 440, 46)) {
                    if (audio && audio->ready())
                        view.audio = !view.audio;
                    else
                        presentation.message("Audio disabled or unavailable on this device.");
                }
                if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_MINUS) || clicked(x, y + 174, 220, 46))
                    view.volume = std::max(0.0f, view.volume - 0.1f);
                if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_EQUAL) || clicked(x + 220, y + 174, 220, 46))
                    view.volume = std::min(1.0f, view.volume + 0.1f);
                if (IsKeyPressed(KEY_ESCAPE) || clicked(x, y + 232, 440, 46))
                    view.screen = previous;
            } else if (view.screen != neon::Screen::Playing) {
                float y = h * 0.55f;
                if (IsKeyPressed(KEY_UP)) view.menuSelection = (view.menuSelection + 2) % 3;
                if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_TAB)) view.menuSelection = (view.menuSelection + 1) % 3;
                bool activate = IsKeyPressed(KEY_ENTER);
                if ((activate && view.menuSelection == 0) ||
                    (view.screen == neon::Screen::Paused && IsKeyPressed(KEY_ESCAPE)) ||
                    clicked(70, y, 300, 46)) {
                    view.screen = neon::Screen::Playing;
                    view.menuSelection = 0;
                } else if ((activate && view.menuSelection == 1) || clicked(70, y + 60, 300, 46)) {
                    previous = view.screen;
                    view.screen = neon::Screen::Settings;
                } else if ((activate && view.menuSelection == 2) || IsKeyPressed(KEY_Q) ||
                           clicked(70, y + 120, 300, 46)) {
                    if (view.screen == neon::Screen::Title)
                        quit = true;
                    else {
                        if (save()) {
                            view.screen = neon::Screen::Title;
                            view.menuSelection = 0;
                        }
                    }
                }
            } else {
                if (IsKeyPressed(KEY_ESCAPE)) {
                    if (view.map)
                        view.map = false;
                    else if (sim->garageOpen)
                        sim->garageOpen = false;
                    else
                        view.screen = neon::Screen::Paused;
                }
                if (IsKeyPressed(KEY_M))
                    view.map = !view.map;
                if (view.map) {
                    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
                        view.waypointSet = false;
                        presentation.message("Waypoint cleared.");
                    }
                    neon::Rect bounds = neon::mapBounds(w, h, true);
                    Vector2 mouse = GetMousePosition();
                    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && bounds.contains({mouse.x, mouse.y})) {
                        neon::Vec2 position{(mouse.x - bounds.x) * neon::citySize / bounds.w,
                                            (mouse.y - bounds.y) * neon::citySize / bounds.h};
                        view.waypoint = sim->city.node(sim->city.nearestNode(position));
                        std::string label = "Road waypoint set.";
                        for (const auto& service : sim->city.services)
                            if (neon::distance(position, service.position) < 24 * neon::citySize / bounds.w) {
                                view.waypoint = service.position;
                                label = "Route set: " + service.name;
                                break;
                            }
                        view.waypointSet = true;
                        presentation.message(
                            sim->mission.active ? "Waypoint stored. Contract route takes priority." : label);
                    }
                }
                if (IsKeyPressed(KEY_F1))
                    view.showHelp = !view.showHelp;
            }
            bool running = view.screen == neon::Screen::Playing && !view.map;
            bool transition = wasRunning != running || wasGarage != sim->garageOpen;
            if (transition)
                suppressFire = true;
            saveRetry = std::max(0.0f, saveRetry - dt);
            if (running && wasRunning && !transition) {
                pending.movement = {static_cast<float>(IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) -
                                        static_cast<float>(IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)),
                                    static_cast<float>(IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) -
                                        static_cast<float>(IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))};
                pending.throttle = -pending.movement.y;
                pending.steering = pending.movement.x;
                pending.aim = presentation.worldMouse();
                pending.sprint = IsKeyDown(KEY_LEFT_SHIFT);
                pending.handbrake = IsKeyDown(KEY_SPACE);
                pending.shoot = IsMouseButtonDown(MOUSE_BUTTON_LEFT) && !suppressFire && !sim->garageOpen;
                pending.enter |= IsKeyPressed(KEY_E);
                pending.interact |= IsKeyPressed(KEY_F);
                pending.reload |= IsKeyPressed(KEY_R);
                pending.honk |= IsKeyPressed(KEY_H);
                for (int i = 0; i < 4; ++i)
                    if (IsKeyPressed(KEY_ONE + i))
                        pending.shopChoice = i + 1;
                if (options.smoke) {
                    pending = {};
                    pending.throttle = frame < 150 ? 0.8f : 0.25f;
                    pending.steering = frame > 160 ? 0.2f : 0;
                    pending.aim = sim->playerPosition() + neon::Vec2{200, -30};
                    pending.shoot = frame == 80 || frame == 100;
                    if (sim->garageOpen)
                        pending = {};
                }
                accumulator += dt;
                for (int steps = 0; accumulator >= neon::fixedStep && steps < 6; ++steps) {
                    sim->tick(pending);
                    presentation.ingest(sim->events);
                    if (audio && view.audio)
                        audio->ingest(sim->events);
                    pending.enter = pending.interact = pending.reload = pending.honk = false;
                    pending.shopChoice = 0;
                    accumulator -= neon::fixedStep;
                }
                if (sim->saveRevision != lastRevision && saveRetry == 0) {
                    if (save())
                        lastRevision = sim->saveRevision;
                    else
                        saveRetry = 5;
                }
            } else {
                accumulator = 0;
                pending = {};
            }
            if (options.smoke && !running && sim->time != previousSimulationTime) {
                std::cerr << "Paused smoke scene advanced simulation.\n";
                result = 6;
                quit = true;
            }
            if (view.waypointSet && !sim->mission.active &&
                neon::distance(sim->playerPosition(), view.waypoint) < 60) {
                view.waypointSet = false;
                presentation.message("Waypoint reached.");
            }
            if (audio)
                audio->update(*sim, view, dt);
            if (view.screen == neon::Screen::Playing && !view.map && !sim->garageOpen)
                HideCursor();
            else
                ShowCursor();
            presentation.render(*sim, view, dt);
            if (!presentation.ready()) {
                std::cerr << "Render target unavailable after resize.\n";
                result = 3;
                quit = true;
            }
            ++frame;
            if (options.smoke && frame >= options.frames) {
                auto parent = options.screenshot.parent_path();
                std::error_code ec;
                if (!parent.empty())
                    std::filesystem::create_directories(parent, ec);
                if (ec) {
                    std::cerr << "Cannot create screenshot directory: " << ec.message() << '\n';
                    result = 4;
                }
                Image capture = LoadImageFromScreen();
                bool exported =
                    !ec && IsImageValid(capture) && ExportImage(capture, options.screenshot.string().c_str());
                UnloadImage(capture);
                std::cout << "SMOKE frames=" << frame << " digest=" << sim->digest()
                          << " vehicles=" << sim->vehicles.size()
                          << " screenshot=" << options.screenshot.string() << '\n';
                if (!exported || !std::filesystem::exists(options.screenshot, ec) || ec ||
                    !neon::finite(sim->playerPosition()))
                    result = 4;
                quit = true;
            }
        }
        save();
        ShowCursor();
    }
    CloseWindow();
    return result;
}
int main(int argc, char** argv) {
    try {
        return run(argc, argv);
    } catch (const std::exception& error) {
        std::cerr << "Neon Borough failed: " << error.what() << '\n';
        if (IsWindowReady())
            CloseWindow();
        return 1;
    }
}
