#include "audio.h"

#include <SDL3/SDL.h>

#include <map>
#include <vector>

namespace {

SDL_AudioStream *g_stream = nullptr;
SDL_AudioSpec g_spec = {SDL_AUDIO_F32, 2, 44100};
std::map<int, std::vector<Uint8>> g_sounds;

}  // namespace

bool audio_open(const std::string &asset_dir) {
    g_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &g_spec, nullptr, nullptr);
    if (g_stream == nullptr) {
        SDL_Log("audio: %s", SDL_GetError());
        return false;
    }
    for (int id = 124; id <= 129; id++) {
        std::string path = asset_dir + "/sounds/" + std::to_string(id) + ".wav";
        SDL_AudioSpec spec;
        Uint8 *buf = nullptr;
        Uint32 len = 0;
        if (!SDL_LoadWAV(path.c_str(), &spec, &buf, &len)) {
            SDL_Log("audio: %s: %s", path.c_str(), SDL_GetError());
            continue;
        }
        Uint8 *out = nullptr;
        int out_len = 0;
        if (SDL_ConvertAudioSamples(&spec, buf, (int)len, &g_spec, &out, &out_len)) {
            g_sounds[id].assign(out, out + out_len);
            SDL_free(out);
        }
        SDL_free(buf);
    }
    SDL_ResumeAudioStreamDevice(g_stream);
    return true;
}

void audio_play(int sound_id) {
    if (g_stream == nullptr) {
        return;
    }
    auto it = g_sounds.find(sound_id);
    if (it == g_sounds.end() || it->second.empty()) {
        return;
    }
    SDL_ClearAudioStream(g_stream);
    SDL_PutAudioStreamData(g_stream, it->second.data(), (int)it->second.size());
}

void audio_close() {
    if (g_stream != nullptr) {
        SDL_DestroyAudioStream(g_stream);
        g_stream = nullptr;
    }
    g_sounds.clear();
}
