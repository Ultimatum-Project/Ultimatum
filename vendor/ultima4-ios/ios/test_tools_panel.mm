#import <UIKit/UIKit.h>
#include "test_tools_panel.h"

@interface Zu4TestToolsPanel : UIView <UITableViewDataSource, UITableViewDelegate>
@property(nonatomic, strong) UIView *card;
@property(nonatomic, strong) UITableView *tableView;
@property(nonatomic, strong) UILabel *titleLabel;
@property(nonatomic, strong) UILabel *subtitleLabel;
@property(nonatomic, strong) NSArray<NSDictionary *> *sections;
@property(nonatomic, assign) Zu4TestToolsSubmit submit;
@property(nonatomic, assign) void *context;
@end

static NSDictionary *testItem(NSString *title, NSString *detail, NSString *action,
                              NSString *type, BOOL value, BOOL enabled, BOOL danger) {
    return @{ @"title": title ?: @"", @"detail": detail ?: @"", @"action": action ?: @"",
              @"type": type ?: @"action", @"value": @(value), @"enabled": @(enabled),
              @"danger": @(danger) };
}

static NSDictionary *testSection(NSString *title, NSArray<NSDictionary *> *items) {
    return @{ @"title": title, @"items": items };
}

@implementation Zu4TestToolsPanel

- (void)layoutSubviews {
    [super layoutSubviews];
    if (self.userInteractionEnabled) [self.superview bringSubviewToFront:self];
    CGRect safe = UIEdgeInsetsInsetRect(self.bounds, self.safeAreaInsets);
    BOOL portrait = safe.size.height > safe.size.width;
    if (portrait) {
        self.card.frame = safe;
        self.card.layer.cornerRadius = 0;
        self.card.layer.borderWidth = 0;
    } else {
        CGFloat width = MIN(580, MAX(440, safe.size.width * 0.62));
        self.card.frame = CGRectMake(CGRectGetMaxX(safe) - width - 8,
                                     CGRectGetMinY(safe) + 8, width, safe.size.height - 16);
        self.card.layer.cornerRadius = 16;
        self.card.layer.borderWidth = 1;
    }
}

- (NSDictionary *)itemAtIndexPath:(NSIndexPath *)indexPath {
    return self.sections[indexPath.section][@"items"][indexPath.row];
}

- (void)finish:(NSString *)action enabled:(BOOL)enabled {
    Zu4TestToolsSubmit callback = self.submit;
    void *context = self.context;
    if (!callback) return;
    self.submit = nullptr;
    self.userInteractionEnabled = NO;
    self.accessibilityElementsHidden = YES;
    // Keep the outgoing surface composited until its replacement is fully
    // attached. Removing it here exposes the SDL game for one frame between
    // synchronous native menu reads. Done is a real dismissal and has no
    // replacement, so it still leaves immediately.
    if ([action isEqualToString:@"done"]) [self removeFromSuperview];
    callback(action.UTF8String, enabled ? 1 : 0, context);
}

- (void)done { [self finish:@"done" enabled:NO]; }
- (void)back { [self finish:@"back" enabled:NO]; }

