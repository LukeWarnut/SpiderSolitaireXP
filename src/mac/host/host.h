#pragma once

#include <cstdint>
#include <string>

class Game;

enum ConfirmKind {
    CONFIRM_OK = 0,
    CONFIRM_YESNO = 1,
    CONFIRM_YESNOCANCEL = 2,
};

enum Answer {
    ANSWER_OK = 1,
    ANSWER_CANCEL = 2,
    ANSWER_YES = 6,
    ANSWER_NO = 7,
};

/* WAVE resource ids in spider.exe (PlaySoundW 0x7c-0x81). */
enum SoundId {
    SND_DEAL = 124,
    SND_DROP = 125,
    SND_HINT = 126,
    SND_NOHINT = 127,
    SND_PICKUP = 128,
    SND_WIN = 129,
};

/* One row of GameWin::suit_slot: HighScore, Wins, Losses, StreakWins,
 * StreakLosses, StreakCurrent, FWinStreak. */
struct LevelStats {
    int high = 0;
    int wins = 0;
    int losses = 0;
    int streak_wins = 0;
    int streak_losses = 0;
    int streak_current = 0;
    int streak_is_win = 0;
};

/* The values load_settings / save_settings kept under HKCU\Software\Microsoft\Spider. */
struct Settings {
    int window_x = -1;
    int window_y = -1;
    int window_w = 1024;
    int window_h = 720;
    int maximized = 0;
    int animate = 1;       /* AnimDeal */
    int save_on_exit = 0;  /* SaveOnExit */
    int load_at_start = 0; /* LoadAtStart */
    int prompt_save = 1;   /* PromptSave */
    int prompt_load = 1;   /* PromptLoad */
    int sound = 1;         /* Sound */
    int difficulty = 1;    /* NumSuits: 1, 2, or 4 */
    LevelStats stats[3];
};

struct MenuState {
    bool restart = false;
    bool undo = false;
    bool deal = false;
    bool hint = false;
    bool save = false;
    bool open = false;
};

/* Platform services the game calls. The app implements them with SDL3 and
 * AppKit; the headless test implements them with stubs. */
class Host {
public:
    virtual ~Host() = default;
    virtual uint32_t ticks() const = 0;
    virtual void play(int sound_id) = 0;
    virtual int confirm(const char *text, int kind) = 0;
    virtual void message(const char *text) = 0;
    virtual bool difficulty(int *mode) = 0;
    /* Non-blocking: the win animation keeps running while it is shown.
     * "Yes" posts CMD_NEW back to the game. */
    virtual void won_prompt() = 0;
    virtual void about() = 0;
    virtual void stats(Game *game) = 0;
    virtual void options(Game *game) = 0;
    virtual void help() = 0;
    virtual void sync_menu(const MenuState &menu) = 0;
    virtual void load_settings(Settings &settings) = 0;
    virtual void save_settings(const Settings &settings) = 0;
    virtual std::string save_path() = 0;
    virtual bool save_exists() = 0;
    /* Called after save_to writes spider.sav, so a host with an asynchronous
     * filesystem (the WASM port's IDBFS) can flush it. Native files are
     * already on disk, so the default does nothing. */
    virtual void persist() {}
};
