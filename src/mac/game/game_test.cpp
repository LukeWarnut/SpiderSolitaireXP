/* Headless driver: deals, moves, undoes, deals a row, saves, and loads
 * without SDL, Metal, or AppKit. */
#include "game.h"

#include <cstdio>
#include <filesystem>
#include <string>

namespace {

class TestHost : public Host {
public:
    uint32_t now = 1000;
    int answer = ANSWER_YES;
    int mode_out = 1;
    int won = 0;
    std::string dir;

    uint32_t ticks() const override { return now; }
    void play(int) override {}
    int confirm(const char *, int) override { return answer; }
    void message(const char *) override {}
    bool difficulty(int *mode) override {
        *mode = mode_out;
        return true;
    }
    void won_prompt() override { won++; }
    void about() override {}
    void stats(Game *) override {}
    void options(Game *) override {}
    void help() override {}
    void sync_menu(const MenuState &) override {}
    void load_settings(Settings &s) override { s = Settings(); }
    void save_settings(const Settings &) override {}
    std::string save_path() override { return dir + "/spider.sav"; }
    bool save_exists() override { return std::filesystem::exists(save_path()); }
};

int failures = 0;

void check(bool ok, const char *what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        failures++;
    }
}

int count_cards(const Game &g, const std::vector<int> (&piles)[10]) {
    (void)g;
    int n = 0;
    for (const auto &p : piles) {
        n += (int)p.size();
    }
    return n;
}

}  // namespace

