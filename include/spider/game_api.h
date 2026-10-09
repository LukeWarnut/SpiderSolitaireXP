#pragma once

#include "spider.h"
#include "game_rt.h"
#include <htmlhelp.h>

/* Shared declarations for decompiled game functions.
 *
 * The instruction bytes do not contain these names. objdiff still requires the
 * COFF symbol on our object to equal the symbol on the split object, and a
 * call's relocation must name the callee's symbol. Declare each function once,
 * here, and use that declaration from every caller. tools/sync_symbols.py
 * copies the compiled symbol into config/XPSP1/symbols.txt; names.txt is only
 * the label objdiff shows.
 *
 * Add a name by editing the declaration, listing it in config/XPSP1/names.txt,
 * rebuilding the unit, then running: python3 configure.py && ninja
 */

struct CardNode {
    int value;
    CardNode *prev;
    CardNode *link;
};

struct CardList {
    CardNode *head;
    CardNode *node_at(int index);
    CardNode *tail(int *count);
    int unlink(int index);
    int append(int *value, int *count);
    void drain();
    void *release(unsigned char flags);
};

struct CardColumn {
    int pad[10];
    int slots[10];
    int caps[10];
    int slot_empty(int index);
    int any_empty();
};

struct Vec3 {
    float x;
    float y;
    float z;
};


float __stdcall vec_length_sq(Vec3 *v);
Vec3 *__stdcall vec_normalize(Vec3 *out, Vec3 *v);
Vec3 *__stdcall vec_add(Vec3 *out, Vec3 *a, Vec3 *b);
Vec3 *__stdcall vec_scale(Vec3 *out, float s, Vec3 *v);
Vec3 *__stdcall vec_div(Vec3 *out, Vec3 *v, float s);

struct PileTable {
    CardList *piles[10];
    int counts[10];
    int card_value(int pile, int index);
    void trim_pile(int pile, int keep);
    void splice(int src, int src_i, int dst, int dst_i);
};

struct TripleTable {
    int mode;
    int decks;
    int limit;
    int *ptr;
    int idx;
    void set_suit(int i, int value);
    int suit(int i);
    int rank(int i);
    int face(int i);
    int *next();
    void shuffle(int seed);
};

struct SurfaceBlit {
    char pad0[0x10];
    int cx;
    int cy;
    char pad1[0x3980 - 0x18];
    HDC hdcSrc;
    void blit_to(HDC hdc);
};

struct LayoutInner {
    int pad[2];
    int hi;
    int pad2;
    int lo;
};

struct LayoutBox {
    HWND hwnd;
    LayoutInner *inner;
    int pad[3];
    int a;
    int b;
    int card_x(int i);
    int pile_at(int x);
    void fill_rect(RECT *out);
    void draw_stock(void *hdc, int dx, int dy);
};

struct LayoutWin {
    HWND hwnd;
    int deal_top();
    void center_rect(RECT *rc);
};

struct ClearedSets {
    char pad[0xF10];
    int vals[4];
    int total();
};

extern "C" HINSTANCE g_hinst;
extern "C" wchar_t g_strbuf[0x400];
extern "C" wchar_t *__stdcall load_string(UINT id);
extern "C" int __stdcall alert_box(HWND hwnd, UINT textId, UINT capId, UINT type);

struct TenListBoard {
    CardList *lists[10];
    int counts[10];
    int extras[10];
    TenListBoard();
    void clear_lists();
    void destroy_lists();
    void *release(unsigned char flags);
    void add_card(int pile, int value, int hidden);
};

struct AttrTable {
    int tag;
    int count;
    int span;
    void *p;
    int zero;
    AttrTable(int n);
    void clear_buf();
    void *release(unsigned char flags);
};

