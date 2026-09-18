// Opt-in simulator integration tests, never included in a device build.
#import <UIKit/UIKit.h>
#include "context.h"
#include "game.h"
#include "topic_panel.h"
#include "zu4_ios_ui.h"
#include "journal_notebook.h"
#include "save_snapshot.h"
#include "mobile_layout.h"
#include <cstdio>
#include <cstdlib>
extern int gameSave();

@interface Zu4JournalPanel : UIView
@property(nonatomic, strong) UIView *card;
@property(nonatomic, strong) UIStackView *modeControl;
@property(nonatomic, assign) NSInteger journalMode;
@property(nonatomic, strong) NSArray<UIButton *> *modeButtons;
@property(nonatomic, strong) UISearchBar *searchBar;
@property(nonatomic, strong) UITableView *tableView;
@property(nonatomic, strong) UIButton *addNoteButton;
@property(nonatomic, strong) NSArray *groups;
@property(nonatomic, strong) NSArray *noteRows;
@property(nonatomic, strong) UITextView *noteText;
@property(nonatomic, strong) UIView *editor;
@property(nonatomic, strong) UIScrollView *detailScroll;
@property(nonatomic, strong) UIStackView *detailStack;
@property(nonatomic, assign) Zu4JournalAccess access;
- (void)changeMode;
- (NSArray *)listItems;
- (NSArray *)visibleListItems;
- (NSDictionary *)sourceForPassage:(int)index;
- (void)showSource:(NSDictionary *)source;
- (void)newNote;
- (void)editPassage:(UIButton *)button;
- (void)textViewDidChange:(UITextView *)view;
- (void)doneNote;
- (void)cancelNote;
- (void)confirmDeleteNote;
- (void)closeJournal;
- (void)searchBar:(UISearchBar *)bar textDidChange:(NSString *)query;
@end

