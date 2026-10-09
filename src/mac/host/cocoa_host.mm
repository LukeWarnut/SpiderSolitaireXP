#include "cocoa_host.h"

#include "audio.h"
#include "game.h"

#import <AppKit/AppKit.h>

namespace {

const char *kLevelNames[3] = {"Easy", "Medium", "Difficult"};

Uint32 g_command_event = 0;
NSAlert *g_won_alert = nil;
NSImage *g_icon = nil;

void post_command(int id) {
    SDL_Event e;
    SDL_zero(e);
    e.type = CocoaHost::command_event();
    e.user.code = id;
    SDL_PushEvent(&e);
}

NSWindow *cocoa_window(SDL_Window *window) {
    return (__bridge NSWindow *)SDL_GetPointerProperty(SDL_GetWindowProperties(window),
                                                       SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr);
}

NSAlert *make_alert(NSString *title, NSString *text) {
    NSAlert *alert = [NSAlert new];
    alert.messageText = title;
    alert.informativeText = text ?: @"";
    if (g_icon != nil) {
        alert.icon = g_icon;
    }
    return alert;
}

NSModalResponse run_alert(SDL_Window *window, NSAlert *alert) {
    SDL_CaptureMouse(false);
    NSModalResponse r = [alert runModal];
    [cocoa_window(window) makeKeyAndOrderFront:nil];
    return r;
}

NSButton *check_box(NSString *title, NSRect frame, bool on) {
    NSButton *b = [NSButton checkboxWithTitle:title target:nil action:nil];
    b.frame = frame;
    b.state = on ? NSControlStateValueOn : NSControlStateValueOff;
    return b;
}

NSTextField *label(NSString *text, NSRect frame) {
    NSTextField *t = [NSTextField labelWithString:text];
    t.frame = frame;
    return t;
}

}  // namespace

/* Menu actions. Plain-key equivalents (D, M, F1-F5) are also delivered to SDL,
 * which handles them; picks from the menu itself or with Command go here. */
@interface SpiderMenuTarget : NSObject
- (void)pick:(NSMenuItem *)item;
- (void)radio:(id)sender;
@end

@implementation SpiderMenuTarget
- (void)pick:(NSMenuItem *)item {
    NSEvent *ev = [NSApp currentEvent];
    if (ev.type == NSEventTypeKeyDown && (ev.modifierFlags & NSEventModifierFlagCommand) == 0) {
        return;
    }
    post_command((int)item.tag);
}
- (void)radio:(id)sender {
    (void)sender;
}
@end

/* Statistics dialog body (dlg_stats / fill_stats): a level picker and the
 * seven counters for that level. */
@interface SpiderStatsView : NSView
@property(nonatomic) Settings *settings;
@property(nonatomic, strong) NSSegmentedControl *levels;
@property(nonatomic, strong) NSMutableArray<NSTextField *> *values;
- (void)refresh;
@end

@implementation SpiderStatsView
- (instancetype)initWithSettings:(Settings *)settings level:(int)level {
    self = [super initWithFrame:NSMakeRect(0, 0, 300, 230)];
    _settings = settings;
    _levels = [NSSegmentedControl segmentedControlWithLabels:@[ @"Easy", @"Medium", @"Difficult" ]
                                                trackingMode:NSSegmentSwitchTrackingSelectOne
                                                      target:self
                                                      action:@selector(changed:)];
    _levels.frame = NSMakeRect(20, 196, 260, 26);
    _levels.selectedSegment = level;
    [self addSubview:_levels];
    NSArray *names = @[ @"High Score:", @"Wins:", @"Losses:", @"Win Rate:", @"Most Wins:", @"Most Losses:",
                        @"Current:" ];
    _values = [NSMutableArray new];
    for (NSUInteger i = 0; i < names.count; i++) {
        CGFloat y = 162 - (CGFloat)i * 24;
        [self addSubview:label(names[i], NSMakeRect(30, y, 120, 18))];
        NSTextField *v = label(@"", NSMakeRect(150, y, 130, 18));
        [_values addObject:v];
        [self addSubview:v];
    }
    [self refresh];
    return self;
}
- (void)changed:(id)sender {
    (void)sender;
    [self refresh];
}
- (void)refresh {
    const LevelStats &s = _settings->stats[_levels.selectedSegment];
    int pct = 0;
    if (s.wins > 0) {
        pct = (int)((double)s.wins / (double)(s.losses + s.wins) * 100.0);
    }
    _values[0].stringValue = [NSString stringWithFormat:@"%d", s.high];
    _values[1].stringValue = [NSString stringWithFormat:@"%d", s.wins];
    _values[2].stringValue = [NSString stringWithFormat:@"%d", s.losses];
    _values[3].stringValue = [NSString stringWithFormat:@"%d %%", pct];
    _values[4].stringValue = [NSString stringWithFormat:@"%d", s.streak_wins];
    _values[5].stringValue = [NSString stringWithFormat:@"%d", s.streak_losses];
    _values[6].stringValue = [NSString stringWithFormat:s.streak_is_win ? @"%d Wins" : @"%d Losses",
                                                        s.streak_current];
}
@end

