/* Spider rules lifted from the decompiled GameWin / GameBoard / DealView /
 * ColumnOp / AnimState sources, as one 64-bit object with no Win32 calls.
 * Layout constants (71x96 cards, 7px face-down step, 0x1c face-up step,
 * 0x6a stock band) are the original's. */
#include "game.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <ctime>
#include <fstream>

namespace {

constexpr int kSlideMs = 100;  /* slide_drag: until now > t0 + 100 */
constexpr int kBlinkMs = 250;  /* blink_move: Sleep(0xFA) */
constexpr float kRandUnit = 3.051851e-05f;

const char *kTxtNewGame = "Are you sure you want to start a new game?";
const char *kTxtRestart = "Are you sure you want to restart this game from the beginning?";
const char *kTxtNoDeal = "You are not allowed to deal a new row while there are any empty slots.";
const char *kTxtReplace =
    "A saved game already exists.  Are you sure you want to replace your previously saved game with your "
    "current game?";
const char *kTxtDiscard =
    "Are you sure you want to discard the game you are currently playing, and load your previously saved game?";
const char *kTxtSaveFail = "Unable to save game.";
const char *kTxtLoadFail = "Unable to load game.";
const char *kTxtSaveFirst = "Do you want to save this game before closing it?";

}  // namespace

Game::Game(Host *host_in) : host(host_in) {
    for (int i = 0; i < 10; i++) {
        gap[i] = 0x1c;
    }
}

/* libc rand(): the shuffle depends on it, so seeds deal the same layouts as XP. */
int Game::crt_rand() {
    rng_state = rng_state * 214013u + 2531011u;
    return (int)((rng_state >> 16) & 0x7fff);
}

int Game::fx_rand() {
    fx_rng = fx_rng * 214013u + 2531011u;
    return (int)((fx_rng >> 16) & 0x7fff);
}

LevelStats &Game::stats_row() {
    if (settings.difficulty == 1) {
        return settings.stats[0];
    }
    if (settings.difficulty == 2) {
        return settings.stats[1];
    }
    return settings.stats[2];
}

MenuState Game::menu_state() const {
    MenuState menu;
    bool live = started && suit_lock == 0;
    menu.restart = started;
    menu.undo = live && undo_n > 0;
    menu.deal = live && deals < 5;
    menu.hint = live;
    menu.save = live;
    menu.open = save_file_exists;
    return menu;
}

void Game::refresh_menu() { host->sync_menu(menu_state()); }

/* TripleTable::shuffle with decks = 2. */
void Game::shuffle(int seed_value) {
    int bits[kDeck] = {};
    rng_state = (uint32_t)seed_value;
    for (int pass = 0; pass < 2; pass++) {
        for (int suit = 0; suit < 4; suit++) {
            for (int face = 0; face <= 12; face++) {
                int rem;
                do {
                    rem = crt_rand() % kDeck;
                } while (bits[rem] != 0);
                bits[rem] = 1;
                int mapped = suit;
                if (settings.difficulty == 1) {
                    mapped = 3;
                } else if (settings.difficulty == 2) {
                    if (mapped == 0) {
                        mapped = 3;
                    }
                    if (mapped == 1) {
                        mapped = 2;
                    }
                }
                deck[rem].suit = mapped;
                deck[rem].face = face;
                deck[rem].up = 0;
            }
        }
    }
    deck_idx = 0;
}

void Game::clear_piles() {
    for (int i = 0; i < 10; i++) {
        piles[i].clear();
        hidden[i] = 0;
    }
}

void Game::add_card(int pile, int value, int face_up) {
    piles[pile].push_back(value);
    if (face_up == 0) {
        hidden[pile]++;
    }
}

int Game::card_at(int pile, int index) const {
    if (pile < 0 || pile >= 10 || index < 0 || index >= (int)piles[pile].size()) {
        return -1;
    }
    return piles[pile][index];
}

bool Game::column_empty(int pile) const { return piles[pile].empty(); }

bool Game::any_empty() const {
    for (int i = 0; i < 10; i++) {
        if (piles[i].empty()) {
            return true;
        }
    }
    return false;
}

/* DealView::suit_of: really "face up". An empty column counts as face up. */
int Game::suit_of(int pile, int index) const {
    if (column_empty(pile)) {
        return 1;
    }
    int v = card_at(pile, index);
    return v < 0 ? 0 : deck[v].up;
}

/* DealView::rank_of: the suit. */
int Game::rank_of(int pile, int index) const {
    if (column_empty(pile)) {
        return -1;
    }
    int v = card_at(pile, index);
    return v < 0 ? -1 : deck[v].suit;
}

int Game::face_of(int pile, int index) const {
    if (column_empty(pile)) {
        return -1;
    }
    int v = card_at(pile, index);
    return v < 0 ? -1 : deck[v].face;
}

int Game::card_code(int pile, int index) const {
    if (index == -1) {
        return CODE_EMPTY;
    }
    int v = card_at(pile, index);
    if (v < 0) {
        return CODE_EMPTY;
    }
    if (deck[v].up == 0) {
        return CODE_BACK;
    }
    return deck[v].suit * 13 + deck[v].face + 1;
}

