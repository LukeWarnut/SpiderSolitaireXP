/* Emscripten bindings for the Game core. main.js fetches the bitmaps,
 * mounts IDBFS, then calls startGame() after the HTML UI is ready so
 * ASYNCIFY dialogs have SpiderUI to talk to. */
#include "bmp.h"
#include "game.h"
#include "wasm_host.h"

#include <emscripten.h>
#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace {

/* One entry of the image table: a decoded BMP as RGBA8. */
struct WebImage {
    std::string name;
    int w = 0;
    int h = 0;
    std::vector<uint8_t> rgba;
};

std::vector<WebImage> g_images;
std::unique_ptr<WasmHost> g_host;
std::unique_ptr<Game> g_game;

int card_code_for(const std::string &name) {
    if (name.rfind("CARD", 0) == 0 && name.size() > 4) {
        return std::atoi(name.c_str() + 4);
    }
    if (name == "CARDBACK") {
        return CODE_BACK;
    }
    if (name == "108") {
        return CODE_EMPTY;
    }
    return 0;
}

/* Copy a JS Uint8Array into a BMP and run the same post-pass as the Metal
 * renderer (octagon stroke on the face cards, transparent corners). */
bool add_bitmap(const std::string &name, emscripten::val u8) {
    const size_t n = u8["length"].as<size_t>();
    std::vector<uint8_t> bytes(n);
    emscripten::val(emscripten::typed_memory_view(n, bytes.data())).call<void>("set", u8);
    Image decoded;
    if (!load_bmp_mem(bytes, decoded)) {
        return false;
    }
    const int code = card_code_for(name);
    if ((code >= 0xe && code <= 0x17) || (code >= 0x1b && code <= 0x24)) {
        stroke_octagon(decoded);
    }
    if (code != 0) {
        punch_corners(decoded);
    }
    g_images.push_back({name, decoded.w, decoded.h, std::move(decoded.rgba)});
    return true;
}

void start_game() {
    g_host = std::make_unique<WasmHost>();
    g_game = std::make_unique<Game>(g_host.get());
    g_game->startup();
}

bool game_ready() { return g_game != nullptr; }

/* Sprite lookup for the renderer: card slice by code. main.js uploads the
 * table once as canvases and blits with drawImage. */
int image_count() { return (int)g_images.size(); }

std::string image_name(int i) {
    if (i < 0 || i >= (int)g_images.size()) {
        return "";
    }
    return g_images[i].name;
}

int image_width(int i) {
    if (i < 0 || i >= (int)g_images.size()) {
        return 0;
    }
    return g_images[i].w;
}

int image_height(int i) {
    if (i < 0 || i >= (int)g_images.size()) {
        return 0;
    }
    return g_images[i].h;
}

/* The win dialog's "Yes" button and the menu bar send commands through here,
 * from real events, so ASYNCIFY dialogs inside command() can unwind. */
void post_command(int id) {
    if (g_game) {
        g_game->command(id);
    }
}

void on_resize(int w, int h) {
    if (g_game) {
        g_game->layout(w, h);
    }
}

void on_mouse_down(int button, float x, float y) {
    if (g_game) {
        g_game->mouse_down(button, x, y);
    }
}

void on_mouse_move(float x, float y, bool left_down) {
    if (g_game) {
        g_game->mouse_move(x, y, left_down);
    }
}

void on_mouse_up(int button, float x, float y) {
    if (g_game) {
        g_game->mouse_up(button, x, y);
    }
}

void on_tick() {
    if (g_game) {
        g_game->tick();
    }
}

bool is_busy() { return g_game && g_game->busy(); }

Frame current_frame() {
    Frame frame;
    if (g_game) {
        g_game->build_frame(frame);
    }
    return frame;
}

bool closing_wanted() { return g_game && g_game->closing; }

Settings *game_settings() { return g_game ? &g_game->settings : nullptr; }

emscripten::val image_pixels(int i) {
    if (i < 0 || i >= (int)g_images.size()) {
        return emscripten::val::null();
    }
    auto &img = g_images[i];
    return emscripten::val(emscripten::typed_memory_view(img.rgba.size(), img.rgba.data()));
}

}  // namespace