static SpiderMenuTarget *g_target = nil;
static NSMenuItem *g_items[6];

CocoaHost::CocoaHost(SDL_Window *w, const std::string &asset_dir) : window(w), assets(asset_dir) {
    NSString *icon = [NSString stringWithUTF8String:(assets + "/icons/103.ico").c_str()];
    g_icon = [[NSImage alloc] initWithContentsOfFile:icon];
    if (g_icon != nil) {
        NSApp.applicationIconImage = g_icon;
    } else {
        SDL_Log("icon: cannot load %s", icon.UTF8String);
    }
}

Uint32 CocoaHost::command_event() {
    if (g_command_event == 0) {
        g_command_event = SDL_RegisterEvents(1);
    }
    return g_command_event;
}

void CocoaHost::install_menu() {
    g_target = [SpiderMenuTarget new];
    NSMenu *bar = [NSMenu new];

    auto add = [](NSMenu *menu, NSString *title, int cmd, NSString *key, NSEventModifierFlags mods) {
        NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:title action:@selector(pick:) keyEquivalent:key ?: @""];
        item.keyEquivalentModifierMask = mods;
        item.target = g_target;
        item.tag = cmd;
        [menu addItem:item];
        return item;
    };
    auto fkey = [](unichar k) { return [NSString stringWithCharacters:&k length:1]; };
    auto submenu = [&](NSString *title) {
        NSMenuItem *top = [NSMenuItem new];
        NSMenu *menu = [[NSMenu alloc] initWithTitle:title];
        menu.autoenablesItems = NO;
        top.submenu = menu;
        [bar addItem:top];
        return menu;
    };
    const NSEventModifierFlags cmd = NSEventModifierFlagCommand;

    NSMenu *app = submenu(@"Spider");
    add(app, @"About Spider", CMD_ABOUT, nil, 0);
    [app addItem:[NSMenuItem separatorItem]];
    [app addItemWithTitle:@"Hide Spider" action:@selector(hide:) keyEquivalent:@"h"];
    NSMenuItem *others = [app addItemWithTitle:@"Hide Others"
                                        action:@selector(hideOtherApplications:)
                                 keyEquivalent:@"h"];
    others.keyEquivalentModifierMask = cmd | NSEventModifierFlagOption;
    [app addItemWithTitle:@"Show All" action:@selector(unhideAllApplications:) keyEquivalent:@""];
    [app addItem:[NSMenuItem separatorItem]];
    add(app, @"Quit Spider", CMD_EXIT, @"q", cmd);

    NSMenu *game = submenu(@"Game");
    add(game, @"New Game", CMD_NEW, fkey(NSF2FunctionKey), 0);
    add(game, @"New Game", CMD_NEW, @"n", cmd);
    g_items[0] = add(game, @"Restart This Game", CMD_RESTART, nil, 0);
    [game addItem:[NSMenuItem separatorItem]];
    g_items[1] = add(game, @"Undo", CMD_UNDO, @"z", cmd);
    g_items[2] = add(game, @"Deal Next Row", CMD_DEAL, @"d", 0);
    g_items[3] = add(game, @"Show An Available Move", CMD_HINT, @"m", 0);
    [game addItem:[NSMenuItem separatorItem]];
    add(game, @"Difficulty…", CMD_DIFFICULTY, fkey(NSF3FunctionKey), 0);
    add(game, @"Statistics…", CMD_STATS, fkey(NSF4FunctionKey), 0);
    add(game, @"Options…", CMD_OPTIONS, fkey(NSF5FunctionKey), 0);
    [game addItem:[NSMenuItem separatorItem]];
    g_items[4] = add(game, @"Save This Game", CMD_SAVE, @"s", cmd);
    g_items[5] = add(game, @"Open Last Saved Game", CMD_OPEN, @"o", cmd);
    /* Cmd-N duplicates F2; keep one visible entry. */
    [game itemAtIndex:1].hidden = YES;
    [game itemAtIndex:1].allowsKeyEquivalentWhenHidden = YES;

    NSMenu *win = submenu(@"Window");
    [win addItemWithTitle:@"Minimize" action:@selector(performMiniaturize:) keyEquivalent:@"m"];
    [win addItemWithTitle:@"Zoom" action:@selector(performZoom:) keyEquivalent:@""];
    NSApp.windowsMenu = win;

    NSMenu *help = submenu(@"Help");
    add(help, @"Spider Help", CMD_HELP, fkey(NSF1FunctionKey), 0);
    NSApp.helpMenu = help;

    NSApp.mainMenu = bar;
    [NSApp activateIgnoringOtherApps:YES];
}