bool Game::can_drop(int src_pile, int src_card, int dst_pile) const {
    if (src_pile == dst_pile || column_empty(src_pile)) {
        return false;
    }
    if (column_empty(dst_pile)) {
        return true;
    }
    int last = (int)piles[dst_pile].size() - 1;
    if (suit_of(dst_pile, last) == 0 || suit_of(src_pile, src_card) == 0) {
        return false;
    }
    return face_of(dst_pile, last) == face_of(src_pile, src_card) + 1;
}

bool Game::run_ok(int pile, int card) const {
    int n = (int)piles[pile].size();
    if (column_empty(pile) || card < 0 || card >= n || suit_of(pile, card) == 0) {
        return false;
    }
    int suit = rank_of(pile, card);
    int face = face_of(pile, card);
    for (int i = card + 1; i < n; i++) {
        if (rank_of(pile, i) != suit || face_of(pile, i) != face - 1) {
            return false;
        }
        face--;
    }
    return true;
}

bool Game::full_suit(int pile) const {
    int count = (int)piles[pile].size();
    if (count < 13) {
        return false;
    }
    int suit = rank_of(pile, count - 1);
    int face = face_of(pile, count - 1);
    for (int i = count - 2, step = 2; step <= 13; step++, i--) {
        if (suit_of(pile, i) == 0 || face_of(pile, i) != face + 1 || rank_of(pile, i) != suit) {
            return false;
        }
        face++;
    }
    return true;
}

int Game::deal_top() const { return client_h - 0x6a; }

int Game::column_x(int pile) const { return (pitch + kCardW) * pile + margin; }

/* LayoutBox::card_x */
int Game::stock_x(int i) const { return (pitch + kCardW) * 9 - i * 12 + margin; }

/* Layout::score */
int Game::card_y(int pile, int index) const {
    if (index == -1) {
        return 10;
    }
    int cap = hidden[pile];
    if (index < cap) {
        return index * 7 + 10;
    }
    return gap[pile] * (index - cap) + cap * 7 + 10;
}

/* ColumnOp::place */
void Game::card_rect(int pile, int lo, int hi, int &l, int &t, int &r, int &b) const {
    l = column_x(pile);
    r = l + kCardW;
    t = card_y(pile, lo);
    int cap = hidden[pile];
    if (column_empty(pile)) {
        b = t + kCardH;
    } else if (lo < cap) {
        if (hi < cap) {
            b = (hi - lo) * 7 + t + kCardH;
        } else {
            b = gap[pile] * (hi - cap) + cap * 7 + 0x6a;
        }
    } else {
        b = gap[pile] * (hi - lo) + t + kCardH;
    }
}

/* LayoutBox::fill_rect */
void Game::stock_rect(int &l, int &t, int &r, int &b) const {
    int n = (kDeck - deck_idx + 9) / 10 - 1;
    l = stock_x(n < 0 ? 0 : n);
    r = stock_x(0) + kCardW;
    t = deal_top();
    b = t + kCardH;
}

/* LayoutWin::center_rect */
void Game::score_rect(int &l, int &t, int &r, int &b) const {
    l = (client_w - 0xc8) / 2;
    r = (client_w + 0xc8) / 2;
    t = deal_top();
    b = client_h - 10;
}

int Game::pile_at(int x) const {
    int step = pitch + kCardW;
    if (x < margin || step <= 0) {
        return -2;
    }
    int q = (x - margin) / step;
    if ((x - margin) % step > kCardW || q >= 10) {
        return -2;
    }
    return q;
}

int Game::hit_card(int pile, int y) const {
    if (y < 10) {
        return -2;
    }
    y -= 10;
    if (column_empty(pile)) {
        return y < kCardH ? -1 : -2;
    }
    int n = (int)piles[pile].size();
    int cap = hidden[pile];
    int step = gap[pile];
    int top = cap * 7;
    int mid = (n - cap - 1) * step;
    if (y < top) {
        return y / 7;
    }
    if (y < top + mid) {
        return (y - top) / step + cap;
    }
    if (y < top + mid + kCardH) {
        return n - 1;
    }
    return -2;
}

/* ColumnOp::fit_piles: shrink the face-up step until the column clears the stock band. */
void Game::fit_piles() {
    if (client_h <= 0x6a) {
        return;
    }
    int limit = deal_top();
    for (int pile = 0; pile < 10; pile++) {
        if (column_empty(pile)) {
            continue;
        }
        gap[pile] = 0x1c;
        int top = (int)piles[pile].size() - 1;
        for (;;) {
            int l, t, r, b;
            card_rect(pile, top, top, l, t, r, b);
            if (b < limit) {
                break;
            }
            if (gap[pile] < 0x10 && t < limit - 0x10) {
                break;
            }
            if (gap[pile] < 1) {
                break;
            }
            gap[pile]--;
        }
    }
}

/* WM_SIZE in wnd_proc */
void Game::layout(int width, int height) {
    client_w = width;
    client_h = height;
    pitch = (width - 0x2c6) / 0xb;
    margin = pitch < 0 ? 0 : pitch;
    fit_piles();
}

void Game::splice(int src, int src_i, int dst) {
    std::vector<int> &from = piles[src];
    std::vector<int> &to = piles[dst];
    if (src_i < 0 || src_i > (int)from.size()) {
        return;
    }
    to.insert(to.end(), from.begin() + src_i, from.end());
    from.erase(from.begin() + src_i, from.end());
}