int run_game_tests() {
    TestHost host;
    host.dir = std::filesystem::temp_directory_path().string() + "/spider-mac-test";
    std::filesystem::create_directories(host.dir);
    std::filesystem::remove(host.save_path());

    Game game(&host);
    game.layout(1024, 720);
    game.settings.animate = 0;
    game.settings.sound = 0;
    game.settings.difficulty = 1;
    game.new_game(1);

    check(count_cards(game, game.piles) == 54, "opening deal has 54 cards");
    check(game.piles[0].size() == 6 && game.piles[4].size() == 5, "columns 0-3 have 6, 4-9 have 5");
    check(game.hidden[0] == 5 && game.hidden[4] == 4, "all but the top card face down");
    check(game.score == 500 && game.moves == 0, "score starts at 500");
    check(game.card_code(0, 0) == CODE_BACK, "face-down card draws the back");
    check(game.deck_idx == 54, "stock index after the opening deal");
    for (int p = 0; p < 10; p++) {
        int code = game.card_code(p, (int)game.piles[p].size() - 1);
        check(code >= 40 && code <= 52, "one-suit game deals only spades");
    }

    /* Same seed, same layout. */
    Game twin(&host);
    twin.layout(1024, 720);
    twin.settings = game.settings;
    twin.new_game(1);
    for (int p = 0; p < 10; p++) {
        check(twin.piles[p] == game.piles[p], "shuffle is deterministic per seed");
    }

    check(game.save_to(host.save_path()), "save");
    Game loaded(&host);
    loaded.layout(1024, 720);
    loaded.settings.animate = 0;
    check(loaded.load_from(host.save_path()), "load");
    for (int p = 0; p < 10; p++) {
        check(loaded.piles[p] == game.piles[p] && loaded.hidden[p] == game.hidden[p], "save round-trip piles");
    }
    check(loaded.score == game.score && loaded.seed == game.seed, "save round-trip score and seed");

    bool moved = false;
    for (int src = 0; src < 10 && !moved; src++) {
        int card = (int)game.piles[src].size() - 1;
        for (int dst = 0; dst < 10 && !moved; dst++) {
            if (game.run_ok(src, card) && game.can_drop(src, card, dst)) {
                game.move_run(src, card, dst, true, false);
                moved = true;
            }
        }
    }
    check(moved, "a legal opening move exists for seed 1");
    check(game.moves == 1 && game.undo_n == 1 && game.score == 499, "move records undo and costs a point");
    check(count_cards(game, game.piles) == 54, "move keeps the card count");
    game.pop_undo();
    for (int p = 0; p < 10; p++) {
        check(game.piles[p] == loaded.piles[p] && game.hidden[p] == loaded.hidden[p], "undo restores the columns");
    }

    game.deal();
    check(count_cards(game, game.piles) == 64 && game.deals == 1, "deal adds a row of ten");
    check(game.undo_n == 0, "deal clears undo");

    game.collect_moves();
    check(!game.hints.empty(), "hint search finds a move after the deal");

    /* Hint blink is frame-driven: source, then destination, then off. */
    game.command(CMD_HINT);
    check(game.hint_phase == 1, "hint shows the source run");
    host.now += 300;
    game.tick();
    check(game.hint_phase == 2, "hint moves to the destination");
    host.now += 300;
    game.tick();
    check(game.hint_phase == 0, "hint ends");

    /* Animated deal slides one card per 100ms and hides it until it lands. */
    game.settings.animate = 1;
    game.deal();
    check(game.slides.size() == 10, "animated deal queues ten slides");
    Frame frame;
    game.build_frame(frame);
    int before = (int)frame.board.size();
    for (int i = 0; i < 12; i++) {
        game.tick();
        host.now += 101;
    }
    check(game.slides.empty(), "slides finish");
    game.build_frame(frame);
    check((int)frame.board.size() == before + 10, "landed cards join their columns");

    /* A completed king-to-ace run leaves the board and scores 100. */
    Game win(&host);
    win.layout(1024, 720);
    win.settings.animate = 0;
    win.settings.sound = 0;
    win.new_game(7);
    win.clear_piles();
    for (int v = 0; v < kDeck; v++) {
        win.deck[v].suit = 3;
        win.deck[v].face = 12 - (v % 13);
        win.deck[v].up = 1;
    }
    for (int k = 0; k < 13; k++) {
        win.add_card(0, k, 1);
    }
    win.cards_out = 13;
    int score_before = win.score;
    check(win.full_suit(0), "king-to-ace is a full suit");
    win.take_suit(0);
    check(win.piles[0].empty() && win.completed[3] == 1, "full suit is removed");
    check(win.score == score_before + 100, "full suit scores 100");
    check(host.won == 1 && win.celebrating, "last suit starts the win animation and prompt");
    for (int i = 0; i < 40; i++) {
        host.now += 50;
        win.tick();
    }
    win.build_frame(frame);
    check(!frame.fx.empty() && frame.win_text, "win frame has particles and text");
    win.command(CMD_STATS);
    check(!win.celebrating, "a command ends the win animation");

    /* Animated suit removal keeps the run in the column. Only the card whose
     * slide has started leaves; the completed king is drawn at the lower left
     * the whole time. */
    Game fly(&host);
    fly.layout(1024, 720);
    fly.settings.animate = 1;
    fly.settings.sound = 0;
    fly.deck[0].suit = 3;
    fly.deck[0].face = 0;
    fly.deck[0].up = 0;
    fly.add_card(0, 0, 0);
    for (int k = 0; k < 13; k++) {
        fly.deck[k + 1].suit = 3;
        fly.deck[k + 1].face = 12 - k;
        fly.deck[k + 1].up = 1;
        fly.add_card(0, k + 1, 1);
    }
    fly.cards_out = 14;
    fly.started = true;
    check(fly.full_suit(0), "run on top of a face-down card");
    int score_fly = fly.score;
    fly.take_suit(0);
    check(fly.piles[0].size() == 14, "the run stays in the column when the animation starts");
    check(fly.slides.size() == 13 && fly.score == score_fly, "scoring waits until the run has flown");
    Frame flying;
    fly.build_frame(flying);
    int home = 0;
    bool king_slot = false;
    for (const Sprite &s : flying.board) {
        if (s.x == (float)fly.column_x(0)) {
            home++;
        }
    }
    for (const Sprite &s : flying.front) {
        if (s.code == 52 && s.y == (float)fly.deal_top()) {
            king_slot = true;
        }
    }
    check(home == 14, "every card of the run is still drawn");
    check(king_slot, "the completed suit is shown at the lower left");
    fly.tick();
    fly.build_frame(flying);
    home = 0;
    for (const Sprite &s : flying.board) {
        if (s.x == (float)fly.column_x(0)) {
            home++;
        }
    }
    check(home == 13, "the ace leaves the column when its slide starts");
    check(!flying.front.empty(), "the ace is drawn in flight");
    for (int i = 0; i < 13; i++) {
        host.now += 101;
        fly.tick();
    }
    check(fly.slides.empty(), "suit slides finish");
    check(fly.piles[0].size() == 1 && fly.deck[0].up == 1, "the card under the run flips after the king leaves");
    check(fly.score == score_fly + 100 && fly.cards_out == 1, "the suit scores when the animation ends");
    check(host.won == 1, "a suit that does not empty the table does not win");

    std::filesystem::remove(host.save_path());
    if (failures == 0) {
        std::printf("game tests ok\n");
    }
    return failures == 0 ? 0 : 1;
}

int main() { return run_game_tests(); }