void CocoaHost::dismiss_sheet() {
    if (g_won_alert != nil) {
        NSWindow *w = cocoa_window(window);
        [w endSheet:g_won_alert.window returnCode:NSAlertSecondButtonReturn];
        g_won_alert = nil;
    }
}

uint32_t CocoaHost::ticks() const { return (uint32_t)SDL_GetTicks(); }

void CocoaHost::play(int sound_id) { audio_play(sound_id); }

int CocoaHost::confirm(const char *text, int kind) {
    NSAlert *alert = make_alert(@"Spider", @(text));
    if (kind == CONFIRM_OK) {
        [alert addButtonWithTitle:@"OK"];
        run_alert(window, alert);
        return ANSWER_OK;
    }
    [alert addButtonWithTitle:@"Yes"];
    [alert addButtonWithTitle:@"No"];
    if (kind == CONFIRM_YESNOCANCEL) {
        [alert addButtonWithTitle:@"Cancel"];
    }
    NSModalResponse r = run_alert(window, alert);
    if (r == NSAlertFirstButtonReturn) {
        return ANSWER_YES;
    }
    if (r == NSAlertSecondButtonReturn) {
        return ANSWER_NO;
    }
    return ANSWER_CANCEL;
}

void CocoaHost::message(const char *text) { confirm(text, CONFIRM_OK); }

/* Dialog 119 (dlg_deal_level). */
bool CocoaHost::difficulty(int *mode) {
    NSAlert *alert = make_alert(@"Difficulty", @"Select the game difficulty level that you want:");
    [alert addButtonWithTitle:@"OK"];
    [alert addButtonWithTitle:@"Cancel"];
    NSView *box = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 240, 78)];
    NSArray *titles = @[ @"Easy: One Suit", @"Medium: Two Suits", @"Difficult: Four Suits" ];
    const int modes[3] = {1, 2, 4};
    NSMutableArray<NSButton *> *radios = [NSMutableArray new];
    for (int i = 0; i < 3; i++) {
        NSButton *b = [NSButton radioButtonWithTitle:titles[i] target:g_target action:@selector(radio:)];
        b.frame = NSMakeRect(0, 54 - i * 26, 240, 20);
        b.state = modes[i] == *mode ? NSControlStateValueOn : NSControlStateValueOff;
        [radios addObject:b];
        [box addSubview:b];
    }
    alert.accessoryView = box;
    if (run_alert(window, alert) != NSAlertFirstButtonReturn) {
        return false;
    }
    for (int i = 0; i < 3; i++) {
        if (radios[i].state == NSControlStateValueOn) {
            *mode = modes[i];
        }
    }
    return true;
}

