#pragma once

#include <string>

/* One-shot WAV playback. A new sound replaces the one playing, like
 * PlaySoundW with SND_ASYNC. */
bool audio_open(const std::string &asset_dir);
void audio_play(int sound_id);
void audio_close();