void Game::push_undo(const UndoRec &rec) {
    if (undo_n == 150) {
        std::memmove(undo, undo + 1, sizeof(UndoRec) * 149);
        undo_n--;
    }
    undo[undo_n++] = rec;
}

void Game::add_score(int delta) {
    score = std::max(0, score + delta);
    LevelStats &row = stats_row();
    if (score > row.high) {
        row.high = score;
    }
}

void Game::tally(bool won) {
    if (!won) {
        if (moves == 0 && deals == 0) {
            return;
        }
        bool unfinished = false;
        for (int i = 0; i < 4; i++) {
            if (completed[i] < 2) {
                unfinished = true;
            }
        }
        if (!unfinished) {
            return;
        }
    }
    LevelStats &row = stats_row();
    if (won) {
        if (row.streak_is_win != 0) {
            row.streak_current++;
        } else {
            row.streak_is_win = 1;
            row.streak_current = 1;
        }
        row.streak_wins = std::max(row.streak_wins, row.streak_current);
        row.wins++;
    } else {
        if (row.streak_is_win != 0) {
            row.streak_is_win = 0;
            row.streak_current = 1;
        } else {
            row.streak_current++;
        }
        row.streak_losses = std::max(row.streak_losses, row.streak_current);
        row.losses++;
    }
    host->save_settings(settings);
}

/* GameWin::move_run. Undo calls it with record = false and flip = the
 * recorded flip, which turns the uncovered card back down. */
void Game::move_run(int src_pile, int src_card, int dst_pile, bool record, bool flip) {
    int last = std::max(0, (int)piles[dst_pile].size() - 1);
    bool empty = column_empty(dst_pile);
    splice(src_pile, src_card, dst_pile);
    if (record) {
        UndoRec rec;
        rec.src = src_pile;
        rec.dst = dst_pile;
        rec.at = empty ? 0 : last + 1;
        int prev = src_card - 1;
        if (!column_empty(src_pile) && suit_of(src_pile, prev) == 0) {
            rec.flipped = 1;
            hidden[src_pile]--;
            deck[card_at(src_pile, prev)].up = 1;
        }
        push_undo(rec);
    } else if (flip) {
        hidden[dst_pile]++;
        int v = card_at(dst_pile, last);
        if (v >= 0) {
            deck[v].up = 0;
        }
    }
    moves++;
    hints_ready = false;
    hint_phase = 0;
    add_score(-1);
    fit_piles();
}

void Game::pop_undo() {
    if (undo_n <= 0) {
        return;
    }
    UndoRec rec = undo[--undo_n];
    move_run(rec.dst, rec.at, rec.src, false, rec.flipped != 0);
}

/* GameWin::take_suit. With animation on, each of the 13 cards slides to the
 * completed-suit slot, ace first. */
void Game::take_suit(int pile) {
    int n = (int)piles[pile].size();
    int suit = rank_of(pile, n - 1);
    int total = completed[0] + completed[1] + completed[2] + completed[3];
    if (total < 8) {
        completed_order[total] = suit;
    }
    completed[suit]++;
    total++;
    undo_n = 0;
    hints_ready = false;
    hint_phase = 0;
    if (settings.animate) {
        /* begin_drag slides one card, then trim_pile removes it. The other
         * twelve stay in the column until it is their turn. The king is
         * already drawn in the completed-suit slot. */
        float tx = (float)(margin - 12 + total * 12);
        float ty = (float)deal_top();
        for (int c = n - 1; c >= n - 13; c--) {
            Slide s;
            s.deal = false;
            s.pile = pile;
            s.index = c;
            s.code = card_code(pile, c);
            s.x0 = (float)column_x(pile);
            s.y0 = (float)card_y(pile, c);
            s.x1 = tx;
            s.y1 = ty;
            s.settle = c == n - 13;
            slides.push_back(s);
        }
        return;
    }
    if (settings.sound) {
        host->play(SND_DEAL);
    }
    piles[pile].resize(n - 13);
    cards_out -= 13;
    add_score(100);
    fit_piles();
    if (!column_empty(pile)) {
        int top = (int)piles[pile].size() - 1;
        if (suit_of(pile, top) == 0) {
            hidden[pile]--;
            deck[card_at(pile, top)].up = 1;
        }
    }
    if (cards_out <= 0) {
        won_game();
    }
}

/* GameWin::won_game plus AnimState::reset_draw. */
void Game::won_game() {
    finish_slides();
    suit_lock = 1;
    tally(true);
    refresh_menu();
    celebrating = true;
    win_t0 = host->ticks();
    fx_rng = win_t0 | 1u;
    burst(banks[0]);
    burst(banks[1]);
    float now = (float)win_t0 * 0.001f;
    banks[0].stamp = now;
    banks[1].stamp = now;
    if (settings.sound) {
        host->play(SND_WIN);
    }
    host->won_prompt();
}

bool Game::slide_hides(int pile, int index) const {
    for (const Slide &s : slides) {
        if (s.pile != pile || s.index != index) {
            continue;
        }
        /* A dealt card stays hidden until it lands. A suit card stays in the
         * column until its own slide starts. */
        if (s.deal || s.started) {
            return true;
        }
    }
    return false;
}

