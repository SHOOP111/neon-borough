#include "neon/presentation.hpp"
#include <cstdint>
#include <vector>

namespace neon {
Sound Audio::makeSound(int kind) {
    constexpr int rate = 22050;
    float duration = kind == 2 ? 0.8f : kind == 3 ? 0.6f : kind == 4 ? 0.28f : 0.14f;
    unsigned int count = static_cast<unsigned int>(duration * rate);
    std::vector<std::int16_t> data(count);
    Rng noise(static_cast<std::uint64_t>(kind) + 300);
    for (unsigned int i = 0; i < count; ++i) {
        float t = static_cast<float>(i) / rate, env = std::max(0.0f, 1.0f - t / duration), value = 0;
        if (kind == 0)
            value = (noise.range(-1, 1) * 0.75f + std::sin(t * 2 * pi * 160) * 0.25f) * std::exp(-t * 42);
        if (kind == 1)
            value = (noise.range(-1, 1) * 0.5f + std::sin(t * 2 * pi * 85) * 0.5f) * env * env;
        if (kind == 2)
            value = (noise.range(-1, 1) * 0.65f + std::sin(t * 2 * pi * (60 - t * 35)) * 0.35f) * env * env;
        if (kind == 3) {
            float f = t < 0.2f ? 523.25f : t < 0.4f ? 659.25f : 783.99f;
            value = std::sin(t * 2 * pi * f) * env * 0.5f;
        }
        if (kind == 4)
            value = (std::sin(t * 2 * pi * 220) + std::sin(t * 2 * pi * 277.18f)) * 0.25f * env;
        data[i] = static_cast<std::int16_t>(std::clamp(value, -1.0f, 1.0f) * 22000);
    }
    Wave wave{};
    wave.frameCount = count;
    wave.sampleRate = rate;
    wave.sampleSize = 16;
    wave.channels = 1;
    wave.data = data.data();
    return LoadSoundFromWave(wave);
}
Audio::Audio() {
    InitAudioDevice();
    deviceReady_ = IsAudioDeviceReady();
    if (!deviceReady_)
        return;
    for (int i = 0; i < 5; ++i)
        sounds_[static_cast<std::size_t>(i)] = makeSound(i);
    SetAudioStreamBufferSizeDefault(1024);
    music_ = LoadAudioStream(22050, 32, 1);
    ready_ = IsAudioStreamValid(music_) && std::all_of(sounds_.begin(), sounds_.end(), [](Sound sound) {
        return IsSoundValid(sound);
    });
    if (ready_) PlayAudioStream(music_);
}
Audio::~Audio() {
    if (!deviceReady_)
        return;
    if (IsAudioStreamValid(music_)) {
        StopAudioStream(music_);
        UnloadAudioStream(music_);
    }
    for (Sound sound : sounds_)
        if (IsSoundValid(sound)) UnloadSound(sound);
    CloseAudioDevice();
}
void Audio::ingest(const std::vector<Event>& events) {
    if (!ready_)
        return;
    for (const auto& event : events) {
        int index = event.kind == EventKind::Shot        ? 0
                    : event.kind == EventKind::Impact    ? 1
                    : event.kind == EventKind::Explosion ? 2
                    : event.kind == EventKind::Reward    ? 3
                    : event.kind == EventKind::Siren     ? 4
                                                         : -1;
        if (index < 0 || (index == 1 && event.value < 20))
            continue;
        if (index == 0 && lastShot_ > 0)
            continue;
        float level = index == 2 ? 0.65f : index == 1 ? 0.35f : 0.5f;
        SetSoundVolume(sounds_[static_cast<std::size_t>(index)], level);
        PlaySound(sounds_[static_cast<std::size_t>(index)]);
        if (index == 0)
            lastShot_ = 0.06f;
    }
}
void Audio::update(const Simulation& sim, const ViewState& view, float dt) {
    if (!ready_)
        return;
    lastShot_ = std::max(0.0f, lastShot_ - dt);
    SetMasterVolume(view.audio ? std::clamp(view.volume, 0.0f, 1.0f) : 0);
    SetAudioStreamVolume(music_, 0.46f);
    float speed = 0;
    if (const auto* car = sim.vehicle(sim.player.vehicle))
        speed = length(car->velocity);
    bool siren = false;
    for (const auto& car : sim.vehicles)
        if (car.active && car.driver == 1 && car.health > 0 &&
            distance(car.position, sim.playerPosition()) < 500 && sim.wanted() > 0)
            siren = true;
    // Main-thread synthesis avoids callback lifetime races. Two 1024-frame buffers.
    for (int buffer = 0; buffer < 2 && IsAudioStreamProcessed(music_); ++buffer) {
        constexpr double rate = 22050, beat = 60.0 / 100.0;
        constexpr double bass[]{65.406, 65.406, 77.782, 58.270, 65.406, 87.307, 77.782, 58.270};
        for (float& sample : samples_) {
            double t = musicClock_, phase = std::fmod(t, beat), step = std::fmod(t, beat / 2);
            int note = static_cast<int>(std::fmod(t / beat, 8.0));
            double f = bass[note];
            double pad = std::sin(t * 2 * pi * f * 4) * 0.045 + std::sin(t * 2 * pi * f * 6) * 0.025;
            double synth = (std::sin(t * 2 * pi * f) + std::sin(t * 2 * pi * f * 2) * 0.28) *
                           std::exp(-phase * 7) * 0.18;
            double kick = std::sin(2 * pi * (48 * phase + 8 * (1 - std::exp(-phase * 30)))) *
                          std::exp(-phase * 23) * 0.24;
            double hat =
                std::sin(t * 2 * pi * 8731) * std::sin(t * 2 * pi * 6203) * std::exp(-step * 70) * 0.06;
            enginePhase_ =
                std::fmod(enginePhase_ + 2 * pi * (38 + speed * 0.32f) / static_cast<float>(rate), 2 * pi);
            double engine = sim.player.vehicle == -1 || view.screen != Screen::Playing || view.map
                                ? 0
                                : (std::sin(enginePhase_) + 0.3 * std::sin(enginePhase_ * 2)) * 0.12;
            double police = siren && view.screen == Screen::Playing && !view.map
                                ? std::sin(t * 2 * pi * (680 + 130 * std::sin(t * 2.8))) * 0.055 : 0;
            sample = static_cast<float>(std::clamp(pad + synth + kick + hat + engine + police, -0.8, 0.8));
            musicClock_ += 1 / rate;
        }
        UpdateAudioStream(music_, samples_.data(), static_cast<int>(samples_.size()));
    }
}
} // namespace neon
