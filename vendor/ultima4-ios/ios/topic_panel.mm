#import <UIKit/UIKit.h>
#include "topic_panel.h"
#include "mobile_layout.h"
#include "settings.h"

@interface Zu4TopicPanel : UIView <UITextFieldDelegate, UIScrollViewDelegate>
@property(nonatomic, assign) BOOL directionMode;
@property(nonatomic, assign) BOOL spellbookMode;
@property(nonatomic, assign) BOOL conversationMode;
@property(nonatomic, assign) BOOL compactMenuMode;
@property(nonatomic, assign) BOOL fullScreenMode;
@property(nonatomic, assign) BOOL denseFullScreenMode;
@property(nonatomic, assign) BOOL controlsOnlyMode;
@property(nonatomic, assign) BOOL partySelectionMode;
@property(nonatomic, strong) UIView *card;
@property(nonatomic, assign) Zu4TopicSubmit submit;
@property(nonatomic, assign) void *context;
@property(nonatomic, strong) NSArray<NSString *> *keywords;
@property(nonatomic, strong) UITextField *custom;
@property(nonatomic, strong) UIScrollView *scrollView;
@property(nonatomic, strong) UIStackView *stack;
@property(nonatomic, strong) UIStackView *persistentFooter;
@property(nonatomic, strong) UILabel *headingLabel;
@property(nonatomic, assign) NSInteger maxLength;
@property(nonatomic, assign) BOOL wideLayout;
@property(nonatomic, strong) NSArray<UIStackView *> *optionRows;
@property(nonatomic, strong) UIStackView *denseSpellActions;
@property(nonatomic, strong) NSArray<UIButton *> *denseSpellButtons;
@property(nonatomic, strong) NSArray<UIStackView *> *denseSpellRows;
@property(nonatomic, assign) NSInteger denseSpellColumns;
- (void)rebuildDenseSpellGridForColumns:(NSInteger)columns;
@end
static Zu4TopicPanel *g_retiring_panel = nil;
static const NSInteger Zu4ResponsivePairRowTag = 0x5A553450;
@implementation Zu4TopicPanel
- (void)rebuildDenseSpellGridForColumns:(NSInteger)columns {
    if (!self.denseSpellButtons.count || self.denseSpellColumns == columns) return;
    for (UIStackView *oldRow in self.denseSpellRows) {
        for (UIView *button in oldRow.arrangedSubviews.copy) {
            [oldRow removeArrangedSubview:button];
            [button removeFromSuperview];
        }
        [self.denseSpellActions removeArrangedSubview:oldRow];
        [oldRow removeFromSuperview];
    }
    NSMutableArray<UIStackView *> *newRows = [NSMutableArray array];
    UIStackView *row = nil;
    for (NSInteger position = 0; position < (NSInteger)self.denseSpellButtons.count; ++position) {
        if (position % columns == 0) {
            row = [[UIStackView alloc] init];
            row.axis = UILayoutConstraintAxisHorizontal;
            row.distribution = UIStackViewDistributionFillEqually;
            row.alignment = UIStackViewAlignmentFill;
            row.spacing = 8;
            [self.denseSpellActions addArrangedSubview:row];
            [newRows addObject:row];
        }
        [row addArrangedSubview:self.denseSpellButtons[position]];
    }
    self.denseSpellRows = newRows;
    self.denseSpellColumns = columns;
}
- (void)keyboardChanged:(NSNotification *)note {
    dispatch_async(dispatch_get_main_queue(), ^{
        [self.superview bringSubviewToFront:self];
        [self setNeedsLayout];
        [self layoutIfNeeded];
    });
}
- (void)dealloc {
    [NSNotificationCenter.defaultCenter removeObserver:self];
}
- (void)layoutSubviews {
    // SDL responds to keyboard/rotation events by relaying out its root view.
    // Keep the native interaction sheet above it regardless of observer order.
    [self.superview bringSubviewToFront:self];
    UIEdgeInsets safe = self.safeAreaInsets;
    CGRect area = UIEdgeInsetsInsetRect(self.bounds, safe);
    BOOL portrait = area.size.height > area.size.width;
    if (self.denseSpellButtons.count)
        // Portrait is a true scan-friendly list. Landscape uses two columns so
        // every spell keeps a full 44-point touch row without scrolling.
        [self rebuildDenseSpellGridForColumns:portrait ? 1 : 2];
    CGFloat keyboardTop = CGRectGetMaxY(area);
    if (@available(iOS 15.0, *)) keyboardTop = MIN(keyboardTop, CGRectGetMinY(self.keyboardLayoutGuide.layoutFrame));
    CGFloat bottom = MAX(CGRectGetMinY(area) + 160, keyboardTop - 8);
    CGRect sheet = zu4SheetFrame(self.bounds, safe);
    CGFloat top = sheet.origin.y;
    if (keyboardTop < CGRectGetMaxY(area) - 20)
        top = MIN(top, MAX(CGRectGetMinY(area) + 100, bottom - 220));
    self.card.frame = CGRectMake(sheet.origin.x, top, sheet.size.width, MAX(48, bottom - top));
    BOOL accessible = UIContentSizeCategoryIsAccessibilityCategory(self.traitCollection.preferredContentSizeCategory);
    if (self.partySelectionMode) {
        // Four two-column rows fit below the world at ordinary text sizes.
        // Larger text uses the safe area rather than hiding party members.
        CGFloat rowHeight = MAX(44, ceil([UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline].lineHeight * 2) + 12);
        CGFloat height = 68 + 12 + self.optionRows.count * rowHeight +
            MAX(0, (NSInteger)self.optionRows.count - 1) * 6;
        if (accessible || height > bottom - top) {
            self.card.frame = CGRectInset(area, 8, 8);
        }
        // Preserve access at extreme accessibility sizes on a short landscape
        // screen; ordinary sizes never install an active pan recognizer.
        self.scrollView.scrollEnabled = accessible && height > self.card.frame.size.height;
        self.scrollView.bounces = NO;
    }
    BOOL controlsInSideRail = self.controlsOnlyMode && !portrait;
    BOOL wide = NO;
    for (UIStackView *row in self.optionRows) {
        BOOL responsivePair = row.tag == Zu4ResponsivePairRowTag;
        BOOL vertical = (responsivePair && portrait) ||
            (accessible && !self.denseFullScreenMode && !self.partySelectionMode) || controlsInSideRail;
        row.axis = vertical ? UILayoutConstraintAxisVertical : UILayoutConstraintAxisHorizontal;
        row.distribution = vertical ? UIStackViewDistributionFill : UIStackViewDistributionFillEqually;
        if (responsivePair)
            for (UIView *view in row.arrangedSubviews)
                if ([view isKindOfClass:UIButton.class]) {
                    UIButton *button = (UIButton *)view;
                    button.titleLabel.numberOfLines = 2;
                    button.titleLabel.adjustsFontSizeToFitWidth = YES;
                    button.titleLabel.minimumScaleFactor = portrait ? 0.85 : 0.72;
                    button.titleLabel.font = [UIFont preferredFontForTextStyle:
                        portrait ? UIFontTextStyleHeadline : UIFontTextStyleSubheadline];
                    [button invalidateIntrinsicContentSize];
                }
    }
    if (self.wideLayout != wide) {
        self.wideLayout = wide;
        self.stack.axis = wide ? UILayoutConstraintAxisHorizontal : UILayoutConstraintAxisVertical;
        self.stack.alignment = wide ? UIStackViewAlignmentTop : UIStackViewAlignmentFill;
        self.stack.distribution = wide ? UIStackViewDistributionFillEqually : UIStackViewDistributionFill;
        self.stack.spacing = wide ? 24 : 16;
    }
    if (portrait && (self.spellbookMode || self.conversationMode)) {
        // Six topics plus the stable footer occupy four button rows on every
        // page, so the conversation should not resize as its content changes.
        CGFloat desiredHeight = self.conversationMode ? 390 : 470;
        CGFloat height = MIN(desiredHeight, CGRectGetHeight(area) - 80);
        self.card.frame = CGRectMake(sheet.origin.x, bottom - height, sheet.size.width, height);
    } else if (!portrait && self.conversationMode) {
        // Keep the conversation readable without covering most of the world.
        // Accessibility text gets the larger composition when it needs it.
        CGFloat width = accessible ? MIN(560, area.size.width - 16)
                                   : MIN(460, MAX(380, area.size.width * 0.52));
        CGFloat x = CGRectGetMaxX(area) - width - 4;
        self.card.frame = CGRectMake(x, CGRectGetMinY(area) + 4, width, MAX(160, area.size.height - 8));
    }
    if (self.fullScreenMode || (self.compactMenuMode && accessible)) {
        self.card.frame = CGRectInset(area, 8, 8);
    } else if (self.compactMenuMode) {
        // Compact is a presentation, not a fixed row budget. Measure at the
        // actual sheet width so new actions and wrapping titles remain visible.
        CGRect menu = sheet;
        if (!portrait && self.optionRows.count >= 5) {
            // Dense landscape menus reclaim width before needing scrolling.
            menu.origin.x = CGRectGetMinX(area) + 8;
            menu.size.width = area.size.width - 16;
        }
        CGFloat maximumHeight = bottom - CGRectGetMinY(area) - 8;
        CGSize content = [self.stack systemLayoutSizeFittingSize:CGSizeMake(menu.size.width - 24, 0)
            withHorizontalFittingPriority:UILayoutPriorityRequired
            verticalFittingPriority:UILayoutPriorityFittingSizeLevel];
        CGFloat desiredHeight = MAX(260, ceil(content.height) + 68 + 12 + 2);
        CGFloat height = MIN(desiredHeight, maximumHeight);
        self.card.frame = CGRectMake(menu.origin.x, bottom - height, menu.size.width, height);
    }
    if (self.controlsOnlyMode) {
        NSInteger buttonCount = 0;
        for (UIStackView *row in self.optionRows) buttonCount += row.arrangedSubviews.count;
        if (controlsInSideRail) {
            // A narrow safe-area rail avoids the legacy text at the bottom and
            // covers only a small corner of the artwork on notched phones.
            CGFloat width = MIN(132, CGRectGetWidth(area) - 16);
            CGFloat height = accessible ? 144 : (buttonCount > 1 ? 124 : 68);
            for (UIStackView *row in self.optionRows)
                for (UIView *control in row.arrangedSubviews)
                    if ([control isKindOfClass:UIButton.class]) {
                        UIButton *button = (UIButton *)control;
                        button.titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
                        button.titleLabel.adjustsFontSizeToFitWidth = YES;
                        button.titleLabel.minimumScaleFactor = 0.8;
                        button.titleLabel.numberOfLines = 1;
                        button.contentEdgeInsets = UIEdgeInsetsMake(8, 2, 8, 2);
                    }
            self.card.frame = CGRectMake(CGRectGetMaxX(area) - width - 4,
                                         CGRectGetMinY(area) + 8, width, height);
        } else {
            CGFloat height = accessible ? 144 : 68;
            CGFloat width = MIN(CGRectGetWidth(area) - 16, portrait ? 420 : 560);
            self.card.frame = CGRectMake(CGRectGetMidX(area) - width / 2,
                                         CGRectGetMaxY(area) - height - 8, width, height);
        }
    }
    if (self.directionMode) {
        CGRect direction = zu4DirectionFrame(self.bounds, safe, settings.dpadSize, settings.flipControls);
        direction.origin.y = bottom - 180;
        self.card.frame = direction;
    }
    [super layoutSubviews];
    if (self.compactMenuMode) {
        // Overflow at extreme text sizes/short windows must still be reachable;
        // ordinary menus fit completely and have no active pan recognizer.
        self.scrollView.scrollEnabled = self.scrollView.contentSize.height > self.scrollView.bounds.size.height + 1;
        self.scrollView.bounces = NO;
    }
}
- (UIView *)hitTest:(CGPoint)point withEvent:(UIEvent *)event {
    UIView *hit = [super hitTest:point withEvent:event];
    return self.directionMode && hit == self ? nil : hit;
}
- (void)traitCollectionDidChange:(UITraitCollection *)previous {
    [super traitCollectionDidChange:previous];
    [self setNeedsLayout];
}
- (void)scrollViewWillEndDragging:(UIScrollView *)scrollView withVelocity:(CGPoint)velocity
             targetContentOffset:(inout CGPoint *)targetContentOffset {
    // SDL's nested iOS event loop can leave UIKit's momentum phase active,
    // suppressing every button in the sheet. End at the finger's exact release
    // point so dragging never creates a lingering interaction state.
    *targetContentOffset = scrollView.contentOffset;
}
- (void)scrollViewDidEndDragging:(UIScrollView *)scrollView willDecelerate:(BOOL)decelerate {
    if (decelerate) [scrollView setContentOffset:scrollView.contentOffset animated:NO];
}
- (void)finish:(NSString *)keyword textEntry:(BOOL)textEntry {
    Zu4TopicSubmit callback = self.submit;
    void *context = self.context;
    if (!callback) return;
    self.submit = nullptr;
    self.userInteractionEnabled = NO;
    [self endEditing:YES];
    // Keep the outgoing sheet covering the renderer while the synchronous
    // engine input loop prepares its replacement. zu4_topic_panel_show removes
    // it only after the next panel has been fully built, eliminating the frame
    // where gameplay controls and legacy text used to flash through.
    g_retiring_panel = self;
    callback(keyword.UTF8String, textEntry, context);
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.12 * NSEC_PER_SEC)),
        dispatch_get_main_queue(), ^{
            if (g_retiring_panel == self) {
                [self removeFromSuperview];
                g_retiring_panel = nil;
            }
        });
}
- (void)choose:(UIButton *)button { [self finish:self.keywords[button.tag] textEntry:NO]; }
- (void)hideKeyboard { [self endEditing:YES]; [self setNeedsLayout]; }
- (void)customize:(UIButton *)button {
    self.custom.hidden = NO;
    [self.custom becomeFirstResponder];
}
- (BOOL)textFieldShouldReturn:(UITextField *)field {
    if (field.text.length > 0 && field.text.length <= self.maxLength) [self finish:field.text textEntry:YES];
    return YES;
}
- (BOOL)textField:(UITextField *)field shouldChangeCharactersInRange:(NSRange)range replacementString:(NSString *)string {
    NSString *result = [field.text stringByReplacingCharactersInRange:range withString:string];
    NSCharacterSet *allowed = [NSCharacterSet characterSetWithCharactersInString:@"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 "];
    return result.length <= self.maxLength && [result rangeOfCharacterFromSet:allowed.invertedSet].location == NSNotFound;
}
@end