void Game::retire_slide(const Slide &s) {
    if (s.deal) {
        return;
    }
    std::vector<int> &pile = piles[s.pile];
    if (!pile.empty() && (int)pile.size() - 1 == s.index) {
        pile.pop_back();
    }
    if (!s.settle) {
        return;
    }
    if (!column_empty(s.pile)) {
        int top = (int)piles[s.pile].size() - 1;
        if (suit_of(s.pile, top) == 0) {
            hidden[s.pile]--;
            deck[card_at(s.pile, top)].up = 1;
        }
    }
    cards_out -= 13;
    add_score(100);
    fit_piles();
    if (cards_out <= 0) {
        won_game();
    }
}

void Game::finish_slides() {
    std::vector<Slide> pending;
    pending.swap(slides);
    for (const Slide &s : pending) {
        retire_slide(s);
    }
}

void Game::advance_slides() {
    while (!slides.empty()) {
        Slide &s = slides.front();
        uint32_t now = host->ticks();
        if (!s.started) {
            if (s.deal) {
                /* anim_deal: start from the leftmost stock stack still showing. */
                int waiting = 0;
                for (const Slide &o : slides) {
                    if (o.deal && !o.started) {
                        waiting++;
                    }
                }
                int stacks = std::max(1, (kDeck - deck_idx + waiting + 9) / 10);
                s.x0 = (float)stock_x(stacks - 1);
                s.y0 = (float)deal_top();
                s.x1 = (float)column_x(s.pile);
                s.y1 = (float)card_y(s.pile, s.index);
            }
            s.t0 = now;
            s.started = true;
            if (settings.sound) {
                host->play(SND_DEAL);
            }
        }
        if (now <= s.t0 + kSlideMs) {
            return;
        }
        Slide done = s;
        slides.erase(slides.begin());
        retire_slide(done);
    }
}

/* GameWin::deal */
void Game::deal() {
    if (deals >= 5 || suit_lock != 0) {
        return;
    }
    if (any_empty()) {
        host->message(kTxtNoDeal);
        return;
    }
    if (settings.animate == 0 && settings.sound) {
        host->play(SND_DEAL);
    }
    for (int pile = 0; pile < 10 && deck_idx < kDeck; pile++) {
        int idx = deck_idx++;
        deck[idx].up = 1;
        add_card(pile, idx, 1);
        if (settings.animate) {
            Slide s;
            s.pile = pile;
            s.index = (int)piles[pile].size() - 1;
            s.code = card_code(pile, s.index);
            slides.push_back(s);
        }
    }
    deals++;
    cards_out += 10;
    hints_ready = false;
    hint_phase = 0;
    undo_n = 0;
    fit_piles();
    for (int pile = 0; pile < 10 && suit_lock == 0; pile++) {
        if (full_suit(pile)) {
            take_suit(pile);
        }
    }
}

/* GameWin::new_game */
void Game::new_game(int seed_in) {
    if (suit_lock == 0) {
        tally(false);
    }
    slides.clear();
    dragging = false;
    peeking = false;
    celebrating = false;
    hint_phase = 0;
    hints_ready = false;
    suit_lock = 0;
    started = true;
    seed = seed_in;
    shuffle(seed_in);
    clear_piles();
    deals = 0;
    undo_n = 0;
    moves = 0;
    score = 0;
    std::memset(completed, 0, sizeof(completed));
    std::memset(completed_order, 0, sizeof(completed_order));
    add_score(500);
    int n = 0;
    for (int pass = 0; pass < 5; pass++) {
        for (int pile = 0; pile < 10; pile++) {
            if (pass != 4 || pile <= 3) {
                deck_idx++;
                deck[n].up = 0;
                add_card(pile, n, 0);
                n++;
            }
        }
    }
    for (int pile = 0; pile < 10; pile++) {
        deck_idx++;
        deck[n].up = 1;
        add_card(pile, n, 1);
        if (settings.animate) {
            Slide s;
            s.pile = pile;
            s.index = (int)piles[pile].size() - 1;
            s.code = card_code(pile, s.index);
            slides.push_back(s);
        }
        n++;
    }
    cards_out = n;
    fit_piles();
    refresh_menu();
}

/* GameWin::collect_moves plus RecBank::sort_desc (stable insertion sort). */
void Game::collect_moves() {
    hints.clear();
    hint_pos = 0;
    for (int src = 0; src < 10; src++) {
        for (int dst = 0; dst < 10; dst++) {
            if (src == dst) {
                continue;
            }
            int last = (int)piles[src].size() - 1;
            int kept = last;
            if (last > 0) {
                int prev = last - 1;
                do {
                    kept = last;
                    if (rank_of(src, last) != rank_of(src, prev) ||
                        face_of(src, last) != face_of(src, prev) - 1 || suit_of(src, prev) == 0) {
                        break;
                    }
                    last--;
                    prev--;
                    kept = last;
                } while (last > 0);
            }
            if (!can_drop(src, last, dst)) {
                continue;
            }
            int dst_last = (int)piles[dst].size() - 1;
            int kind = 2;
            if (rank_of(src, last) == rank_of(dst, dst_last)) {
                kind = 3;
            } else if (column_empty(dst)) {
                kind = 1;
            }
            HintMove rec;
            rec.same = rank_of(src, last) == rank_of(dst, dst_last);
            rec.src = src;
            rec.src_card = kept;
            rec.dst = dst;
            rec.dst_card = dst_last;
            rec.kind = kind;
            if (hints.size() < 0x1f) {
                hints.push_back(rec);
            }
        }
    }
    for (int i = 1; i < (int)hints.size(); i++) {
        for (int j = i; j > 0 && hints[j].kind > hints[j - 1].kind; j--) {
            std::swap(hints[j], hints[j - 1]);
        }
    }
    hints_ready = true;
}

