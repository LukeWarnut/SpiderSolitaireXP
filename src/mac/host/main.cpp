/* WinMain and wnd_proc for macOS: an SDL3 window with a Metal layer, an
 * event pump that feeds the game, and one redraw per display refresh while
 * anything is moving. */
#include "audio.h"
#include "cocoa_host.h"
#include "game.h"
#include "renderer.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <algorithm>
#include <cstdlib>
#include <string>

namespace {

/* WM_GETMINMAXINFO: 600x400 client. */
constexpr int kMinW = 600;
constexpr int kMinH = 400;

std::string find_assets() {
    if (const char *env = std::getenv("SPIDER_ASSETS")) {
        return env;
    }
    const char *base = SDL_GetBasePath();
    std::string dir = base ? base : "./";
    return dir + "assets";
}

void remember_window(SDL_Window *window, Settings &s) {
    SDL_WindowFlags flags = SDL_GetWindowFlags(window);
    s.maximized = (flags & SDL_WINDOW_MAXIMIZED) != 0;
    if (s.maximized || (flags & SDL_WINDOW_MINIMIZED) != 0) {
        return;
    }
    SDL_GetWindowPosition(window, &s.window_x, &s.window_y);
    SDL_GetWindowSize(window, &s.window_w, &s.window_h);
}

int command_for_key(SDL_Keycode key) {
    switch (key) {
    case SDLK_D:
        return CMD_DEAL;
    case SDLK_M:
        return CMD_HINT;
    case SDLK_F1:
        return CMD_HELP;
    case SDLK_F2:
        return CMD_NEW;
    case SDLK_F3:
        return CMD_DIFFICULTY;
    case SDLK_F4:
        return CMD_STATS;
    case SDLK_F5:
        return CMD_OPTIONS;
    default:
        return 0;
    }
}

/* --snapshot: no dialogs, no sound, no saved preferences. */
class SnapshotHost : public CocoaHost {
public:
    using CocoaHost::CocoaHost;
    uint32_t now = 100000;
    uint32_t ticks() const override { return now; }
    void play(int) override {}
    void won_prompt() override {}
    void sync_menu(const MenuState &) override {}
    void load_settings(Settings &s) override { s = Settings(); }
    void save_settings(const Settings &) override {}
};

}  // namespace

/* Reaches into Game to stage the win screen for --snapshot. */
struct GameProbe {
    static void stage_win(Game &g, SnapshotHost &host) {
        g.clear_piles();
        for (int s = 0; s < 4; s++) {
            g.completed[s] = 2;
        }
        for (int i = 0; i < 8; i++) {
            g.completed_order[i] = i % 4;
        }
        g.cards_out = 0;
        g.won_game();
        for (int i = 0; i < 60; i++) {
            host.now += 33;
            g.tick();
        }
    }
};

namespace {

/* Spider --snapshot out.png [board|drag|hint|win] [width height scale] */
int run_snapshot(SDL_Window *window, Renderer &renderer, const std::string &assets, int argc, char **argv) {
    std::string out = argv[2];
    std::string scene = argc > 3 ? argv[3] : "board";
    int w = argc > 4 ? std::atoi(argv[4]) : 1024;
    int h = argc > 5 ? std::atoi(argv[5]) : 720;
    float scale = argc > 6 ? (float)std::atof(argv[6]) : 2.0f;

    SnapshotHost host(window, assets);
    Game game(&host);
    game.layout(w, h);
    game.settings.animate = 0;
    game.settings.sound = 0;
    game.settings.difficulty = 4;
    game.new_game(1);
    game.command(CMD_DEAL);
    game.command(CMD_DEAL);
    if (scene == "drag" || scene == "hint") {
        Frame probe;
        game.build_frame(probe);
        /* Pick up the top card of column 3 and carry it 120pt right and down. */
        int pitch = (w - 0x2c6) / 0xb;
        float x3 = (float)(std::max(0, pitch) + 3 * (pitch + kCardW));
        const Sprite *top = nullptr;
        for (const Sprite &s : probe.board) {
            if (s.x == x3 && (top == nullptr || s.y > top->y)) {
                top = &s;
            }
        }
        if (scene == "drag" && top != nullptr) {
            game.mouse_down(1, top->x + 20, top->y + 20);
            game.mouse_move(top->x + 140, top->y + 140, true);
        } else {
            game.command(CMD_HINT);
        }
    } else if (scene == "win") {
        GameProbe::stage_win(game, host);
    }
    Frame frame;
    game.build_frame(frame);
    bool ok = renderer.snapshot(frame, w, h, scale, out);
    SDL_Log("snapshot %s: %s", scene.c_str(), ok ? out.c_str() : "failed");
    return ok ? 0 : 1;
}

}  // namespace

