#pragma once

#include "host.h"

#include <string>

/* Host backed by the browser: emscripten clock, Web Audio, HTML menus and
 * dialogs (ui.js), localStorage settings, and an IDBFS-backed spider.sav.
 * The modal dialogs (confirm, difficulty, stats, options) are EM_ASYNC_JS
 * awaits, so the game runs under -sASYNCIFY. */
class WasmHost : public Host {
public:
    uint32_t ticks() const override;
    void play(int sound_id) override;
    int confirm(const char *text, int kind) override;
    void message(const char *text) override;
    bool difficulty(int *mode) override;
    void won_prompt() override;
    void about() override;
    void stats(Game *game) override;
    void options(Game *game) override;
    void help() override;
    void sync_menu(const MenuState &menu) override;
    void load_settings(Settings &settings) override;
    void save_settings(const Settings &settings) override;
    std::string save_path() override;
    bool save_exists() override;
    void persist() override;
};