/* Dialog 130 (dlg_timer): stays up while the fireworks run. */
void CocoaHost::won_prompt() {
    dismiss_sheet();
    NSAlert *alert = make_alert(@"Congratulations, you won!", @"Do you want to start another game?");
    [alert addButtonWithTitle:@"Yes"];
    [alert addButtonWithTitle:@"No"];
    g_won_alert = alert;
    [alert beginSheetModalForWindow:cocoa_window(window)
                  completionHandler:^(NSModalResponse r) {
                    if (g_won_alert == alert) {
                        g_won_alert = nil;
                        if (r == NSAlertFirstButtonReturn) {
                            post_command(CMD_NEW);
                        }
                    }
                  }];
}

/* Dialog 107 (dlg_about): bitmap 106 and the copyright string. */
void CocoaHost::about() {
    NSAlert *alert = make_alert(@"About Spider", @"\u00a9 1998-2000 Microsoft Corporation.\nAll rights reserved.");
    [alert addButtonWithTitle:@"OK"];
    NSString *path = [NSString stringWithUTF8String:(assets + "/bitmaps/106.bmp").c_str()];
    NSImage *art = [[NSImage alloc] initWithContentsOfFile:path];
    if (art != nil) {
        NSImageView *view = [NSImageView imageViewWithImage:art];
        view.frame = NSMakeRect(0, 0, art.size.width, art.size.height);
        alert.accessoryView = view;
    }
    run_alert(window, alert);
}

/* Dialog 118 (dlg_stats). */
void CocoaHost::stats(Game *game) {
    for (;;) {
        NSAlert *alert = make_alert(@"Spider Statistics", nil);
        [alert addButtonWithTitle:@"OK"];
        [alert addButtonWithTitle:@"Reset"];
        int level = game->settings.difficulty == 1 ? 0 : game->settings.difficulty == 2 ? 1 : 2;
        SpiderStatsView *view = [[SpiderStatsView alloc] initWithSettings:&game->settings level:level];
        alert.accessoryView = view;
        if (run_alert(window, alert) != NSAlertSecondButtonReturn) {
            return;
        }
        if (confirm("Are you sure you want to reset all game statistics?", CONFIRM_YESNO) == ANSWER_YES) {
            game->reset_stats();
        }
    }
}

/* Dialog 117 (dlg_options). */
void CocoaHost::options(Game *game) {
    Settings &s = game->settings;
    NSAlert *alert = make_alert(@"Spider Options", nil);
    [alert addButtonWithTitle:@"OK"];
    [alert addButtonWithTitle:@"Cancel"];
    NSView *box = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 320, 150)];
    NSArray *titles = @[
        @"Animate when dealing cards", @"Automatically save game on exit",
        @"Automatically open previous game at startup", @"Prompt before saving a game",
        @"Prompt before opening a saved game", @"Use sound effects"
    ];
    int *fields[6] = {&s.animate, &s.save_on_exit, &s.load_at_start, &s.prompt_save, &s.prompt_load, &s.sound};
    NSButton *boxes[6];
    for (int i = 0; i < 6; i++) {
        boxes[i] = check_box(titles[i], NSMakeRect(0, 125 - i * 25, 320, 20), *fields[i] != 0);
        [box addSubview:boxes[i]];
    }
    alert.accessoryView = box;
    if (run_alert(window, alert) != NSAlertFirstButtonReturn) {
        return;
    }
    for (int i = 0; i < 6; i++) {
        *fields[i] = boxes[i].state == NSControlStateValueOn ? 1 : 0;
    }
}

/* spider.chm is not shipped; a short summary stands in for HtmlHelp. */
void CocoaHost::help() {
    NSAlert *alert = make_alert(
        @"Spider Help",
        @"Remove all the cards from the ten stacks by building runs from king down to ace in one suit. "
        @"A completed run leaves the table.\n\n"
        @"Drag a card, or a run of one suit in descending order, onto a card one rank higher, or onto an empty "
        @"stack. Click the stock (lower right) to deal a new row; every stack must have a card first. Click the "
        @"score box or press M for a hint. Right-click a face-up card to see it in full.\n\n"
        @"You start with 500 points, lose one per move, and gain 100 per completed run.");
    [alert addButtonWithTitle:@"OK"];
    run_alert(window, alert);
}

