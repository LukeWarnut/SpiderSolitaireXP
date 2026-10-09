#pragma once

#include "host.h"

#include <SDL3/SDL.h>

#include <string>

/* Host backed by SDL3 (clock, audio, events) and AppKit (menu bar, alerts,
 * NSUserDefaults). Menu picks arrive as SDL user events carrying the
 * command id. */
class CocoaHost : public Host {
public:
    CocoaHost(SDL_Window *window, const std::string &asset_dir);

    static Uint32 command_event();
    void install_menu();
    void dismiss_sheet();

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

private:
    SDL_Window *window;
    std::string assets;
};