- (void)switchChanged:(UISwitch *)sender {
    NSInteger section = sender.tag / 1000;
    NSInteger row = sender.tag % 1000;
    if (section < 0 || section >= (NSInteger)self.sections.count) return;
    NSArray *items = self.sections[section][@"items"];
    if (row < 0 || row >= (NSInteger)items.count) return;
    [self finish:items[row][@"action"] enabled:sender.on];
}

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView { return self.sections.count; }
- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section {
    return [self.sections[section][@"items"] count];
}
- (NSString *)tableView:(UITableView *)tableView titleForHeaderInSection:(NSInteger)section {
    return self.sections[section][@"title"];
}
- (NSString *)tableView:(UITableView *)tableView titleForFooterInSection:(NSInteger)section {
    if (section != (NSInteger)self.sections.count - 1) return nil;
    return @"The first adventure-changing action in each Debug Tools session creates a recovery checkpoint. Session switches reset when the app restarts.";
}

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath {
    static NSString *identifier = @"Zu4TestToolCell";
    UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:identifier];
    if (!cell) cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleSubtitle reuseIdentifier:identifier];
    NSDictionary *item = [self itemAtIndexPath:indexPath];
    BOOL enabled = [item[@"enabled"] boolValue];
    BOOL danger = [item[@"danger"] boolValue];
    NSString *type = item[@"type"];
    cell.textLabel.text = item[@"title"];
    cell.detailTextLabel.text = item[@"detail"];
    cell.accessibilityLabel = item[@"title"];
    cell.accessibilityHint = item[@"detail"];
    cell.accessibilityValue = nil;
    cell.textLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    cell.textLabel.adjustsFontForContentSizeCategory = YES;
    cell.detailTextLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleCaption1];
    cell.detailTextLabel.adjustsFontForContentSizeCategory = YES;
    cell.textLabel.textColor = !enabled ? UIColor.tertiaryLabelColor :
        (danger ? UIColor.systemRedColor : UIColor.labelColor);
    cell.detailTextLabel.textColor = UIColor.secondaryLabelColor;
    cell.backgroundColor = UIColor.secondarySystemGroupedBackgroundColor;
    cell.selectionStyle = enabled && ![type isEqualToString:@"info"]
        ? UITableViewCellSelectionStyleDefault : UITableViewCellSelectionStyleNone;
    cell.accessoryView = nil;
    cell.accessoryType = UITableViewCellAccessoryNone;
    cell.isAccessibilityElement = YES;
    cell.accessibilityTraits = UIAccessibilityTraitNone;
    if ([type isEqualToString:@"toggle"]) {
        UISwitch *toggle = [[UISwitch alloc] init];
        toggle.on = [item[@"value"] boolValue];
        toggle.enabled = enabled;
        // The row is the single VoiceOver element; the visible accessory still
        // handles direct touch. This avoids UIKit exposing the label and switch
        // as two identical controls.
        toggle.isAccessibilityElement = NO;
        toggle.tag = indexPath.section * 1000 + indexPath.row;
        [toggle addTarget:self action:@selector(switchChanged:) forControlEvents:UIControlEventValueChanged];
        cell.accessoryView = toggle;
        cell.accessibilityValue = [item[@"value"] boolValue] ? @"On" : @"Off";
        cell.accessibilityTraits = UIAccessibilityTraitButton;
    } else if ([type isEqualToString:@"disclosure"] && enabled) {
        cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
        cell.accessibilityTraits = UIAccessibilityTraitButton;
    } else if ([type isEqualToString:@"action"] && enabled) {
        cell.accessibilityTraits = UIAccessibilityTraitButton;
    }
    return cell;
}

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath {
    [tableView deselectRowAtIndexPath:indexPath animated:YES];
    NSDictionary *item = [self itemAtIndexPath:indexPath];
    if (![item[@"enabled"] boolValue] || [item[@"type"] isEqualToString:@"info"]) return;
    if ([item[@"type"] isEqualToString:@"toggle"]) {
        [self finish:item[@"action"] enabled:![item[@"value"] boolValue]];
        return;
    }
    [self finish:item[@"action"] enabled:NO];
}
@end

int zu4_test_tools_available(void) {
    NSString *bundle = NSBundle.mainBundle.bundleIdentifier.lowercaseString ?: @"";
    NSNumber *override = [NSBundle.mainBundle objectForInfoDictionaryKey:@"ZU4TestToolsEnabled"];
    return [bundle hasSuffix:@".ultima4preview"] || override.boolValue;
}

static UIWindow *testToolsWindow(void) {
    for (UIWindow *candidate in UIApplication.sharedApplication.windows)
        if (candidate.isKeyWindow) return candidate;
    return nil;
}

void zu4_test_tools_panel_dismiss(void) {
    if (!NSThread.isMainThread) {
        dispatch_sync(dispatch_get_main_queue(), ^{ zu4_test_tools_panel_dismiss(); });
        return;
    }
    UIWindow *window = testToolsWindow();
    for (UIView *view in window.subviews.copy)
        if ([view isKindOfClass:Zu4TestToolsPanel.class]) [view removeFromSuperview];
}