void CocoaHost::sync_menu(const MenuState &m) {
    if (g_items[0] == nil) {
        return;
    }
    g_items[0].enabled = m.restart;
    g_items[1].enabled = m.undo;
    g_items[2].enabled = m.deal;
    g_items[3].enabled = m.hint;
    g_items[4].enabled = m.save;
    g_items[5].enabled = m.open;
}

/* Registry values from load_settings, kept in an NSUserDefaults plist. */
void CocoaHost::load_settings(Settings &s) {
    s = Settings();
    NSUserDefaults *d = [NSUserDefaults standardUserDefaults];
    auto get = [&](NSString *key, int &out) {
        if ([d objectForKey:key] != nil) {
            out = (int)[d integerForKey:key];
        }
    };
    get(@"WndState", s.maximized);
    get(@"WndX", s.window_x);
    get(@"WndY", s.window_y);
    get(@"WndWidth", s.window_w);
    get(@"WndHeight", s.window_h);
    get(@"AnimDeal", s.animate);
    get(@"SaveOnExit", s.save_on_exit);
    get(@"LoadAtStart", s.load_at_start);
    get(@"PromptSave", s.prompt_save);
    get(@"PromptLoad", s.prompt_load);
    get(@"Sound", s.sound);
    get(@"NumSuits", s.difficulty);
    for (int i = 0; i < 3; i++) {
        NSString *lv = @(kLevelNames[i]);
        LevelStats &row = s.stats[i];
        get([@"HighScore_" stringByAppendingString:lv], row.high);
        get([@"Wins_" stringByAppendingString:lv], row.wins);
        get([@"Losses_" stringByAppendingString:lv], row.losses);
        get([@"StreakWins_" stringByAppendingString:lv], row.streak_wins);
        get([@"StreakLosses_" stringByAppendingString:lv], row.streak_losses);
        get([@"StreakCurrent_" stringByAppendingString:lv], row.streak_current);
        get([@"FWinStreak_" stringByAppendingString:lv], row.streak_is_win);
    }
}

void CocoaHost::save_settings(const Settings &s) {
    NSUserDefaults *d = [NSUserDefaults standardUserDefaults];
    auto set = [&](NSString *key, int v) { [d setInteger:v forKey:key]; };
    set(@"WndState", s.maximized);
    set(@"WndX", s.window_x);
    set(@"WndY", s.window_y);
    set(@"WndWidth", s.window_w);
    set(@"WndHeight", s.window_h);
    set(@"AnimDeal", s.animate);
    set(@"SaveOnExit", s.save_on_exit);
    set(@"LoadAtStart", s.load_at_start);
    set(@"PromptSave", s.prompt_save);
    set(@"PromptLoad", s.prompt_load);
    set(@"Sound", s.sound);
    set(@"NumSuits", s.difficulty);
    for (int i = 0; i < 3; i++) {
        NSString *lv = @(kLevelNames[i]);
        const LevelStats &row = s.stats[i];
        set([@"HighScore_" stringByAppendingString:lv], row.high);
        set([@"Wins_" stringByAppendingString:lv], row.wins);
        set([@"Losses_" stringByAppendingString:lv], row.losses);
        set([@"StreakWins_" stringByAppendingString:lv], row.streak_wins);
        set([@"StreakLosses_" stringByAppendingString:lv], row.streak_losses);
        set([@"StreakCurrent_" stringByAppendingString:lv], row.streak_current);
        set([@"FWinStreak_" stringByAppendingString:lv], row.streak_is_win);
    }
}

/* open_saved wrote spider.sav to My Documents (CSIDL_PERSONAL); here it
 * lives in Application Support. */
std::string CocoaHost::save_path() {
    NSURL *base = [[NSFileManager defaultManager] URLsForDirectory:NSApplicationSupportDirectory
                                                         inDomains:NSUserDomainMask]
                      .firstObject;
    NSURL *dir = [base URLByAppendingPathComponent:@"Spider" isDirectory:YES];
    [[NSFileManager defaultManager] createDirectoryAtURL:dir withIntermediateDirectories:YES attributes:nil error:nil];
    return [dir URLByAppendingPathComponent:@"spider.sav"].path.UTF8String;
}

bool CocoaHost::save_exists() {
    return [[NSFileManager defaultManager] fileExistsAtPath:@(save_path().c_str())];
}