// A swipe beginning on a button must become scrolling instead of a stuck press.
@interface Zu4PanelScroll : UIScrollView
@end
@implementation Zu4PanelScroll
- (BOOL)touchesShouldCancelInContentView:(UIView *)view { return YES; }
@end

@interface Zu4PanelButton : UIButton
@property(nonatomic, assign) CGFloat measuredWidth;
@property(nonatomic, assign) CGFloat minimumHeight;
@property(nonatomic, assign) CGFloat verticalPadding;
@end
@implementation Zu4PanelButton
- (CGSize)intrinsicContentSize {
    CGFloat width = self.bounds.size.width > 24 ? self.bounds.size.width : 160;
    CGRect text = [self.currentTitle boundingRectWithSize:CGSizeMake(width - 24, CGFLOAT_MAX)
        options:NSStringDrawingUsesLineFragmentOrigin | NSStringDrawingUsesFontLeading
        attributes:@{NSFontAttributeName:self.titleLabel.font} context:nil];
    if (self.titleLabel.numberOfLines > 0)
        text.size.height = MIN(text.size.height,
            self.titleLabel.font.lineHeight * self.titleLabel.numberOfLines);
    return CGSizeMake(UIViewNoIntrinsicMetric,
        MAX(self.minimumHeight, ceil(text.size.height) + self.verticalPadding));
}
- (void)layoutSubviews {
    [super layoutSubviews];
    if (self.measuredWidth != self.bounds.size.width) {
        self.measuredWidth = self.bounds.size.width;
        [self invalidateIntrinsicContentSize];
    }
}
@end

static UIButton *topicButtonWithDensity(NSString *title, id target, SEL action, BOOL compact) {
    Zu4PanelButton *button = [Zu4PanelButton buttonWithType:UIButtonTypeSystem];
    button.minimumHeight = compact ? 44 : 48;
    button.verticalPadding = compact ? 12 : 20;
    [button setTitle:title forState:UIControlStateNormal];
    button.titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
    button.titleLabel.adjustsFontForContentSizeCategory = YES;
    button.titleLabel.numberOfLines = 0;
    button.titleLabel.textAlignment = NSTextAlignmentCenter;
    button.contentEdgeInsets = compact ? UIEdgeInsetsMake(6, 10, 6, 10)
                                       : UIEdgeInsetsMake(10, 12, 10, 12);
    button.backgroundColor = [UIColor colorWithWhite:0.16 alpha:1];
    button.tintColor = [UIColor colorWithRed:0.94 green:0.81 blue:0.49 alpha:1];
    button.layer.cornerRadius = 10;
    [button.heightAnchor constraintGreaterThanOrEqualToConstant:button.minimumHeight].active = YES;
    [button addTarget:target action:action forControlEvents:UIControlEventTouchUpInside];
    return button;
}

static UIButton *topicButton(NSString *title, id target, SEL action) {
    return topicButtonWithDensity(title, target, action, NO);
}

@interface Zu4JournalPanel : UIView <UITableViewDataSource, UITableViewDelegate, UISearchBarDelegate, UITextViewDelegate>
@property(nonatomic, strong) UIView *card;
@property(nonatomic, strong) UILabel *titleLabel;
@property(nonatomic, strong) UIButton *backButton;
@property(nonatomic, strong) UISearchBar *searchBar;
@property(nonatomic, strong) UITableView *tableView;
@property(nonatomic, strong) UIView *listNavigation;
@property(nonatomic, strong) UIButton *previousListButton;
@property(nonatomic, strong) UIButton *nextListButton;
@property(nonatomic, strong) UILabel *listPageLabel;
@property(nonatomic, strong) UIScrollView *detailScroll;
@property(nonatomic, strong) UIStackView *detailStack;
@property(nonatomic, strong) UIView *detailNavigation;
@property(nonatomic, strong) UIButton *previousDetailButton;
@property(nonatomic, strong) UIButton *nextDetailButton;
@property(nonatomic, strong) NSArray<NSDictionary *> *groups;
@property(nonatomic, strong) NSArray<NSDictionary *> *filteredGroups;
@property(nonatomic, strong) NSArray<NSString *> *sections;
@property(nonatomic, strong) NSMutableDictionary<NSString *, NSNumber *> *savedOffsets;
@property(nonatomic, strong) NSString *selectedLocation;
@property(nonatomic, strong) NSDictionary *selectedGroup;
@property(nonatomic, assign) NSInteger listPage;
@property(nonatomic, assign) NSInteger lastListPageSize;
@property(nonatomic, assign) Zu4JournalAccess access;
@property(nonatomic, strong) UIStackView *modeControl;
@property(nonatomic, assign) NSInteger journalMode;
@property(nonatomic, strong) NSArray<UIButton *> *modeButtons;
@property(nonatomic, strong) UIButton *addNoteButton;
@property(nonatomic, strong) NSArray<NSDictionary *> *noteRows;
@property(nonatomic, strong) NSDictionary *selectedNote;
@property(nonatomic, strong) UIView *editor;
@property(nonatomic, strong) UITextView *noteText;
@property(nonatomic, strong) UILabel *editorMessage;
@property(nonatomic, strong) UIButton *doneNoteButton;
@property(nonatomic, strong) UIButton *cancelNoteButton;
@property(nonatomic, strong) UIButton *deleteNoteButton;
@property(nonatomic, assign) uint64_t editingIdentifier;
@property(nonatomic, assign) int editingPassage;
@property(nonatomic, assign) BOOL confirmingDelete;
@property(nonatomic, assign) CGFloat keyboardTop;
@end

static NSString *journalString(const char *value) {
    return value ? [NSString stringWithUTF8String:value] ?: @"" : @"";
}

// Dialogue files contain hard line wraps for the original 1985 text window.
// Preserve real blank-line paragraph breaks, but let UIKit wrap each paragraph
// naturally for the current device width.
static NSString *readingText(NSString *rawText) {
    NSString *unixText = [[rawText ?: @"" stringByReplacingOccurrencesOfString:@"\r\n" withString:@"\n"]
        stringByReplacingOccurrencesOfString:@"\r" withString:@"\n"];
    NSMutableArray<NSString *> *paragraphs = [NSMutableArray array];
    for (NSString *part in [unixText componentsSeparatedByString:@"\n\n"]) {
        NSArray<NSString *> *words = [part componentsSeparatedByCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet];
        NSString *paragraph = [[words filteredArrayUsingPredicate:
            [NSPredicate predicateWithBlock:^BOOL(NSString *word, NSDictionary *bindings) {
                return word.length > 0;
            }]] componentsJoinedByString:@" "];
        if (paragraph.length && ![paragraph isEqualToString:@"Your Interest:"])
            [paragraphs addObject:paragraph];
    }
    return [paragraphs componentsJoinedByString:@"\n\n"];
}

static UILabel *journalLabel(NSString *text, UIFontTextStyle style, UIColor *color) {
    UILabel *label = [[UILabel alloc] init];
    label.text = text;
    label.numberOfLines = 0;
    label.textColor = color;
    label.font = [UIFont preferredFontForTextStyle:style];
    label.adjustsFontForContentSizeCategory = YES;
    return label;
}

static UIButton *journalIcon(NSString *symbol, NSString *label, id target, SEL action) {
    UIButton *button = [UIButton buttonWithType:UIButtonTypeSystem];
    [button setImage:[UIImage systemImageNamed:symbol withConfiguration:
        [UIImageSymbolConfiguration configurationWithPointSize:21 weight:UIImageSymbolWeightRegular]] forState:UIControlStateNormal];
    button.tintColor = [UIColor colorWithRed:0.94 green:0.81 blue:0.49 alpha:1];
    button.accessibilityLabel = label;
    [button.widthAnchor constraintEqualToConstant:44].active = YES;
    [button.heightAnchor constraintEqualToConstant:44].active = YES;
    [button addTarget:target action:action forControlEvents:UIControlEventTouchUpInside];
    return button;
}

