#include "wasm_host.h"

#include "game.h"

#include <emscripten.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>

namespace {

/* IDBFS mount point, created in main() before the game starts. */
const char *kSavePath = "/spider/spider.sav";

}  // namespace

/* ui.js / audio.js entry points. The dialogs return Promises, so their side
 * is EM_ASYNC_JS and the game unwinds into the event loop while they wait. */
EM_JS(void, wasm_ui_play, (int id), { SpiderAudio.play(id); });

EM_ASYNC_JS(int, wasm_ui_confirm, (const char *text, int kind), {
    return await SpiderUI.confirm(UTF8ToString(text), kind);
});

EM_ASYNC_JS(int, wasm_ui_difficulty, (int mode), { return await SpiderUI.difficulty(mode); });

EM_ASYNC_JS(int, wasm_ui_options, (const char *csv), {
    const bits = await SpiderUI.options(UTF8ToString(csv));
    return bits === null ? -1 : bits;
});

EM_ASYNC_JS(int, wasm_ui_stats, (const char *body), { return await SpiderUI.stats(UTF8ToString(body)); });

EM_JS(void, wasm_ui_won, (), { SpiderUI.won(); });
EM_JS(void, wasm_ui_about, (), { SpiderUI.about(); });
EM_JS(void, wasm_ui_help, (), { SpiderUI.help(); });

EM_JS(void, wasm_ui_menu, (int restart, int undo, int deal, int hint, int save, int open), {
    SpiderUI.syncMenu({restart : !!restart, undo : !!undo, deal : !!deal, hint : !!hint, save : !!save, open : !!open});
});

EM_JS(void, wasm_settings_save, (const char *data), { localStorage.setItem("spider.settings", UTF8ToString(data)); });

EM_JS(char *, wasm_settings_load, (), {
    const s = localStorage.getItem("spider.settings") || "";
    const n = lengthBytesUTF8(s) + 1;
    const p = _malloc(n);
    stringToUTF8(s, p, n);
    return p;
});

EM_ASYNC_JS(void, wasm_fs_persist, (), {
    await new Promise((resolve) => FS.syncfs(false, () => resolve()));
});

uint32_t WasmHost::ticks() const { return (uint32_t)emscripten_get_now(); }

void WasmHost::play(int sound_id) { wasm_ui_play(sound_id); }

int WasmHost::confirm(const char *text, int kind) { return wasm_ui_confirm(text, kind); }

void WasmHost::message(const char *text) { confirm(text, CONFIRM_OK); }

bool WasmHost::difficulty(int *mode) {
    int picked = wasm_ui_difficulty(*mode);
    if (picked < 0) {
        return false;
    }
    *mode = picked;
    return true;
}

void WasmHost::won_prompt() { wasm_ui_won(); }

void WasmHost::about() { wasm_ui_about(); }

/* dlg_stats: a table of the seven counters per level, with a Reset button.
 * The body is preformatted lines ("label<TAB>value" per level) so ui.js does
 * not need to know the stat layout. Reset confirms, then reopens. */
void WasmHost::stats(Game *game) {
    for (;;) {
        std::string body;
        const char *names[3] = {"Easy", "Medium", "Difficult"};
        for (int i = 0; i < 3; i++) {
            const LevelStats &s = game->settings.stats[i];
            int pct = s.wins > 0 ? (int)((double)s.wins / (double)(s.losses + s.wins) * 100.0) : 0;
            char line[256];
            std::snprintf(line, sizeof(line), "%s\t%d\t%d\t%d\t%d %%\t%d\t%d\t%d %s\n", names[i], s.high, s.wins,
                          s.losses, pct, s.streak_wins, s.streak_losses, s.streak_current,
                          s.streak_is_win ? "Wins" : "Losses");
            body += line;
        }
        if (!wasm_ui_stats(body.c_str())) {
            return;
        }
        if (confirm("Are you sure you want to reset all game statistics?", CONFIRM_YESNO) == ANSWER_YES) {
            game->reset_stats();
        }
    }
}

/* dlg_options: six checkboxes, passed in and out as one "0,1,..." string. */
void WasmHost::options(Game *game) {
    Settings &s = game->settings;
    char csv[24];
    std::snprintf(csv, sizeof(csv), "%d,%d,%d,%d,%d,%d", s.animate, s.save_on_exit, s.load_at_start, s.prompt_save,
                  s.prompt_load, s.sound);
    int bits = wasm_ui_options(csv);
    if (bits < 0) {
        return;
    }
    s.animate = bits & 1;
    s.save_on_exit = bits & 2;
    s.load_at_start = bits & 4;
    s.prompt_save = bits & 8;
    s.prompt_load = bits & 16;
    s.sound = bits & 32;
}