int main(int argc, char **argv) {
    SDL_SetAppMetadata("Spider", "5.1.2600.1106", "org.spiderxp.Spider");
    bool snapshot = argc > 2 && std::string(argv[1]) == "--snapshot";
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS)) {
        SDL_Log("SDL_Init: %s", SDL_GetError());
        return 1;
    }
    std::string assets = find_assets();

    SDL_Window *window = SDL_CreateWindow("Spider", 1024, 720,
                                          SDL_WINDOW_METAL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY |
                                              SDL_WINDOW_HIDDEN);
    if (window == nullptr) {
        SDL_Log("SDL_CreateWindow: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_SetWindowMinimumSize(window, kMinW, kMinH);

    Renderer renderer;
    if (!renderer.init(window, assets)) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Spider",
                                 ("Cannot start the Metal renderer or load card bitmaps from " + assets +
                                  ". Build the spider target, or set SPIDER_ASSETS to the output of "
                                  "tools/extract_assets.py.")
                                     .c_str(),
                                 nullptr);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    if (snapshot) {
        int rc = run_snapshot(window, renderer, assets, argc, argv);
        renderer.shutdown();
        SDL_DestroyWindow(window);
        SDL_Quit();
        return rc;
    }
    audio_open(assets);

    CocoaHost host(window, assets);
    host.install_menu();
    Game game(&host);

    Settings boot;
    host.load_settings(boot);
    SDL_SetWindowSize(window, std::max(kMinW, boot.window_w), std::max(kMinH, boot.window_h));
    if (boot.window_x >= 0 && boot.window_y >= 0) {
        SDL_SetWindowPosition(window, boot.window_x, boot.window_y);
    } else {
        SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    }
    if (boot.maximized) {
        SDL_MaximizeWindow(window);
    }
    SDL_ShowWindow(window);

    int cw = 0, ch = 0;
    SDL_GetWindowSize(window, &cw, &ch);
    game.layout(cw, ch);
    Frame frame;
    game.build_frame(frame);
    renderer.draw(frame, cw, ch);

    game.startup();
    remember_window(window, game.settings);

    const Uint32 cmd_event = CocoaHost::command_event();
    bool running = true;
    while (running) {
        SDL_Event e;
        bool have = game.busy() ? SDL_PollEvent(&e) : SDL_WaitEventTimeout(&e, 500);
        while (have && running) {
            switch (e.type) {
            case SDL_EVENT_QUIT:
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                remember_window(window, game.settings);
                if (game.try_close()) {
                    running = false;
                }
                break;
            case SDL_EVENT_WINDOW_RESIZED:
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            case SDL_EVENT_WINDOW_MAXIMIZED:
            case SDL_EVENT_WINDOW_RESTORED:
                SDL_GetWindowSize(window, &cw, &ch);
                game.layout(cw, ch);
                remember_window(window, game.settings);
                break;
            case SDL_EVENT_WINDOW_MOVED:
                remember_window(window, game.settings);
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                if (e.button.button == SDL_BUTTON_LEFT) {
                    SDL_CaptureMouse(true);
                    game.mouse_down(1, e.button.x, e.button.y);
                } else if (e.button.button == SDL_BUTTON_RIGHT) {
                    game.mouse_down(3, e.button.x, e.button.y);
                }
                break;
            case SDL_EVENT_MOUSE_BUTTON_UP:
                if (e.button.button == SDL_BUTTON_LEFT) {
                    SDL_CaptureMouse(false);
                    game.mouse_up(1, e.button.x, e.button.y);
                } else if (e.button.button == SDL_BUTTON_RIGHT) {
                    game.mouse_up(3, e.button.x, e.button.y);
                }
                break;
            case SDL_EVENT_MOUSE_MOTION:
                game.mouse_move(e.motion.x, e.motion.y, (e.motion.state & SDL_BUTTON_LMASK) != 0);
                break;
            case SDL_EVENT_KEY_DOWN:
                if (e.key.repeat || (e.key.mod & (SDL_KMOD_GUI | SDL_KMOD_CTRL | SDL_KMOD_ALT)) != 0) {
                    break;
                }
                if (e.key.key == SDLK_ESCAPE) {
                    SDL_MinimizeWindow(window);
                } else if (int id = command_for_key(e.key.key)) {
                    host.dismiss_sheet();
                    game.command(id);
                }
                break;
            default:
                if (e.type == cmd_event) {
                    host.dismiss_sheet();
                    remember_window(window, game.settings);
                    game.command(e.user.code);
                    if (game.closing) {
                        running = false;
                    }
                }
                break;
            }
            have = SDL_PollEvent(&e);
        }
        if (!running) {
            break;
        }
        game.tick();
        game.build_frame(frame);
        renderer.draw(frame, cw, ch);
    }

    host.dismiss_sheet();
    audio_close();
    renderer.shutdown();
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
