#include "neon/presentation.hpp"
#include "neon/save.hpp"
#include <charconv>
#include <iostream>
#include <memory>
#include <string_view>

namespace {
struct Options {
    std::uint64_t seed{2025};int frames{240};bool smoke{},noAudio{},noSave{},seedSet{},help{};
    std::filesystem::path screenshot{"neon-smoke.png"},savePath=neon::defaultSavePath();
};
template<class T> bool number(std::string_view text,T& value) {
    auto [p,ec]=std::from_chars(text.data(),text.data()+text.size(),value);return ec==std::errc{} && p==text.data()+text.size();
}
bool parse(int argc,char** argv,Options& o) {
    for(int i=1;i<argc;++i) {
        std::string_view arg=argv[i];
        auto next=[&]()->std::string_view{if(i+1<argc)return argv[++i];return {};};
        if(arg=="--smoke") o.smoke=true;
        else if(arg=="--no-audio") o.noAudio=true;
        else if(arg=="--no-save") o.noSave=true;
        else if(arg=="--help" || arg=="-h") o.help=true;
        else if(arg=="--frames") {if(!number(next(),o.frames) || o.frames<1 || o.frames>20000)return false;}
        else if(arg=="--seed") {if(!number(next(),o.seed))return false;o.seedSet=true;}
        else if(arg=="--screenshot") {auto v=next();if(v.empty())return false;o.screenshot=v;}
        else if(arg=="--save-dir") {auto v=next();if(v.empty())return false;o.savePath=std::filesystem::path(v)/"progress.sav";}
        else {std::cerr<<"Unknown argument: "<<arg<<'\n';return false;}
    }
    if(o.smoke) {o.noSave=true;o.noAudio=true;}
    return true;
}
bool clicked(float x,float y,float w,float h) {return IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(),{x,y,w,h});}
}
int main(int argc,char** argv) {
    Options options;
    if(!parse(argc,argv,options)) {std::cerr<<"Invalid arguments. Run --help.\n";return 2;}
    if(options.help) {
        std::cout<<"Neon Borough\n--seed N --no-audio --no-save --save-dir PATH\n"
                    "--smoke --frames N --screenshot PATH (CI-only scripted renderer smoke test)\n";return 0;
    }
    std::optional<neon::Progress> saved;std::string loadError;
    if(!options.noSave) {
        saved=neon::loadProgress(options.savePath,&loadError);
        if(!saved && std::filesystem::exists(options.savePath)) {
            auto backup=options.savePath;backup += ".bak";saved=neon::loadProgress(backup);
            if(saved) loadError="Recovered the previous save backup.";
        }
        if(saved && (!options.seedSet || saved->seed==options.seed)) options.seed=saved->seed;else saved.reset();
    }
    auto sim=std::make_unique<neon::Simulation>(options.seed);
    if(saved) sim->applyProgress(*saved);
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    SetTraceLogLevel(LOG_INFO);InitWindow(1280,800,"NEON BOROUGH / the streets don't sleep");
    if(!IsWindowReady()) {std::cerr<<"Cannot initialize graphics.\n";return 3;}
    SetWindowMinSize(1100,700);SetExitKey(KEY_NULL);SetTargetFPS(options.smoke?60:120);
    int result=0;
    {
        neon::Presentation presentation;
        std::unique_ptr<neon::Audio> audio;
        if(!options.noAudio) audio=std::make_unique<neon::Audio>();
        neon::ViewState view;view.audio=!options.noAudio;
        neon::Screen previous=neon::Screen::Title;
        bool quit=false;float accumulator=0;int frame=0,lastRevision=sim->saveRevision;
        neon::Input pending;
        if(!loadError.empty() && loadError!="No save found.") presentation.message(loadError);
        presentation.ingest(sim->events);
        if(options.smoke) {
            view.screen=neon::Screen::Playing;view.showHelp=false;
            sim->player.position={580,100};
            int id=sim->spawnVehicle({580,100},neon::VehicleKind::Sport,0);
            sim->player.vehicle=id;sim->vehicle(id)->heading=0;sim->startMission(0);
        }
        auto save=[&] {
            if(options.noSave) return;
            auto status=neon::saveProgress(options.savePath,sim->snapshot());
            if(!status.ok) {std::cerr<<status.message<<'\n';presentation.message(status.message);}
        };
        while(!quit && !WindowShouldClose()) {
            float dt=options.smoke?neon::fixedStep:std::min(GetFrameTime(),0.1f);
            float w=static_cast<float>(GetScreenWidth()),h=static_cast<float>(GetScreenHeight());
            if(IsKeyPressed(KEY_F11)) {ToggleFullscreen();view.fullscreen=IsWindowFullscreen();}
            if(view.screen==neon::Screen::Settings) {
                float x=w/2-220,y=h*0.36f;
                if(IsKeyPressed(KEY_R) || clicked(x,y,440,46)) view.rain=!view.rain;
                if(IsKeyPressed(KEY_V) || clicked(x,y+58,440,46)) view.effects=!view.effects;
                if(IsKeyPressed(KEY_N) || clicked(x,y+116,440,46)) view.audio=!view.audio;
                if(IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_MINUS) || clicked(x,y+174,220,46)) view.volume=std::max(0.0f,view.volume-0.1f);
                if(IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_EQUAL) || clicked(x+220,y+174,220,46)) view.volume=std::min(1.0f,view.volume+0.1f);
                if(IsKeyPressed(KEY_ESCAPE) || clicked(x,y+232,440,46)) view.screen=previous;
            } else if(view.screen!=neon::Screen::Playing) {
                float y=h*0.55f;
                if(IsKeyPressed(KEY_ENTER) || (view.screen==neon::Screen::Paused && IsKeyPressed(KEY_ESCAPE)) || clicked(70,y,300,46)) view.screen=neon::Screen::Playing;
                else if(clicked(70,y+60,300,46)) {previous=view.screen;view.screen=neon::Screen::Settings;}
                else if(clicked(70,y+120,300,46)) {if(view.screen==neon::Screen::Title)quit=true;else {save();view.screen=neon::Screen::Title;}}
            } else {
                if(IsKeyPressed(KEY_ESCAPE)) {if(view.map)view.map=false;else if(sim->garageOpen)sim->garageOpen=false;else view.screen=neon::Screen::Paused;}
                if(IsKeyPressed(KEY_M)) view.map=!view.map;
                if(IsKeyPressed(KEY_F1)) view.showHelp=!view.showHelp;
            }
            bool running=view.screen==neon::Screen::Playing && !view.map;
            if(running) {
                pending.movement={static_cast<float>(IsKeyDown(KEY_D)||IsKeyDown(KEY_RIGHT))-static_cast<float>(IsKeyDown(KEY_A)||IsKeyDown(KEY_LEFT)),
                                  static_cast<float>(IsKeyDown(KEY_S)||IsKeyDown(KEY_DOWN))-static_cast<float>(IsKeyDown(KEY_W)||IsKeyDown(KEY_UP))};
                pending.throttle=-pending.movement.y;pending.steering=pending.movement.x;
                pending.aim=presentation.worldMouse();pending.sprint=IsKeyDown(KEY_LEFT_SHIFT);pending.handbrake=IsKeyDown(KEY_SPACE);
                pending.shoot=IsMouseButtonDown(MOUSE_BUTTON_LEFT) && !sim->garageOpen;
                pending.enter|=IsKeyPressed(KEY_E);pending.interact|=IsKeyPressed(KEY_F);pending.reload|=IsKeyPressed(KEY_R);pending.honk|=IsKeyPressed(KEY_H);
                for(int i=0;i<4;++i) if(IsKeyPressed(KEY_ONE+i)) pending.shopChoice=i+1;
                if(options.smoke) {
                    pending={};pending.throttle=frame<150?0.8f:0.25f;pending.steering=frame>160?0.2f:0;
                    pending.aim=sim->playerPosition()+neon::Vec2{200,-30};pending.shoot=frame==80 || frame==100;
                }
                accumulator+=dt;
                for(int steps=0;accumulator>=neon::fixedStep && steps<6;++steps) {
                    sim->tick(pending);presentation.ingest(sim->events);
                    if(audio && view.audio) audio->ingest(sim->events,view.volume);
                    pending.enter=pending.interact=pending.reload=pending.honk=false;pending.shopChoice=0;accumulator-=neon::fixedStep;
                }
                if(sim->saveRevision!=lastRevision) {save();lastRevision=sim->saveRevision;}
            } else {accumulator=0;pending={};}
            if(audio) audio->update(*sim,view,dt);
            if(view.screen==neon::Screen::Playing && !view.map) HideCursor();else ShowCursor();
            presentation.render(*sim,view,dt);
            ++frame;
            if(options.smoke && frame>=options.frames) {
                auto parent=options.screenshot.parent_path();std::error_code ec;
                if(!parent.empty()) std::filesystem::create_directories(parent,ec);
                TakeScreenshot(options.screenshot.string().c_str());
                std::cout<<"SMOKE frames="<<frame<<" digest="<<sim->digest()<<" vehicles="<<sim->vehicles.size()<<" screenshot="<<options.screenshot.string()<<'\n';
                if(!std::filesystem::exists(options.screenshot) || !std::isfinite(sim->playerPosition().x) || !std::isfinite(sim->playerPosition().y)) result=4;
                quit=true;
            }
        }
        save();ShowCursor();
    }
    CloseWindow();return result;
}