void Game::place_hint(int pile, int lo) {
    int l, t, r, b;
    card_rect(pile, lo, (int)piles[pile].size() - 1, l, t, r, b);
    hint_x = l;
    hint_y = t;
    hint_w = r - l;
    hint_h = b - t;
}

/* GameWin::blink_move: invert the source run, then the destination, 250ms each. */
void Game::blink() {
    if (!hints_ready) {
        collect_moves();
    }
    if (hints.empty()) {
        if (settings.sound) {
            host->play(SND_NOHINT);
        }
        return;
    }
    if (settings.sound) {
        host->play(SND_HINT);
    }
    hint_index = hint_pos;
    hint_pos = (hint_pos + 1) % (int)hints.size();
    const HintMove &rec = hints[hint_index];
    place_hint(rec.src, rec.src_card);
    hint_phase = 1;
    hint_until = host->ticks() + kBlinkMs;
}

bool Game::save_to(const std::string &path) {
    std::vector<int32_t> buf;
    buf.push_back(settings.difficulty);
    buf.push_back(seed);
    buf.push_back(deck_idx);
    buf.push_back(cards_out);
    buf.push_back(moves);
    buf.push_back(deals);
    for (int k = 0; k < 4; k++) {
        buf.push_back(completed[k]);
    }
    for (int k = 0; k < 8; k++) {
        buf.push_back(completed_order[k]);
    }
    for (int pile = 0; pile < 10; pile++) {
        buf.push_back((int)piles[pile].size());
        buf.push_back(hidden[pile]);
        for (int v : piles[pile]) {
            buf.push_back(v);
        }
    }
    buf.push_back(score);
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        return false;
    }
    out.write(reinterpret_cast<const char *>(buf.data()), (std::streamsize)(buf.size() * 4));
    if (!out) {
        return false;
    }
    save_file_exists = true;
    return true;
}

/* GameWin::load_game. Reads the same little-endian int32 layout XP wrote. */
bool Game::load_from(const std::string &path) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) {
        return false;
    }
    std::streamsize bytes = in.tellg();
    if (bytes < 24 || bytes % 4 != 0 || bytes > 0x10000) {
        return false;
    }
    std::vector<int32_t> buf((size_t)bytes / 4);
    in.seekg(0);
    in.read(reinterpret_cast<char *>(buf.data()), bytes);
    if (!in) {
        return false;
    }
    int n = (int)buf.size();
    int i = 0;
    auto next = [&](int &out) {
        if (i >= n) {
            return false;
        }
        out = buf[i++];
        return true;
    };

    int mode = 4;
    if (buf[0] < 5) {
        mode = buf[0];
        i = 1;
    }
    if (mode != 1 && mode != 2) {
        mode = 4;
    }
    int new_seed, idx, out_cards, new_moves, new_deals;
    int done[4] = {};
    int order[8] = {};
    if (!next(new_seed) || !next(idx) || !next(out_cards) || !next(new_moves) || !next(new_deals)) {
        return false;
    }
    for (int k = 0; k < 4; k++) {
        if (!next(done[k])) {
            return false;
        }
    }
    if (i < n && buf[i] < 5) {
        for (int k = 0; k < 8; k++) {
            if (!next(order[k])) {
                return false;
            }
        }
    }
    std::vector<int> new_piles[10];
    int new_hidden[10] = {};
    for (int pile = 0; pile < 10; pile++) {
        int count, down;
        if (!next(count) || !next(down) || count < 0 || count > kDeck || down < 0 || down > count) {
            return false;
        }
        for (int k = 0; k < count; k++) {
            int v;
            if (!next(v) || v < 0 || v >= kDeck) {
                return false;
            }
            new_piles[pile].push_back(v);
        }
        new_hidden[pile] = down;
    }
    if (idx < 0 || idx > kDeck || new_deals < 0 || new_deals > 5) {
        return false;
    }

    settings.difficulty = mode;
    seed = new_seed;
    shuffle(seed);
    deck_idx = idx;
    cards_out = out_cards;
    moves = new_moves;
    deals = new_deals;
    std::memcpy(completed, done, sizeof(done));
    std::memcpy(completed_order, order, sizeof(order));
    for (int pile = 0; pile < 10; pile++) {
        piles[pile] = new_piles[pile];
        hidden[pile] = new_hidden[pile];
        for (int k = 0; k < (int)piles[pile].size(); k++) {
            deck[piles[pile][k]].up = k >= hidden[pile] ? 1 : 0;
        }
    }
    score = i < n ? buf[i] : 500;
    slides.clear();
    dragging = false;
    peeking = false;
    celebrating = false;
    hint_phase = 0;
    hints_ready = false;
    undo_n = 0;
    suit_lock = 0;
    started = true;
    save_file_exists = true;
    fit_piles();
    return true;
}

