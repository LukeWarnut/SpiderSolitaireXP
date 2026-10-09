#pragma once

#include "host.h"

#include <cstdint>
#include <string>
#include <vector>

/* Menu and accelerator ids from spider.rc. */
enum CommandId {
    CMD_ABOUT = 40002,
    CMD_HELP = 40003,
    CMD_EXIT = 40004,
    CMD_NEW = 40005,
    CMD_RESTART = 40006,
    CMD_DEAL = 40007,
    CMD_UNDO = 40010,
    CMD_SAVE = 40011,
    CMD_OPEN = 40012,
    CMD_HINT = 40013,
    CMD_STATS = 40014,
    CMD_OPTIONS = 40015,
    CMD_DIFFICULTY = 40017,
};

/* Card codes passed to draw_card: 1-52 faces, then these bitmaps. */
enum CardCode {
    CODE_BACK = 0x68,  /* CARDBACK */
    CODE_FELT = 0x69,  /* FELT */
    CODE_EMPTY = 0x6c, /* bitmap 108: empty column outline */
};

constexpr int kCardW = 0x47;
constexpr int kCardH = 0x60;
constexpr int kDeck = 104;

/* One entry of TripleTable: suit (0-3), face (0 = ace .. 12 = king), face up. */
struct DeckCard {
    int suit = 0;
    int face = 0;
    int up = 0;
};

/* One undo row (GameWin::undo): run moved from src to dst starting at `at`. */
struct UndoRec {
    int flipped = 0;
    int src = 0;
    int dst = 0;
    int at = 0;
};

/* One Rec24 from collect_moves. */
struct HintMove {
    int same = 0;
    int src = 0;
    int src_card = 0;
    int dst = 0;
    int dst_card = 0;
    int kind = 0;
};

/* A card in flight, replacing the blocking slide_drag loop. Deal slides go
 * from the stock to (pile, index); suit slides go from the column to the
 * completed-suit slot. */
struct Slide {
    bool deal = true;
    int pile = 0;
    int index = 0;
    int code = 0;
    float x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    uint32_t t0 = 0;
    bool started = false;
    /* Last card of a removed suit: flip the card underneath and score it. */
    bool settle = false;
};

struct FxItem {
    float x = 0, y = 0, z = 0;
    float vel0x = 0, vel0y = 0, vel0z = 0;
    float velx = 0, vely = 0, velz = 0;
    float accx = 0, accy = 0, accz = 0;
    float r = 0, g = 0, b = 0;
    float life_span = 1;
    float life = 0;
    float scale = 1;
};

struct FxBank {
    FxItem items[100];
    float stamp = 0;
    float gx = -2, gy = -2, gz = -2;
    int ready = 0;
};

struct Sprite {
    int code = 0;
    float x = 0;
    float y = 0;
};

struct Particle {
    float x = 0, y = 0, rad = 0;
    float r = 1, g = 1, b = 1;
};

/* Everything the renderer needs for one frame, in client coordinates. */
struct Frame {
    std::vector<Sprite> board; /* drawn under the score box */
    std::vector<Sprite> front; /* stock, completed suits, peek, drag, slide */
    bool show_score = false;
    int score_x = 0, score_y = 0, score_w = 0, score_h = 0;
    int score = 0;
    int moves = 0;
    bool hint_on = false;
    int hint_x = 0, hint_y = 0, hint_w = 0, hint_h = 0;
    std::vector<Particle> fx;
    bool win_text = false;
    float win_r = 1, win_g = 1, win_b = 1;
};

class Game {
public:
    explicit Game(Host *host);

    void startup();
    void layout(int width, int height);
    void command(int id);
    void mouse_down(int button, float x, float y);
    void mouse_move(float x, float y, bool left_down);
    void mouse_up(int button, float x, float y);
    void tick();
    bool busy() const;
    bool try_close();
    void build_frame(Frame &frame) const;
    MenuState menu_state() const;

    void new_game(int seed);
    bool save_to(const std::string &path);
    bool load_from(const std::string &path);
    void reset_stats();

    Settings settings;
    bool closing = false;

    friend int run_game_tests();
    friend struct GameProbe;

private:
    Host *host;

    int client_w = 1024;
    int client_h = 720;
    int margin = 0; /* GameWin::b */
    int pitch = 0;  /* GameWin::a, negative when columns overlap */

    DeckCard deck[kDeck];
    int deck_idx = 0;
    std::vector<int> piles[10];
    int hidden[10] = {};
    int gap[10] = {};
    int deals = 0;
    int seed = 1;
    int score = 0;
    int moves = 0;
    int cards_out = 0;
    int completed[4] = {};
    int completed_order[8] = {};
    int suit_lock = 1;
    bool started = false;
    bool save_file_exists = false;

    UndoRec undo[150];
    int undo_n = 0;

    std::vector<HintMove> hints;
    int hint_pos = 0;
    bool hints_ready = false;
    int hint_phase = 0;
    int hint_index = 0;
    uint32_t hint_until = 0;
    int hint_x = 0, hint_y = 0, hint_w = 0, hint_h = 0;

    bool dragging = false;
    int lift_pile = 0;
    int lift_card = 0;
    int drag_x = 0, drag_y = 0, drag_w = 0, drag_h = 0;
    int origin_x = 0, origin_y = 0;

    bool peeking = false;
    int peek_pile = 0;
    int peek_card = 0;

    std::vector<Slide> slides;
    uint32_t rng_state = 1;
    uint32_t fx_rng = 1;

    bool celebrating = false;
    FxBank banks[2];
    uint32_t win_t0 = 0;

    int crt_rand();
    int fx_rand();
    void shuffle(int seed_value);
    void clear_piles();
    void add_card(int pile, int value, int face_up);
    int card_at(int pile, int index) const;
    int suit_of(int pile, int index) const;
    int face_of(int pile, int index) const;
    int rank_of(int pile, int index) const;
    int card_code(int pile, int index) const;
    bool column_empty(int pile) const;
    bool any_empty() const;
    bool can_drop(int src_pile, int src_card, int dst_pile) const;
    bool run_ok(int pile, int card) const;
    bool full_suit(int pile) const;
    int card_y(int pile, int index) const;
    int column_x(int pile) const;
    int stock_x(int i) const;
    int deal_top() const;
    int pile_at(int x) const;
    int hit_card(int pile, int y) const;
    void card_rect(int pile, int lo, int hi, int &l, int &t, int &r, int &b) const;
    void stock_rect(int &l, int &t, int &r, int &b) const;
    void score_rect(int &l, int &t, int &r, int &b) const;
    void fit_piles();
    void splice(int src, int src_i, int dst);
    void move_run(int src_pile, int src_card, int dst_pile, bool record, bool flip);
    void take_suit(int pile);
    void won_game();
    void push_undo(const UndoRec &rec);
    void pop_undo();
    void add_score(int delta);
    void tally(bool won);
    void collect_moves();
    void blink();
    void place_hint(int pile, int lo);
    void deal();
    void choose_difficulty();
    void finish_slides();
    void advance_slides();
    void retire_slide(const Slide &s);
    bool slide_hides(int pile, int index) const;
    void burst(FxBank &bank);
    void run_bank(FxBank &bank, float now);
    void refresh_menu();
    LevelStats &stats_row();
};

int run_game_tests();