@implementation Zu4JournalPanel
- (NSString *)modeTitle { return @[@"Journal", @"Current clues", @"My notes"][MIN(2, MAX(0, self.journalMode))]; }
- (void)chooseMode:(UIButton *)button {
    self.journalMode = button.tag;
    // Complete UIKit's touch dispatch before changing the table/view hierarchy.
    // SDL pumps UIKit from a nested run loop while the engine owns the thread.
    dispatch_async(dispatch_get_main_queue(), ^{ if (self.superview) [self changeMode]; });
}
- (NSDictionary *)sourceForPassage:(int)index {
    for (NSDictionary *group in self.groups)
        for (NSDictionary *entry in group[@"entries"])
            if ([entry[@"index"] intValue] == index) return @{@"group":group, @"entry":entry};
    return nil;
}
- (NSString *)attachedText:(int)index {
    uint64_t identifier = self.access.attachedNote ? self.access.attachedNote(index) : 0;
    for (int i = 0; identifier && self.access.noteCount && i < self.access.noteCount(); ++i) {
        Zu4JournalNote note;
        if (self.access.noteAt && self.access.noteAt(i, &note) && note.identifier == identifier) return journalString(note.text);
    }
    return @"";
}
- (BOOL)matchesEntry:(NSDictionary *)entry group:(NSDictionary *)group query:(NSString *)needle {
    NSString *haystack = [NSString stringWithFormat:@"%@ %@ %@ %@ %@", group[@"title"], entry[@"place"],
        entry[@"topic"], entry[@"text"], [self attachedText:[entry[@"index"] intValue]]];
    return !needle.length || [haystack.lowercaseString containsString:needle];
}
- (void)refreshNotes {
    NSMutableArray *rows = [NSMutableArray array];
    NSString *needle = self.searchBar.text.lowercaseString;
    for (int i = 0; self.access.noteCount && i < self.access.noteCount(); ++i) {
        Zu4JournalNote note;
        if (!self.access.noteAt || !self.access.noteAt(i, &note)) continue;
        NSDictionary *source = [self sourceForPassage:note.passage];
        NSString *text = journalString(note.text);
        if (needle.length && ![text.lowercaseString containsString:needle] &&
            (!source || ![self matchesEntry:source[@"entry"] group:source[@"group"] query:needle])) continue;
        [rows addObject:@{@"id":@(note.identifier), @"index":@(note.passage), @"text":text, @"source":source ?: @{}}];
    }
    self.noteRows = [[rows reverseObjectEnumerator] allObjects];
}
- (void)changeMode {
    [self endEditing:YES];
    for (UIButton *button in self.modeButtons) {
        BOOL selected = button.tag == self.journalMode;
        button.backgroundColor = [UIColor colorWithWhite:selected ? 0.32 : 0.16 alpha:1];
        button.accessibilityTraits = UIAccessibilityTraitButton | (selected ? UIAccessibilityTraitSelected : 0);
    }
    self.selectedLocation = nil;
    self.selectedGroup = nil;
    self.selectedNote = nil;
    self.backButton.hidden = YES;
    self.titleLabel.text = [self modeTitle];
    self.searchBar.hidden = self.tableView.hidden = self.listNavigation.hidden = NO;
    self.detailScroll.hidden = self.detailNavigation.hidden = YES;
    [self searchBar:self.searchBar textDidChange:self.searchBar.text ?: @""];
}
- (void)layoutSubviews {
    [super layoutSubviews];
    [self.superview bringSubviewToFront:self];
    CGRect safe = UIEdgeInsetsInsetRect(self.bounds, self.safeAreaInsets);
    // The journal is a paused, deep reading view: retain its full-screen
    // safe-area card rather than shrinking it to the active-play sheet.
    CGRect sheet = CGRectInset(safe, 8, 8);
    CGFloat rowHeight = MAX(64, ceil([UIFont preferredFontForTextStyle:UIFontTextStyleHeadline].lineHeight +
        [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline].lineHeight * 2 + 12));
    if (self.keyboardTop > 0 && self.keyboardTop < CGRectGetMaxY(sheet)) {
        CGFloat bottom = self.keyboardTop - 8;
        sheet.origin.y = MAX(safe.origin.y + 8, MIN(sheet.origin.y, bottom - 260));
        sheet.size.height = MAX(120, bottom - sheet.origin.y);
    }
    self.card.frame = sheet;
    self.tableView.rowHeight = rowHeight;
    NSInteger pageSize = [self listPageSize];
    if (self.lastListPageSize != pageSize) {
        self.lastListPageSize = pageSize;
        dispatch_async(dispatch_get_main_queue(), ^{ [self updateListNavigation]; });
    }
}
- (void)closeJournal {
    [self endEditing:YES];
    if (self.access.textInput) self.access.textInput(0);
    [self removeFromSuperview];
}
- (void)dealloc { [NSNotificationCenter.defaultCenter removeObserver:self]; }
- (void)journalKeyboardChanged:(NSNotification *)note {
    CGRect frame = [note.userInfo[UIKeyboardFrameEndUserInfoKey] CGRectValue];
    self.keyboardTop = [self convertRect:frame fromView:nil].origin.y;
    [self setNeedsLayout]; [self layoutIfNeeded];
    [self updateListNavigation];
}
- (void)journalKeyboardHidden:(NSNotification *)note {
    self.keyboardTop = 0;
    [self setNeedsLayout];
}
- (void)updateDetailNavigation {
    [self.detailScroll layoutIfNeeded];
    CGFloat maximum = MAX(0, self.detailScroll.contentSize.height - self.detailScroll.bounds.size.height);
    CGFloat offset = MIN(MAX(0, self.detailScroll.contentOffset.y), maximum);
    if (fabs(offset - self.detailScroll.contentOffset.y) > 0.5)
        [self.detailScroll setContentOffset:CGPointMake(0, offset) animated:NO];
    self.previousDetailButton.enabled = offset > 0.5;
    self.nextDetailButton.enabled = offset < maximum - 0.5;
    self.previousDetailButton.alpha = self.previousDetailButton.enabled ? 1 : 0.3;
    self.nextDetailButton.alpha = self.nextDetailButton.enabled ? 1 : 0.3;
}
- (void)moveDetail:(UIButton *)button {
    CGFloat maximum = MAX(0, self.detailScroll.contentSize.height - self.detailScroll.bounds.size.height);
    CGFloat step = MAX(1, MIN(self.detailScroll.bounds.size.height * 0.82, self.detailScroll.bounds.size.height - 48));
    CGFloat target = MIN(maximum, MAX(0, self.detailScroll.contentOffset.y + button.tag * step));
    [self.detailScroll setContentOffset:CGPointMake(0, target) animated:NO];
    if (self.selectedGroup) self.savedOffsets[self.selectedGroup[@"key"]] = @(target);
    [self updateDetailNavigation];
    UIAccessibilityPostNotification(UIAccessibilityPageScrolledNotification,
        button.tag < 0 ? @"Previous journal section" : @"Next journal section");
}
- (NSInteger)listPageSize { return MAX(1, (NSInteger)floor(self.tableView.bounds.size.height / MAX(64, self.tableView.rowHeight))); }
- (NSArray *)listItems {
    NSInteger mode = self.journalMode;
    if (mode == 2) return self.noteRows ?: @[];
    if (mode == 1) {
        NSMutableArray *clues = [NSMutableArray array];
        for (NSDictionary *group in self.filteredGroups)
            for (NSDictionary *entry in group[@"entries"])
                if (self.access.favorite && self.access.favorite([entry[@"index"] intValue]) &&
                    [self matchesEntry:entry group:group query:self.searchBar.text.lowercaseString])
                    [clues addObject:@{@"group":group, @"entry":entry}];
        return [clues sortedArrayUsingComparator:^NSComparisonResult(NSDictionary *a, NSDictionary *b) {
            return [b[@"entry"][@"index"] compare:a[@"entry"][@"index"]];
        }];
    }
    if (!self.selectedLocation) return self.sections;
    return [self.filteredGroups filteredArrayUsingPredicate:
        [NSPredicate predicateWithBlock:^BOOL(NSDictionary *group, NSDictionary *bindings) {
            return [group[@"section"] isEqualToString:self.selectedLocation];
        }]];
}
- (NSArray *)visibleListItems {
    NSArray *items = [self listItems];
    NSInteger size = [self listPageSize];
    NSInteger first = MIN(self.listPage * size, (NSInteger)items.count);
    NSInteger length = MIN(size, (NSInteger)items.count - first);
    return [items subarrayWithRange:NSMakeRange(first, length)];
}
- (void)updateListNavigation {
    NSArray *items = [self listItems];
    NSInteger size = [self listPageSize];
    NSInteger pages = MAX(1, ((NSInteger)items.count + size - 1) / size);
    self.listPage = MIN(MAX(0, self.listPage), pages - 1);
    self.previousListButton.enabled = self.listPage > 0;
    self.nextListButton.enabled = self.listPage + 1 < pages;
    self.previousListButton.alpha = self.previousListButton.enabled ? 1 : 0.3;
    self.nextListButton.alpha = self.nextListButton.enabled ? 1 : 0.3;
    self.listPageLabel.text = [NSString stringWithFormat:@"%ld of %ld",
        (long)self.listPage + 1, (long)pages];
    self.addNoteButton.hidden = self.journalMode != 2;
    if (!items.count) {
        NSString *message = self.journalMode == 1 ? @"No current clues. Bookmark a recorded passage to keep it here." :
            (self.journalMode == 2 ? @"No personal notes. Use New note, or attach one to a recorded passage." : @"No matching journal records.");
        UILabel *empty = journalLabel(message, UIFontTextStyleBody, [UIColor colorWithWhite:0.75 alpha:1]);
        empty.textAlignment = NSTextAlignmentCenter;
        self.tableView.backgroundView = empty;
    } else self.tableView.backgroundView = nil;
    [self.tableView reloadData];
}
- (void)moveList:(UIButton *)button {
    self.listPage += button.tag;
    [self updateListNavigation];
    UIAccessibilityPostNotification(UIAccessibilityPageScrolledNotification, self.listPageLabel.text);
}
- (void)showLocation:(NSString *)location {
    self.selectedLocation = location;
    self.listPage = 0;
    self.backButton.hidden = NO;
    [self.backButton setTitle:@"‹ Journal" forState:UIControlStateNormal];
    self.backButton.accessibilityLabel = @"Back to locations";
    self.titleLabel.text = location;
    [self updateListNavigation];
    UIAccessibilityPostNotification(UIAccessibilityScreenChangedNotification, self.titleLabel);
}
- (void)backToIndex {
    if (self.selectedGroup)
        self.savedOffsets[self.selectedGroup[@"key"]] = @(self.detailScroll.contentOffset.y);
    if (!self.detailScroll.hidden) {
        self.selectedGroup = nil;
        self.selectedNote = nil;
        self.titleLabel.text = self.selectedLocation ?: [self modeTitle];
        [self.backButton setTitle:@"‹ Journal" forState:UIControlStateNormal];
        self.backButton.accessibilityLabel = @"Back to locations";
        self.searchBar.hidden = NO;
        self.tableView.hidden = NO;
        self.listNavigation.hidden = NO;
        self.detailScroll.hidden = YES;
        self.detailNavigation.hidden = YES;
        self.backButton.hidden = !self.selectedLocation;
        [self updateListNavigation];
    } else {
        self.selectedLocation = nil;
        self.listPage = 0;
        self.backButton.hidden = YES;
        self.titleLabel.text = [self modeTitle];
        [self updateListNavigation];
    }
    UIAccessibilityPostNotification(UIAccessibilityScreenChangedNotification, self.titleLabel);
}
- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView { return 1; }
- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section {
    return [self visibleListItems].count;
}
- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)path {
    static NSString *identifier = @"journal-source";
    UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:identifier];
    if (!cell) cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleSubtitle reuseIdentifier:identifier];
    id item = [self visibleListItems][path.row];
    cell.textLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
    cell.textLabel.adjustsFontForContentSizeCategory = YES;
    if (self.journalMode == 1) {
        cell.textLabel.text = item[@"group"][@"title"];
        cell.detailTextLabel.text = [NSString stringWithFormat:@"%@ · %@", item[@"entry"][@"place"], readingText(item[@"entry"][@"text"])];
    } else if (self.journalMode == 2) {
        cell.textLabel.text = [item[@"source"] count] ? [NSString stringWithFormat:@"My note · %@", item[@"source"][@"group"][@"title"]] : @"My note";
        cell.detailTextLabel.text = [item[@"source"] count] ? [NSString stringWithFormat:@"%@ · %@", item[@"source"][@"entry"][@"place"], item[@"text"]] : item[@"text"];
    } else if (!self.selectedLocation) {
        NSString *location = item;
        NSArray *records = [self.filteredGroups filteredArrayUsingPredicate:
            [NSPredicate predicateWithBlock:^BOOL(NSDictionary *group, NSDictionary *bindings) {
                return [group[@"section"] isEqualToString:location];
            }]];
        NSUInteger people = 0, notes = 0;
        for (NSDictionary *group in records) {
            if ([group[@"kindLabel"] isEqualToString:@"Person"]) ++people;
            notes += [group[@"entries"] count];
        }
        cell.textLabel.text = location;
        NSString *peopleText = [NSString stringWithFormat:@"%lu %@", (unsigned long)people,
            people == 1 ? @"person" : @"people"];
        NSString *notesText = [NSString stringWithFormat:@"%lu %@", (unsigned long)notes,
            notes == 1 ? @"note" : @"notes"];
        cell.detailTextLabel.text = [NSString stringWithFormat:@"%@ · %@", peopleText, notesText];
    } else {
        NSDictionary *group = item;
        cell.textLabel.text = group[@"title"];
        NSUInteger count = [group[@"entries"] count];
        NSString *kindLabel = group[@"kindLabel"];
        NSString *countText = [NSString stringWithFormat:@"%lu %@", (unsigned long)count,
            count == 1 ? @"note" : @"notes"];
        cell.detailTextLabel.text = [kindLabel isEqualToString:@"Person"]
            ? countText : [NSString stringWithFormat:@"%@ · %@", kindLabel, countText];
    }
    cell.detailTextLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
    cell.detailTextLabel.adjustsFontForContentSizeCategory = YES;
    cell.detailTextLabel.numberOfLines = 2;
    cell.textLabel.textColor = UIColor.whiteColor;
    cell.detailTextLabel.textColor = [UIColor colorWithWhite:0.72 alpha:1];
    cell.backgroundColor = UIColor.clearColor;
    cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
    cell.accessibilityLabel = [NSString stringWithFormat:@"%@, %@", cell.textLabel.text, cell.detailTextLabel.text];
    return cell;
}
- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)path {
    [tableView deselectRowAtIndexPath:path animated:YES];
    id item = [self visibleListItems][path.row];
    [self endEditing:YES];
    if (self.journalMode == 1) [self showSource:item];
    else if (self.journalMode == 2) [self showNote:item];
    else if (!self.selectedLocation) [self showLocation:item];
    else [self showGroup:item];
}
- (void)showGroup:(NSDictionary *)group {
    self.selectedNote = nil;
    self.selectedGroup = group;
    self.backButton.hidden = NO;
    [self.backButton setTitle:@"‹ Back" forState:UIControlStateNormal];
    self.backButton.accessibilityLabel = [NSString stringWithFormat:@"Back to %@", self.selectedLocation ?: [self modeTitle]];
    self.titleLabel.text = group[@"title"];
    self.searchBar.hidden = YES;
    self.tableView.hidden = YES;
    self.listNavigation.hidden = YES;
    self.detailScroll.hidden = NO;
    self.detailNavigation.hidden = NO;
    for (UIView *view in self.detailStack.arrangedSubviews) {
        [self.detailStack removeArrangedSubview:view];
        [view removeFromSuperview];
    }
    NSArray *places = group[@"places"];
    if (places.count)
        [self.detailStack addArrangedSubview:journalLabel([places componentsJoinedByString:@" · "],
            UIFontTextStyleSubheadline, [UIColor colorWithWhite:0.72 alpha:1])];
    for (NSDictionary *entry in group[@"entries"]) {
        UIStackView *content = [[UIStackView alloc] init];
        content.axis = UILayoutConstraintAxisVertical;
        content.spacing = 6;
        content.layoutMarginsRelativeArrangement = YES;
        content.layoutMargins = UIEdgeInsetsMake(12, 14, 12, 14);
        content.backgroundColor = [UIColor colorWithWhite:0.13 alpha:1];
        content.layer.cornerRadius = 12;
        int index = [entry[@"index"] intValue];
        content.tag = 0x4A0000 + index;
        NSString *topic = entry[@"topic"];
        UIStackView *header = [[UIStackView alloc] init];
        header.axis = UILayoutConstraintAxisHorizontal;
        header.alignment = UIStackViewAlignmentCenter;
        header.spacing = 4;
        UILabel *captionLabel = journalLabel(@"", UIFontTextStyleCaption1,
            [UIColor colorWithRed:0.94 green:0.81 blue:0.49 alpha:1]);
        [header addArrangedSubview:captionLabel];
        if (topic.length > 1) {
            NSString *caption = [topic caseInsensitiveCompare:@"Introduction"] == NSOrderedSame
                ? @"INTRODUCTION" : [NSString stringWithFormat:@"ASKED: %@", topic.uppercaseString];
            captionLabel.text = caption;
        }
        BOOL bookmarked = self.access.favorite && self.access.favorite(index);
        BOOL hasNote = [self attachedText:index].length > 0;
        UIButton *bookmark = journalIcon(bookmarked ? @"bookmark.fill" : @"bookmark",
            bookmarked ? @"Remove bookmark" : @"Bookmark passage", self, @selector(bookmarkPassage:));
        bookmark.tag = index;
        bookmark.accessibilityIdentifier = [NSString stringWithFormat:@"journal-bookmark-%d", index];
        bookmark.accessibilityValue = bookmarked ? @"Bookmarked" : @"Not bookmarked";
        bookmark.accessibilityTraits = UIAccessibilityTraitButton | (bookmarked ? UIAccessibilityTraitSelected : 0);
        UIButton *note = journalIcon(@"square.and.pencil", hasNote ? @"Edit personal note" : @"Add personal note", self, @selector(editPassage:));
        note.tag = index;
        note.accessibilityIdentifier = [NSString stringWithFormat:@"journal-note-%d", index];
        note.accessibilityValue = hasNote ? @"Note attached" : @"No attached note";
        if (hasNote) {
            UIImageView *badge = [[UIImageView alloc] initWithImage:[UIImage systemImageNamed:@"checkmark.circle.fill"]];
            badge.tintColor = UIColor.systemTealColor;
            badge.backgroundColor = content.backgroundColor;
            badge.layer.cornerRadius = 7;
            badge.isAccessibilityElement = NO;
            badge.accessibilityIdentifier = @"journal-note-attached";
            badge.translatesAutoresizingMaskIntoConstraints = NO;
            [note addSubview:badge];
            [NSLayoutConstraint activateConstraints:@[
                [badge.widthAnchor constraintEqualToConstant:14], [badge.heightAnchor constraintEqualToConstant:14],
                [badge.topAnchor constraintEqualToAnchor:note.topAnchor constant:4],
                [badge.trailingAnchor constraintEqualToAnchor:note.trailingAnchor constant:-3]]];
        }
        [header addArrangedSubview:bookmark]; [header addArrangedSubview:note];
        [content addArrangedSubview:header];
        [content addArrangedSubview:journalLabel(readingText(entry[@"text"]), UIFontTextStyleBody, UIColor.whiteColor)];
        if (hasNote) {
            [content addArrangedSubview:journalLabel(@"MY NOTE", UIFontTextStyleCaption1, UIColor.systemTealColor)];
            [content addArrangedSubview:journalLabel([self attachedText:index], UIFontTextStyleBody, UIColor.systemTealColor)];
        }
        if ([places count] > 1 && [entry[@"place"] length])
            [content addArrangedSubview:journalLabel(entry[@"place"], UIFontTextStyleCaption2,
                [UIColor colorWithWhite:0.62 alpha:1])];
        [self.detailStack addArrangedSubview:content];
    }
    NSNumber *saved = self.savedOffsets[group[@"key"]];
    [self layoutIfNeeded];
    CGFloat maximum = MAX(0, self.detailScroll.contentSize.height - self.detailScroll.bounds.size.height);
    [self.detailScroll setContentOffset:CGPointMake(0, MIN(saved.doubleValue, maximum)) animated:NO];
    dispatch_async(dispatch_get_main_queue(), ^{ [self updateDetailNavigation]; });
    UIAccessibilityPostNotification(UIAccessibilityScreenChangedNotification, self.titleLabel);
}
- (void)searchBar:(UISearchBar *)searchBar textDidChange:(NSString *)query {
    NSString *needle = query.lowercaseString;
    if (!needle.length) self.filteredGroups = self.groups;
    else self.filteredGroups = [self.groups filteredArrayUsingPredicate:
        [NSPredicate predicateWithBlock:^BOOL(NSDictionary *group, NSDictionary *bindings) {
            NSMutableString *haystack = [NSMutableString stringWithFormat:@"%@ %@ ", group[@"title"],
                [group[@"places"] componentsJoinedByString:@" "]];
            for (NSDictionary *entry in group[@"entries"])
                [haystack appendFormat:@"%@ %@ %@ ", entry[@"topic"], entry[@"text"], [self attachedText:[entry[@"index"] intValue]]];
            return [haystack.lowercaseString containsString:needle];
        }]];
    [self refreshNotes]; [self rebuildSections];
}
- (void)searchBarTextDidBeginEditing:(UISearchBar *)bar { if (self.access.textInput) self.access.textInput(1); }
- (void)searchBarTextDidEndEditing:(UISearchBar *)bar { if (self.access.textInput && !self.editor) self.access.textInput(0); }
- (void)searchBarSearchButtonClicked:(UISearchBar *)bar { [bar resignFirstResponder]; }
- (void)showSource:(NSDictionary *)source {
    [self showGroup:source[@"group"]];
    [self.card layoutIfNeeded]; [self.detailScroll layoutIfNeeded];
    UIView *entry = [self.detailStack viewWithTag:0x4A0000 + [source[@"entry"][@"index"] intValue]];
    CGFloat maximum = MAX(0, self.detailScroll.contentSize.height - self.detailScroll.bounds.size.height);
    [self.detailScroll setContentOffset:CGPointMake(0, MIN(maximum, entry.frame.origin.y)) animated:NO];
    [self updateDetailNavigation];
}
- (void)bookmarkPassage:(UIButton *)button {
    if (!self.access.toggleFavorite || !self.access.toggleFavorite((int)button.tag)) {
        [button setImage:[UIImage systemImageNamed:@"exclamationmark.triangle"] forState:UIControlStateNormal];
        button.accessibilityValue = @"Bookmark save failed";
        UIAccessibilityPostNotification(UIAccessibilityAnnouncementNotification, @"Could not save bookmark. Please try again.");
        return;
    }
    self.savedOffsets[self.selectedGroup[@"key"]] = @(self.detailScroll.contentOffset.y);
    [self showGroup:self.selectedGroup];
}
- (void)showNote:(NSDictionary *)note {
    self.selectedGroup = nil; self.selectedNote = note;
    self.titleLabel.text = @"My note"; self.backButton.hidden = NO;
    [self.backButton setTitle:@"‹ Back" forState:UIControlStateNormal];
    self.searchBar.hidden = self.tableView.hidden = self.listNavigation.hidden = YES;
    self.detailScroll.hidden = self.detailNavigation.hidden = NO;
    for (UIView *view in self.detailStack.arrangedSubviews) { [self.detailStack removeArrangedSubview:view]; [view removeFromSuperview]; }
    [self.detailStack addArrangedSubview:journalLabel(@"MY NOTE", UIFontTextStyleCaption1, UIColor.systemTealColor)];
    [self.detailStack addArrangedSubview:journalLabel(note[@"text"], UIFontTextStyleBody, UIColor.whiteColor)];
    [self.detailStack addArrangedSubview:topicButton(@"Edit note", self, @selector(editSelectedNote))];
    if ([note[@"source"] count]) {
        NSDictionary *source = note[@"source"];
        [self.detailStack addArrangedSubview:journalLabel([NSString stringWithFormat:@"Attached to %@ · %@", source[@"group"][@"title"], source[@"entry"][@"place"]], UIFontTextStyleSubheadline, UIColor.systemTealColor)];
        [self.detailStack addArrangedSubview:topicButton(@"View original passage", self, @selector(viewSelectedSource))];
    }
    [self.detailScroll setContentOffset:CGPointZero animated:NO];
    dispatch_async(dispatch_get_main_queue(), ^{ [self updateDetailNavigation]; });
}
- (void)viewSelectedSource { NSDictionary *source = self.selectedNote[@"source"]; if (source.count) [self showSource:source]; }
- (void)editSelectedNote { [self openEditor:[self.selectedNote[@"index"] intValue] identifier:[self.selectedNote[@"id"] unsignedLongLongValue] text:self.selectedNote[@"text"]]; }
- (void)editPassage:(UIButton *)button { [self openEditor:(int)button.tag identifier:self.access.attachedNote ? self.access.attachedNote((int)button.tag) : 0 text:[self attachedText:(int)button.tag]]; }
- (void)newNote { [self openEditor:-1 identifier:0 text:@""]; }
- (void)openEditor:(int)passage identifier:(uint64_t)identifier text:(NSString *)text {
    [self endEditing:YES];
    self.editingPassage = passage; self.editingIdentifier = identifier; self.confirmingDelete = NO;
    self.editor = [[UIView alloc] init]; self.editor.translatesAutoresizingMaskIntoConstraints = NO;
    self.editor.backgroundColor = self.card.backgroundColor; self.editor.accessibilityViewIsModal = YES;
    [self.card addSubview:self.editor];
    UILabel *heading = journalLabel(@"My note", UIFontTextStyleTitle2, UIColor.whiteColor);
    heading.translatesAutoresizingMaskIntoConstraints = NO; [self.editor addSubview:heading];
    self.noteText = [[UITextView alloc] init]; self.noteText.translatesAutoresizingMaskIntoConstraints = NO;
    self.noteText.backgroundColor = [UIColor colorWithWhite:0.13 alpha:1]; self.noteText.textColor = UIColor.whiteColor;
    self.noteText.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody]; self.noteText.adjustsFontForContentSizeCategory = YES;
    self.noteText.accessibilityLabel = @"Personal note text"; self.noteText.text = text; self.noteText.delegate = self;
    [self.editor addSubview:self.noteText];
    self.editorMessage = journalLabel(@"Only Done saves your writing.", UIFontTextStyleCaption1, UIColor.systemTealColor);
    self.editorMessage.translatesAutoresizingMaskIntoConstraints = NO; [self.editor addSubview:self.editorMessage];
    UIStackView *buttons = [[UIStackView alloc] init]; buttons.axis = UILayoutConstraintAxisHorizontal;
    buttons.distribution = UIStackViewDistributionFillEqually; buttons.spacing = 8; buttons.translatesAutoresizingMaskIntoConstraints = NO;
    self.cancelNoteButton = topicButton(@"Cancel", self, @selector(cancelNote));
    self.deleteNoteButton = topicButton(@"Delete", self, @selector(confirmDeleteNote)); self.deleteNoteButton.hidden = !identifier;
    self.doneNoteButton = topicButton(@"Done", self, @selector(doneNote));
    [buttons addArrangedSubview:self.cancelNoteButton]; [buttons addArrangedSubview:self.deleteNoteButton]; [buttons addArrangedSubview:self.doneNoteButton];
    [self.editor addSubview:buttons];
    [NSLayoutConstraint activateConstraints:@[
        [self.editor.leadingAnchor constraintEqualToAnchor:self.card.leadingAnchor], [self.editor.trailingAnchor constraintEqualToAnchor:self.card.trailingAnchor],
        [self.editor.topAnchor constraintEqualToAnchor:self.card.topAnchor], [self.editor.bottomAnchor constraintEqualToAnchor:self.card.bottomAnchor],
        [heading.leadingAnchor constraintEqualToAnchor:self.editor.leadingAnchor constant:14], [heading.topAnchor constraintEqualToAnchor:self.editor.topAnchor constant:10],
        [self.noteText.leadingAnchor constraintEqualToAnchor:heading.leadingAnchor], [self.noteText.trailingAnchor constraintEqualToAnchor:self.editor.trailingAnchor constant:-14],
        [self.noteText.topAnchor constraintEqualToAnchor:heading.bottomAnchor constant:8], [self.noteText.bottomAnchor constraintEqualToAnchor:self.editorMessage.topAnchor constant:-8],
        [self.editorMessage.leadingAnchor constraintEqualToAnchor:heading.leadingAnchor], [self.editorMessage.trailingAnchor constraintEqualToAnchor:self.noteText.trailingAnchor],
        [self.editorMessage.bottomAnchor constraintEqualToAnchor:buttons.topAnchor constant:-8],
        [buttons.leadingAnchor constraintEqualToAnchor:heading.leadingAnchor], [buttons.trailingAnchor constraintEqualToAnchor:self.noteText.trailingAnchor],
        [buttons.bottomAnchor constraintEqualToAnchor:self.editor.bottomAnchor constant:-12], [buttons.heightAnchor constraintEqualToConstant:48]]];
    if (self.access.textInput) self.access.textInput(1);
    [self textViewDidChange:self.noteText]; [self.noteText becomeFirstResponder];
}
- (void)textViewDidChange:(UITextView *)view {
    BOOL valid = [view.text stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet].length && [view.text lengthOfBytesUsingEncoding:NSUTF8StringEncoding] <= 4000;
    self.doneNoteButton.enabled = valid;
    self.editorMessage.text = [view.text lengthOfBytesUsingEncoding:NSUTF8StringEncoding] > 4000 ? @"Please shorten this note (4,000-byte limit)." : @"Only Done saves your writing.";
}
- (void)finishEditor {
    [self endEditing:YES]; [self.editor removeFromSuperview]; self.editor = nil;
    if (self.access.textInput) self.access.textInput(0);
    [self searchBar:self.searchBar textDidChange:self.searchBar.text ?: @""];
}
- (void)cancelNote {
    if (self.confirmingDelete) {
        self.confirmingDelete = NO; self.noteText.editable = YES; self.deleteNoteButton.hidden = NO;
        [self.doneNoteButton setTitle:@"Done" forState:UIControlStateNormal]; [self.cancelNoteButton setTitle:@"Cancel" forState:UIControlStateNormal];
        [self textViewDidChange:self.noteText]; return;
    }
    [self finishEditor];
}
- (void)confirmDeleteNote {
    [self endEditing:YES]; self.confirmingDelete = YES; self.noteText.editable = NO; self.deleteNoteButton.hidden = YES;
    self.editorMessage.text = @"Delete this personal note? The original passage and bookmark will remain.";
    [self.doneNoteButton setTitle:@"Delete note" forState:UIControlStateNormal]; self.doneNoteButton.enabled = YES;
    [self.cancelNoteButton setTitle:@"Keep note" forState:UIControlStateNormal];
}
- (void)doneNote {
    uint64_t identifier = self.confirmingDelete ? (self.access.deleteNote && self.access.deleteNote(self.editingIdentifier) ? 1 : 0) :
        (self.access.saveNote ? self.access.saveNote(self.editingPassage, self.editingIdentifier, self.noteText.text.UTF8String) : 0);
    if (!identifier) { self.editorMessage.text = @"Could not save. Your draft is still here; try again or Cancel."; return; }
    BOOL deleted = self.confirmingDelete;
    NSDictionary *group = self.selectedGroup;
    if (group) self.savedOffsets[group[@"key"]] = @(self.detailScroll.contentOffset.y);
    [self finishEditor];
    if (deleted) [self backToIndex];
    else if (group) [self showGroup:group];
    else {
        for (NSDictionary *note in self.noteRows) if ([note[@"id"] unsignedLongLongValue] == identifier) { [self showNote:note]; return; }
        [self changeMode];
    }
}
- (void)rebuildSections {
    NSMutableOrderedSet<NSString *> *locations = [NSMutableOrderedSet orderedSet];
    for (NSDictionary *group in self.filteredGroups) [locations addObject:group[@"section"]];
    self.sections = [locations.array sortedArrayUsingComparator:^NSComparisonResult(NSString *a, NSString *b) {
        BOOL aUnknown = [a isEqualToString:@"Location not recorded"];
        BOOL bUnknown = [b isEqualToString:@"Location not recorded"];
        if (aUnknown != bUnknown) return aUnknown ? NSOrderedDescending : NSOrderedAscending;
        return [a localizedStandardCompare:b];
    }];
    self.listPage = 0;
    [self updateListNavigation];
}
@end