/* GameWin::reset_suit_slots */
void Game::reset_stats() {
    for (LevelStats &row : settings.stats) {
        row = LevelStats();
    }
    if (suit_lock == 0) {
        stats_row().high = score;
    }
    host->save_settings(settings);
}

/* GameWin::deal_prompt */
void Game::choose_difficulty() {
    if (moves > 0 && cards_out > 0 && suit_lock == 0) {
        int ans = host->confirm(kTxtSaveFirst, CONFIRM_YESNOCANCEL);
        if (ans == ANSWER_CANCEL) {
            return;
        }
        if (ans == ANSWER_YES && !save_to(host->save_path())) {
            host->message(kTxtSaveFail);
        }
    }
    int mode = settings.difficulty;
    if (!host->difficulty(&mode)) {
        return;
    }
    settings.difficulty = mode;
    host->save_settings(settings);
    new_game((int)std::time(nullptr));
}

/* GameWin::on_command. Any command ends the win animation (clear_draw) and
 * finishes cards still sliding, the way the blocking original never let a
 * command start mid-slide. */
void Game::command(int id) {
    finish_slides();
    celebrating = false;
    peeking = false;
    switch (id) {
    case CMD_NEW:
        if ((moves != 0 || deals != 0) && suit_lock == 0 &&
            host->confirm(kTxtNewGame, CONFIRM_YESNO) != ANSWER_YES) {
            break;
        }
        new_game((int)std::time(nullptr));
        break;
    case CMD_RESTART:
        if ((moves > 0 || deals > 0) && host->confirm(kTxtRestart, CONFIRM_YESNO) == ANSWER_YES) {
            new_game(seed);
        }
        break;
    case CMD_DEAL:
        deal();
        break;
    case CMD_UNDO:
        if (suit_lock == 0) {
            pop_undo();
        }
        break;
    case CMD_SAVE:
        if (!started || suit_lock != 0) {
            break;
        }
        if (settings.prompt_save && save_file_exists &&
            host->confirm(kTxtReplace, CONFIRM_YESNO) != ANSWER_YES) {
            break;
        }
        if (!save_to(host->save_path())) {
            host->message(kTxtSaveFail);
        }
        break;
    case CMD_OPEN:
        save_file_exists = host->save_exists();
        if (!save_file_exists) {
            host->message(kTxtLoadFail);
            break;
        }
        if (suit_lock == 0 && settings.prompt_load &&
            host->confirm(kTxtDiscard, CONFIRM_YESNO) != ANSWER_YES) {
            break;
        }
        if (!load_from(host->save_path())) {
            host->message(kTxtLoadFail);
        }
        break;
    case CMD_HINT:
        if (started && suit_lock == 0) {
            blink();
        }
        break;
    case CMD_STATS:
        host->stats(this);
        break;
    case CMD_OPTIONS:
        host->options(this);
        host->save_settings(settings);
        break;
    case CMD_DIFFICULTY:
        choose_difficulty();
        break;
    case CMD_EXIT:
        try_close();
        break;
    case CMD_HELP:
        host->help();
        break;
    case CMD_ABOUT:
        host->about();
        break;
    default:
        break;
    }
    refresh_menu();
}

/* WinMain after CreateWindow: open the saved game or ask for a difficulty. */
void Game::startup() {
    host->load_settings(settings);
    if (settings.difficulty != 1 && settings.difficulty != 2) {
        settings.difficulty = 4;
    }
    suit_lock = 1;
    save_file_exists = host->save_exists();
    refresh_menu();
    if (settings.load_at_start && save_file_exists) {
        if (load_from(host->save_path())) {
            refresh_menu();
            return;
        }
        host->message(kTxtLoadFail);
    }
    choose_difficulty();
    refresh_menu();
}

void Game::mouse_down(int button, float fx, float fy) {
    int x = (int)fx;
    int y = (int)fy;
    if (button == 3) {
        /* on_right_down: show a covered face-up card on top until release. */
        if (dragging || suit_lock != 0) {
            return;
        }
        int pile = pile_at(x);
        if (pile < 0) {
            return;
        }
        int card = hit_card(pile, y);
        if (card == -2 || card < hidden[pile]) {
            return;
        }
        peeking = true;
        peek_pile = pile;
        peek_card = card;
        return;
    }
    if (button != 1 || peeking || suit_lock != 0 || dragging) {
        return;
    }
    if (!slides.empty()) {
        finish_slides();
    }
    int pile = pile_at(x);
    if (pile < 0) {
        return;
    }
    int card = hit_card(pile, y);
    if (card == -2 || !run_ok(pile, card)) {
        return;
    }
    if (settings.sound) {
        host->play(SND_PICKUP);
    }
    dragging = true;
    lift_pile = pile;
    lift_card = card;
    int l, t, r, b;
    card_rect(pile, card, (int)piles[pile].size() - 1, l, t, r, b);
    drag_x = l;
    drag_y = t;
    drag_w = r - l;
    drag_h = b - t;
    origin_x = x - l;
    origin_y = y - t;
}

void Game::mouse_move(float x, float y, bool left_down) {
    if (!dragging || !left_down) {
        return;
    }
    drag_x = (int)x - origin_x;
    drag_y = (int)y - origin_y;
}