void WasmHost::help() { wasm_ui_help(); }

void WasmHost::sync_menu(const MenuState &m) {
    wasm_ui_menu(m.restart, m.undo, m.deal, m.hint, m.save, m.open);
}

/* Registry values from load_settings, kept as "key=value" lines in one
 * localStorage string (parsing JSON in C++ would pull in a library). */
void WasmHost::load_settings(Settings &s) {
    s = Settings();
    char *raw = wasm_settings_load();
    if (raw == nullptr) {
        return;
    }
    const char *keys[] = {"WndX",     "WndY",     "WndWidth", "WndHeight",  "WndState",  "AnimDeal",
                          "SaveOnExit", "LoadAtStart", "PromptSave", "PromptLoad", "Sound", "NumSuits"};
    int *fields[] = {&s.window_x,    &s.window_y,    &s.window_w,    &s.window_h,   &s.maximized,
                     &s.animate,     &s.save_on_exit, &s.load_at_start, &s.prompt_save, &s.prompt_load,
                     &s.sound,       &s.difficulty};
    const char *stat_keys[7] = {"HighScore", "Wins",         "Losses",       "StreakWins",
                                "StreakLosses", "StreakCurrent", "FWinStreak"};
    int LevelStats::*stat_fields[7] = {&LevelStats::high,   &LevelStats::wins,         &LevelStats::losses,
                                       &LevelStats::streak_wins, &LevelStats::streak_losses, &LevelStats::streak_current,
                                       &LevelStats::streak_is_win};
    std::string data(raw);
    free(raw);
    auto find_value = [&](const std::string &key) -> int {
        std::string head = key + "=";
        size_t p = data.find(head);
        if (p == std::string::npos || (p != 0 && data[p - 1] != '\n')) {
            return -1;
        }
        return std::atoi(data.c_str() + p + head.size());
    };
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
        int v = find_value(keys[i]);
        if (v >= 0) {
            *fields[i] = v;
        }
    }
    const char *levels[3] = {"Easy", "Medium", "Difficult"};
    for (int i = 0; i < 3; i++) {
        for (int f = 0; f < 7; f++) {
            int v = find_value(std::string(stat_keys[f]) + "_" + levels[i]);
            if (v >= 0) {
                s.stats[i].*stat_fields[f] = v;
            }
        }
    }
}

void WasmHost::save_settings(const Settings &s) {
    std::string data;
    auto put = [&](const char *key, int v) {
        data += key;
        data += "=";
        data += std::to_string(v);
        data += "\n";
    };
    put("WndX", s.window_x);
    put("WndY", s.window_y);
    put("WndWidth", s.window_w);
    put("WndHeight", s.window_h);
    put("WndState", s.maximized);
    put("AnimDeal", s.animate);
    put("SaveOnExit", s.save_on_exit);
    put("LoadAtStart", s.load_at_start);
    put("PromptSave", s.prompt_save);
    put("PromptLoad", s.prompt_load);
    put("Sound", s.sound);
    put("NumSuits", s.difficulty);
    const char *levels[3] = {"Easy", "Medium", "Difficult"};
    for (int i = 0; i < 3; i++) {
        const LevelStats &row = s.stats[i];
        std::string lv = std::string("_") + levels[i];
        put(("HighScore" + lv).c_str(), row.high);
        put(("Wins" + lv).c_str(), row.wins);
        put(("Losses" + lv).c_str(), row.losses);
        put(("StreakWins" + lv).c_str(), row.streak_wins);
        put(("StreakLosses" + lv).c_str(), row.streak_losses);
        put(("StreakCurrent" + lv).c_str(), row.streak_current);
        put(("FWinStreak" + lv).c_str(), row.streak_is_win);
    }
    wasm_settings_save(data.c_str());
}

std::string WasmHost::save_path() {
    /* main() already created and mounted /spider. */
    return kSavePath;
}

bool WasmHost::save_exists() {
    std::ifstream in(kSavePath, std::ios::binary);
    return in.good();
}

void WasmHost::persist() { wasm_fs_persist(); }