void zu4_test_tools_panel_show(const Zu4TestToolsState *state,
                               Zu4TestToolsSubmit submit, void *context) {
    if (!NSThread.isMainThread) {
        dispatch_sync(dispatch_get_main_queue(), ^{
            zu4_test_tools_panel_show(state, submit, context);
        });
        return;
    }
    UIWindow *window = testToolsWindow();
    if (!window || !state) { submit("done", 0, context); return; }

    NSMutableArray<Zu4TestToolsPanel *> *outgoing = [NSMutableArray array];
    for (UIView *view in window.subviews)
        if ([view isKindOfClass:Zu4TestToolsPanel.class])
            [outgoing addObject:(Zu4TestToolsPanel *)view];

    Zu4TestToolsPanel *panel = [[Zu4TestToolsPanel alloc] initWithFrame:window.bounds];
    panel.translatesAutoresizingMaskIntoConstraints = NO;
    panel.backgroundColor = [UIColor colorWithWhite:0 alpha:0.55];
    if (@available(iOS 13.0, *)) panel.overrideUserInterfaceStyle = UIUserInterfaceStyleDark;
    panel.accessibilityViewIsModal = YES;
    panel.submit = submit;
    panel.context = context;

    UIView *card = [[UIView alloc] init];
    card.backgroundColor = UIColor.systemGroupedBackgroundColor;
    card.layer.borderColor = UIColor.separatorColor.CGColor;
    card.clipsToBounds = YES;
    panel.card = card;
    [panel addSubview:card];

    NSString *page = state->page ? [NSString stringWithUTF8String:state->page] : @"root";
    BOOL rootPage = [page isEqualToString:@"root"];
    NSDictionary<NSString *, NSString *> *pageTitles = @{
        @"root": @"Debug Tools", @"session": @"Session", @"navigation": @"Navigation",
        @"party": @"Party & Inventory", @"world": @"World", @"diagnostics": @"Diagnostics",
        @"danger": @"Danger Zone"
    };
    UILabel *title = [[UILabel alloc] init];
    title.text = pageTitles[page] ?: @"Debug Tools";
    title.font = [UIFont preferredFontForTextStyle:UIFontTextStyleTitle1];
    title.adjustsFontForContentSizeCategory = YES;
    title.textColor = UIColor.labelColor;
    title.translatesAutoresizingMaskIntoConstraints = NO;
    panel.titleLabel = title;
    [card addSubview:title];

    UIButton *back = [UIButton buttonWithType:UIButtonTypeSystem];
    [back setTitle:@"‹ Tools" forState:UIControlStateNormal];
    back.titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    back.hidden = rootPage;
    back.translatesAutoresizingMaskIntoConstraints = NO;
    [back addTarget:panel action:@selector(back) forControlEvents:UIControlEventTouchUpInside];
    [card addSubview:back];

    NSString *location = state->location ? [NSString stringWithUTF8String:state->location] : @"Unknown";
    UILabel *subtitle = [[UILabel alloc] init];
    subtitle.text = [NSString stringWithFormat:@"Slot %d  •  %@  •  Debug changes can autosave",
        state->activeSlot, location];
    subtitle.font = [UIFont preferredFontForTextStyle:UIFontTextStyleCaption1];
    subtitle.adjustsFontForContentSizeCategory = YES;
    subtitle.textColor = UIColor.secondaryLabelColor;
    subtitle.translatesAutoresizingMaskIntoConstraints = NO;
    panel.subtitleLabel = subtitle;
    [card addSubview:subtitle];

    UIButton *done = [UIButton buttonWithType:UIButtonTypeSystem];
    [done setTitle:@"Done" forState:UIControlStateNormal];
    done.titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
    done.translatesAutoresizingMaskIntoConstraints = NO;
    [done addTarget:panel action:@selector(done) forControlEvents:UIControlEventTouchUpInside];
    [card addSubview:done];

    UITableView *table = [[UITableView alloc] initWithFrame:CGRectZero style:UITableViewStyleInsetGrouped];
    table.dataSource = panel;
    table.delegate = panel;
    table.backgroundColor = UIColor.systemGroupedBackgroundColor;
    table.rowHeight = UITableViewAutomaticDimension;
    table.estimatedRowHeight = 54;
    table.alwaysBounceVertical = NO;
    table.bounces = NO;
    table.translatesAutoresizingMaskIntoConstraints = NO;
    panel.tableView = table;
    [card addSubview:table];

    BOOL ordinaryMap = !state->combat;
    NSString *worldOnly = state->worldMap ? @"" : @"World map only";
    NSMutableArray<NSDictionary *> *sections = [NSMutableArray array];
    if (rootPage) {
        [sections addObject:testSection(@"CATEGORIES", @[
            testItem(@"Session", @"Collision, visibility, map preview and wind lock", @"page_session", @"disclosure", NO, YES, NO),
            testItem(@"Navigation", @"Named destinations and map travel", @"page_navigation", @"disclosure", NO, YES, NO),
            testItem(@"Party & Inventory", @"Equipment, supplies, companions and virtues", @"page_party", @"disclosure", NO, YES, NO),
            testItem(@"World", @"Moons, wind, creatures and transport", @"page_world", @"disclosure", NO, YES, NO),
            testItem(@"Diagnostics", @"Location, torch and virtue values", @"page_diagnostics", @"disclosure", NO, YES, NO),
            testItem(@"Danger Zone", @"Destructive and end-state commands", @"page_danger", @"disclosure", NO, YES, YES)
        ])];
    } else if ([page isEqualToString:@"session"]) {
        [sections addObject:testSection(@"SESSION", @[
            testItem(@"Walk through terrain", @"Ignore collision while moving", @"collision", @"toggle",
                     state->collisionOverride, ordinaryMap, NO),
            testItem(@"See through opaque tiles", @"Reveal terrain normally hidden by walls", @"opacity", @"toggle",
                     state->seeThroughWalls, ordinaryMap, NO),
            testItem(@"Preview GEM map", @"Open the native Peer view without spending a gem", @"peer_preview", @"action",
                     NO, ordinaryMap, NO),
            testItem(@"Lock wind direction", state->windDirection ? [NSString stringWithUTF8String:state->windDirection] : @"",
                     @"wind_lock", @"toggle", state->windLocked, ordinaryMap, NO)
        ])];
    } else if ([page isEqualToString:@"navigation"]) {
        [sections addObject:testSection(@"NAVIGATION", @[
            testItem(@"Go to location", @"Choose a named world destination", @"goto", @"disclosure", NO, ordinaryMap, NO),
            testItem(@"Go to moongate", worldOnly, @"moongate", @"disclosure", NO,
                     ordinaryMap && state->worldMap, NO),
            testItem(@"Go to dungeon entrance", state->canDungeonTeleport ? @"" : @"World map on foot or horse only",
                     @"dungeon", @"disclosure", NO, ordinaryMap && state->canDungeonTeleport, NO),
            testItem(@"Go to altar room", worldOnly, @"altar", @"disclosure", NO,
                     ordinaryMap && state->worldMap, NO),
            testItem(@"Return to Lord British", @"Teleport to the throne room", @"lord_british", @"action", NO,
                     ordinaryMap, NO),
            testItem(@"Exit current map", state->worldMap ? @"Already on the world map" : @"Return to the parent map",
                     @"exit_map", @"action", NO, ordinaryMap && !state->worldMap, NO)
        ])];
    } else if ([page isEqualToString:@"party"]) {
        [sections addObject:testSection(@"PARTY & INVENTORY", @[
            testItem(@"Grant equipment", @"Weapons and armor", @"equipment", @"action", NO, YES, NO),
            testItem(@"Maximize party stats", @"Strength, dexterity, intelligence, level and health", @"stats", @"action", NO, YES, NO),
            testItem(@"Grant quest items", @"Runes, stones, keys, food and gold", @"items", @"action", NO, YES, NO),
            testItem(@"Grant 99 reagents", @"All eight reagents", @"reagents", @"action", NO, YES, NO),
            testItem(@"Grant 99 spell mixtures", @"All twenty-six spells", @"mixtures", @"action", NO, YES, NO),
            testItem(@"Recruit eligible companions", @"Fill available party positions", @"companions", @"action", NO, YES, NO),
            testItem(@"Complete all virtues", @"Set Avatar virtue state", @"virtues", @"action", NO, YES, NO)
        ])];
    } else if ([page isEqualToString:@"world"]) {
        [sections addObject:testSection(@"WORLD", @[
            testItem(@"Advance moons", @"Advance Trammel one phase", @"moons", @"action", NO, ordinaryMap, NO),
            testItem(@"Set wind direction", state->windDirection ? [NSString stringWithUTF8String:state->windDirection] : @"",
                     @"wind", @"disclosure", NO, ordinaryMap, NO),
            testItem(@"Summon creature", @"Choose a common creature or enter a name", @"summon", @"disclosure", NO,
                     ordinaryMap, NO),
            testItem(@"Create transport", worldOnly, @"transport", @"disclosure", NO,
                     ordinaryMap && state->worldMap, NO)
        ])];
    } else if ([page isEqualToString:@"diagnostics"]) {
        NSString *coordinates = state->dungeon
            ? [NSString stringWithFormat:@"%@  •  %d, %d, %d", location, state->x, state->y, state->z]
            : [NSString stringWithFormat:@"%@  •  %d, %d", location, state->x, state->y];
        [sections addObject:testSection(@"DIAGNOSTICS", @[
            testItem(@"Current location", coordinates, @"", @"info", NO, YES, NO),
            testItem(@"Torch duration", [NSString stringWithFormat:@"%d", state->torchDuration], @"", @"info", NO, YES, NO),
            testItem(@"Virtue values", @"Inspect all eight values", @"virtue_values", @"disclosure", NO, YES, NO)
        ])];
    } else if ([page isEqualToString:@"danger"]) {
        [sections addObject:testSection(@"DANGER ZONE", @[
            testItem(@"Destroy nearby object", @"Choose an adjacent direction", @"destroy", @"action", NO,
                     ordinaryMap, YES),
            testItem(@"Destroy all creatures", state->combat ? @"Clear this battlefield" : @"Clear creatures from the current map",
                     @"clear_creatures", @"action", NO, YES, YES),
            testItem(@"End combat immediately", state->combat ? @"No virtue adjustment" : @"Combat only",
                     @"end_combat", @"action", NO, state->combat, YES),
            testItem(@"Go to the final altar", worldOnly, @"final_altar", @"action", NO,
                     ordinaryMap && state->worldMap, YES)
        ])];
    }
    panel.sections = sections;

    [window addSubview:panel];
    [NSLayoutConstraint activateConstraints:@[
        [panel.leadingAnchor constraintEqualToAnchor:window.leadingAnchor],
        [panel.trailingAnchor constraintEqualToAnchor:window.trailingAnchor],
        [panel.topAnchor constraintEqualToAnchor:window.topAnchor],
        [panel.bottomAnchor constraintEqualToAnchor:window.bottomAnchor],
        [back.leadingAnchor constraintEqualToAnchor:card.leadingAnchor constant:12],
        [back.centerYAnchor constraintEqualToAnchor:title.centerYAnchor],
        [back.heightAnchor constraintGreaterThanOrEqualToConstant:44],
        [back.widthAnchor constraintEqualToConstant:rootPage ? 0 : 72],
        [title.leadingAnchor constraintEqualToAnchor:back.trailingAnchor constant:4],
        [title.topAnchor constraintEqualToAnchor:card.topAnchor constant:12],
        [title.trailingAnchor constraintLessThanOrEqualToAnchor:done.leadingAnchor constant:-8],
        [done.trailingAnchor constraintEqualToAnchor:card.trailingAnchor constant:-16],
        [done.centerYAnchor constraintEqualToAnchor:title.centerYAnchor],
        [done.heightAnchor constraintGreaterThanOrEqualToConstant:44],
        [done.widthAnchor constraintGreaterThanOrEqualToConstant:60],
        [subtitle.leadingAnchor constraintEqualToAnchor:title.leadingAnchor],
        [subtitle.trailingAnchor constraintEqualToAnchor:card.trailingAnchor constant:-20],
        [subtitle.topAnchor constraintEqualToAnchor:title.bottomAnchor constant:2],
        [table.leadingAnchor constraintEqualToAnchor:card.leadingAnchor],
        [table.trailingAnchor constraintEqualToAnchor:card.trailingAnchor],
        [table.topAnchor constraintEqualToAnchor:subtitle.bottomAnchor constant:6],
        [table.bottomAnchor constraintEqualToAnchor:card.bottomAnchor]
    ]];
    [panel setNeedsLayout];
    [window layoutIfNeeded];
    // The new panel and its cells now have valid geometry. Retire the previous
    // panel in the same UI transaction so no world frame can appear between
    // native pages.
    for (Zu4TestToolsPanel *oldPanel in outgoing) [oldPanel removeFromSuperview];
    UIAccessibilityPostNotification(UIAccessibilityScreenChangedNotification, title);
}