/* on_left_up / on_right_up */
void Game::mouse_up(int button, float fx, float fy) {
    int x = (int)fx;
    int y = (int)fy;
    if (button == 3) {
        peeking = false;
        return;
    }
    if (button != 1) {
        return;
    }
    if (!dragging) {
        if (suit_lock != 0) {
            return;
        }
        int l, t, r, b;
        if (deals < 5) {
            stock_rect(l, t, r, b);
            if (x > l && x < r && y > t && y < b) {
                command(CMD_DEAL);
                return;
            }
        }
        score_rect(l, t, r, b);
        if (x >= l && x < r && y >= t && y < b) {
            command(CMD_HINT);
        }
        return;
    }
    dragging = false;
    if (settings.sound) {
        host->play(SND_DROP);
    }
    int target = -1;
    int i = pile_at(drag_x);
    int last = pile_at(drag_x + drag_w);
    if (last < 0) {
        last = std::min(i + 1, 9);
    }
    for (; i <= last && target == -1; i++) {
        if (i < 0 || i >= 10) {
            continue;
        }
        int top = (int)piles[i].size() - 1;
        int l, t, r, b;
        card_rect(i, top, top, l, t, r, b);
        bool hit = drag_x < r && drag_x + drag_w > l && drag_y < b && drag_y + drag_h > t;
        if (hit && can_drop(lift_pile, lift_card, i)) {
            target = i;
        }
    }
    if (target >= 0) {
        move_run(lift_pile, lift_card, target, true, false);
        if (full_suit(target)) {
            take_suit(target);
        }
    }
    refresh_menu();
}

/* WM_CLOSE */
bool Game::try_close() {
    if (closing) {
        return true;
    }
    if (suit_lock == 0) {
        if (settings.save_on_exit) {
            bool skip = settings.prompt_save && save_file_exists &&
                        host->confirm(kTxtReplace, CONFIRM_YESNO) != ANSWER_YES;
            if (skip || !save_to(host->save_path())) {
                tally(false);
            }
        } else {
            int ans = host->confirm(kTxtSaveFirst, CONFIRM_YESNOCANCEL);
            if (ans == ANSWER_YES) {
                if (!save_to(host->save_path())) {
                    host->message(kTxtSaveFail);
                    return false;
                }
            } else if (ans == ANSWER_NO) {
                tally(false);
            } else {
                return false;
            }
        }
    }
    host->save_settings(settings);
    closing = true;
    return true;
}

bool Game::busy() const {
    return !slides.empty() || hint_phase != 0 || celebrating || dragging || peeking;
}

/* AnimState::burst_fx */
void Game::burst(FxBank &bank) {
    auto unit = [&]() { return ((float)fx_rand() - (float)fx_rand()) * kRandUnit; };
    float base_x = unit() * 20.0f;
    float base_y = 30.0f;
    float speed = (float)fx_rand() * kRandUnit * 10.0f + 20.0f;
    int side = fx_rand() % 2;
    int mid = fx_rand() % 2;
    int end = fx_rand() % 2;
    if (side == 0 && mid == 0) {
        end = 1;
    }
    bank.gx = side ? -2.0f : -150.0f;
    bank.gy = mid ? -2.0f : -150.0f;
    bank.gz = end ? -2.0f : -150.0f;
    bank.ready = 0;
    for (FxItem &it : bank.items) {
        it = FxItem();
        it.vel0x = base_x;
        it.vel0y = base_y;
        float dx = unit(), dy = unit(), dz = unit();
        float len = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (len < 1.0e-6f) {
            len = 1.0f;
        }
        it.accx = base_x + speed * dx / len;
        it.accy = base_y + speed * dy / len;
        it.accz = speed * dz / len;
        it.r = it.g = it.b = 0.1f;
        it.scale = 0.2f;
        it.life_span = (unit() * 0.125f + 1.0f) * 4.0f;
        it.life = 0;
    }
}

/* AnimState::run_fx: one second of launch, then the burst decays under gravity. */
void Game::run_bank(FxBank &bank, float now) {
    float age = now - bank.stamp - 1.0f;
    bank.ready = 0;
    for (FxItem &it : bank.items) {
        if (age < 0.0f) {
            float s = age - ((float)(fx_rand() - fx_rand()) * kRandUnit + 1.0f) * 0.05f;
            it.x = it.velx + it.vel0x * s / 1.5f;
            it.y = it.vely + it.vel0y * s / 1.5f;
            it.z = it.velz + it.vel0z * s / 1.5f;
            continue;
        }
        float f = (1.0f - std::exp(age * -1.8f)) * 0.308642f;
        float kx = it.accx * 1.8f;
        float ky = it.accy * 1.8f - 6.8f;
        float kz = it.accz * 1.8f;
        it.x = it.velx + kx * f;
        it.y = it.vely + age * -6.8f / 1.8f + ky * f;
        it.z = it.velz + kz * f;
        it.life = age / it.life_span;
        float l2 = it.life * it.life;
        it.r = std::exp(l2 * bank.gx);
        it.g = std::exp(l2 * bank.gy);
        it.b = std::exp(l2 * bank.gz);
        it.scale = std::exp(-l2);
        if (it.life >= 1.0f) {
            bank.ready++;
        }
    }
}