struct GameBoard {
    int pad;
    AttrTable *attrs;
    TenListBoard *board;
    char pad0c[0x14 - 0x0C];
    int a;
    int b;
    int pad1c;
    int f20;
    int f24;
    int drag_pile;
    int drag_card;
    int extras[10];
    int f58;
    char pad5c[0x350 - 0x5C];
    int in_play;
    char pad354[0xF10 - 0x354];
    int z[4];
    char padf20[0xF5C - 0xF20];
    int suit_lock;
    char padf60[0xF78 - 0xF60];
    int f78;
    int f7c;
    char padf80[0xFD4 - 0xF80];
    int drag_a;
    int drag_b;
    int drag_c;
    int drag_d;
    HGDIOBJ bmp_a;
    HGDIOBJ bmp_b;
    HGDIOBJ bmp_c;
    HGDIOBJ old_a;
    HGDIOBJ old_b;
    HGDIOBJ old_c;
    RECT drag_rc;
    RECT drag_home;
    int drag_x1;
    int drag_y1;
    int origin_x;
    int origin_y;
    int lift_pile;
    int lift_card;
    int hwnd;
    GameBoard();
    void drop_attrs();
    void drop_board();
    void reset_attrs();
    void reset_board();
    ~GameBoard();
    int on_mouse_move(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    int on_right_down(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    int on_right_up(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    void paint_board(HDC hdc, int dx, int dy);
    void draw_card(void *hdc, int x, int y, int h, int a, int b);
    void refresh_drag(int a, int b, int c, int d, RECT *old_rc, RECT *new_rc);
    void slide_drag(HDC hdc0, HDC hdc1, HDC hdc2, HDC hdc3, RECT *src, RECT *dst, int hold);
    void paint_column(HDC hdc, int pile, int card);
    void draw_felt(HDC hdc, int x0, int y0, RECT *clip, RECT *rc);
    void begin_drag(int pile, int card);
    int on_left_down(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    int on_left_up(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
};

struct DealBox {
    HWND hwnd;
    char pad[0x14];
    int field18;
    void fill(RECT *out);
};

struct DealView {
    int pad;
    TripleTable *triples;
    void *col;
    int suit_of(int pile, int card);
    int rank_of(int pile, int card);
    int face_of(int pile, int card);
    int rank_kind(int p0, int c0, int p1, int c1);
    int card_code(int pile, int index);
    int run_ok(int pile, int card);
    int full_suit(int pile);
    int can_drop(int src_pile, int src_card, int dst_pile);
};

struct FxItem {
    float x;
    float y;
    float z;
    Vec3 vel0;
    Vec3 vel;
    Vec3 acc;
    float r;
    float g;
    float b;
    float pad3c;
    float life;
    float scale;
};

struct FxBank {
    FxItem items[100];
    float stamp;
    float gx;
    float gy;
    float gz;
    int ready;
};

struct AnimState {
    HWND hwnd;
    int b;
    RECT dirty;
    RECT rc18;
    RECT rc28;
    wchar_t win_text[100];
    HGDIOBJ gdi100;
    HGDIOBJ gdi104;
    FxBank fx[2];
    DWORD t0;
    HDC hdcA;
    HGDIOBJ bmpA;
    HGDIOBJ oldA;
    HDC hdcB;
    HGDIOBJ bmpB;
    HGDIOBJ oldB;
    void clear_draw();
    void reset_draw();
    void stop();
    void shutdown();
    void *release(unsigned char flags);
    AnimState *init_draw(HWND hwnd);
    void tick_fx(FxBank *bank);
    void step_fx(FxItem *item);
    void burst_fx(FxBank *bank);
    void prep_blit();
    void run_fx(FxBank *bank);
    void paint();
};

struct Quad4 {
    int a;
    int b;
    int c;
    int d;
};

struct BlitBoard {
    char pad0[8];
    Quad4 src;
    char pad1[0x28 - 0x18];
    Quad4 dst;
    char pad2[0x3974 - 0x38];
    HDC hdcSrc;
    char pad3[0x3980 - 0x3978];
    HDC hdcDst;
    void blit();
};

struct UndoRec {
    char pad[24];
};

struct UndoRow {
    int v[5];
};

struct UndoBuf {
    char pad[0x354];
    UndoRow rows[150];
    int count;
    void pop_row();
};

struct UndoRing {
    char pad[0x60];
    int limit;
    int idx;
    UndoRec items[1];
    UndoRec *next();
};

struct GameWin {
    HWND hwnd;
    int *mode;
    TenListBoard *board;
    AnimState *anim;
    int seed;
    int a;
    int b;
    int level;
    char pad0b_a[0x58 - 0x20];
    int f58;
    int f5c;
    int f60;
    int f64;
    char pad0b_b[0x350 - 0x68];
    int in_play;
    int undo[0x96][5];
    int undo_n;
    int z[4];
    int z2[8];
    int show;
    RECT wrect;
    int saved;
    int suit_src;
    int suit_lock;
    int use_fx;
    int opt_f64;
    int opt_f68;
    int opt_f6c;
    int opt_f70;
    int play_snd;
    int iconic;
    int active;
    int suit_slot[3][7];
    void disable_undo_menu();
    void new_game(int seed);
    void seed_now();
    void show_dialog_107();
    void show_dialog_118();
    void show_dialog_117();
    void reset_suit_slots();
    int push_undo(int *rec);
    HANDLE open_saved(DWORD access, DWORD creation);
    void add_score(int delta);
    void pop_undo();
    void check_saved();
    void deal_prompt();
    void tally(int arg);
    void won_game();
    void paint_hdc(HDC hdc);
    void draw_cleared(void *hdc, int dx, int dy);
    void blink_move();
    void collect_moves();
    void anim_deal(int pile, int card);
    void take_suit(int pile);
    void load_game();
    int save_game();
    void save_settings();
    void load_settings();
    void move_run(int src_pile, int src_card, int dst_pile, int record, int flip);
    void deal();
    LRESULT on_command(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
};

/* One object at gold 0x01010FC8: built as GameBoard, used as GameWin by the window code. */
extern "C" GameBoard g_board;
#define g_game (*(GameWin *)&g_board)

extern "C" INT_PTR CALLBACK fn_01002E68(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
extern "C" INT_PTR CALLBACK fn_010070D0(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
extern "C" INT_PTR CALLBACK fn_010075D4(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
extern "C" INT_PTR CALLBACK fn_010073B6(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
extern "C" INT_PTR CALLBACK fn_010076A9(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
extern "C" void __stdcall fn_010071F3(HWND hwnd, GameWin *g);

struct ColumnInner {
    int pad[10];
    int slots[10];
    int caps[10];
};

struct Layout {
    int pad[2];
    ColumnInner *inner;
    char pad2[0x24];
    int extras[10];
    int score(int pile, int n);
};

struct ColumnOp {
    HWND hwnd;
    int pad1;
    ColumnInner *inner;
    int pad2[2];
    int a;
    int b;
    int pad3[5];
    int extras[10];
    void place(int a, int b, int c, int d);
    void place_on_top(int a, int b, int c);
    int hit_card(int pile, int y);
    int fit_piles();
};

struct Rec24 {
    int a;
    int b;
    int c;
    int d;
    int e;
    int f;
};

struct RecBank {
    char pad[0x60];
    int count;
    int pad2;
    Rec24 items[0x1f];
    int push(Rec24 *rec);
    void sort_desc();
};

struct StrideCall {
    void apply(char *p, int stride, int count, void (__fastcall *fn)(void *));
};

void *__fastcall stride_keep(void *p);
void *__fastcall stride_fill(void *p);

extern "C" LRESULT __stdcall wnd_proc_thunk(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
extern "C" void spider_boot(void);

extern "C" wchar_t g_strBuf[0x400];