void zu4_journal_panel_show(const char **texts, const char **sources, const char **speakers,
                            const char **places, const char **kinds, const char **topics,
                            const int *indices, int count, const Zu4JournalAccess *access) {
    if (!NSThread.isMainThread) {
        dispatch_sync(dispatch_get_main_queue(), ^{
            zu4_journal_panel_show(texts, sources, speakers, places, kinds, topics, indices, count, access);
        });
        return;
    }
    UIWindow *window = nil;
    for (UIWindow *candidate in UIApplication.sharedApplication.windows)
        if (candidate.isKeyWindow) { window = candidate; break; }
    if (!window) return;
    Zu4JournalPanel *panel = [[Zu4JournalPanel alloc] init];
    if (access) panel.access = *access;
    panel.overrideUserInterfaceStyle = UIUserInterfaceStyleDark;
    panel.translatesAutoresizingMaskIntoConstraints = NO;
    panel.backgroundColor = [UIColor colorWithWhite:0 alpha:0.45];
    panel.accessibilityViewIsModal = YES;
    panel.savedOffsets = [NSMutableDictionary dictionary];
    panel.card = [[UIView alloc] init];
    panel.card.backgroundColor = [UIColor colorWithRed:0.055 green:0.075 blue:0.09 alpha:0.99];
    panel.card.layer.cornerRadius = 16;
    panel.card.layer.borderWidth = 1;
    panel.card.layer.borderColor = [UIColor colorWithWhite:0.35 alpha:1].CGColor;
    panel.card.clipsToBounds = YES;
    [panel addSubview:panel.card];

    panel.backButton = [UIButton buttonWithType:UIButtonTypeSystem];
    [panel.backButton setTitle:@"‹ Journal" forState:UIControlStateNormal];
    panel.backButton.titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
    [panel.backButton addTarget:panel action:@selector(backToIndex) forControlEvents:UIControlEventTouchUpInside];
    panel.backButton.accessibilityLabel = @"Back to journal index";
    panel.backButton.hidden = YES;
    panel.backButton.translatesAutoresizingMaskIntoConstraints = NO;
    [panel.card addSubview:panel.backButton];
    panel.titleLabel = journalLabel(@"Journal", UIFontTextStyleTitle2, UIColor.whiteColor);
    panel.titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleTitle2];
    panel.titleLabel.textAlignment = NSTextAlignmentCenter;
    panel.titleLabel.numberOfLines = 1;
    panel.titleLabel.adjustsFontSizeToFitWidth = YES;
    panel.titleLabel.minimumScaleFactor = 0.7;
    panel.titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    [panel.card addSubview:panel.titleLabel];
    UIButton *close = [UIButton buttonWithType:UIButtonTypeSystem];
    [close setTitle:@"Close" forState:UIControlStateNormal];
    close.titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
    close.accessibilityLabel = @"Close journal";
    [close addTarget:panel action:@selector(closeJournal) forControlEvents:UIControlEventTouchUpInside];
    close.translatesAutoresizingMaskIntoConstraints = NO;
    [panel.card addSubview:close];

    panel.modeControl = [[UIStackView alloc] init];
    panel.modeControl.axis = UILayoutConstraintAxisHorizontal;
    panel.modeControl.distribution = UIStackViewDistributionFillEqually;
    panel.modeControl.spacing = 8;
    panel.modeControl.translatesAutoresizingMaskIntoConstraints = NO;
    NSMutableArray *modeButtons = [NSMutableArray array];
    for (NSString *title in @[@"Places", @"Current clues", @"My notes"]) {
        UIButton *button = topicButtonWithDensity(title, panel, @selector(chooseMode:), YES);
        button.tag = modeButtons.count;
        button.titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
        button.titleLabel.adjustsFontSizeToFitWidth = YES;
        button.titleLabel.minimumScaleFactor = 0.7;
        button.backgroundColor = [UIColor colorWithWhite:button.tag == 0 ? 0.32 : 0.16 alpha:1];
        button.accessibilityTraits = UIAccessibilityTraitButton | (button.tag == 0 ? UIAccessibilityTraitSelected : 0);
        [panel.modeControl addArrangedSubview:button]; [modeButtons addObject:button];
    }
    panel.modeButtons = modeButtons;
    [panel.card addSubview:panel.modeControl];

    panel.searchBar = [[UISearchBar alloc] init];
    panel.searchBar.placeholder = @"Search people, places, topics, and notes";
    panel.searchBar.searchBarStyle = UISearchBarStyleMinimal;
    panel.searchBar.delegate = panel;
    panel.searchBar.translatesAutoresizingMaskIntoConstraints = NO;
    [panel.card addSubview:panel.searchBar];
    panel.tableView = [[UITableView alloc] initWithFrame:CGRectZero style:UITableViewStylePlain];
    panel.tableView.backgroundColor = UIColor.clearColor;
    panel.tableView.separatorColor = [UIColor colorWithWhite:0.25 alpha:1];
    panel.tableView.dataSource = panel;
    panel.tableView.delegate = panel;
    panel.tableView.rowHeight = UITableViewAutomaticDimension;
    panel.tableView.estimatedRowHeight = 60;
    panel.tableView.alwaysBounceVertical = NO;
    panel.tableView.scrollEnabled = NO;
    panel.tableView.showsVerticalScrollIndicator = NO;
    panel.tableView.translatesAutoresizingMaskIntoConstraints = NO;
    [panel.card addSubview:panel.tableView];

    panel.listNavigation = [[UIView alloc] init];
    panel.listNavigation.translatesAutoresizingMaskIntoConstraints = NO;
    [panel.card addSubview:panel.listNavigation];
    UIStackView *listButtons = [[UIStackView alloc] init];
    listButtons.axis = UILayoutConstraintAxisHorizontal;
    listButtons.distribution = UIStackViewDistributionFillEqually;
    listButtons.spacing = 10;
    listButtons.translatesAutoresizingMaskIntoConstraints = NO;
    [panel.listNavigation addSubview:listButtons];
    panel.previousListButton = topicButton(@"‹ Prev", panel, @selector(moveList:));
    panel.previousListButton.tag = -1;
    panel.previousListButton.accessibilityLabel = @"Previous journal page";
    panel.listPageLabel = journalLabel(@"1 of 1", UIFontTextStyleSubheadline,
        [UIColor colorWithWhite:0.72 alpha:1]);
    panel.listPageLabel.textAlignment = NSTextAlignmentCenter;
    panel.nextListButton = topicButton(@"Next ›", panel, @selector(moveList:));
    panel.nextListButton.tag = 1;
    panel.nextListButton.accessibilityLabel = @"Next journal page";
    [listButtons addArrangedSubview:panel.previousListButton];
    [listButtons addArrangedSubview:panel.listPageLabel];
    panel.addNoteButton = topicButton(@"New note", panel, @selector(newNote));
    panel.addNoteButton.hidden = YES;
    [listButtons addArrangedSubview:panel.addNoteButton];
    [listButtons addArrangedSubview:panel.nextListButton];
    for (UIButton *button in @[panel.previousListButton, panel.nextListButton, panel.addNoteButton])
        button.titleLabel.font = [UIFont systemFontOfSize:13 weight:UIFontWeightSemibold];
    panel.detailScroll = [[UIScrollView alloc] init];
    panel.detailScroll.alwaysBounceVertical = NO;
    panel.detailScroll.bounces = NO;
    // UIKit's drag recognizer can remain in tracking under SDL's nested event
    // loop, which also suppresses the journal's buttons. Keep the transcript
    // continuous, but advance it with fixed viewport controls instead.
    panel.detailScroll.scrollEnabled = NO;
    panel.detailScroll.showsVerticalScrollIndicator = NO;
    panel.detailScroll.hidden = YES;
    panel.detailScroll.translatesAutoresizingMaskIntoConstraints = NO;
    [panel.card addSubview:panel.detailScroll];
    panel.detailStack = [[UIStackView alloc] init];
    panel.detailStack.axis = UILayoutConstraintAxisVertical;
    panel.detailStack.spacing = 12;
    panel.detailStack.translatesAutoresizingMaskIntoConstraints = NO;
    [panel.detailScroll addSubview:panel.detailStack];

    panel.detailNavigation = [[UIView alloc] init];
    panel.detailNavigation.hidden = YES;
    panel.detailNavigation.translatesAutoresizingMaskIntoConstraints = NO;
    [panel.card addSubview:panel.detailNavigation];
    UIStackView *detailButtons = [[UIStackView alloc] init];
    detailButtons.axis = UILayoutConstraintAxisHorizontal;
    detailButtons.distribution = UIStackViewDistributionFillEqually;
    detailButtons.spacing = 10;
    detailButtons.translatesAutoresizingMaskIntoConstraints = NO;
    [panel.detailNavigation addSubview:detailButtons];
    panel.previousDetailButton = topicButton(@"‹ Previous", panel, @selector(moveDetail:));
    panel.previousDetailButton.tag = -1;
    panel.previousDetailButton.accessibilityLabel = @"Previous journal section";
    panel.nextDetailButton = topicButton(@"Next ›", panel, @selector(moveDetail:));
    panel.nextDetailButton.tag = 1;
    panel.nextDetailButton.accessibilityLabel = @"Next journal section";
    [detailButtons addArrangedSubview:panel.previousDetailButton];
    [detailButtons addArrangedSubview:panel.nextDetailButton];

    [NSLayoutConstraint activateConstraints:@[
        [panel.backButton.leadingAnchor constraintEqualToAnchor:panel.card.leadingAnchor constant:12],
        [panel.backButton.topAnchor constraintEqualToAnchor:panel.card.topAnchor constant:8],
        [panel.backButton.widthAnchor constraintGreaterThanOrEqualToConstant:80],
        [panel.backButton.heightAnchor constraintGreaterThanOrEqualToConstant:44],
        [close.trailingAnchor constraintEqualToAnchor:panel.card.trailingAnchor constant:-12],
        [close.topAnchor constraintEqualToAnchor:panel.card.topAnchor constant:8],
        [close.widthAnchor constraintGreaterThanOrEqualToConstant:64],
        [close.heightAnchor constraintGreaterThanOrEqualToConstant:44],
        [panel.titleLabel.leadingAnchor constraintGreaterThanOrEqualToAnchor:panel.backButton.trailingAnchor constant:8],
        [panel.titleLabel.trailingAnchor constraintLessThanOrEqualToAnchor:close.leadingAnchor constant:-8],
        [panel.titleLabel.centerXAnchor constraintEqualToAnchor:panel.card.centerXAnchor],
        [panel.titleLabel.centerYAnchor constraintEqualToAnchor:close.centerYAnchor],
        [panel.searchBar.leadingAnchor constraintEqualToAnchor:panel.card.leadingAnchor constant:8],
        [panel.searchBar.trailingAnchor constraintEqualToAnchor:panel.card.trailingAnchor constant:-8],
        [panel.modeControl.leadingAnchor constraintEqualToAnchor:panel.card.leadingAnchor constant:12],
        [panel.modeControl.trailingAnchor constraintEqualToAnchor:panel.card.trailingAnchor constant:-12],
        [panel.modeControl.topAnchor constraintEqualToAnchor:close.bottomAnchor constant:2],
        [panel.modeControl.heightAnchor constraintEqualToConstant:44],
        [panel.searchBar.topAnchor constraintEqualToAnchor:panel.modeControl.bottomAnchor constant:2],
        [panel.tableView.leadingAnchor constraintEqualToAnchor:panel.card.leadingAnchor constant:8],
        [panel.tableView.trailingAnchor constraintEqualToAnchor:panel.card.trailingAnchor constant:-8],
        [panel.tableView.topAnchor constraintEqualToAnchor:panel.searchBar.bottomAnchor],
        [panel.tableView.bottomAnchor constraintEqualToAnchor:panel.listNavigation.topAnchor constant:-8],
        [panel.listNavigation.leadingAnchor constraintEqualToAnchor:panel.card.leadingAnchor constant:14],
        [panel.listNavigation.trailingAnchor constraintEqualToAnchor:panel.card.trailingAnchor constant:-14],
        [panel.listNavigation.bottomAnchor constraintEqualToAnchor:panel.card.bottomAnchor constant:-12],
        [panel.listNavigation.heightAnchor constraintEqualToConstant:48],
        [listButtons.leadingAnchor constraintEqualToAnchor:panel.listNavigation.leadingAnchor],
        [listButtons.trailingAnchor constraintEqualToAnchor:panel.listNavigation.trailingAnchor],
        [listButtons.topAnchor constraintEqualToAnchor:panel.listNavigation.topAnchor],
        [listButtons.bottomAnchor constraintEqualToAnchor:panel.listNavigation.bottomAnchor],
        [panel.detailScroll.leadingAnchor constraintEqualToAnchor:panel.card.leadingAnchor constant:14],
        [panel.detailScroll.trailingAnchor constraintEqualToAnchor:panel.card.trailingAnchor constant:-14],
        [panel.detailScroll.topAnchor constraintEqualToAnchor:panel.modeControl.bottomAnchor constant:8],
        [panel.detailScroll.bottomAnchor constraintEqualToAnchor:panel.detailNavigation.topAnchor constant:-10],
        [panel.detailStack.leadingAnchor constraintEqualToAnchor:panel.detailScroll.contentLayoutGuide.leadingAnchor],
        [panel.detailStack.trailingAnchor constraintEqualToAnchor:panel.detailScroll.contentLayoutGuide.trailingAnchor],
        [panel.detailStack.topAnchor constraintEqualToAnchor:panel.detailScroll.contentLayoutGuide.topAnchor],
        [panel.detailStack.bottomAnchor constraintEqualToAnchor:panel.detailScroll.contentLayoutGuide.bottomAnchor],
        [panel.detailStack.widthAnchor constraintEqualToAnchor:panel.detailScroll.frameLayoutGuide.widthAnchor],
        [panel.detailNavigation.leadingAnchor constraintEqualToAnchor:panel.card.leadingAnchor constant:14],
        [panel.detailNavigation.trailingAnchor constraintEqualToAnchor:panel.card.trailingAnchor constant:-14],
        [panel.detailNavigation.bottomAnchor constraintEqualToAnchor:panel.card.bottomAnchor constant:-12],
        [panel.detailNavigation.heightAnchor constraintEqualToConstant:48],
        [detailButtons.leadingAnchor constraintEqualToAnchor:panel.detailNavigation.leadingAnchor],
        [detailButtons.trailingAnchor constraintEqualToAnchor:panel.detailNavigation.trailingAnchor],
        [detailButtons.topAnchor constraintEqualToAnchor:panel.detailNavigation.topAnchor],
        [detailButtons.bottomAnchor constraintEqualToAnchor:panel.detailNavigation.bottomAnchor]]];

    NSMutableDictionary<NSString *, NSMutableDictionary *> *byKey = [NSMutableDictionary dictionary];
    for (int i = 0; i < count; ++i) {
        NSString *speaker = journalString(speakers[i]);
        NSString *source = journalString(sources[i]);
        NSString *place = journalString(places[i]);
        NSString *kind = journalString(kinds[i]);
        NSString *location = place;
        NSString *legacyPrefix = @"Conversation in ";
        if (!location.length && [source hasPrefix:legacyPrefix] && source.length > legacyPrefix.length)
            location = [source substringFromIndex:legacyPrefix.length];
        if (!location.length) location = @"Location not recorded";
        NSString *kindLabel = speaker.length ? @"Person" :
            ([kind isEqualToString:@"vision"] ? @"Vision" :
             ([kind isEqualToString:@"writing"] ? @"Writing" : @"Earlier conversation"));
        NSString *title = speaker.length ? speaker : source;
        if (!title.length) title = kindLabel;
        NSString *key = [NSString stringWithFormat:@"%@|%@|%@", location, kindLabel, title];
        NSMutableDictionary *group = byKey[key];
        if (!group) {
            group = [@{@"key": key, @"section": location, @"title": title, @"kindLabel": kindLabel,
                @"places": [NSMutableArray array], @"entries": [NSMutableArray array], @"order": @(i)} mutableCopy];
            byKey[key] = group;
        }
        if (![group[@"places"] containsObject:location]) [group[@"places"] addObject:location];
        [group[@"entries"] addObject:@{@"text": journalString(texts[i]), @"place": location,
            @"topic": journalString(topics[i]), @"index":@(indices ? indices[i] : i)}];
        group[@"order"] = @(i);
    }
    panel.groups = [[byKey allValues] sortedArrayUsingComparator:^NSComparisonResult(NSDictionary *a, NSDictionary *b) {
        return [b[@"order"] compare:a[@"order"]];
    }];
    panel.filteredGroups = panel.groups;
    [panel refreshNotes];
    [panel rebuildSections];
    if (!count) {
        UILabel *empty = journalLabel(@"Conversations, writings, and shrine visions will appear here after you encounter them.",
            UIFontTextStyleBody, [UIColor colorWithWhite:0.75 alpha:1]);
        empty.textAlignment = NSTextAlignmentCenter;
        panel.tableView.backgroundView = empty;
    }
    [window addSubview:panel];
    [NSNotificationCenter.defaultCenter addObserver:panel selector:@selector(journalKeyboardChanged:) name:UIKeyboardWillChangeFrameNotification object:nil];
    [NSNotificationCenter.defaultCenter addObserver:panel selector:@selector(journalKeyboardHidden:) name:UIKeyboardWillHideNotification object:nil];
    [NSLayoutConstraint activateConstraints:@[
        [panel.leadingAnchor constraintEqualToAnchor:window.leadingAnchor],
        [panel.trailingAnchor constraintEqualToAnchor:window.trailingAnchor],
        [panel.topAnchor constraintEqualToAnchor:window.topAnchor],
        [panel.bottomAnchor constraintEqualToAnchor:window.bottomAnchor]]];
    UIAccessibilityPostNotification(UIAccessibilityScreenChangedNotification, panel.titleLabel);
}