static void check(bool good, const char *message) {
    fprintf(stderr, "%s %s\n", good ? "PASS" : "FAIL", message); fflush(stderr);
    if (!good) abort();
}
static Zu4JournalPanel *panel() {
    for (UIWindow *window in UIApplication.sharedApplication.windows)
        for (UIView *view in window.subviews) if ([view isKindOfClass:Zu4JournalPanel.class]) return (Zu4JournalPanel *)view;
    return nil;
}
static uint64_t failSave(int, uint64_t, const char *) { return 0; }
static UIView *identifiedView(UIView *root, NSString *identifier) {
    if ([root.accessibilityIdentifier isEqualToString:identifier]) return root;
    for (UIView *child in root.subviews) { UIView *match = identifiedView(child, identifier); if (match) return match; }
    return nil;
}
static UIButton *passageIcon(Zu4JournalPanel *view, NSString *kind, int index) {
    return (UIButton *)identifiedView(view.detailStack, [NSString stringWithFormat:@"journal-%@-%d", kind, index]);
}
static void checkModeTargets(Zu4JournalPanel *view, int passage, int step) {
    if (step == 0) check(zu4_mobile_journal_toggle_favorite(passage), "prepare empty Current clues regression");
    if (step == 9) check(zu4_mobile_journal_toggle_favorite(passage), "prepare populated Current clues regression");
    if (step == 18) {
        [view closeJournal];
        check(!zu4_journal_panel_is_visible(), "mode-button regression returns to main controls");
        fprintf(stderr, "JOURNAL BUTTON TARGETS COMPLETE\n"); fflush(stderr);
        zu4_mobile_journal();
        [panel() showSource:[panel() sourceForPassage:passage]];
        return;
    }
    NSInteger mode = step % 3;
    [view.modeButtons[mode] sendActionsForControlEvents:UIControlEventTouchUpInside];
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 20 * NSEC_PER_MSEC), dispatch_get_main_queue(), ^{
        check(view.journalMode == mode, "native mode-button target switches mode after dispatch");
        [view.window layoutIfNeeded];
        check([view listItems].count == (mode == 1 ? (step < 9 ? 0 : 1) : (mode == 2 ? 2 : [view listItems].count)),
              "repeated mode transitions keep empty/populated clue and note lists correct");
        checkModeTargets(view, passage, step + 1);
    });
}
void zu4_run_journal_runtime_tests() {
    check([NSBundle.mainBundle.bundleIdentifier isEqualToString:@"org.ultimatumproject.tests.journal"], "isolated journal simulator bundle");
    game->paused = true;
    unsigned moves = c->saveGame->moves; int food = c->saveGame->food;
    if (getenv("ZU4_JOURNAL_RELOAD")) {
        check(zu4_mobile_journal_note_count() == 2, "notes survive actual engine relaunch");
        zu4_mobile_journal();
        Zu4JournalPanel *view = panel();
        view.journalMode = 1; [view changeMode];
        check([view listItems].count == 1, "bookmark survives actual engine relaunch");
        view.journalMode = 2; [view changeMode];
        check([view listItems].count == 2, "reloaded native My notes contains attached and standalone notes");
        fprintf(stderr, "JOURNAL RELOAD COMPLETE\n"); fflush(stderr); return;
    }
    // Reset only this opt-in disposable bundle, making reruns deterministic.
    zu4_mobile_journal();
    while (zu4_mobile_journal_note_count()) {
        Zu4JournalNote note;
        check(zu4_mobile_journal_note_at(0, &note) && zu4_mobile_journal_delete_note(note.identifier), "reset isolated notebook fixture");
    }
    for (NSDictionary *group in panel().groups) for (NSDictionary *entry in group[@"entries"])
        if (zu4_mobile_journal_favorite([entry[@"index"] intValue])) zu4_mobile_journal_toggle_favorite([entry[@"index"] intValue]);
    [panel() closeJournal];
    gameRecordRevealedText("Runtime first recorded passage.", "Fixture in Britain", "Fixture", "Britain", "person", "Introduction");
    gameRecordRevealedText("Runtime second recorded passage.", "Fixture in Britain", "Fixture", "Britain", "person", "job");
    zu4_mobile_journal();
    Zu4JournalPanel *view = panel();
    check(view && zu4_journal_panel_is_visible(), "real engine opens native journal");
    int windCounter = c->windCounter; game->paused = false;
    game->timerFired();
    check(c->windCounter == windCounter && c->saveGame->moves == moves, "native journal freezes actual world timer even without global pause");
    game->paused = true;
    [view.window layoutIfNeeded]; [view layoutIfNeeded]; [view.card layoutIfNeeded];
    check(view.journalMode == 0, "Places is default");
    CGRect safe = UIEdgeInsetsInsetRect(view.bounds, view.safeAreaInsets);
    check(CGRectEqualToRect(view.card.frame, CGRectInset(safe, 8, 8)), "journal fills safe-area screen in portrait and landscape");
    check(view.modeControl.bounds.size.height >= 44, "journal mode targets are at least 44 points");
    int first = -1, second = -1;
    for (NSDictionary *group in view.groups) for (NSDictionary *entry in group[@"entries"]) {
        if ([entry[@"text"] isEqualToString:@"Runtime first recorded passage."]) first = [entry[@"index"] intValue];
        if ([entry[@"text"] isEqualToString:@"Runtime second recorded passage."]) second = [entry[@"index"] intValue];
    }
    check(first >= 0 && second > first, "native passage indices map to actual recorded history");
    check(zu4_mobile_journal_toggle_favorite(second), "bookmark commits through real engine backend");
    uint64_t attached = zu4_mobile_journal_save_note(second, 0, "Personal nebula theory\n日本語 café");
    check(attached && zu4_mobile_journal_note_count() == 1, "attached Unicode multiline note commits immediately");
    view.journalMode = 1; [view changeMode];
    check([view listItems].count == 1, "Current clues contains only bookmarked passage");
    [view showSource:[view listItems][0]];
    check(view.detailStack.arrangedSubviews.count >= 2, "bookmark opens original continuous transcript");
    [view.card layoutIfNeeded];
    UIButton *bookmarkIcon = passageIcon(view, @"bookmark", second);
    UIButton *noteIcon = passageIcon(view, @"note", second);
    check(bookmarkIcon.currentImage && noteIcon.currentImage && !bookmarkIcon.currentTitle.length && !noteIcon.currentTitle.length,
          "passage actions are native SF Symbol icons without large text buttons");
    check(bookmarkIcon.bounds.size.width == 44 && bookmarkIcon.bounds.size.height == 44 &&
          noteIcon.bounds.size.width == 44 && noteIcon.bounds.size.height == 44, "both passage icons retain 44-point tap targets");
    check([bookmarkIcon.accessibilityValue isEqualToString:@"Bookmarked"] &&
          [noteIcon.accessibilityValue isEqualToString:@"Note attached"] && identifiedView(noteIcon, @"journal-note-attached"),
          "bookmarked and attached-note states are visible and accessibility labeled");
    [bookmarkIcon sendActionsForControlEvents:UIControlEventTouchUpInside];
    check(!zu4_mobile_journal_favorite(second) && zu4_mobile_journal_attached_note(second) == attached,
          "bookmark icon target removes bookmark without deleting attached note");
    bookmarkIcon = passageIcon(view, @"bookmark", second);
    check([bookmarkIcon.accessibilityValue isEqualToString:@"Not bookmarked"], "unbookmarked icon updates state");
    [bookmarkIcon sendActionsForControlEvents:UIControlEventTouchUpInside];
    check(zu4_mobile_journal_favorite(second), "bookmark icon target restores bookmark");
    noteIcon = passageIcon(view, @"note", second);
    [noteIcon sendActionsForControlEvents:UIControlEventTouchUpInside];
    check(view.editor && [view.noteText.text containsString:@"nebula"], "note icon target opens existing personal-note editor");
    [view cancelNote];
    check(!view.editor && zu4_mobile_journal_attached_note(second) == attached, "note icon editor Cancel preserves saved note");
    UIButton *emptyNoteIcon = passageIcon(view, @"note", first);
    check([emptyNoteIcon.accessibilityValue isEqualToString:@"No attached note"] && !identifiedView(emptyNoteIcon, @"journal-note-attached"),
          "passage without note has no attached-note indicator");
    [emptyNoteIcon sendActionsForControlEvents:UIControlEventTouchUpInside];
    check(view.editor && !view.noteText.text.length, "add-note icon target opens empty draft");
    [view cancelNote];
    [view newNote]; view.noteText.text = @"Unsaved cancel draft"; [view cancelNote];
    check(zu4_mobile_journal_note_count() == 1 && !view.editor, "Cancel discards draft and returns to journal");
    Zu4JournalAccess access = view.access, failing = access; failing.saveNote = failSave;
    view.access = failing; [view newNote]; view.noteText.text = @"Keep failed draft"; [view doneNote];
    check(view.editor && [view.noteText.text isEqualToString:@"Keep failed draft"] && zu4_mobile_journal_note_count() == 1,
          "save failure retains editor draft and leaves stored notes unchanged");
    [view cancelNote]; view.access = access;
    view.journalMode = 2; [view changeMode];
    check(!view.addNoteButton.hidden && view.addNoteButton.bounds.size.height >= 44, "My notes offers reachable New note target");
    [view newNote]; view.noteText.text = @"Return to Britain: comet reminder"; [view textViewDidChange:view.noteText]; [view doneNote];
    check(zu4_mobile_journal_note_count() == 2 && !view.editor, "Done creates standalone personal note");
    view.searchBar.text = @"nebula"; [view changeMode];
    check([view listItems].count == 1, "search finds attached personal writing");
    view.searchBar.text = @"comet"; [view changeMode];
    check([view listItems].count == 1, "search finds standalone personal writing");
    view.searchBar.text = @"";
    check(zu4_mobile_journal_toggle_favorite(second), "unbookmark commits");
    check(zu4_mobile_journal_attached_note(second) == attached, "unbookmark preserves attached note");
    check(zu4_mobile_journal_toggle_favorite(second), "restore fixture bookmark");
    UIButton *button = [UIButton buttonWithType:UIButtonTypeSystem]; button.tag = second;
    [view editPassage:button]; view.noteText.text = @"Personal nebula theory\n日本語 café — revised"; [view textViewDidChange:view.noteText]; [view doneNote];
    check(zu4_mobile_journal_attached_note(second) == attached && zu4_mobile_journal_note_count() == 2, "native Done edits attached note without duplicating it");
    [view editPassage:button]; view.noteText.text = @"Draft must not replace note";
    [view confirmDeleteNote]; [view cancelNote]; [view cancelNote];
    check(zu4_mobile_journal_attached_note(second) == attached, "Keep note then Cancel preserves stored writing");
    uint64_t disposable = zu4_mobile_journal_save_note(-1, 0, "Disposable fixture note");
    check(disposable && zu4_mobile_journal_delete_note(disposable), "standalone note deletion persists");
    JournalNotebook persisted; TopicJournal history;
    check(history.load(gameSaveDirectory() + "topics.txt") && persisted.load(gameSaveDirectory() + "journal-notebook.dat") && persisted.validFor(history), "immediate files validate against unchanged source journal");
    check(persisted.notes().size() == 2 && !history.knows("nebula"), "personal notes do not unlock engine dialogue topics");
    check(c->saveGame->moves == moves && c->saveGame->food == food, "journal actions consume no world turns or resources");
    [view closeJournal];
    check(!zu4_journal_panel_is_visible() && !zu4_ios_native_text_input_active(), "Close restores gameplay controls and keyboard ownership");
    check(gameSave() == 1, "real engine checkpoint includes journal notebook");
    check(persisted.load(gameSaveDirectory() + "journal-notebook.dat") && persisted.notes().size() == 2, "published checkpoint retains both notes");
    zu4_mobile_journal(); view = panel(); view.journalMode = 1; [view changeMode];
    fprintf(stderr, "JOURNAL RUNTIME COMPLETE\n"); fflush(stderr);
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 300 * NSEC_PER_MSEC), dispatch_get_main_queue(), ^{
        checkModeTargets(view, second, 0);
    });
}