EMSCRIPTEN_BINDINGS(spider) {
    using namespace emscripten;

    value_object<Sprite>("Sprite").field("code", &Sprite::code).field("x", &Sprite::x).field("y", &Sprite::y);

    value_object<Particle>("Particle")
        .field("x", &Particle::x)
        .field("y", &Particle::y)
        .field("rad", &Particle::rad)
        .field("r", &Particle::r)
        .field("g", &Particle::g)
        .field("b", &Particle::b);

    register_vector<Sprite>("SpriteList");
    register_vector<Particle>("ParticleList");

    value_object<Frame>("Frame")
        .field("board", &Frame::board)
        .field("front", &Frame::front)
        .field("show_score", &Frame::show_score)
        .field("score_x", &Frame::score_x)
        .field("score_y", &Frame::score_y)
        .field("score_w", &Frame::score_w)
        .field("score_h", &Frame::score_h)
        .field("score", &Frame::score)
        .field("moves", &Frame::moves)
        .field("hint_on", &Frame::hint_on)
        .field("hint_x", &Frame::hint_x)
        .field("hint_y", &Frame::hint_y)
        .field("hint_w", &Frame::hint_w)
        .field("hint_h", &Frame::hint_h)
        .field("fx", &Frame::fx)
        .field("win_text", &Frame::win_text)
        .field("win_r", &Frame::win_r)
        .field("win_g", &Frame::win_g)
        .field("win_b", &Frame::win_b);

    class_<LevelStats>("LevelStats")
        .property("high", &LevelStats::high)
        .property("wins", &LevelStats::wins)
        .property("losses", &LevelStats::losses)
        .property("streak_wins", &LevelStats::streak_wins)
        .property("streak_losses", &LevelStats::streak_losses)
        .property("streak_current", &LevelStats::streak_current)
        .property("streak_is_win", &LevelStats::streak_is_win);

    class_<Settings>("Settings")
        .property("window_x", &Settings::window_x)
        .property("window_y", &Settings::window_y)
        .property("window_w", &Settings::window_w)
        .property("window_h", &Settings::window_h)
        .property("maximized", &Settings::maximized)
        .property("animate", &Settings::animate)
        .property("save_on_exit", &Settings::save_on_exit)
        .property("load_at_start", &Settings::load_at_start)
        .property("prompt_save", &Settings::prompt_save)
        .property("prompt_load", &Settings::prompt_load)
        .property("sound", &Settings::sound)
        .property("difficulty", &Settings::difficulty);

    enum_<CommandId>("CommandId")
        .value("ABOUT", CMD_ABOUT)
        .value("HELP", CMD_HELP)
        .value("EXIT", CMD_EXIT)
        .value("NEW", CMD_NEW)
        .value("RESTART", CMD_RESTART)
        .value("DEAL", CMD_DEAL)
        .value("UNDO", CMD_UNDO)
        .value("SAVE", CMD_SAVE)
        .value("OPEN", CMD_OPEN)
        .value("HINT", CMD_HINT)
        .value("STATS", CMD_STATS)
        .value("OPTIONS", CMD_OPTIONS)
        .value("DIFFICULTY", CMD_DIFFICULTY);

    function("postCommand", &post_command);
    function("layout", &on_resize);
    function("mouseDown", &on_mouse_down);
    function("mouseMove", &on_mouse_move);
    function("mouseUp", &on_mouse_up);
    function("tick", &on_tick);
    function("busy", &is_busy);
    function("frame", &current_frame);
    function("closingWanted", &closing_wanted);
    function("imageCount", &image_count);
    function("imageName", &image_name);
    function("imageWidth", &image_width);
    function("imageHeight", &image_height);
    function("imagePixels", &image_pixels);
    function("settings", &game_settings, allow_raw_pointers());
    function("addBitmap", &add_bitmap);
    function("startGame", &start_game);
    function("gameReady", &game_ready);
}