int zu4_journal_panel_is_visible(void) {
    for (UIWindow *window in UIApplication.sharedApplication.windows)
        for (UIView *view in window.subviews)
            if ([view isKindOfClass:Zu4JournalPanel.class]) return 1;
    return 0;
}

void zu4_topic_panel_show(const char *text, const char **keywords, const char **labels, const int *roles,
                         int count, int maxLength, int nameEntry, int compactDetails,
                         Zu4TopicPanelStyle style, Zu4TopicSubmit submit, void *context) {
    if (!NSThread.isMainThread) {
        dispatch_sync(dispatch_get_main_queue(), ^{
            zu4_topic_panel_show(text, keywords, labels, roles, count, maxLength, nameEntry,
                                 compactDetails, style, submit, context);
        });
        return;
    }
    UIWindow *window = nil;
    for (UIWindow *candidate in UIApplication.sharedApplication.windows)
        if (candidate.isKeyWindow) { window = candidate; break; }
    if (!window) { submit("bye", 0, context); return; }
    Zu4TopicPanel *panel = [[Zu4TopicPanel alloc] init];
    panel.translatesAutoresizingMaskIntoConstraints = NO;
    panel.backgroundColor = UIColor.clearColor;
    panel.card = [[UIView alloc] init];
    panel.card.backgroundColor = [UIColor colorWithRed:0.055 green:0.075 blue:0.09 alpha:0.98];
    panel.card.layer.cornerRadius = 16;
    panel.card.layer.borderWidth = 1;
    panel.card.layer.borderColor = [UIColor colorWithWhite:0.35 alpha:1].CGColor;
    panel.card.clipsToBounds = YES;
    [panel addSubview:panel.card];
    panel.submit = submit;
    panel.context = context;
    panel.maxLength = maxLength;
    panel.compactMenuMode = style == ZU4_TOPIC_PANEL_COMPACT_MENU;
    panel.denseFullScreenMode = style == ZU4_TOPIC_PANEL_DENSE_FULLSCREEN;
    panel.fullScreenMode = style == ZU4_TOPIC_PANEL_FULLSCREEN || panel.denseFullScreenMode || style == ZU4_TOPIC_PANEL_READING_FULLSCREEN;
    panel.controlsOnlyMode = style == ZU4_TOPIC_PANEL_CONTROLS_ONLY;
    panel.partySelectionMode = style == ZU4_TOPIC_PANEL_PARTY_SELECTION;
    if (panel.fullScreenMode) panel.backgroundColor = UIColor.blackColor;
    if (panel.controlsOnlyMode) {
        panel.card.backgroundColor = UIColor.clearColor;
        panel.card.layer.borderWidth = 0;
        panel.card.layer.cornerRadius = 0;
        panel.card.clipsToBounds = NO;
    }
    panel.directionMode = count == 5 && !strcmp(keywords[0], "north") &&
        !strcmp(keywords[1], "south") && !strcmp(keywords[2], "west") &&
        !strcmp(keywords[3], "east") && !strcmp(keywords[4], "cancel");
    panel.spellbookMode = !strncmp(text, "Spellbook", 9);
    BOOL hasGoodbye = NO;
    NSInteger goodbyeIndex = NSNotFound;
    NSInteger resumeIndex = NSNotFound;
    NSInteger backIndex = NSNotFound;
    NSInteger dismissIndex = NSNotFound;
    NSInteger pagePreviousIndex = NSNotFound;
    NSInteger pageNextIndex = NSNotFound;
    NSInteger pageLabelIndex = NSNotFound;
    for (int i = 0; i < count; ++i) {
        if (!strcmp(keywords[i], "bye")) { hasGoodbye = YES; goodbyeIndex = i; }
        if (!strcmp(keywords[i], "resume")) resumeIndex = i;
        if (roles && roles[i] == ZU4_TOPIC_CHOICE_BACK && backIndex == NSNotFound) backIndex = i;
        if (roles && roles[i] == ZU4_TOPIC_CHOICE_DISMISS && dismissIndex == NSNotFound) dismissIndex = i;
        if (!strncmp(keywords[i], "__page_previous", 15)) pagePreviousIndex = i;
        if (!strncmp(keywords[i], "__page_next", 11)) pageNextIndex = i;
        if (!strcmp(keywords[i], "__page_label")) pageLabelIndex = i;
    }
    panel.conversationMode = !nameEntry && maxLength > 0 && hasGoodbye;
    panel.accessibilityViewIsModal = YES;
    [NSNotificationCenter.defaultCenter addObserver:panel selector:@selector(keyboardChanged:)
        name:UIKeyboardWillShowNotification object:nil];
    [NSNotificationCenter.defaultCenter addObserver:panel selector:@selector(keyboardChanged:)
        name:UIKeyboardWillHideNotification object:nil];
    NSMutableArray *keys = [NSMutableArray array];
    [window addSubview:panel];
    [NSLayoutConstraint activateConstraints:@[
        [panel.leadingAnchor constraintEqualToAnchor:window.leadingAnchor],
        [panel.trailingAnchor constraintEqualToAnchor:window.trailingAnchor],
        [panel.topAnchor constraintEqualToAnchor:window.topAnchor],
        [panel.bottomAnchor constraintEqualToAnchor:window.bottomAnchor]]];
    UIScrollView *scroll = [[Zu4PanelScroll alloc] init];
    scroll.canCancelContentTouches = YES;
    scroll.delaysContentTouches = YES;
    scroll.bounces = YES;
    scroll.alwaysBounceVertical = NO;
    scroll.directionalLockEnabled = YES;
    scroll.decelerationRate = UIScrollViewDecelerationRateFast;
    scroll.delegate = panel;
    scroll.indicatorStyle = UIScrollViewIndicatorStyleWhite;
    scroll.translatesAutoresizingMaskIntoConstraints = NO;
    scroll.keyboardDismissMode = UIScrollViewKeyboardDismissModeOnDrag;
    if (style != ZU4_TOPIC_PANEL_READING_FULLSCREEN && (panel.conversationMode || panel.compactMenuMode || panel.fullScreenMode || panel.controlsOnlyMode || panel.partySelectionMode)) {
        // Conversation pages are sized to fit. Removing the pan recognizer
        // prevents a stray drag from trapping all subsequent button taps. The
        // compact pause menu and progressively disclosed Experience screens
        // are also designed to fit without scrolling.
        scroll.scrollEnabled = NO;
        scroll.bounces = NO;
    }
    panel.scrollView = scroll;
    if (@available(iOS 11.0, *))
        if (panel.controlsOnlyMode)
            scroll.contentInsetAdjustmentBehavior = UIScrollViewContentInsetAdjustmentNever;
    [panel.card addSubview:scroll];
    BOOL headedPanel = !panel.conversationMode &&
        (panel.compactMenuMode || panel.fullScreenMode || panel.spellbookMode ||
         backIndex != NSNotFound || dismissIndex != NSNotFound);
    CGFloat scrollTop = panel.conversationMode ? 56 :
        (headedPanel ? 68 : (panel.controlsOnlyMode ? 4 : 12));
    CGFloat scrollInset = panel.controlsOnlyMode ? 4 : 12;
    BOOL hasPageControls = pagePreviousIndex != NSNotFound || pageNextIndex != NSNotFound;
    if (panel.spellbookMode || hasPageControls) {
        UIStackView *footer = [[UIStackView alloc] init];
        footer.axis = UILayoutConstraintAxisVertical;
        footer.spacing = 6;
        footer.translatesAutoresizingMaskIntoConstraints = NO;
        panel.persistentFooter = footer;
        [panel.card addSubview:footer];
        [NSLayoutConstraint activateConstraints:@[
            [footer.leadingAnchor constraintEqualToAnchor:panel.card.leadingAnchor constant:12],
            [footer.trailingAnchor constraintEqualToAnchor:panel.card.trailingAnchor constant:-12],
            [footer.bottomAnchor constraintEqualToAnchor:panel.card.bottomAnchor constant:-8]]];
    }
    NSLayoutYAxisAnchor *scrollBottomAnchor = panel.persistentFooter
        ? panel.persistentFooter.topAnchor : panel.card.bottomAnchor;
    CGFloat scrollBottomInset = panel.persistentFooter ? -8 : -scrollInset;
    [NSLayoutConstraint activateConstraints:@[
        [scroll.leadingAnchor constraintEqualToAnchor:panel.card.leadingAnchor constant:scrollInset],
        [scroll.trailingAnchor constraintEqualToAnchor:panel.card.trailingAnchor constant:-scrollInset],
        [scroll.topAnchor constraintEqualToAnchor:panel.card.topAnchor constant:scrollTop],
        [scroll.bottomAnchor constraintEqualToAnchor:scrollBottomAnchor constant:scrollBottomInset]]];
    if (panel.conversationMode) {
        UILabel *heading = journalLabel(@"Conversation", UIFontTextStyleSubheadline,
            [UIColor colorWithWhite:0.72 alpha:1]);
        heading.translatesAutoresizingMaskIntoConstraints = NO;
        [panel.card addSubview:heading];
        UIButton *goodbye = [UIButton buttonWithType:UIButtonTypeSystem];
        [goodbye setTitle:@"Goodbye" forState:UIControlStateNormal];
        goodbye.titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
        goodbye.accessibilityLabel = @"End conversation";
        goodbye.tag = goodbyeIndex;
        [goodbye addTarget:panel action:@selector(choose:) forControlEvents:UIControlEventTouchUpInside];
        goodbye.translatesAutoresizingMaskIntoConstraints = NO;
        [panel.card addSubview:goodbye];
        [NSLayoutConstraint activateConstraints:@[
            [heading.leadingAnchor constraintEqualToAnchor:panel.card.leadingAnchor constant:16],
            [heading.centerYAnchor constraintEqualToAnchor:goodbye.centerYAnchor],
            [goodbye.trailingAnchor constraintEqualToAnchor:panel.card.trailingAnchor constant:-16],
            [goodbye.topAnchor constraintEqualToAnchor:panel.card.topAnchor constant:6],
            [goodbye.heightAnchor constraintGreaterThanOrEqualToConstant:44],
            [goodbye.widthAnchor constraintGreaterThanOrEqualToConstant:88]]];
    }
    NSString *rawText = [NSString stringWithUTF8String:text];
    NSString *headingText = nil;
    if (headedPanel) {
        NSRange sectionBreak = [rawText rangeOfString:@"\n\n"];
        if (sectionBreak.location == NSNotFound) {
            sectionBreak = [rawText rangeOfString:@"\n"];
        }
        if (sectionBreak.location != NSNotFound) {
            headingText = [rawText substringToIndex:sectionBreak.location];
            rawText = [rawText substringFromIndex:NSMaxRange(sectionBreak)];
        } else {
            headingText = rawText;
            rawText = @"";
        }
        UIButton *leadingAction = nil;
        if (backIndex != NSNotFound) {
            leadingAction = [UIButton buttonWithType:UIButtonTypeSystem];
            NSString *title = [NSString stringWithUTF8String:labels[backIndex]];
            if (![title hasPrefix:@"‹"]) title = [@"‹ " stringByAppendingString:title];
            [leadingAction setTitle:title forState:UIControlStateNormal];
            leadingAction.titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
            [leadingAction setContentHuggingPriority:UILayoutPriorityRequired
                                            forAxis:UILayoutConstraintAxisHorizontal];
            [leadingAction setContentCompressionResistancePriority:UILayoutPriorityRequired
                                                           forAxis:UILayoutConstraintAxisHorizontal];
            leadingAction.accessibilityIdentifier = @"zu4-panel-navigation";
            leadingAction.accessibilityLabel = [NSString stringWithFormat:@"Back, %@",
                [NSString stringWithUTF8String:labels[backIndex]]];
            leadingAction.tag = backIndex;
            [leadingAction addTarget:panel action:@selector(choose:) forControlEvents:UIControlEventTouchUpInside];
            leadingAction.translatesAutoresizingMaskIntoConstraints = NO;
            [panel.card addSubview:leadingAction];
        }
        UIButton *headerAction = nil;
        NSInteger headerActionIndex = dismissIndex;
        if (headerActionIndex == NSNotFound && panel.compactMenuMode && resumeIndex != NSNotFound)
            headerActionIndex = resumeIndex;
        if (headerActionIndex != NSNotFound) {
            headerAction = [UIButton buttonWithType:UIButtonTypeSystem];
            NSString *title = [NSString stringWithUTF8String:labels[headerActionIndex]];
            [headerAction setTitle:title forState:UIControlStateNormal];
            headerAction.titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
            [headerAction setContentHuggingPriority:UILayoutPriorityRequired
                                           forAxis:UILayoutConstraintAxisHorizontal];
            [headerAction setContentCompressionResistancePriority:UILayoutPriorityRequired
                                                          forAxis:UILayoutConstraintAxisHorizontal];
            if (!strcmp(keywords[headerActionIndex], "resume"))
                headerAction.tintColor = [UIColor colorWithRed:0.94 green:0.81 blue:0.49 alpha:1];
            headerAction.accessibilityIdentifier = @"zu4-panel-navigation";
            headerAction.accessibilityLabel = title;
            headerAction.tag = headerActionIndex;
            [headerAction addTarget:panel action:@selector(choose:) forControlEvents:UIControlEventTouchUpInside];
            headerAction.translatesAutoresizingMaskIntoConstraints = NO;
            [panel.card addSubview:headerAction];
        }
        UILabel *heading = journalLabel(headingText, UIFontTextStyleTitle2, UIColor.whiteColor);
        heading.font = [UIFont preferredFontForTextStyle:UIFontTextStyleTitle2];
        heading.numberOfLines = 1;
        heading.lineBreakMode = NSLineBreakByTruncatingTail;
        [heading setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                                 forAxis:UILayoutConstraintAxisHorizontal];
        heading.translatesAutoresizingMaskIntoConstraints = NO;
        panel.headingLabel = heading;
        [panel.card addSubview:heading];
        NSMutableArray<NSLayoutConstraint *> *headingConstraints = [NSMutableArray arrayWithObject:
            [heading.topAnchor constraintEqualToAnchor:panel.card.topAnchor constant:12]];
        if (leadingAction) {
            [headingConstraints addObjectsFromArray:@[
                [leadingAction.leadingAnchor constraintEqualToAnchor:panel.card.leadingAnchor constant:12],
                [leadingAction.topAnchor constraintEqualToAnchor:panel.card.topAnchor constant:6],
                [leadingAction.heightAnchor constraintGreaterThanOrEqualToConstant:44],
                [leadingAction.widthAnchor constraintGreaterThanOrEqualToConstant:44],
                [heading.leadingAnchor constraintGreaterThanOrEqualToAnchor:leadingAction.trailingAnchor constant:8]]];
            NSLayoutConstraint *centeredHeading =
                [heading.centerXAnchor constraintEqualToAnchor:panel.card.centerXAnchor];
            centeredHeading.priority = UILayoutPriorityDefaultHigh;
            [headingConstraints addObject:centeredHeading];
        } else {
            [headingConstraints addObject:
                [heading.leadingAnchor constraintEqualToAnchor:panel.card.leadingAnchor constant:16]];
        }
        if (headerAction) {
            [headingConstraints addObjectsFromArray:@[
                [heading.trailingAnchor constraintLessThanOrEqualToAnchor:headerAction.leadingAnchor constant:-8],
                [headerAction.trailingAnchor constraintEqualToAnchor:panel.card.trailingAnchor constant:-12],
                [headerAction.topAnchor constraintEqualToAnchor:panel.card.topAnchor constant:6],
                [headerAction.heightAnchor constraintGreaterThanOrEqualToConstant:44],
                [headerAction.widthAnchor constraintGreaterThanOrEqualToConstant:78]]];
        } else {
            [headingConstraints addObject:[heading.trailingAnchor
                constraintLessThanOrEqualToAnchor:panel.card.trailingAnchor constant:-16]];
        }
        [NSLayoutConstraint activateConstraints:headingConstraints];
        if (panel.compactMenuMode && [headingText isEqualToString:@"Ultima IV"]) {
            NSString *buildNumber = NSBundle.mainBundle.infoDictionary[@"CFBundleVersion"] ?: @"?";
            UILabel *buildLabel = journalLabel(
                [NSString stringWithFormat:@"Build %@", buildNumber],
                UIFontTextStyleCaption1, [UIColor colorWithRed:0.94 green:0.81 blue:0.49 alpha:1]);
            buildLabel.font = [UIFont monospacedDigitSystemFontOfSize:12 weight:UIFontWeightSemibold];
            buildLabel.textAlignment = NSTextAlignmentCenter;
            buildLabel.accessibilityLabel = [NSString stringWithFormat:@"Build %@", buildNumber];
            buildLabel.layer.shadowColor = UIColor.blackColor.CGColor;
            buildLabel.layer.shadowOpacity = 0.9;
            buildLabel.layer.shadowRadius = 2;
            buildLabel.layer.shadowOffset = CGSizeZero;
            buildLabel.translatesAutoresizingMaskIntoConstraints = NO;
            [panel addSubview:buildLabel];
            [NSLayoutConstraint activateConstraints:@[
                [buildLabel.centerXAnchor constraintEqualToAnchor:panel.safeAreaLayoutGuide.centerXAnchor],
                [buildLabel.topAnchor constraintEqualToAnchor:panel.safeAreaLayoutGuide.topAnchor constant:8],
                [buildLabel.heightAnchor constraintGreaterThanOrEqualToConstant:24]]];
        }
    }
    // Reagent merchants use six short choices plus Cancel. At the standard
    // 48-point row height that grid overflows the portrait sheet by only a few
    // points, needlessly making the whole list scroll. Keep the playbook's
    // 44-point minimum target while reclaiming enough vertical room to fit.
    BOOL compactChoiceGrid = !panel.conversationMode &&
        [rawText rangeOfString:@"Your Interest:" options:NSCaseInsensitiveSearch].location != NSNotFound;
    UIStackView *stack = [[UIStackView alloc] init];
    stack.axis = UILayoutConstraintAxisVertical;
    stack.spacing = (compactChoiceGrid || panel.denseFullScreenMode || panel.partySelectionMode) ? 6 : (panel.compactMenuMode ? 8 : 10);
    stack.translatesAutoresizingMaskIntoConstraints = NO;
    panel.stack = stack;
    [scroll addSubview:stack];
    [NSLayoutConstraint activateConstraints:@[
        [stack.leadingAnchor constraintEqualToAnchor:scroll.contentLayoutGuide.leadingAnchor],
        [stack.trailingAnchor constraintEqualToAnchor:scroll.contentLayoutGuide.trailingAnchor],
        [stack.topAnchor constraintEqualToAnchor:scroll.contentLayoutGuide.topAnchor],
        [stack.bottomAnchor constraintEqualToAnchor:scroll.contentLayoutGuide.bottomAnchor],
        [stack.widthAnchor constraintEqualToAnchor:scroll.frameLayoutGuide.widthAnchor]]];
    if (panel.conversationMode)
        [stack.heightAnchor constraintEqualToAnchor:scroll.frameLayoutGuide.heightAnchor].active = YES;
    UILabel *body = [[UILabel alloc] init];
    body.numberOfLines = 0;
    body.textColor = UIColor.whiteColor;
    body.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    body.adjustsFontForContentSizeCategory = YES;
    body.text = compactDetails ? rawText : readingText(rawText);
    if (compactDetails || panel.compactMenuMode || panel.denseFullScreenMode)
        body.font = [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
    body.hidden = panel.controlsOnlyMode || body.text.length == 0;
    if (panel.directionMode) body.text = [body.text stringByAppendingString:@"\nUse the D-pad."];
    [stack addArrangedSubview:body];
    if (panel.denseFullScreenMode && compactDetails) {
        UIView *spacer = [[UIView alloc] init];
        [spacer setContentHuggingPriority:UILayoutPriorityDefaultLow
                                 forAxis:UILayoutConstraintAxisVertical];
        [stack addArrangedSubview:spacer];
        [stack.heightAnchor constraintGreaterThanOrEqualToAnchor:scroll.frameLayoutGuide.heightAnchor].active = YES;
    }
    UIStackView *actions = [[UIStackView alloc] init];
    actions.axis = UILayoutConstraintAxisVertical;
    actions.spacing = (compactChoiceGrid || panel.denseFullScreenMode || panel.partySelectionMode) ? 6 : (panel.compactMenuMode ? 8 : 10);
    [stack addArrangedSubview:actions];
    BOOL spellbook = panel.spellbookMode;
    BOOL responsivePairGrid = NO;
    if (panel.denseFullScreenMode && !spellbook)
        for (int i = 0; i < count; ++i)
            if (i != backIndex && i != dismissIndex &&
                [[NSString stringWithUTF8String:labels[i]] containsString:@"\n"]) {
                responsivePairGrid = YES;
                break;
            }
    BOOL singleColumn = panel.fullScreenMode && !responsivePairGrid;
    if (!panel.partySelectionMode && !responsivePairGrid)
        for (int i = 0; i < count; ++i)
            if ([NSString stringWithUTF8String:labels[i]].length > 40) singleColumn = YES;
    NSMutableArray<UIStackView *> *rows = [NSMutableArray array];
    UIStackView *row = nil;
    for (int i = 0; i < count; ++i) [keys addObject:[NSString stringWithUTF8String:keywords[i]]];
    if (spellbook) {
        NSMutableArray<UIButton *> *denseSpellButtons = panel.denseFullScreenMode
            ? [NSMutableArray array] : nil;
        NSInteger firstSpell = NSNotFound, lastSpell = NSNotFound;
        NSInteger previousIndex = NSNotFound, nextIndex = NSNotFound;
        NSCharacterSet *notDigits = NSCharacterSet.decimalDigitCharacterSet.invertedSet;
        for (int i = 0; i < count; ++i) {
            NSString *key = keys[i];
            if (key.length && [key rangeOfCharacterFromSet:notDigits].location == NSNotFound) {
                NSInteger number = key.integerValue;
                firstSpell = firstSpell == NSNotFound ? number : MIN(firstSpell, number);
                lastSpell = lastSpell == NSNotFound ? number : MAX(lastSpell, number);
                UIButton *button = topicButtonWithDensity([NSString stringWithUTF8String:labels[i]],
                    panel, @selector(choose:), panel.denseFullScreenMode);
                button.tag = i;
                button.contentHorizontalAlignment = UIControlContentHorizontalAlignmentLeft;
                button.titleLabel.textAlignment = NSTextAlignmentLeft;
                button.accessibilityLabel = [@"Spell: " stringByAppendingString:[NSString stringWithUTF8String:labels[i]]];
                if (panel.denseFullScreenMode) {
                    Zu4PanelButton *denseButton = (Zu4PanelButton *)button;
                    denseButton.minimumHeight = 44;
                    denseButton.verticalPadding = 8;
                    denseButton.contentEdgeInsets = UIEdgeInsetsMake(4, 12, 4, 12);
                    denseButton.titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
                    denseButton.titleLabel.numberOfLines = 1;
                    denseButton.titleLabel.adjustsFontSizeToFitWidth = YES;
                    denseButton.titleLabel.minimumScaleFactor = 0.78;
                    [denseSpellButtons addObject:button];
                } else {
                    [actions addArrangedSubview:button];
                }
            } else if ([key isEqualToString:@"previous"]) previousIndex = i;
            else if ([key isEqualToString:@"next"]) nextIndex = i;
        }
        if (panel.denseFullScreenMode) {
            panel.denseSpellActions = actions;
            actions.spacing = 4;
            panel.denseSpellButtons = denseSpellButtons;
            BOOL portrait = CGRectGetHeight(window.bounds) > CGRectGetWidth(window.bounds);
            [panel rebuildDenseSpellGridForColumns:portrait ? 1 : 2];
        }
        row = [[UIStackView alloc] init];
        row.axis = UILayoutConstraintAxisHorizontal;
        row.distribution = UIStackViewDistributionFillEqually;
        row.alignment = UIStackViewAlignmentFill;
        row.spacing = 8;
        NSInteger navigationIndices[2] = {previousIndex, nextIndex};
        for (int navigationPosition = 0; navigationPosition < 2; ++navigationPosition) {
            NSInteger index = navigationIndices[navigationPosition];
            BOOL available = index != NSNotFound;
            NSString *title = navigationPosition == 0 ? @"‹ Previous" : @"Next ›";
            UIButton *button = topicButton(title, panel, @selector(choose:));
            button.tag = available ? index : 0;
            button.enabled = available;
            button.alpha = available ? 1 : 0.3;
            button.backgroundColor = UIColor.clearColor;
            button.layer.borderWidth = 1;
            button.layer.borderColor = [UIColor colorWithWhite:0.45 alpha:1].CGColor;
            button.accessibilityIdentifier = @"zu4-panel-navigation";
            button.accessibilityLabel = navigationPosition == 0 ? @"Previous spell page" : @"Next spell page";
            [row addArrangedSubview:button];
        }
        [panel.persistentFooter addArrangedSubview:row];
        [rows addObject:row];
        if (firstSpell != NSNotFound) {
            UILabel *pageLabel = journalLabel(
                [NSString stringWithFormat:@"%C–%C of 26 spells", (unichar)('A' + firstSpell), (unichar)('A' + lastSpell)],
                UIFontTextStyleCaption1, [UIColor colorWithWhite:0.7 alpha:1]);
            pageLabel.textAlignment = NSTextAlignmentCenter;
            [panel.persistentFooter addArrangedSubview:pageLabel];
        }
    } else {
        NSMutableArray<NSNumber *> *regularIndices = [NSMutableArray array];
        NSMutableArray<NSNumber *> *topicNavigationIndices = [NSMutableArray array];
        for (int i = 0; i < count; ++i) {
            NSString *key = keys[i];
            if ([key hasPrefix:@"__previous_topics"] || [key hasPrefix:@"__next_topics"])
                [topicNavigationIndices addObject:@(i)];
            else if (!(panel.directionMode && i < 4) && !(panel.conversationMode && i == goodbyeIndex) &&
                     i != backIndex && i != dismissIndex &&
                     i != pagePreviousIndex && i != pageNextIndex && i != pageLabelIndex &&
                     !(panel.compactMenuMode && i == resumeIndex))
                [regularIndices addObject:@(i)];
        }
        NSInteger position = 0;
        for (NSNumber *number in regularIndices) {
            NSInteger i = number.integerValue;
            UIButton *button = topicButtonWithDensity([NSString stringWithUTF8String:labels[i]],
                panel, @selector(choose:), compactChoiceGrid || panel.denseFullScreenMode || panel.partySelectionMode);
            if (panel.partySelectionMode) {
                button.titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
                button.titleLabel.numberOfLines = 2;
                button.titleLabel.adjustsFontSizeToFitWidth = YES;
                button.titleLabel.minimumScaleFactor = 0.85;
                button.accessibilityIdentifier = @"zu4-party-selection";
            }
            button.tag = i;
            NSString *label = [NSString stringWithUTF8String:labels[i]];
            if ([label hasPrefix:@"Back"] || [label hasPrefix:@"Close"])
                button.accessibilityIdentifier = @"zu4-panel-navigation";
            else if (panel.fullScreenMode) {
                button.contentHorizontalAlignment = UIControlContentHorizontalAlignmentLeft;
                button.titleLabel.textAlignment = NSTextAlignmentLeft;
            }
            if (singleColumn || position % 2 == 0) {
                row = [[UIStackView alloc] init];
                row.axis = UILayoutConstraintAxisHorizontal;
                row.distribution = UIStackViewDistributionFillEqually;
                row.spacing = (compactChoiceGrid || panel.denseFullScreenMode || panel.partySelectionMode) ? 8 :
                    (panel.compactMenuMode ? 8 : 10);
                if (responsivePairGrid) row.tag = Zu4ResponsivePairRowTag;
                [actions addArrangedSubview:row];
                [rows addObject:row];
            }
            [row addArrangedSubview:button];
            ++position;
        }
        if (topicNavigationIndices.count) {
            row = [[UIStackView alloc] init];
            row.axis = UILayoutConstraintAxisHorizontal;
            row.distribution = UIStackViewDistributionFillEqually;
            row.spacing = 10;
            NSMutableArray<UIButton *> *navigationButtons = [NSMutableArray array];
            for (NSNumber *number in topicNavigationIndices) {
                NSInteger i = number.integerValue;
                NSString *key = keys[i];
                NSString *title = [key hasPrefix:@"__previous_topics"] ? @"‹ Previous" : @"Next ›";
                UIButton *button = topicButton(title, panel, @selector(choose:));
                button.tag = i;
                button.enabled = ![key hasSuffix:@"_disabled"];
                button.alpha = button.enabled ? 1 : 0.3;
                button.backgroundColor = UIColor.clearColor;
                button.layer.borderWidth = 1;
                button.layer.borderColor = [UIColor colorWithWhite:0.45 alpha:1].CGColor;
                button.accessibilityIdentifier = @"zu4-panel-navigation";
                button.accessibilityLabel = [key hasPrefix:@"__previous_topics"]
                    ? @"Previous conversation topics" : @"Next conversation topics";
                [navigationButtons addObject:button];
            }
            if (panel.conversationMode) {
                UIView *spacer = [[UIView alloc] init];
                [spacer setContentHuggingPriority:UILayoutPriorityDefaultLow forAxis:UILayoutConstraintAxisVertical];
                [actions addArrangedSubview:spacer];
                if (navigationButtons.count) [row addArrangedSubview:navigationButtons.firstObject];
                UIButton *customize = topicButton(@"Something else…", panel, @selector(customize:));
                customize.accessibilityLabel = @"Ask something else";
                [row addArrangedSubview:customize];
                if (navigationButtons.count > 1) [row addArrangedSubview:navigationButtons.lastObject];
            } else {
                for (UIButton *button in navigationButtons) [row addArrangedSubview:button];
            }
            [actions addArrangedSubview:row];
            if (!panel.conversationMode) [rows addObject:row];
        }
        if (hasPageControls) {
            row = [[UIStackView alloc] init];
            row.axis = UILayoutConstraintAxisHorizontal;
            row.distribution = UIStackViewDistributionFillEqually;
            row.alignment = UIStackViewAlignmentFill;
            row.spacing = 8;
            NSInteger navigationIndices[2] = {pagePreviousIndex, pageNextIndex};
            for (int navigationPosition = 0; navigationPosition < 2; ++navigationPosition) {
                NSInteger index = navigationIndices[navigationPosition];
                BOOL available = index != NSNotFound &&
                    ![[NSString stringWithUTF8String:keywords[index]] hasSuffix:@"_disabled"];
                NSString *title = index == NSNotFound
                    ? (navigationPosition == 0 ? @"‹ Previous" : @"Next ›")
                    : [NSString stringWithUTF8String:labels[index]];
                UIButton *button = topicButtonWithDensity(title, panel, @selector(choose:), YES);
                button.tag = index == NSNotFound ? 0 : index;
                button.enabled = available;
                button.alpha = available ? 1 : 0.3;
                button.backgroundColor = UIColor.clearColor;
                button.layer.borderWidth = 1;
                button.layer.borderColor = [UIColor colorWithWhite:0.45 alpha:1].CGColor;
                button.accessibilityIdentifier = @"zu4-panel-navigation";
                button.accessibilityLabel = navigationPosition == 0 ? @"Previous page" : @"Next page";
                [row addArrangedSubview:button];
            }
            [panel.persistentFooter addArrangedSubview:row];
            [rows addObject:row];
            if (pageLabelIndex != NSNotFound) {
                UILabel *pageLabel = journalLabel([NSString stringWithUTF8String:labels[pageLabelIndex]],
                    UIFontTextStyleCaption1, [UIColor colorWithWhite:0.7 alpha:1]);
                pageLabel.textAlignment = NSTextAlignmentCenter;
                [panel.persistentFooter addArrangedSubview:pageLabel];
            }
        }
    }
    panel.optionRows = rows;
    panel.keywords = keys;
    if (panel.controlsOnlyMode) {
        UIAccessibilityElement *prompt = [[UIAccessibilityElement alloc] initWithAccessibilityContainer:panel];
        prompt.accessibilityLabel = readingText(rawText);
        prompt.accessibilityTraits = UIAccessibilityTraitStaticText;
        prompt.accessibilityFrameInContainerSpace = panel.bounds;
        NSMutableArray *elements = [NSMutableArray arrayWithObject:prompt];
        for (UIStackView *optionRow in rows)
            for (UIView *control in optionRow.arrangedSubviews)
                if ([control isKindOfClass:UIButton.class]) [elements addObject:control];
        panel.accessibilityElements = elements;
    }
    if (maxLength > 0 && !panel.conversationMode)
        [actions addArrangedSubview:topicButton(nameEntry ? @"Enter name" : @"Ask something else…", panel, @selector(customize:))];
    panel.custom = [[UITextField alloc] init];
    panel.custom.borderStyle = UITextBorderStyleRoundedRect;
    panel.custom.placeholder = nameEntry ? @"Character name" : @"Custom keyword — Return to ask";
    panel.custom.accessibilityLabel = nameEntry ? @"Character name" : @"Custom conversation keyword";
    panel.custom.autocorrectionType = UITextAutocorrectionTypeNo;
    panel.custom.autocapitalizationType = UITextAutocapitalizationTypeNone;
    panel.custom.returnKeyType = nameEntry ? UIReturnKeyDone : UIReturnKeySend;
    panel.custom.delegate = panel;
    UIView *keyboardBar = [[UIView alloc] initWithFrame:CGRectMake(0, 0, window.bounds.size.width, 44)];
    keyboardBar.backgroundColor = [UIColor colorWithWhite:0.12 alpha:1];
    UIButton *hideKeyboard = [UIButton buttonWithType:UIButtonTypeSystem];
    hideKeyboard.translatesAutoresizingMaskIntoConstraints = NO;
    [hideKeyboard setTitle:@"Hide Keyboard" forState:UIControlStateNormal];
    hideKeyboard.titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
    [hideKeyboard addTarget:panel action:@selector(hideKeyboard) forControlEvents:UIControlEventTouchUpInside];
    [keyboardBar addSubview:hideKeyboard];
    [NSLayoutConstraint activateConstraints:@[
        [hideKeyboard.trailingAnchor constraintEqualToAnchor:keyboardBar.trailingAnchor constant:-16],
        [hideKeyboard.topAnchor constraintEqualToAnchor:keyboardBar.topAnchor],
        [hideKeyboard.bottomAnchor constraintEqualToAnchor:keyboardBar.bottomAnchor],
        [hideKeyboard.widthAnchor constraintGreaterThanOrEqualToConstant:132]]];
    panel.custom.inputAccessoryView = keyboardBar;
    panel.custom.hidden = YES;
    [panel.custom.heightAnchor constraintGreaterThanOrEqualToConstant:48].active = YES;
    [actions insertArrangedSubview:panel.custom atIndex:0];
    if (g_retiring_panel && g_retiring_panel != panel) {
        [g_retiring_panel removeFromSuperview];
        g_retiring_panel = nil;
    }
    UIAccessibilityPostNotification(UIAccessibilityScreenChangedNotification,
        panel.headingLabel ?: (body.hidden ? panel.card : body));
}

int zu4_topic_panel_is_visible(void) {
    for (UIWindow *window in UIApplication.sharedApplication.windows)
        for (UIView *view in window.subviews)
            if ([view isKindOfClass:Zu4TopicPanel.class] || [view isKindOfClass:Zu4JournalPanel.class]) return 1;
    return 0;
}

static Zu4TopicPanel *directionPanel(void) {
    for (UIWindow *window in UIApplication.sharedApplication.windows)
        for (UIView *view in window.subviews)
            if ([view isKindOfClass:Zu4TopicPanel.class] && ((Zu4TopicPanel *)view).directionMode &&
                ((Zu4TopicPanel *)view).submit != nullptr)
                return (Zu4TopicPanel *)view;
    return nil;
}
int zu4_topic_panel_direction_active(void) { return directionPanel() != nil; }
int zu4_topic_panel_choose_direction(const char *direction) {
    Zu4TopicPanel *panel = directionPanel();
    if (!panel) return 0;
    [panel finish:[NSString stringWithUTF8String:direction] textEntry:NO];
    return 1;
}