void Game::tick() {
    advance_slides();
    uint32_t now_ms = host->ticks();
    if (hint_phase == 1 && now_ms >= hint_until) {
        if (hint_index < (int)hints.size()) {
            const HintMove &rec = hints[hint_index];
            place_hint(rec.dst, rec.dst_card);
        }
        hint_phase = 2;
        hint_until = now_ms + kBlinkMs;
    } else if (hint_phase == 2 && now_ms >= hint_until) {
        hint_phase = 0;
    }
    if (!celebrating) {
        return;
    }
    float now = (float)now_ms * 0.001f;
    for (FxBank &bank : banks) {
        run_bank(bank, now);
        if (now >= bank.stamp && bank.ready == 100) {
            bank.stamp = now;
            burst(bank);
        }
    }
}

void Game::build_frame(Frame &frame) const {
    frame = Frame();
    auto push = [](std::vector<Sprite> &dst, int code, float x, float y) {
        Sprite s;
        s.code = code;
        s.x = x;
        s.y = y;
        dst.push_back(s);
    };

    /* Empty-column outlines go first: with negative pitch the original clipped
     * them out from under the column to their left. */
    for (int pile = 0; pile < 10; pile++) {
        bool lifted_all = dragging && lift_pile == pile && lift_card == 0;
        if (column_empty(pile) || lifted_all) {
            push(frame.board, CODE_EMPTY, (float)column_x(pile), 10);
        }
    }
    for (int pile = 0; pile < 10; pile++) {
        int stop = (int)piles[pile].size();
        if (dragging && lift_pile == pile) {
            stop = lift_card;
        }
        for (int i = 0; i < stop; i++) {
            if (!slide_hides(pile, i)) {
                push(frame.board, card_code(pile, i), (float)column_x(pile), (float)card_y(pile, i));
            }
        }
    }

    if (started) {
        int l, t, r, b;
        score_rect(l, t, r, b);
        frame.show_score = true;
        frame.score_x = l;
        frame.score_y = t;
        frame.score_w = r - l;
        frame.score_h = b - t;
        frame.score = score;
        frame.moves = moves;
    }

    /* Stock: one back per remaining row, plus the row still in flight. */
    int pending_deals = 0;
    for (const Slide &s : slides) {
        if (s.deal && !s.started) {
            pending_deals++;
        }
    }
    int remain = kDeck - deck_idx + pending_deals;
    if (started) {
        int stacks = (remain + 9) / 10;
        for (int i = 0; i < stacks; i++) {
            push(frame.front, CODE_BACK, (float)stock_x(i), (float)deal_top());
        }
    }

    /* Completed suits: king of each, 12px apart. The slot is drawn as soon as
     * the suit is taken; the cards still in the column fly onto it. */
    int done = std::min(8, completed[0] + completed[1] + completed[2] + completed[3]);
    for (int i = 0; i < done; i++) {
        push(frame.front, (completed_order[i] + 1) * 13, (float)(margin + i * 12), (float)deal_top());
    }

    if (peeking) {
        push(frame.front, card_code(peek_pile, peek_card), (float)column_x(peek_pile),
             (float)card_y(peek_pile, peek_card));
    }

    if (dragging) {
        int n = (int)piles[lift_pile].size();
        int step = gap[lift_pile];
        for (int i = lift_card; i < n; i++) {
            push(frame.front, card_code(lift_pile, i), (float)drag_x, (float)(drag_y + (i - lift_card) * step));
        }
    }

    if (!slides.empty() && slides.front().started) {
        const Slide &s = slides.front();
        float k = std::min(1.0f, (float)(host->ticks() - s.t0) * 0.01f);
        push(frame.front, s.code, s.x0 + (s.x1 - s.x0) * k, s.y0 + (s.y1 - s.y0) * k);
    }

    if (hint_phase != 0) {
        frame.hint_on = true;
        frame.hint_x = hint_x;
        frame.hint_y = hint_y;
        frame.hint_w = hint_w;
        frame.hint_h = hint_h;
    }

    if (!celebrating) {
        return;
    }
    /* AnimState::step_fx */
    int w = client_w;
    int h = client_h;
    int s = std::min(w, h) / 0x32;
    for (const FxBank &bank : banks) {
        for (const FxItem &it : bank.items) {
            if (it.life >= 1.0f) {
                continue;
            }
            Particle p;
            p.x = (float)((int)((float)s * it.x) + w / 2);
            p.y = (float)(h + h / -2 - (int)((float)s * it.y));
            p.rad = std::max(1.0f, (float)((int)(it.scale * 12.0f) / 2));
            p.r = std::min(1.0f, it.r);
            p.g = std::min(1.0f, it.g);
            p.b = std::min(1.0f, it.b);
            frame.fx.push_back(p);
        }
    }
    /* AnimState::prep_blit: hue cycle over 10 seconds. */
    unsigned rem = (host->ticks() - win_t0) % 10000u;
    float x = (float)rem * 0.0001f * 6.0f;
    int band = (int)std::floor(x);
    float frac = x - (float)band;
    float rgb[6][3] = {
        {1, frac, 0}, {1 - frac, 1, 0}, {0, 1, frac}, {0, 1 - frac, 1}, {frac, 0, 1}, {1, 0, 1 - frac},
    };
    band = std::clamp(band, 0, 5);
    frame.win_text = true;
    frame.win_r = rgb[band][0];
    frame.win_g = rgb[band][1];
    frame.win_b = rgb[band][2];
}
