#include "topic_panel.h"
#include "mobile_layout.h"
/*
 *  zu4_ios_ui.mm
 *  On-screen touch controls for the iOS port of zu4 (Ultima IV). A native UIKit
 *  overlay of buttons — a movement D-pad plus a few action keys and a keyboard
 *  toggle — sits over the SDL view. Each button synthesises the SDL key event
 *  the engine expects. U4's other commands are single letters, typed via the
 *  on-screen keyboard (the ⌨ button).
 */
#import <UIKit/UIKit.h>
#include <stdint.h>

#include "zu4_ios_ui.h"
#include "SDL.h"
#include "SDL_syswm.h"
#include "direction.h"
#include "settings.h"

// Special tag value meaning "toggle the on-screen keyboard" rather than a key.
#define ZU4_TAG_KEYBOARD 0x7FFFFFFF
#define ZU4_TAG_TALK 0x7FFFFFFE
#define ZU4_TAG_ENTER 0x7FFFFFFD
#define ZU4_TAG_JOURNAL 0x7FFFFFFC
#define ZU4_TAG_PARTY 0x7FFFFFFB
#define ZU4_TAG_CAST 0x7FFFFFFA
#define ZU4_TAG_MENU 0x7FFFFFF9
#define ZU4_TAG_MAP 0x7FFFFFF8
#define ZU4_TAG_REPEAT_ATTACK 0x7FFFFFF7
#define ZU4_TAG_PARTY_MEMBER_BASE 0x70000100
#define ZU4_TAG_HP_TRACK 0x70000200
#define ZU4_TAG_HP_FILL 0x70000201

static void zu4_push_key(SDL_Keycode sym)
{
	SDL_Event e;
	SDL_zero(e);
	e.type = SDL_KEYDOWN;
	e.key.state = SDL_PRESSED;
	e.key.keysym.sym = sym;
	e.key.keysym.scancode = SDL_GetScancodeFromKey(sym);
	SDL_PushEvent(&e);

	e.type = SDL_KEYUP;
	e.key.state = SDL_RELEASED;
	SDL_PushEvent(&e);
}

static void zu4_queue_mobile_action(Zu4MobileAction action, int parameter = 0)
{
    SDL_Event event;
    SDL_zero(event);
    event.type = SDL_USEREVENT;
    event.user.code = ZU4_IOS_ACTION_EVENT;
    event.user.data1 = (void *)(intptr_t)action;
    event.user.data2 = (void *)(intptr_t)parameter;
    SDL_PushEvent(&event);
}

// Shared state (declared up-front so the button target class can use it).
static bool g_ui_installed = false;
static SDL_Window *g_window = NULL;
static UIView *g_root_view = nil;   // the SDL view (fills the window)
static CGRect g_full_frame;         // its normal, full-screen frame
static UIView *g_overlay = nil;     // non-scaled button layer (sibling of the SDL view)
static UIView *g_world_touch = nil;
static UIView *g_combat_touch = nil;
static CAShapeLayer *g_combat_range_layer = nil;
static UILabel *g_world_status = nil;
static UILabel *g_world_log = nil;
static UIView *g_party_roster = nil;
static UIView *g_map_panel = nil;
static NSMutableString *g_recent_messages = nil;
static uint64_t g_minimap_hash = 0;
static UIView *g_dpad = nil;        // movement D-pad container (shifts left when kb is up)
static bool g_kb_shown = false;     // our own record of keyboard visibility (SDL's flag desyncs)
static bool g_native_text_input_active = false;
static bool g_map_place_labels_visible = true;

// How far to nudge the D-pad left while the keyboard is up (points, in the
// overlay's coordinate space, so the net on-screen shift is this * the overlay
// keyboard-scale). Counteracts the inward drift from center-anchored scaling.
static const CGFloat ZU4_DPAD_KB_SHIFT = 150.0;

// A transparent overlay that holds the buttons but lets touches on empty areas fall through
// to the game view below (so tapping the map still works). It lives on the UIWindow — NOT
// inside the SDL view — so the keyboard-scale transform never resizes the buttons.
static void zu4_layout_controls(void);
static void zu4_layout_world_touch_targets(void);
static void zu4_update_combat_targets(BOOL visible);
static void zu4_quiet_keyboard(void);   // defined below; used by keyboardWillShow

@interface Zu4MapPinButton : UIButton
@property(nonatomic) int mapX;
@property(nonatomic) int mapY;
@property(nonatomic) CGFloat xFraction;
@property(nonatomic) CGFloat yFraction;
@end
@implementation Zu4MapPinButton
@end

@interface Zu4MapPlaceButton : UIButton
@property(nonatomic) CGFloat xFraction;
@property(nonatomic) CGFloat yFraction;
@property(nonatomic, copy) NSString *placeDescription;
@property(nonatomic, strong) UILabel *mapLabel;
@end
@implementation Zu4MapPlaceButton
@end

@interface Zu4MinimapButton : UIButton
@end

@interface Zu4CombatTargetButton : UIButton
@property(nonatomic) int targetToken;
@end
@implementation Zu4CombatTargetButton
@end
@implementation Zu4MinimapButton
- (CGRect)imageRectForContentRect:(CGRect)contentRect
{
    CGRect available = CGRectInset(contentRect, 3.0, 3.0);
    CGFloat side = floor(MIN(available.size.width, available.size.height));
    return CGRectMake(CGRectGetMidX(contentRect) - side / 2.0,
                      CGRectGetMidY(contentRect) - side / 2.0,
                      side, side);
}
@end

@interface Zu4MapCanvas : UIView
@property(nonatomic, strong) UIImageView *imageView;
@property(nonatomic, strong) UIView *playerMarker;
@property(nonatomic, strong) CAShapeLayer *gridLayer;
@property(nonatomic, strong) NSArray<Zu4MapPinButton *> *pinButtons;
@property(nonatomic, strong) NSArray<Zu4MapPlaceButton *> *placeButtons;
@property(nonatomic) CGFloat zoomLevel;
@property(nonatomic) CGFloat centerXFraction;
@property(nonatomic) CGFloat centerYFraction;
@property(nonatomic) CGFloat playerXFraction;
@property(nonatomic) CGFloat playerYFraction;
@property(nonatomic) int gridColumns;
@property(nonatomic) int gridRows;
@property(nonatomic) BOOL placeLabelsVisible;
- (void)refreshGeometry;
@end

@implementation Zu4MapCanvas
- (void)layoutSubviews {
    [super layoutSubviews];
    [self refreshGeometry];
}
- (void)refreshGeometry {
    CGFloat baseSide = MIN(self.bounds.size.width, self.bounds.size.height);
    if (baseSide <= 0 || !self.imageView) return;
    CGFloat imageSide = baseSide * self.zoomLevel;
    CGFloat x = self.bounds.size.width / 2.0 - self.centerXFraction * imageSide;
    CGFloat y = self.bounds.size.height / 2.0 - self.centerYFraction * imageSide;
    x = imageSide <= self.bounds.size.width ? (self.bounds.size.width - imageSide) / 2.0
        : MIN(0, MAX(self.bounds.size.width - imageSide, x));
    y = imageSide <= self.bounds.size.height ? (self.bounds.size.height - imageSide) / 2.0
        : MIN(0, MAX(self.bounds.size.height - imageSide, y));
    self.imageView.frame = CGRectMake(x, y, imageSide, imageSide);
    if (self.gridLayer && self.gridColumns > 0 && self.gridRows > 0) {
        self.gridLayer.frame = self.imageView.bounds;
        UIBezierPath *grid = [UIBezierPath bezierPath];
        for (int column = 1; column < self.gridColumns; ++column) {
            CGFloat lineX = imageSide * column / self.gridColumns;
            [grid moveToPoint:CGPointMake(lineX, 0)];
            [grid addLineToPoint:CGPointMake(lineX, imageSide)];
        }
        for (int row = 1; row < self.gridRows; ++row) {
            CGFloat lineY = imageSide * row / self.gridRows;
            [grid moveToPoint:CGPointMake(0, lineY)];
            [grid addLineToPoint:CGPointMake(imageSide, lineY)];
        }
        self.gridLayer.path = grid.CGPath;
    }
    CGFloat markerSize = MIN(20.0, 8.0 + 1.5 * (self.zoomLevel - 1.0));
    self.playerMarker.bounds = CGRectMake(0, 0, markerSize, markerSize);
    self.playerMarker.layer.cornerRadius = markerSize / 2.0;
    self.playerMarker.center = CGPointMake(x + self.playerXFraction * imageSide,
                                           y + self.playerYFraction * imageSide);
    self.playerMarker.hidden = !CGRectIntersectsRect(self.bounds,
        CGRectInset(self.playerMarker.frame, -2, -2));
    for (Zu4MapPinButton *pin in self.pinButtons) {
        pin.bounds = CGRectMake(0, 0, 44, 44);
        pin.center = CGPointMake(x + pin.xFraction * imageSide + 8,
                                 y + pin.yFraction * imageSide - 8);
        pin.hidden = !CGRectIntersectsRect(self.bounds, CGRectInset(pin.frame, -2, -2));
    }
    NSMutableArray<NSValue *> *occupiedLabels = [NSMutableArray array];
    for (Zu4MapPlaceButton *place in self.placeButtons) {
        place.bounds = CGRectMake(0, 0, 44, 44);
        place.center = CGPointMake(x + place.xFraction * imageSide,
                                   y + place.yFraction * imageSide);
        place.hidden = !CGRectIntersectsRect(self.bounds, CGRectInset(place.frame, -2, -2));
        UILabel *label = place.mapLabel;
        label.hidden = place.hidden || !self.placeLabelsVisible;
        if (label.hidden) continue;
        CGSize measured = [label sizeThatFits:CGSizeMake(150, 24)];
        CGFloat labelWidth = MIN(150.0, MAX(44.0, ceil(measured.width) + 12.0));
        CGFloat labelHeight = 24.0;
        CGPoint marker = place.center;
        CGRect candidates[] = {
            CGRectMake(marker.x + 13, marker.y - labelHeight / 2, labelWidth, labelHeight),
            CGRectMake(marker.x - 13 - labelWidth, marker.y - labelHeight / 2, labelWidth, labelHeight),
            CGRectMake(marker.x - labelWidth / 2, marker.y - 15 - labelHeight, labelWidth, labelHeight),
            CGRectMake(marker.x - labelWidth / 2, marker.y + 15, labelWidth, labelHeight)
        };
        CGRect safeBounds = CGRectInset(self.bounds, 4, 4);
        CGRect chosen = CGRectNull;
        for (CGRect candidate : candidates) {
            if (!CGRectContainsRect(safeBounds, candidate)) continue;
            BOOL collides = NO;
            CGRect padded = CGRectInset(candidate, -3, -2);
            for (NSValue *used in occupiedLabels)
                if (CGRectIntersectsRect(padded, used.CGRectValue)) { collides = YES; break; }
            if (!collides) { chosen = candidate; break; }
        }
        if (CGRectIsNull(chosen)) {
            chosen = candidates[0];
            chosen.origin.x = MIN(CGRectGetMaxX(safeBounds) - chosen.size.width,
                                  MAX(CGRectGetMinX(safeBounds), chosen.origin.x));
            chosen.origin.y = MIN(CGRectGetMaxY(safeBounds) - chosen.size.height,
                                  MAX(CGRectGetMinY(safeBounds), chosen.origin.y));
        }
        label.frame = chosen;
        [occupiedLabels addObject:[NSValue valueWithCGRect:chosen]];
    }
}
@end

@interface Zu4MapPanel : UIView <UITextFieldDelegate>
@property(nonatomic, strong) UILabel *titleLabel;
@property(nonatomic, strong) UILabel *helpLabel;
@property(nonatomic, strong) Zu4MapCanvas *mapCanvas;
@property(nonatomic, strong) UIButton *closeButton;
@property(nonatomic, strong) UIButton *addPinButton;
@property(nonatomic, strong) UIButton *placeLabelsButton;
@property(nonatomic, strong) NSArray<UIButton *> *navigationButtons;
@property(nonatomic, strong) UIView *pinEditor;
@property(nonatomic, strong) UIView *pinEditorCard;
@property(nonatomic, strong) UILabel *pinEditorTitle;
@property(nonatomic, strong) UILabel *pinEditorMessage;
@property(nonatomic, strong) UITextField *pinTextField;
@property(nonatomic, strong) UIButton *pinConfirmButton;
@property(nonatomic, strong) UIButton *pinRemoveButton;
@property(nonatomic, strong) UIButton *pinCancelButton;
@property(nonatomic) int pinEditorX;
@property(nonatomic) int pinEditorY;
@property(nonatomic) CGFloat pinKeyboardOverlap;
@property(nonatomic) int mapWidth;
@property(nonatomic) int mapHeight;
@property(nonatomic) int playerX;
@property(nonatomic) int playerY;
@property(nonatomic) BOOL pinsEnabled;
@property(nonatomic) Zu4MapDismiss dismissCallback;
@property(nonatomic) void *dismissContext;
@end

@implementation Zu4MapPanel
- (void)closeMap {
    [self closePinEditorMessage:nil reload:NO];
    g_native_text_input_active = false;
    Zu4MapDismiss callback = self.dismissCallback;
    void *context = self.dismissContext;
    self.dismissCallback = NULL;
    self.dismissContext = NULL;
    [self removeFromSuperview];
    if (g_map_panel == self) g_map_panel = nil;
    UIAccessibilityPostNotification(UIAccessibilityScreenChangedNotification, g_world_status);
    if (callback) callback(context);
}
- (void)reloadPins {
    for (Zu4MapPinButton *button in self.mapCanvas.pinButtons) [button removeFromSuperview];
    if (!self.pinsEnabled) {
        self.mapCanvas.pinButtons = @[];
        return;
    }
    NSMutableArray<Zu4MapPinButton *> *buttons = [NSMutableArray array];
    int count = zu4_mobile_map_pin_count();
    for (int i = 0; i < count; ++i) {
        Zu4MobileMapPin pin = {};
        if (!zu4_mobile_map_pin_at(i, &pin)) continue;
        Zu4MapPinButton *button = [Zu4MapPinButton buttonWithType:UIButtonTypeSystem];
        button.mapX = pin.x;
        button.mapY = pin.y;
        button.xFraction = ((CGFloat)pin.x + 0.5) / self.mapWidth;
        button.yFraction = ((CGFloat)pin.y + 0.5) / self.mapHeight;
        [button setTitle:@"◆" forState:UIControlStateNormal];
        [button setTitleColor:[UIColor colorWithRed:1 green:0.72 blue:0.12 alpha:1]
                      forState:UIControlStateNormal];
        button.titleLabel.font = [UIFont boldSystemFontOfSize:22];
        NSString *label = [NSString stringWithUTF8String:pin.label];
        button.accessibilityLabel = [NSString stringWithFormat:@"Map pin, %@", label ?: @"marked place"];
        button.accessibilityHint = @"Opens this pin for editing or removal.";
        [button addTarget:self action:@selector(editPin:) forControlEvents:UIControlEventTouchUpInside];
        [self.mapCanvas addSubview:button];
        [buttons addObject:button];
    }
    self.mapCanvas.pinButtons = buttons;
    [self.mapCanvas refreshGeometry];
}
- (void)inspectPlace:(Zu4MapPlaceButton *)button {
    self.helpLabel.text = button.placeDescription;
    UIAccessibilityPostNotification(UIAccessibilityAnnouncementNotification,
                                    button.placeDescription);
}
- (void)reloadPlaces {
    for (Zu4MapPlaceButton *button in self.mapCanvas.placeButtons) {
        [button.mapLabel removeFromSuperview];
        [button removeFromSuperview];
    }
    NSMutableArray<Zu4MapPlaceButton *> *buttons = [NSMutableArray array];
    int count = zu4_mobile_map_discovery_count();
    for (int i = 0; i < count; ++i) {
        Zu4MobileMapDiscovery place = {};
        if (!zu4_mobile_map_discovery_at(i, &place)) continue;
        NSString *symbol = @"●";
        NSString *kind = @"Town";
        UIColor *color = [UIColor colorWithRed:0.20 green:0.78 blue:1 alpha:1];
        switch (place.category) {
        case ZU4_MOBILE_MAP_DISCOVERY_CASTLE:
            symbol = @"■"; kind = @"Castle";
            color = [UIColor colorWithRed:1 green:0.91 blue:0.55 alpha:1];
            break;
        case ZU4_MOBILE_MAP_DISCOVERY_VILLAGE:
            symbol = @"○"; kind = @"Village";
            color = [UIColor colorWithRed:0.34 green:0.91 blue:0.72 alpha:1];
            break;
        case ZU4_MOBILE_MAP_DISCOVERY_SHRINE:
            symbol = @"✦"; kind = @"Shrine";
            color = [UIColor colorWithRed:0.82 green:0.53 blue:1 alpha:1];
            break;
        case ZU4_MOBILE_MAP_DISCOVERY_DUNGEON:
            symbol = @"▲"; kind = @"Dungeon";
            color = [UIColor colorWithRed:1 green:0.45 blue:0.24 alpha:1];
            break;
        default:
            break;
        }
        NSString *name = [NSString stringWithUTF8String:place.name] ?: @"Known place";
        Zu4MapPlaceButton *button = [Zu4MapPlaceButton buttonWithType:UIButtonTypeSystem];
        button.xFraction = ((CGFloat)place.x + 0.5) / self.mapWidth;
        button.yFraction = ((CGFloat)place.y + 0.5) / self.mapHeight;
        button.placeDescription = [NSString stringWithFormat:@"%@ — %@", name, kind];
        button.mapLabel = [[UILabel alloc] init];
        button.mapLabel.text = name;
        button.mapLabel.textColor = UIColor.whiteColor;
        button.mapLabel.backgroundColor = [UIColor colorWithWhite:0.03 alpha:0.84];
        button.mapLabel.font = [UIFontMetrics.defaultMetrics scaledFontForFont:
            [UIFont systemFontOfSize:12 weight:UIFontWeightSemibold]];
        button.mapLabel.adjustsFontForContentSizeCategory = YES;
        button.mapLabel.textAlignment = NSTextAlignmentCenter;
        button.mapLabel.lineBreakMode = NSLineBreakByTruncatingTail;
        button.mapLabel.layer.cornerRadius = 5;
        button.mapLabel.layer.borderWidth = 1;
        button.mapLabel.layer.borderColor = [color colorWithAlphaComponent:0.75].CGColor;
        button.mapLabel.clipsToBounds = YES;
        button.mapLabel.userInteractionEnabled = NO;
        button.mapLabel.isAccessibilityElement = NO;
        [button setTitle:symbol forState:UIControlStateNormal];
        [button setTitleColor:color forState:UIControlStateNormal];
        button.titleLabel.font = [UIFont boldSystemFontOfSize:20];
        button.layer.shadowColor = UIColor.blackColor.CGColor;
        button.layer.shadowOpacity = 0.9;
        button.layer.shadowRadius = 1;
        button.layer.shadowOffset = CGSizeZero;
        button.accessibilityLabel = [NSString stringWithFormat:@"Discovered %@, %@",
                                     kind.lowercaseString, name];
        button.accessibilityHint = @"Shows this place name and type.";
        [button addTarget:self action:@selector(inspectPlace:)
                  forControlEvents:UIControlEventTouchUpInside];
        [self.mapCanvas addSubview:button.mapLabel];
        [self.mapCanvas addSubview:button];
        [buttons addObject:button];
    }
    self.mapCanvas.placeButtons = buttons;
    [self.mapCanvas refreshGeometry];
}
- (void)togglePlaceLabels {
    self.mapCanvas.placeLabelsVisible = !self.mapCanvas.placeLabelsVisible;
    g_map_place_labels_visible = self.mapCanvas.placeLabelsVisible;
    [self.placeLabelsButton setTitle:(self.mapCanvas.placeLabelsVisible
        ? @"Hide Labels" : @"Show Labels") forState:UIControlStateNormal];
    self.placeLabelsButton.accessibilityValue = self.mapCanvas.placeLabelsVisible
        ? @"Shown" : @"Hidden";
    [self.mapCanvas refreshGeometry];
}
- (void)pinTextChanged:(UITextField *)field {
    if (field.text.length > 40) {
        NSRange range = [field.text rangeOfComposedCharacterSequencesForRange:NSMakeRange(0, 40)];
        field.text = [field.text substringWithRange:range];
    }
    NSString *trimmed = [field.text stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet];
    self.pinConfirmButton.enabled = trimmed.length > 0;
    self.pinConfirmButton.alpha = self.pinConfirmButton.enabled ? 1.0 : 0.35;
}
- (void)keyboardFrameChanged:(NSNotification *)note {
    CGRect keyboardFrame = [note.userInfo[UIKeyboardFrameEndUserInfoKey] CGRectValue];
    CGRect localFrame = [self convertRect:keyboardFrame fromView:nil];
    self.pinKeyboardOverlap = MAX(0, CGRectGetMaxY(self.bounds) - CGRectGetMinY(localFrame));
    [self setNeedsLayout];
    [self layoutIfNeeded];
}
- (void)keyboardWillHide:(NSNotification *)note {
    self.pinKeyboardOverlap = 0;
    [self setNeedsLayout];
}
- (void)closePinEditorMessage:(NSString *)message reload:(BOOL)reload {
    if (!self.pinEditor) return;
    [self.pinTextField resignFirstResponder];
    [NSNotificationCenter.defaultCenter removeObserver:self
                                                   name:UIKeyboardWillChangeFrameNotification object:nil];
    [NSNotificationCenter.defaultCenter removeObserver:self
                                                   name:UIKeyboardWillHideNotification object:nil];
    [self.pinEditor removeFromSuperview];
    self.pinEditor = nil;
    self.pinEditorCard = nil;
    self.pinEditorTitle = nil;
    self.pinEditorMessage = nil;
    self.pinTextField = nil;
    self.pinConfirmButton = nil;
    self.pinRemoveButton = nil;
    self.pinCancelButton = nil;
    self.pinKeyboardOverlap = 0;
    g_native_text_input_active = false;
    if (message.length) self.helpLabel.text = message;
    if (reload) [self reloadPins];
    [self setNeedsLayout];
    UIAccessibilityPostNotification(UIAccessibilityScreenChangedNotification, self.helpLabel);
}
- (void)savePinEditor {
    NSString *label = [self.pinTextField.text
        stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet];
    if (!label.length) return;
    BOOL changed = zu4_mobile_set_map_pin(self.pinEditorX, self.pinEditorY, label.UTF8String);
    [self closePinEditorMessage:(changed
        ? @"Pin updated. It will be included in the next checkpoint."
        : @"The pin could not be updated. Your existing pins are unchanged.") reload:changed];
}
- (void)removePinEditor {
    BOOL changed = zu4_mobile_remove_map_pin(self.pinEditorX, self.pinEditorY);
    [self closePinEditorMessage:(changed
        ? @"Pin removed. The change will be included in the next checkpoint."
        : @"The pin could not be removed. Your existing pins are unchanged.") reload:changed];
}
- (void)cancelPinEditor {
    [self closePinEditorMessage:nil reload:NO];
}
- (BOOL)textFieldShouldReturn:(UITextField *)textField {
    if (self.pinConfirmButton.enabled) [self savePinEditor];
    return NO;
}
- (UIButton *)pinEditorButton:(NSString *)title selector:(SEL)selector {
    UIButton *button = [UIButton buttonWithType:UIButtonTypeSystem];
    [button setTitle:title forState:UIControlStateNormal];
    button.titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
    button.backgroundColor = [UIColor colorWithWhite:0.18 alpha:1];
    button.layer.cornerRadius = 8;
    button.layer.borderWidth = 1;
    button.layer.borderColor = [UIColor colorWithWhite:1 alpha:0.22].CGColor;
    [button addTarget:self action:selector forControlEvents:UIControlEventTouchUpInside];
    return button;
}
- (void)presentPinEditorX:(int)x y:(int)y label:(NSString *)existingLabel {
    if (self.pinEditor) return;
    g_native_text_input_active = true;
    BOOL editing = existingLabel.length > 0;
    self.pinEditorX = x;
    self.pinEditorY = y;
    self.pinEditor = [[UIView alloc] initWithFrame:self.bounds];
    self.pinEditor.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    self.pinEditor.backgroundColor = [UIColor colorWithWhite:0 alpha:0.62];
    self.pinEditor.accessibilityViewIsModal = YES;
    self.pinEditorCard = [[UIView alloc] init];
    self.pinEditorCard.backgroundColor = [UIColor colorWithWhite:0.075 alpha:1];
    self.pinEditorCard.layer.cornerRadius = 14;
    self.pinEditorCard.layer.borderWidth = 1;
    self.pinEditorCard.layer.borderColor = [UIColor colorWithWhite:1 alpha:0.25].CGColor;
    self.pinEditorTitle = [[UILabel alloc] init];
    self.pinEditorTitle.text = editing ? @"Edit Map Pin" : @"Pin Current Location";
    self.pinEditorTitle.textColor = UIColor.whiteColor;
    self.pinEditorTitle.font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
    self.pinEditorMessage = [[UILabel alloc] init];
    self.pinEditorMessage.text = @"Use a short label you will recognize. Pins are stored with this adventure.";
    self.pinEditorMessage.textColor = [UIColor colorWithWhite:0.76 alpha:1];
    self.pinEditorMessage.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    self.pinEditorMessage.numberOfLines = 2;
    self.pinTextField = [[UITextField alloc] init];
    self.pinTextField.backgroundColor = [UIColor colorWithWhite:0.16 alpha:1];
    self.pinTextField.textColor = UIColor.whiteColor;
    self.pinTextField.tintColor = [UIColor colorWithRed:1 green:0.72 blue:0.12 alpha:1];
    self.pinTextField.layer.cornerRadius = 8;
    self.pinTextField.layer.borderWidth = 1;
    self.pinTextField.layer.borderColor = [UIColor colorWithWhite:1 alpha:0.28].CGColor;
    self.pinTextField.leftView = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 10, 1)];
    self.pinTextField.leftViewMode = UITextFieldViewModeAlways;
    self.pinTextField.rightView = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 10, 1)];
    self.pinTextField.rightViewMode = UITextFieldViewModeAlways;
    self.pinTextField.placeholder = @"Reagent seller, locked door…";
    self.pinTextField.attributedPlaceholder = [[NSAttributedString alloc]
        initWithString:self.pinTextField.placeholder
        attributes:@{NSForegroundColorAttributeName: [UIColor colorWithWhite:0.55 alpha:1]}];
    self.pinTextField.text = existingLabel;
    self.pinTextField.clearButtonMode = UITextFieldViewModeWhileEditing;
    self.pinTextField.returnKeyType = UIReturnKeyDone;
    self.pinTextField.delegate = self;
    self.pinTextField.accessibilityLabel = @"Map pin label";
    [self.pinTextField addTarget:self action:@selector(pinTextChanged:)
                forControlEvents:UIControlEventEditingChanged];
    self.pinConfirmButton = [self pinEditorButton:(editing ? @"Save" : @"Add Pin")
                                         selector:@selector(savePinEditor)];
    self.pinCancelButton = [self pinEditorButton:@"Cancel" selector:@selector(cancelPinEditor)];
    if (editing) {
        self.pinRemoveButton = [self pinEditorButton:@"Remove" selector:@selector(removePinEditor)];
        [self.pinRemoveButton setTitleColor:[UIColor colorWithRed:1 green:0.33 blue:0.31 alpha:1]
                                   forState:UIControlStateNormal];
    }
    [self.pinEditor addSubview:self.pinEditorCard];
    [self.pinEditorCard addSubview:self.pinEditorTitle];
    [self.pinEditorCard addSubview:self.pinEditorMessage];
    [self.pinEditorCard addSubview:self.pinTextField];
    [self.pinEditorCard addSubview:self.pinConfirmButton];
    [self.pinEditorCard addSubview:self.pinCancelButton];
    if (self.pinRemoveButton) [self.pinEditorCard addSubview:self.pinRemoveButton];
    [self addSubview:self.pinEditor];
    [self pinTextChanged:self.pinTextField];
    [NSNotificationCenter.defaultCenter addObserver:self selector:@selector(keyboardFrameChanged:)
                                               name:UIKeyboardWillChangeFrameNotification object:nil];
    [NSNotificationCenter.defaultCenter addObserver:self selector:@selector(keyboardWillHide:)
                                               name:UIKeyboardWillHideNotification object:nil];
    [self setNeedsLayout];
    [self layoutIfNeeded];
    [self.pinTextField becomeFirstResponder];
    UIAccessibilityPostNotification(UIAccessibilityScreenChangedNotification, self.pinEditorTitle);
}
- (void)addCurrentPin {
    NSString *existing = nil;
    for (Zu4MapPinButton *button in self.mapCanvas.pinButtons)
        if (button.mapX == self.playerX && button.mapY == self.playerY) {
            NSString *prefix = @"Map pin, ";
            existing = [button.accessibilityLabel hasPrefix:prefix]
                ? [button.accessibilityLabel substringFromIndex:prefix.length] : button.accessibilityLabel;
            break;
        }
    if (!existing && !zu4_mobile_can_set_map_pin(self.playerX, self.playerY)) {
        self.helpLabel.text = @"Pin limit reached. Remove an existing pin before adding another.";
        UIAccessibilityPostNotification(UIAccessibilityAnnouncementNotification, self.helpLabel.text);
        return;
    }
    [self presentPinEditorX:self.playerX y:self.playerY label:existing];
}
- (void)editPin:(Zu4MapPinButton *)button {
    NSString *prefix = @"Map pin, ";
    NSString *label = [button.accessibilityLabel hasPrefix:prefix]
        ? [button.accessibilityLabel substringFromIndex:prefix.length] : button.accessibilityLabel;
    [self presentPinEditorX:button.mapX y:button.mapY label:label];
}
- (void)changeMap:(UIButton *)sender {
    Zu4MapCanvas *canvas = self.mapCanvas;
    if (sender.tag == 1 || sender.tag == 2) {
        CGFloat next = sender.tag == 1 ? canvas.zoomLevel / 2.0 : canvas.zoomLevel * 2.0;
        canvas.zoomLevel = MIN(8.0, MAX(1.0, next));
    } else {
        CGFloat horizontalSpan = canvas.bounds.size.width /
            MAX(1.0, MIN(canvas.bounds.size.width, canvas.bounds.size.height) * canvas.zoomLevel);
        CGFloat verticalSpan = canvas.bounds.size.height /
            MAX(1.0, MIN(canvas.bounds.size.width, canvas.bounds.size.height) * canvas.zoomLevel);
        if (sender.tag == 3) canvas.centerYFraction -= verticalSpan * 0.3;
        if (sender.tag == 4) canvas.centerYFraction += verticalSpan * 0.3;
        if (sender.tag == 5) canvas.centerXFraction -= horizontalSpan * 0.3;
        if (sender.tag == 6) canvas.centerXFraction += horizontalSpan * 0.3;
        canvas.centerXFraction = MIN(1.0, MAX(0.0, canvas.centerXFraction));
        canvas.centerYFraction = MIN(1.0, MAX(0.0, canvas.centerYFraction));
    }
    [canvas refreshGeometry];
    self.navigationButtons[0].enabled = canvas.zoomLevel > 1.0;
    self.navigationButtons[1].enabled = canvas.zoomLevel < 8.0;
    for (NSUInteger i = 0; i < self.navigationButtons.count; ++i) {
        if (i >= 2) self.navigationButtons[i].enabled = canvas.zoomLevel > 1.0;
        self.navigationButtons[i].alpha = self.navigationButtons[i].enabled ? 1.0 : 0.35;
    }
}
- (void)layoutSubviews {
    [super layoutSubviews];
    UIEdgeInsets safe = self.safeAreaInsets;
    CGRect available = UIEdgeInsetsInsetRect(self.bounds, UIEdgeInsetsMake(safe.top + 12, safe.left + 16,
                                                                           safe.bottom + 12, safe.right + 16));
    self.titleLabel.frame = CGRectMake(CGRectGetMinX(available), CGRectGetMinY(available),
                                       available.size.width - (self.addPinButton ? 196 : 88), 44);
    self.closeButton.frame = CGRectMake(CGRectGetMaxX(available) - 76, CGRectGetMinY(available), 76, 44);
    self.addPinButton.frame = CGRectMake(CGRectGetMaxX(available) - 184, CGRectGetMinY(available), 100, 44);
    self.helpLabel.frame = CGRectMake(CGRectGetMinX(available), CGRectGetMinY(available) + 38,
                                      available.size.width, 30);
    self.mapCanvas.frame = CGRectMake(CGRectGetMinX(available), CGRectGetMinY(available) + 70,
                                      available.size.width, MAX(0, available.size.height - 70));
    CGFloat buttonSize = 44, step = 48;
    CGFloat dpadX = CGRectGetMinX(available) + 8;
    CGFloat dpadY = CGRectGetMaxY(available) - (buttonSize + step * 2);
    self.navigationButtons[2].frame = CGRectMake(dpadX + step, dpadY, buttonSize, buttonSize);
    self.navigationButtons[4].frame = CGRectMake(dpadX, dpadY + step, buttonSize, buttonSize);
    self.navigationButtons[5].frame = CGRectMake(dpadX + step * 2, dpadY + step, buttonSize, buttonSize);
    self.navigationButtons[3].frame = CGRectMake(dpadX + step, dpadY + step * 2, buttonSize, buttonSize);
    CGFloat zoomX = CGRectGetMaxX(available) - buttonSize - 8;
    self.navigationButtons[1].frame = CGRectMake(zoomX, CGRectGetMaxY(available) - buttonSize - step, buttonSize, buttonSize);
    self.navigationButtons[0].frame = CGRectMake(zoomX, CGRectGetMaxY(available) - buttonSize, buttonSize, buttonSize);
    self.placeLabelsButton.frame = CGRectMake(zoomX - 112, CGRectGetMaxY(available) - buttonSize,
                                              104, buttonSize);
    if (self.pinEditor) {
        self.pinEditor.frame = self.bounds;
        CGRect editorArea = UIEdgeInsetsInsetRect(self.pinEditor.bounds, self.safeAreaInsets);
        editorArea.size.height = MAX(0, editorArea.size.height - self.pinKeyboardOverlap);
        BOOL portrait = CGRectGetHeight(self.bounds) > CGRectGetWidth(self.bounds);
        CGFloat cardWidth = MIN(portrait ? 360.0 : 460.0, MAX(280.0, CGRectGetWidth(editorArea) - 24.0));
        CGFloat cardHeight = portrait ? 190.0 : 160.0;
        CGFloat cardX = CGRectGetMidX(editorArea) - cardWidth / 2.0;
        CGFloat cardY = CGRectGetMinY(editorArea) + MAX(8.0, (CGRectGetHeight(editorArea) - cardHeight) / 2.0);
        self.pinEditorCard.frame = CGRectMake(cardX, cardY, cardWidth, cardHeight);
        CGFloat margin = 12.0;
        CGFloat contentWidth = cardWidth - margin * 2.0;
        self.pinEditorTitle.frame = CGRectMake(margin, 8, contentWidth, 26);
        CGFloat messageY = portrait ? 38.0 : 34.0;
        CGFloat messageHeight = portrait ? 34.0 : 18.0;
        self.pinEditorMessage.numberOfLines = portrait ? 2 : 1;
        self.pinEditorMessage.frame = CGRectMake(margin, messageY, contentWidth, messageHeight);
        CGFloat fieldY = portrait ? 78.0 : 57.0;
        self.pinTextField.frame = CGRectMake(margin, fieldY, contentWidth, 44.0);
        CGFloat actionsY = portrait ? 134.0 : 108.0;
        NSUInteger count = self.pinRemoveButton ? 3 : 2;
        CGFloat gap = 8.0;
        CGFloat actionWidth = (contentWidth - gap * (count - 1)) / count;
        NSArray<UIButton *> *actions = self.pinRemoveButton
            ? @[self.pinCancelButton, self.pinRemoveButton, self.pinConfirmButton]
            : @[self.pinCancelButton, self.pinConfirmButton];
        for (NSUInteger i = 0; i < actions.count; ++i)
            actions[i].frame = CGRectMake(margin + i * (actionWidth + gap), actionsY, actionWidth, 44.0);
        [self bringSubviewToFront:self.pinEditor];
    }
}
@end

static BOOL zu4_map_panel_visible(void) { return g_map_panel != nil; }
int zu4_ios_native_text_input_active(void) { return g_native_text_input_active ? 1 : 0; }
void zu4_ios_set_native_text_input_active(int active) {
    if (!active) SDL_FlushEvents(SDL_KEYDOWN, SDL_KEYUP);
    g_native_text_input_active = active != 0;
}

static void zu4_ios_show_map(const unsigned char *rgba, int width, int height,
                             int playerX, int playerY, int pinsEnabled, int discoveriesEnabled,
                             NSString *title, NSString *help, NSString *accessibilityName,
                             CGFloat initialZoom, int gridColumns, int gridRows,
                             Zu4MapDismiss dismiss, void *context)
{
    if (!rgba || width <= 0 || height <= 0 || !g_overlay.window) {
        if (dismiss) dismiss(context);
        return;
    }
    NSData *pixels = [NSData dataWithBytes:rgba length:(NSUInteger)width * height * 4];
    CGDataProviderRef provider = CGDataProviderCreateWithCFData((__bridge CFDataRef)pixels);
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    CGImageRef imageRef = CGImageCreate(width, height, 8, 32, width * 4, colorSpace,
        kCGImageAlphaPremultipliedLast | kCGBitmapByteOrderDefault, provider, NULL, false,
        kCGRenderingIntentDefault);
    UIImage *image = imageRef ? [UIImage imageWithCGImage:imageRef scale:1 orientation:UIImageOrientationUp] : nil;
    if (imageRef) CGImageRelease(imageRef);
    CGColorSpaceRelease(colorSpace);
    CGDataProviderRelease(provider);
    if (!image) {
        if (dismiss) dismiss(context);
        return;
    }

    dispatch_async(dispatch_get_main_queue(), ^{
        if (g_map_panel) [(Zu4MapPanel *)g_map_panel closeMap];
        Zu4MapPanel *panel = [[Zu4MapPanel alloc] initWithFrame:g_overlay.window.bounds];
        panel.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
        panel.backgroundColor = dismiss ? UIColor.blackColor
                                        : [UIColor colorWithWhite:0.025 alpha:0.98];
        panel.accessibilityViewIsModal = YES;
        panel.mapWidth = width;
        panel.mapHeight = height;
        panel.playerX = playerX;
        panel.playerY = playerY;
        panel.pinsEnabled = pinsEnabled != 0;
        panel.dismissCallback = dismiss;
        panel.dismissContext = context;
        panel.titleLabel = [[UILabel alloc] init];
        panel.titleLabel.text = title;
        panel.titleLabel.textColor = UIColor.whiteColor;
        panel.titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleTitle2];
        panel.helpLabel = [[UILabel alloc] init];
        panel.helpLabel.text = help;
        panel.helpLabel.textColor = [UIColor colorWithWhite:0.72 alpha:1];
        panel.helpLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
        panel.helpLabel.numberOfLines = 2;
        panel.mapCanvas = [[Zu4MapCanvas alloc] init];
        panel.mapCanvas.clipsToBounds = YES;
        panel.mapCanvas.zoomLevel = initialZoom;
        panel.mapCanvas.playerXFraction = ((CGFloat)playerX + 0.5) / width;
        panel.mapCanvas.playerYFraction = ((CGFloat)playerY + 0.5) / height;
        panel.mapCanvas.gridColumns = gridColumns;
        panel.mapCanvas.gridRows = gridRows;
        panel.mapCanvas.centerXFraction = panel.mapCanvas.playerXFraction;
        panel.mapCanvas.centerYFraction = panel.mapCanvas.playerYFraction;
        panel.mapCanvas.placeLabelsVisible = g_map_place_labels_visible;
        panel.mapCanvas.imageView = [[UIImageView alloc] initWithImage:image];
        panel.mapCanvas.imageView.layer.magnificationFilter = kCAFilterNearest;
        if (gridColumns > 0 && gridRows > 0) {
            panel.mapCanvas.imageView.layer.borderWidth = 1.5;
            panel.mapCanvas.imageView.layer.borderColor =
                [UIColor colorWithWhite:1 alpha:0.42].CGColor;
            panel.mapCanvas.imageView.layer.masksToBounds = YES;
            panel.mapCanvas.gridLayer = [CAShapeLayer layer];
            panel.mapCanvas.gridLayer.fillColor = UIColor.clearColor.CGColor;
            panel.mapCanvas.gridLayer.strokeColor =
                [UIColor colorWithWhite:1 alpha:0.14].CGColor;
            panel.mapCanvas.gridLayer.lineWidth = 1.0;
            [panel.mapCanvas.imageView.layer addSublayer:panel.mapCanvas.gridLayer];
        }
        panel.mapCanvas.imageView.accessibilityLabel = [NSString stringWithFormat:
            @"%@. Party position column %d, row %d.", accessibilityName, playerX, playerY];
        panel.mapCanvas.imageView.isAccessibilityElement = YES;
        panel.mapCanvas.playerMarker = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 8, 8)];
        panel.mapCanvas.playerMarker.layer.cornerRadius = 4;
        panel.mapCanvas.playerMarker.layer.borderWidth = 1;
        panel.mapCanvas.playerMarker.layer.borderColor = UIColor.whiteColor.CGColor;
        panel.mapCanvas.playerMarker.backgroundColor = UIColor.redColor;
        panel.mapCanvas.playerMarker.userInteractionEnabled = NO;
        if (!UIAccessibilityIsReduceMotionEnabled()) {
            CAKeyframeAnimation *flash = [CAKeyframeAnimation animationWithKeyPath:@"backgroundColor"];
            flash.values = @[(id)UIColor.redColor.CGColor, (id)UIColor.blueColor.CGColor];
            flash.duration = 0.7;
            flash.autoreverses = YES;
            flash.repeatCount = HUGE_VALF;
            [panel.mapCanvas.playerMarker.layer addAnimation:flash forKey:@"partyFlash"];
        } else {
            panel.mapCanvas.playerMarker.layer.borderColor = UIColor.blueColor.CGColor;
            panel.mapCanvas.playerMarker.layer.borderWidth = 2;
        }
        [panel.mapCanvas addSubview:panel.mapCanvas.imageView];
        if (discoveriesEnabled) [panel reloadPlaces];
        [panel.mapCanvas addSubview:panel.mapCanvas.playerMarker];
        [panel reloadPins];
        NSArray *symbols = @[@"−", @"+", @"↑", @"↓", @"←", @"→"];
        NSArray *names = @[@"Zoom out", @"Zoom in", @"Pan up", @"Pan down", @"Pan left", @"Pan right"];
        NSMutableArray *buttons = [NSMutableArray array];
        for (NSInteger i = 0; i < symbols.count; ++i) {
            UIButton *button = [UIButton buttonWithType:UIButtonTypeSystem];
            [button setTitle:symbols[i] forState:UIControlStateNormal];
            button.titleLabel.font = [UIFont boldSystemFontOfSize:24];
            button.backgroundColor = [UIColor colorWithWhite:0.16 alpha:1];
            button.layer.cornerRadius = 8;
            button.layer.borderWidth = 1;
            button.layer.borderColor = [UIColor colorWithWhite:1 alpha:0.28].CGColor;
            button.tag = i + 1;
            button.accessibilityLabel = names[i];
            [button addTarget:panel action:@selector(changeMap:) forControlEvents:UIControlEventTouchUpInside];
            [buttons addObject:button];
        }
        panel.navigationButtons = buttons;
        if (discoveriesEnabled) {
            panel.placeLabelsButton = [UIButton buttonWithType:UIButtonTypeSystem];
            [panel.placeLabelsButton setTitle:(panel.mapCanvas.placeLabelsVisible
                ? @"Hide Labels" : @"Show Labels") forState:UIControlStateNormal];
            panel.placeLabelsButton.titleLabel.font =
                [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
            panel.placeLabelsButton.backgroundColor = [UIColor colorWithWhite:0.16 alpha:1];
            panel.placeLabelsButton.layer.cornerRadius = 8;
            panel.placeLabelsButton.layer.borderWidth = 1;
            panel.placeLabelsButton.layer.borderColor = [UIColor colorWithWhite:1 alpha:0.28].CGColor;
            panel.placeLabelsButton.accessibilityLabel = @"Place labels";
            panel.placeLabelsButton.accessibilityValue = panel.mapCanvas.placeLabelsVisible
                ? @"Shown" : @"Hidden";
            panel.placeLabelsButton.accessibilityHint = @"Shows or hides discovered place names on the map.";
            [panel.placeLabelsButton addTarget:panel action:@selector(togglePlaceLabels)
                              forControlEvents:UIControlEventTouchUpInside];
        }
        panel.closeButton = [UIButton buttonWithType:UIButtonTypeSystem];
        [panel.closeButton setTitle:(dismiss ? @"Return" : @"Done") forState:UIControlStateNormal];
        panel.closeButton.titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
        [panel.closeButton addTarget:panel action:@selector(closeMap) forControlEvents:UIControlEventTouchUpInside];
        if (pinsEnabled) {
            panel.addPinButton = [UIButton buttonWithType:UIButtonTypeSystem];
            [panel.addPinButton setTitle:@"Pin Here" forState:UIControlStateNormal];
            panel.addPinButton.titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
            panel.addPinButton.accessibilityLabel = @"Pin current location";
            panel.addPinButton.accessibilityHint = @"Adds or edits a labeled pin at the party's current or last overworld position.";
            [panel.addPinButton addTarget:panel action:@selector(addCurrentPin) forControlEvents:UIControlEventTouchUpInside];
        }
        [panel addSubview:panel.titleLabel];
        [panel addSubview:panel.helpLabel];
        [panel addSubview:panel.mapCanvas];
        for (UIButton *button in panel.navigationButtons) [panel addSubview:button];
        if (panel.placeLabelsButton) [panel addSubview:panel.placeLabelsButton];
        [panel addSubview:panel.closeButton];
        if (panel.addPinButton) [panel addSubview:panel.addPinButton];
        g_map_panel = panel;
        [g_overlay.window addSubview:panel];
        UIAccessibilityPostNotification(UIAccessibilityScreenChangedNotification, panel.titleLabel);
    });
}

void zu4_ios_show_exploration_map(const unsigned char *rgba, int width, int height,
                                  int playerX, int playerY, int pinsEnabled)
{
    zu4_ios_show_map(rgba, width, height, playerX, playerY, pinsEnabled, 1,
        @"Britannia Map",
        @"Only explored terrain and visited places are shown. Labels can be hidden below.",
        @"Explored map of Britannia", 4.0, 0, 0, NULL, NULL);
}

void zu4_ios_show_dungeon_exploration_map(const unsigned char *rgba, int width, int height,
                                          int playerX, int playerY,
                                          const char *dungeonName, int level)
{
    NSString *dungeon = dungeonName && dungeonName[0]
        ? [NSString stringWithUTF8String:dungeonName] : @"Dungeon";
    NSString *place = [NSString stringWithFormat:@"%@ — Level %d", dungeon, level];
    zu4_ios_show_map(rgba, width, height, playerX, playerY, 0, 0, place,
        @"North ↑  •  8×8 floor  •  Black cells are unseen.",
        [NSString stringWithFormat:@"North-up explored map of %@", place],
        1.0, width, height, NULL, NULL);
}

void zu4_ios_show_gem_map(const unsigned char *rgba, int width, int height,
                          int playerX, int playerY, const char *locationName,
                          Zu4MapDismiss dismiss, void *context)
{
    NSString *location = locationName && locationName[0]
        ? [NSString stringWithUTF8String:locationName] : @"Current Area";
    zu4_ios_show_map(rgba, width, height, playerX, playerY, 0, 0,
        [NSString stringWithFormat:@"%@ — Gem View", location],
        @"Peer across this area. Zoom and pan with the visible controls, then Return.",
        [NSString stringWithFormat:@"Gem view of %@", location], 1.0, 0, 0, dismiss, context);
}

void zu4_ios_dismiss_map(void)
{
    dispatch_async(dispatch_get_main_queue(), ^{
        if (g_map_panel) [(Zu4MapPanel *)g_map_panel closeMap];
    });
}

@interface Zu4PassthroughView : UIView
@end
@implementation Zu4PassthroughView
- (void)layoutSubviews {
    [super layoutSubviews];
    if (self == g_overlay) zu4_layout_controls();
}
// Return YES so hitTest still recurses into subviews (buttons) even when they've
// been transformed outside this view's own bounds — e.g. the D-pad shifted left
// under the keyboard-scale. Passthrough is preserved by hitTest below (empty
// areas still resolve to self and are dropped).
- (BOOL)pointInside:(CGPoint)point withEvent:(UIEvent *)event
{
	return YES;
}
- (UIView *)hitTest:(CGPoint)point withEvent:(UIEvent *)event
{
	UIView *hit = [super hitTest:point withEvent:event];
	return (hit == self) ? nil : hit;   // empty area -> pass through to the game view
}
@end

@interface Zu4ButtonTarget : NSObject {
    NSTimer *movementTimer;
    UIButton *heldDirection;
}
- (void)stopMoving;
- (void)repeatMovement:(NSTimer *)timer;
- (void)onTap:(UIButton *)sender;
- (void)onWorldTap:(UIButton *)sender;
- (void)onCombatTarget:(Zu4CombatTargetButton *)sender;
- (void)keyboardWillShow:(NSNotification *)note;
- (void)keyboardWillHide:(NSNotification *)note;
@end

@implementation Zu4ButtonTarget
- (void)stopMoving {
    [movementTimer invalidate];
    movementTimer = nil;
    heldDirection = nil;
}
- (void)repeatMovement:(NSTimer *)timer {
    if (!heldDirection || !heldDirection.highlighted || !zu4_mobile_can_repeat_movement() ||
        zu4_topic_panel_is_visible() || UIApplication.sharedApplication.applicationState != UIApplicationStateActive) {
        [self stopMoving];
        return;
    }
    int direction = heldDirection.tag == SDLK_UP ? DIR_NORTH : heldDirection.tag == SDLK_DOWN ? DIR_SOUTH :
        heldDirection.tag == SDLK_LEFT ? DIR_WEST : DIR_EAST;
    zu4_queue_mobile_action(ZU4_MOBILE_ACTION_MOVE, direction);
}
- (void)onWorldTap:(UIButton *)sender
{
    if (!sender || !g_world_touch ||
        UIApplication.sharedApplication.applicationState != UIApplicationStateActive) return;
    zu4_mobile_cancel_walk();
    int tag = (int)sender.tag;
    if (tag >= DIR_WEST && tag <= DIR_SOUTH) {
        int token = zu4_mobile_capture_adjacent_interaction(tag);
        if (token) {
            zu4_queue_mobile_action(ZU4_MOBILE_ACTION_ADJACENT, token);
            return;
        }
        int offsetX = tag == DIR_EAST ? 1 : tag == DIR_WEST ? -1 : 0;
        int offsetY = tag == DIR_SOUTH ? 1 : tag == DIR_NORTH ? -1 : 0;
        token = zu4_mobile_capture_walk(offsetX, offsetY);
        if (token) zu4_queue_mobile_action(ZU4_MOBILE_ACTION_WALK_START, token);
        return;
    }
    int offsetX = (tag & 15) - 5;
    int offsetY = ((tag >> 4) & 15) - 5;
    int token = zu4_mobile_capture_walk(offsetX, offsetY);
    if (token) zu4_queue_mobile_action(ZU4_MOBILE_ACTION_WALK_START, token);
}
- (void)onCombatTarget:(Zu4CombatTargetButton *)sender
{
    if (!sender || !zu4_mobile_combat_active() ||
        UIApplication.sharedApplication.applicationState != UIApplicationStateActive) return;
    zu4_queue_mobile_action(ZU4_MOBILE_ACTION_COMBAT_TARGET, sender.targetToken);
}
- (void)onTap:(UIButton *)sender
{
    [self stopMoving];
    if (UIApplication.sharedApplication.applicationState != UIApplicationStateActive) return;
    zu4_mobile_cancel_walk();
    if (zu4_topic_panel_direction_active()) {
        const char *direction = sender.tag == SDLK_UP ? "north" : sender.tag == SDLK_DOWN ? "south" :
            sender.tag == SDLK_LEFT ? "west" : sender.tag == SDLK_RIGHT ? "east" : NULL;
        if (direction) zu4_topic_panel_choose_direction(direction);
        return;
    }
    if (sender.tag == ZU4_TAG_MENU) {
        zu4_queue_mobile_action(ZU4_MOBILE_ACTION_MENU);
        return;
    }
    if (sender.tag == ZU4_TAG_REPEAT_ATTACK) {
        zu4_queue_mobile_action(ZU4_MOBILE_ACTION_COMBAT_REPEAT_ATTACK);
        return;
    }
    if (sender.tag == ZU4_TAG_MAP) {
        zu4_queue_mobile_action(ZU4_MOBILE_ACTION_MAP);
        return;
    }
    if (zu4_mobile_combat_active() && sender.tag == ZU4_TAG_ENTER) {
        zu4_queue_mobile_action(ZU4_MOBILE_ACTION_COMBAT_CYCLE, -1);
        return;
    }
    if (zu4_mobile_combat_active() && sender.tag == ZU4_TAG_JOURNAL) {
        zu4_queue_mobile_action(ZU4_MOBILE_ACTION_COMBAT_CYCLE, 1);
        return;
    }
    if (zu4_mobile_combat_active() && sender.tag == SDLK_RETURN) {
        zu4_queue_mobile_action(ZU4_MOBILE_ACTION_COMBAT_CLEAR_TARGET);
        return;
    }
    if (sender.tag == SDLK_RETURN && zu4_mobile_dungeon_active()) {
        zu4_queue_mobile_action(ZU4_MOBILE_ACTION_DUNGEON_VIEW);
        return;
    }
    if (sender.tag == ZU4_TAG_CAST) {
        zu4_queue_mobile_action(ZU4_MOBILE_ACTION_CAST);
        return;
    }
    if (sender.tag == ZU4_TAG_PARTY) {
        zu4_queue_mobile_action(ZU4_MOBILE_ACTION_PARTY);
        return;
    }
    if (sender.tag >= ZU4_TAG_PARTY_MEMBER_BASE &&
        sender.tag < ZU4_TAG_PARTY_MEMBER_BASE + ZU4_MOBILE_MAX_PARTY_MEMBERS) {
        zu4_queue_mobile_action(ZU4_MOBILE_ACTION_PARTY_MEMBER,
                                (int)(sender.tag - ZU4_TAG_PARTY_MEMBER_BASE));
        return;
    }
    if (sender.tag == ZU4_TAG_JOURNAL) {
        zu4_queue_mobile_action(zu4_mobile_dungeon_active()
            ? ZU4_MOBILE_ACTION_DUNGEON_TORCH : ZU4_MOBILE_ACTION_JOURNAL);
        return;
    }
    if (sender.tag == ZU4_TAG_ENTER) {
        zu4_queue_mobile_action(ZU4_MOBILE_ACTION_CONTEXT);
        return;
    }
    if (sender.tag == ZU4_TAG_TALK) {
        zu4_queue_mobile_action(zu4_mobile_dungeon_active()
            ? ZU4_MOBILE_ACTION_DUNGEON_SEARCH : ZU4_MOBILE_ACTION_TALK);
        return;
    }
	if(sender.tag == ZU4_TAG_KEYBOARD) {
		zu4_ios_toggle_keyboard();
		return;
	}
    if ((sender.tag == SDLK_UP || sender.tag == SDLK_DOWN || sender.tag == SDLK_LEFT || sender.tag == SDLK_RIGHT)
        && zu4_mobile_can_repeat_movement()) {
        BOOL oneShotTurn = zu4_mobile_dungeon_active() && !zu4_mobile_dungeon_top_down() &&
            (sender.tag == SDLK_LEFT || sender.tag == SDLK_RIGHT);
        if (!oneShotTurn) {
            heldDirection = sender;
            movementTimer = [NSTimer timerWithTimeInterval:0.16 target:self selector:@selector(repeatMovement:) userInfo:nil repeats:YES];
            movementTimer.fireDate = [NSDate dateWithTimeIntervalSinceNow:0.35];
            [NSRunLoop.mainRunLoop addTimer:movementTimer forMode:NSRunLoopCommonModes];
        }
        int direction = sender.tag == SDLK_UP ? DIR_NORTH : sender.tag == SDLK_DOWN ? DIR_SOUTH :
            sender.tag == SDLK_LEFT ? DIR_WEST : DIR_EAST;
        zu4_queue_mobile_action(ZU4_MOBILE_ACTION_MOVE, direction | 0x100);
        return;
    }
	zu4_push_key((SDL_Keycode)sender.tag);
}

- (void)keyboardWillShow:(NSNotification *)note
{
	if (zu4_topic_panel_is_visible()) return;
	zu4_quiet_keyboard();   // suppress predictive/suggestion UI
}

- (void)keyboardWillHide:(NSNotification *)note
{
	if (!g_kb_shown || zu4_topic_panel_is_visible()) return;
	dispatch_async(dispatch_get_main_queue(), ^{
		if(g_overlay)
			g_overlay.transform = CGAffineTransformIdentity;
		if(g_dpad)
			g_dpad.transform = CGAffineTransformIdentity;   // back to normal spot
	});
}
@end

// Retain the target for the lifetime of the app so the button actions fire.
static Zu4ButtonTarget *g_btn_target = nil;

// SDL drives text input through a hidden UITextField whose accumulating text
// makes iOS show a predictive/candidate bar (and, on iOS 17+, inline
// predictions) — the "letters filling" the user sees. U4 is single-key driven,
// so we don't want any of that. Walk the view tree, find SDL's text field, and
// switch off every suggestion/prediction feature.
static void zu4_quiet_text_field(UIView *v)
{
	if(v == nil)
		return;
	if([v isKindOfClass:[UITextField class]]) {
		UITextField *tf = (UITextField *)v;
		tf.autocorrectionType = UITextAutocorrectionTypeNo;
		tf.autocapitalizationType = UITextAutocapitalizationTypeNone;
		tf.spellCheckingType = UITextSpellCheckingTypeNo;
		tf.smartQuotesType = UITextSmartQuotesTypeNo;
		tf.smartDashesType = UITextSmartDashesTypeNo;
		tf.smartInsertDeleteType = UITextSmartInsertDeleteTypeNo;
		// iOS 17+ inline predictions (ghosted suggestion text).
		if(@available(iOS 17.0, *))
			tf.inlinePredictionType = UITextInlinePredictionTypeNo;
	}
	for(UIView *sub in v.subviews)
		zu4_quiet_text_field(sub);
}

static void zu4_quiet_keyboard(void)
{
	// Defer so SDL has created/added its text field first.
	dispatch_async(dispatch_get_main_queue(), ^{
		for(UIWindow *w in [UIApplication sharedApplication].windows)
			zu4_quiet_text_field(w);
	});
}

void zu4_ios_show_keyboard(int show)
{
	if(show) {
		if(!SDL_IsTextInputActive())
			SDL_StartTextInput();
		zu4_quiet_text_field(g_root_view.window ?: g_root_view);
		zu4_quiet_keyboard();
		g_kb_shown = true;
	} else {
		if(SDL_IsTextInputActive())
			SDL_StopTextInput();
		g_kb_shown = false;
	}
}

void zu4_ios_toggle_keyboard(void)
{
	// Track our own state, not SDL_IsTextInputActive() (a cold StartTextInput
	// sets the SDL flag without presenting the keyboard, which desyncs the toggle).
	zu4_ios_show_keyboard(g_kb_shown ? 0 : 1);
}

static UIButton *zu4_make_button(NSString *title, long tag, CGRect frame,
                                 Zu4ButtonTarget *target)
{
	UIButton *b = tag == ZU4_TAG_MAP
	    ? [Zu4MinimapButton buttonWithType:UIButtonTypeCustom]
	    : [UIButton buttonWithType:UIButtonTypeCustom];
	b.frame = frame;
	[b setTitle:title forState:UIControlStateNormal];
	b.titleLabel.font = [UIFont boldSystemFontOfSize:(title.length > 2 ? 15 : 22)];
	b.titleLabel.adjustsFontSizeToFitWidth = YES;
	[b setTitleColor:[UIColor whiteColor] forState:UIControlStateNormal];
	b.backgroundColor = [UIColor colorWithWhite:0.20 alpha:0.55];
	b.layer.cornerRadius = 8.0;
	b.layer.borderWidth = 1.0;
	b.layer.borderColor = [UIColor colorWithWhite:1.0 alpha:0.35].CGColor;
	b.tag = tag;
	b.showsTouchWhenHighlighted = YES;
	[b addTarget:target action:@selector(onTap:)
	    forControlEvents:UIControlEventTouchDown];
    if (tag == SDLK_UP || tag == SDLK_DOWN || tag == SDLK_LEFT || tag == SDLK_RIGHT)
        [b addTarget:target action:@selector(stopMoving) forControlEvents:
            UIControlEventTouchUpInside | UIControlEventTouchUpOutside | UIControlEventTouchCancel | UIControlEventTouchDragExit];
	return b;
}

static UIButton *zu4_make_party_status_button(int index, Zu4ButtonTarget *target)
{
    UIButton *button = [UIButton buttonWithType:UIButtonTypeCustom];
    button.tag = ZU4_TAG_PARTY_MEMBER_BASE + index;
    button.backgroundColor = [UIColor colorWithWhite:0.10 alpha:0.86];
    button.layer.cornerRadius = 6.0;
    button.layer.borderWidth = 1.0;
    button.layer.borderColor = [UIColor colorWithWhite:1.0 alpha:0.28].CGColor;
    button.contentHorizontalAlignment = UIControlContentHorizontalAlignmentLeft;
    button.contentVerticalAlignment = UIControlContentVerticalAlignmentCenter;
    button.contentEdgeInsets = UIEdgeInsetsMake(2, 5, 5, 4);
    button.titleLabel.numberOfLines = 2;
    button.titleLabel.font = [UIFont monospacedSystemFontOfSize:11 weight:UIFontWeightSemibold];
    button.titleLabel.adjustsFontSizeToFitWidth = YES;
    button.titleLabel.minimumScaleFactor = 0.72;
    [button setTitleColor:UIColor.whiteColor forState:UIControlStateNormal];
    [button addTarget:target action:@selector(onTap:) forControlEvents:UIControlEventTouchDown];

    UIView *track = [[UIView alloc] init];
    track.tag = ZU4_TAG_HP_TRACK;
    track.userInteractionEnabled = NO;
    track.backgroundColor = [UIColor colorWithWhite:1.0 alpha:0.16];
    track.layer.cornerRadius = 1.5;
    UIView *fill = [[UIView alloc] init];
    fill.tag = ZU4_TAG_HP_FILL;
    fill.userInteractionEnabled = NO;
    fill.backgroundColor = [UIColor colorWithRed:0.36 green:0.82 blue:0.55 alpha:1.0];
    fill.layer.cornerRadius = 1.5;
    [track addSubview:fill];
    [button addSubview:track];
    return button;
}

static NSString *zu4_party_condition_name(char condition)
{
    switch (condition) {
    case 'G': return @"Healthy";
    case 'P': return @"Poisoned";
    case 'S': return @"Asleep";
    case 'D': return @"Dead";
    default: return @"Unknown";
    }
}

static NSString *zu4_party_condition_short(char condition)
{
    switch (condition) {
    case 'G': return @"OK";
    case 'P': return @"PSN";
    case 'S': return @"SLP";
    case 'D': return @"DEAD";
    default: return @"?";
    }
}

static UIColor *zu4_party_condition_color(char condition)
{
    switch (condition) {
    case 'P': return [UIColor colorWithRed:0.44 green:0.88 blue:0.48 alpha:1.0];
    case 'S': return [UIColor colorWithRed:0.46 green:0.76 blue:1.0 alpha:1.0];
    case 'D': return [UIColor colorWithRed:1.0 green:0.44 blue:0.42 alpha:1.0];
    default: return UIColor.whiteColor;
    }
}

static void zu4_update_party_roster(BOOL portrait, CGRect bounds, UIEdgeInsets safe,
                                    CGRect map, BOOL visible)
{
    if (!g_party_roster) return;
    Zu4MobilePartyMemberStatus members[ZU4_MOBILE_MAX_PARTY_MEMBERS] = {};
    int count = visible ? zu4_mobile_party_status(members, ZU4_MOBILE_MAX_PARTY_MEMBERS) : 0;
    g_party_roster.hidden = count <= 0;
    if (count <= 0) return;

    CGFloat bottom = CGRectGetMaxY(bounds) - safe.bottom;
    if (portrait) {
        CGFloat y = CGRectGetMaxY(map) + 4.0;
        CGFloat controlsTop = bottom - 217.0;
        g_party_roster.frame = CGRectMake(safe.left + 8.0, y,
            bounds.size.width - safe.left - safe.right - 16.0,
            MAX(0.0, controlsTop - y - 4.0));
    } else {
        CGFloat y = safe.top + 4.0;
        CGFloat dpadTop = CGRectGetMinY(g_dpad.frame);
        CGFloat x = settings.flipControls ? CGRectGetMaxX(bounds) - safe.right - g_dpad.frame.size.width - 8 : safe.left + 8;
        CGFloat width = settings.flipControls ? g_dpad.frame.size.width : CGRectGetMinX(map) - safe.left - 16;
        g_party_roster.frame = CGRectMake(x, y, MAX(120.0, width),
            MAX(0.0, dpadTop - y - 4.0));
    }

    int columns = portrait ? 4 : (g_party_roster.bounds.size.height < 188 ? 3 : 2);
    int rows = portrait ? 2 : (ZU4_MOBILE_MAX_PARTY_MEMBERS + columns - 1) / columns;
    CGFloat gap = 4.0;
    CGFloat cellWidth = (g_party_roster.bounds.size.width - gap * (columns - 1)) / columns;
    CGFloat cellHeight = (g_party_roster.bounds.size.height - gap * (rows - 1)) / rows;
    for (int i = 0; i < ZU4_MOBILE_MAX_PARTY_MEMBERS; ++i) {
        UIButton *button = (UIButton *)[g_party_roster viewWithTag:ZU4_TAG_PARTY_MEMBER_BASE + i];
        button.hidden = i >= count;
        if (i >= count) continue;
        int row = i / columns, column = i % columns;
        button.frame = CGRectMake(column * (cellWidth + gap), row * (cellHeight + gap),
                                  cellWidth, cellHeight);
        Zu4MobilePartyMemberStatus member = members[i];
        NSString *name = [NSString stringWithUTF8String:member.name] ?: @"Companion";
        NSString *prefix = member.active ? @"▶" : [NSString stringWithFormat:@"%d", i + 1];
        NSString *condition = zu4_party_condition_name(member.condition);
        NSString *shortCondition = zu4_party_condition_short(member.condition);
        [button setTitle:[NSString stringWithFormat:@"%@ %@\n%d/%d %@",
                          prefix, name, member.hp, member.maxHp, shortCondition]
                forState:UIControlStateNormal];
        [button setTitleColor:zu4_party_condition_color(member.condition) forState:UIControlStateNormal];
        button.layer.borderWidth = member.active ? 2.5 : 1.0;
        button.layer.borderColor = (member.active
            ? [UIColor colorWithRed:1.0 green:0.82 blue:0.30 alpha:1.0]
            : [UIColor colorWithWhite:1.0 alpha:0.28]).CGColor;
        button.accessibilityLabel = [NSString stringWithFormat:@"Party member %d, %@", i + 1, name];
        button.accessibilityValue = [NSString stringWithFormat:@"Health %d of %d, %@%@",
            member.hp, member.maxHp, condition, member.active ? @", active combatant" : @""];
        button.accessibilityHint = @"Opens this party member's details.";
        button.accessibilityTraits = UIAccessibilityTraitButton |
            (member.active ? UIAccessibilityTraitSelected : 0);

        UIView *track = [button viewWithTag:ZU4_TAG_HP_TRACK];
        UIView *fill = [track viewWithTag:ZU4_TAG_HP_FILL];
        CGFloat trackWidth = MAX(0.0, button.bounds.size.width - 10.0);
        track.frame = CGRectMake(5.0, MAX(0.0, button.bounds.size.height - 5.0), trackWidth, 3.0);
        CGFloat fraction = member.maxHp > 0 ? (CGFloat)member.hp / member.maxHp : 0.0;
        fraction = MIN(1.0, MAX(0.0, fraction));
        fill.frame = CGRectMake(0, 0, trackWidth * fraction, 3.0);
    }
}

void zu4_ios_setup_ui(SDL_Window *window)
{
	if(g_ui_installed || window == NULL)
		return;

	SDL_SysWMinfo info;
	SDL_VERSION(&info.version);
	if(!SDL_GetWindowWMInfo(window, &info))
		return;

	UIWindow *uiwin = info.info.uikit.window;
	UIViewController *vc = uiwin.rootViewController;
	UIView *root = vc.view;
	if(root == nil)
		return;

	g_ui_installed = true;
	g_window = window;
	g_root_view = root;
	g_full_frame = root.frame;
	g_btn_target = [[Zu4ButtonTarget alloc] init];
	Zu4ButtonTarget *t = g_btn_target;

	[[NSNotificationCenter defaultCenter] addObserver:t
	    selector:@selector(keyboardWillShow:)
	    name:UIKeyboardWillShowNotification object:nil];
	[[NSNotificationCenter defaultCenter] addObserver:t
	    selector:@selector(keyboardWillHide:)
	    name:UIKeyboardWillHideNotification object:nil];

	// Transparent, non-scaled button overlay on the window (not the SDL view).
    g_overlay = [[Zu4PassthroughView alloc] initWithFrame:uiwin.bounds];
	g_overlay.backgroundColor = [UIColor clearColor];
	g_overlay.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    [uiwin addSubview:g_overlay];

    g_world_touch = [[Zu4PassthroughView alloc] init];
    g_world_touch.backgroundColor = UIColor.clearColor;
    g_world_touch.isAccessibilityElement = NO;
    [g_overlay addSubview:g_world_touch];
    // Tile-sized controls avoid UIKit gesture recognizers, which compete with
    // SDL's nested run loop. The D-pad remains the visible accessible movement
    // path; these transparent cells translate a direct world tap into intent.
    for (int y = 0; y < 11; ++y) for (int x = 0; x < 11; ++x) {
        if (x == 5 && y == 5) continue;
        UIButton *target = [UIButton buttonWithType:UIButtonTypeCustom];
        target.tag = (y << 4) | x | 0x100;
        target.backgroundColor = UIColor.clearColor;
        target.isAccessibilityElement = NO;
        [target addTarget:t action:@selector(onWorldTap:) forControlEvents:UIControlEventTouchUpInside];
        [g_world_touch addSubview:target];
    }
    int directDirections[] = { DIR_NORTH, DIR_SOUTH, DIR_WEST, DIR_EAST };
    for (int direction : directDirections) {
        UIButton *target = [UIButton buttonWithType:UIButtonTypeCustom];
        target.tag = direction;
        target.backgroundColor = UIColor.clearColor;
        target.isAccessibilityElement = NO;
        [target addTarget:t action:@selector(onWorldTap:) forControlEvents:UIControlEventTouchUpInside];
        [g_world_touch addSubview:target];
    }

    g_combat_touch = [[Zu4PassthroughView alloc] init];
    g_combat_touch.backgroundColor = UIColor.clearColor;
    g_combat_touch.isAccessibilityElement = NO;
    g_combat_range_layer = [CAShapeLayer layer];
    g_combat_range_layer.fillColor = [UIColor colorWithRed:1.0 green:0.78 blue:0.18 alpha:0.18].CGColor;
    g_combat_range_layer.strokeColor = [UIColor colorWithRed:1.0 green:0.86 blue:0.35 alpha:0.9].CGColor;
    g_combat_range_layer.lineWidth = 1.5;
    g_combat_range_layer.lineDashPattern = @[@4, @3];
    [g_combat_touch.layer addSublayer:g_combat_range_layer];
    [g_overlay addSubview:g_combat_touch];
    for (int i = 0; i < ZU4_MOBILE_MAX_COMBAT_TARGETS; ++i) {
        Zu4CombatTargetButton *target = [Zu4CombatTargetButton buttonWithType:UIButtonTypeCustom];
        target.hidden = YES;
        target.backgroundColor = UIColor.clearColor;
        target.titleLabel.font = [UIFont boldSystemFontOfSize:30];
        target.titleLabel.adjustsFontSizeToFitWidth = YES;
        [target addTarget:t action:@selector(onCombatTarget:) forControlEvents:UIControlEventTouchUpInside];
        [g_combat_touch addSubview:target];
    }

    g_party_roster = [[Zu4PassthroughView alloc] init];
    g_party_roster.hidden = YES;
    [g_overlay addSubview:g_party_roster];
    for (int i = 0; i < ZU4_MOBILE_MAX_PARTY_MEMBERS; ++i)
        [g_party_roster addSubview:zu4_make_party_status_button(i, t)];

	CGRect b = root.bounds;
	UIEdgeInsets safe = root.safeAreaInsets;
	CGFloat right = b.origin.x + b.size.width - safe.right;
	CGFloat bottom = b.origin.y + b.size.height - safe.bottom;

	const CGFloat DS = 52.0;  // d-pad button size (bigger for easier movement)
	const CGFloat S = 48.0;   // action button height
	const CGFloat AW = 76.0;  // readable action-column width
	const CGFloat G = 5.0;    // gap

	// ---- Left side: movement D-pad, anchored bottom-left ----
	// The D-pad lives in its own container so it can be shifted left on its own
	// when the keyboard is up, without disturbing its no-keyboard position.
	// Hug the physical left edge (ignore the safe-area inset, ~an inch in
	// landscape) with just a few points of margin.
	CGFloat dpSpan = DS * 3 + G * 2;
	CGFloat dpx = b.origin.x + 4.0;
	CGFloat dpy = bottom - dpSpan - 10.0;
	g_dpad = [[Zu4PassthroughView alloc] initWithFrame:CGRectMake(dpx, dpy, dpSpan, dpSpan)];
	g_dpad.backgroundColor = [UIColor clearColor];
	[g_overlay addSubview:g_dpad];
	// Buttons positioned relative to the container's origin.
	[g_dpad addSubview:zu4_make_button(@"▲", SDLK_UP,
	         CGRectMake(DS + G, 0, DS, DS), t)];
	[g_dpad addSubview:zu4_make_button(@"◀", SDLK_LEFT,
	         CGRectMake(0, DS + G, DS, DS), t)];
	[g_dpad addSubview:zu4_make_button(@"▶", SDLK_RIGHT,
	         CGRectMake((DS + G) * 2, DS + G, DS, DS), t)];
	[g_dpad addSubview:zu4_make_button(@"▼", SDLK_DOWN,
	         CGRectMake(DS + G, (DS + G) * 2, DS, DS), t)];
    UIButton *repeat = zu4_make_button(@"Repeat\nAttack", ZU4_TAG_REPEAT_ATTACK,
        CGRectMake(DS + G, DS + G, DS, DS), t);
    repeat.titleLabel.numberOfLines = 2;
    repeat.titleLabel.font = [UIFont boldSystemFontOfSize:11];
    repeat.accessibilityLabel = @"Repeat last attack";
    repeat.accessibilityHint = @"Previews this fighter's last valid target. Attack confirms; Clear cancels.";
    repeat.hidden = YES;
    [g_dpad addSubview:repeat];

	// ---- Right side: action buttons, stacked bottom-right ----
	// U4's commands are all typed letters, so the keyboard button is primary.
	NSArray *labels = @[ @"⌨", @"Esc", @"↵", @"Spc" ];
	long tags[] = { ZU4_TAG_KEYBOARD, SDLK_ESCAPE, SDLK_RETURN, SDLK_SPACE };
	int n = 4;
	CGFloat bx = right - AW - 8.0;
	CGFloat by = bottom - (S * n + G * (n - 1)) - 10.0;
	for(int i = 0; i < n; i++) {
		[g_overlay addSubview:zu4_make_button(labels[i], tags[i],
		         CGRectMake(bx, by + (S + G) * i, AW, S), t)];
	}

    UIButton *talk = zu4_make_button(@"Talk", ZU4_TAG_TALK,
        CGRectMake(right - AW - 90.0, bottom - 58.0, 76.0, 48.0), t);
    talk.accessibilityLabel = @"Talk";
    talk.accessibilityHint = @"Choose a direction toward the person you want to speak with.";
    [g_overlay addSubview:talk];
    UIButton *enter = zu4_make_button(@"Interact", ZU4_TAG_ENTER,
        CGRectMake(right - AW - 90.0, bottom - 111.0, 76.0, 48.0), t);
    enter.accessibilityLabel = @"Interact";
    enter.accessibilityHint = @"Performs the action shown when something at your position can be used.";
    [g_overlay addSubview:enter];
    UIButton *journal = zu4_make_button(@"Journal", ZU4_TAG_JOURNAL,
        CGRectMake(right - AW - 90, bottom - 164, 76, 48), t);
    journal.accessibilityLabel = @"Journal";
    [g_overlay addSubview:journal];
    UIButton *party = zu4_make_button(@"Party", ZU4_TAG_PARTY,
        CGRectMake(right - AW - 90, bottom - 217, 76, 48), t);
    party.accessibilityLabel = @"Party and equipment";
    [g_overlay addSubview:party];
    UIButton *mapButton = zu4_make_button(@"Map", ZU4_TAG_MAP, CGRectZero, t);
    [mapButton setTitle:nil forState:UIControlStateNormal];
    mapButton.backgroundColor = [UIColor colorWithWhite:0.02 alpha:0.92];
    mapButton.imageView.contentMode = UIViewContentModeScaleAspectFit;
    mapButton.imageView.layer.magnificationFilter = kCAFilterNearest;
    mapButton.accessibilityLabel = @"Local exploration minimap";
    mapButton.accessibilityHint = @"Opens the full Britannia map.";
    [g_overlay addSubview:mapButton];
    [g_overlay setNeedsLayout];

	// Pre-warm SDL's text-input responder so the FIRST keyboard tap presents it
	// on a single press.
	dispatch_async(dispatch_get_main_queue(), ^{
		SDL_StartTextInput();
		SDL_StopTextInput();
		g_kb_shown = false;
	});
}

// Recompute control geometry from current safe areas after rotation. Do not
// recreate controls: pending direction input and keyboard state must survive.
static void zu4_layout_controls(void) {
    if (!g_overlay || !g_dpad) return;
    CGRect bounds = g_overlay.bounds;
    UIEdgeInsets safe = g_overlay.safeAreaInsets;
    CGFloat right = CGRectGetMaxX(bounds) - safe.right;
    BOOL portrait = bounds.size.height > bounds.size.width;
    if (g_world_touch) {
        g_world_touch.frame = zu4MapFrame(bounds, safe);
        zu4_layout_world_touch_targets();
    }
    if (g_combat_touch) g_combat_touch.frame = zu4MapFrame(bounds, safe);
    g_dpad.frame = zu4DpadFrame(bounds, safe, settings.dpadSize, settings.flipControls);
    CGFloat directionSize = (g_dpad.bounds.size.width - 10) / 3, step = directionSize + 5;
    for (UIView *button in g_dpad.subviews) {
        int column = button.tag == SDLK_LEFT ? 0 : (button.tag == SDLK_RIGHT ? 2 : 1);
        int row = button.tag == SDLK_UP ? 0 : (button.tag == SDLK_DOWN ? 2 : 1);
        button.frame = CGRectMake(column * step, row * step, directionSize, directionSize);
    }
    for (UIView *view in g_overlay.subviews) {
        if (![view isKindOfClass:UIButton.class]) continue;
        UIButton *button = (UIButton *)view;
        if (button.tag == ZU4_TAG_MAP) {
            button.frame = portrait ? CGRectMake(right - 76, safe.top + 8, 68, 52)
                                    : CGRectMake(settings.flipControls ? safe.left + 8 : right - 76, safe.top + 8, 76, 64);
            continue;
        }
        if (button.tag == ZU4_TAG_TALK || button.tag == ZU4_TAG_ENTER || button.tag == ZU4_TAG_JOURNAL || button.tag == ZU4_TAG_PARTY) {
            int row = button.tag == ZU4_TAG_TALK ? 3 : (button.tag == ZU4_TAG_ENTER ? 2 : (button.tag == ZU4_TAG_JOURNAL ? 1 : 0));
            button.frame = zu4ActionFrame(bounds, safe, settings.flipControls, 0, row);
        } else {
            long tags[] = { ZU4_TAG_KEYBOARD, SDLK_ESCAPE, SDLK_RETURN, SDLK_SPACE };
            for (int i = 0; i < 4; ++i)
                if (button.tag == tags[i] || (i == 0 && button.tag == ZU4_TAG_CAST) || (i == 1 && button.tag == ZU4_TAG_MENU))
                    button.frame = zu4ActionFrame(bounds, safe, settings.flipControls, 1, i);
        }
    }
    // SDL owns its view frame and rotation. UIKit overlays must never mutate it.
}

void zu4_ios_refresh_controls(void) {
    [g_btn_target stopMoving];
    [g_overlay setNeedsLayout];
    [g_overlay layoutIfNeeded];
}

static void zu4_layout_world_touch_targets(void) {
    if (!g_world_touch) return;
    CGFloat tile = g_world_touch.bounds.size.width / 11.0;
    CGFloat hit = MAX(44.0, tile);
    CGPoint center = CGPointMake(tile * 5.5, tile * 5.5);
    for (UIView *view in g_world_touch.subviews) {
        if (view.tag & 0x100) {
            int x = ((int)view.tag & 15);
            int y = (((int)view.tag >> 4) & 15);
            view.frame = CGRectMake(x * tile, y * tile, tile, tile);
            continue;
        }
        CGPoint target = center;
        if (view.tag == DIR_NORTH) target.y -= tile;
        else if (view.tag == DIR_SOUTH) target.y += tile;
        else if (view.tag == DIR_WEST) target.x -= tile;
        else if (view.tag == DIR_EAST) target.x += tile;
        view.frame = CGRectMake(target.x - hit / 2, target.y - hit / 2, hit, hit);
    }
}

static NSString *zu4_combat_direction_name(int direction) {
    if (direction == DIR_NORTH) return @"north";
    if (direction == DIR_SOUTH) return @"south";
    if (direction == DIR_WEST) return @"west";
    if (direction == DIR_EAST) return @"east";
    return @"unknown direction";
}

static void zu4_update_combat_targets(BOOL visible) {
    if (!g_combat_touch) return;
    g_combat_touch.hidden = !visible;
    g_combat_touch.userInteractionEnabled = visible;
    if (!visible) {
        g_combat_range_layer.path = nil;
        for (UIView *view in g_combat_touch.subviews) view.hidden = YES;
        return;
    }
    CGFloat tile = g_combat_touch.bounds.size.width / 11.0;
    int count = MIN(zu4_mobile_combat_target_count(), ZU4_MOBILE_MAX_COMBAT_TARGETS);
    UIBezierPath *rangePath = [UIBezierPath bezierPath];
    Zu4CombatTargetButton *selectedButton = nil;
    int buttonIndex = 0;
    for (UIView *view in g_combat_touch.subviews) {
        if (![view isKindOfClass:Zu4CombatTargetButton.class]) continue;
        Zu4CombatTargetButton *button = (Zu4CombatTargetButton *)view;
        Zu4MobileCombatTarget target = {};
        BOOL valid = buttonIndex < count && zu4_mobile_combat_target_at(buttonIndex, &target);
        ++buttonIndex;
        button.hidden = !valid;
        if (!valid) continue;
        CGFloat hit = MAX(44.0, tile);
        button.frame = CGRectMake((target.screenX + 0.5) * tile - hit / 2.0,
                                  (target.screenY + 0.5) * tile - hit / 2.0,
                                  hit, hit);
        button.targetToken = target.token;
        UIColor *color = target.selected
            ? [UIColor colorWithRed:1.0 green:0.84 blue:0.25 alpha:1.0]
            : [UIColor colorWithWhite:1.0 alpha:0.92];
        [button setTitle:target.selected ? @"◎" : @"○" forState:UIControlStateNormal];
        [button setTitleColor:color forState:UIControlStateNormal];
        button.accessibilityLabel = [NSString stringWithFormat:@"Target %s", target.name];
        button.accessibilityValue = [NSString stringWithFormat:@"%d %@ %@%@",
            target.distance, target.distance == 1 ? @"tile" : @"tiles",
            zu4_combat_direction_name(target.direction), target.selected ? @", selected" : @""];
        button.accessibilityHint = target.selected
            ? @"Double tap to clear this target. Attack confirms the attack."
            : @"Selects this combatant without using a turn.";
        button.accessibilityTraits = UIAccessibilityTraitButton |
            (target.selected ? UIAccessibilityTraitSelected : 0);
        if (target.selected) {
            selectedButton = button;
            int dx = target.screenX == target.attackerScreenX ? 0 :
                     (target.screenX > target.attackerScreenX ? 1 : -1);
            int dy = target.screenY == target.attackerScreenY ? 0 :
                     (target.screenY > target.attackerScreenY ? 1 : -1);
            int x = target.attackerScreenX + dx;
            int y = target.attackerScreenY + dy;
            while (x != target.screenX || y != target.screenY) {
                [rangePath appendPath:[UIBezierPath bezierPathWithRoundedRect:
                    CGRectInset(CGRectMake(x * tile, y * tile, tile, tile), 2, 2) cornerRadius:3]];
                x += dx;
                y += dy;
            }
            [rangePath appendPath:[UIBezierPath bezierPathWithRoundedRect:
                CGRectInset(CGRectMake(target.screenX * tile, target.screenY * tile, tile, tile), 2, 2)
                cornerRadius:3]];
        }
    }
    g_combat_range_layer.frame = g_combat_touch.bounds;
    g_combat_range_layer.path = rangePath.CGPath;
    if (selectedButton) [g_combat_touch bringSubviewToFront:selectedButton];
}

static void zu4_update_dungeon_dpad(BOOL dungeon, BOOL overhead) {
    if (!g_dpad) return;
    for (UIView *view in g_dpad.subviews) {
        if (![view isKindOfClass:UIButton.class]) continue;
        UIButton *button = (UIButton *)view;
        if (button.tag == ZU4_TAG_REPEAT_ATTACK) continue;
        NSString *title = nil;
        NSString *label = nil;
        NSString *hint = nil;
        if (dungeon && !overhead) {
            button.titleLabel.numberOfLines = 2;
            button.titleLabel.textAlignment = NSTextAlignmentCenter;
            button.titleLabel.font = [UIFont boldSystemFontOfSize:12];
            if (button.tag == SDLK_UP) {
                title = @"Forward"; label = @"Move forward";
                hint = @"Advances one dungeon cell in the direction you are facing.";
            } else if (button.tag == SDLK_DOWN) {
                title = @"Back"; label = @"Move backward";
                hint = @"Retreats one dungeon cell without changing your facing.";
            } else if (button.tag == SDLK_LEFT) {
                title = @"Turn\nLeft"; label = @"Turn left";
                hint = @"Rotates left in place.";
            } else if (button.tag == SDLK_RIGHT) {
                title = @"Turn\nRight"; label = @"Turn right";
                hint = @"Rotates right in place.";
            }
        } else {
            button.titleLabel.numberOfLines = 1;
            button.titleLabel.font = [UIFont boldSystemFontOfSize:22];
            if (button.tag == SDLK_UP) title = @"▲", label = @"Move north";
            else if (button.tag == SDLK_DOWN) title = @"▼", label = @"Move south";
            else if (button.tag == SDLK_LEFT) title = @"◀", label = @"Move west";
            else if (button.tag == SDLK_RIGHT) title = @"▶", label = @"Move east";
            if (dungeon) hint = @"Moves one cell in this cardinal direction and updates facing.";
        }
        if (title) [button setTitle:title forState:UIControlStateNormal];
        button.accessibilityLabel = label;
        button.accessibilityHint = hint;
    }
}

static BOOL zu4_update_minimap_button(UIButton *button) {
    const int side = 21;
    unsigned char pixels[side * side * 4] = {};
    int mapKind = zu4_mobile_minimap(pixels, side);
    if (!mapKind) return NO;
    button.accessibilityLabel = mapKind == 2
        ? @"Current dungeon level minimap" : @"Local exploration minimap";
    button.accessibilityHint = mapKind == 2
        ? @"Opens the explored map for this dungeon level."
        : @"Opens the full Britannia map.";
    uint64_t hash = UINT64_C(1469598103934665603);
    for (unsigned char byte : pixels) {
        hash ^= byte;
        hash *= UINT64_C(1099511628211);
    }
    if (hash == g_minimap_hash && [button imageForState:UIControlStateNormal]) return YES;
    NSData *data = [NSData dataWithBytes:pixels length:sizeof(pixels)];
    CGDataProviderRef provider = CGDataProviderCreateWithCFData((__bridge CFDataRef)data);
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    CGImageRef imageRef = CGImageCreate(side, side, 8, 32, side * 4, colorSpace,
        kCGImageAlphaPremultipliedLast | kCGBitmapByteOrderDefault, provider, NULL, false,
        kCGRenderingIntentDefault);
    UIImage *image = imageRef ? [UIImage imageWithCGImage:imageRef scale:1
                                              orientation:UIImageOrientationUp] : nil;
    if (imageRef) CGImageRelease(imageRef);
    CGColorSpaceRelease(colorSpace);
    CGDataProviderRelease(provider);
    if (!image) return NO;
    [button setImage:[image imageWithRenderingMode:UIImageRenderingModeAlwaysOriginal]
            forState:UIControlStateNormal];
    g_minimap_hash = hash;
    return YES;
}

int zu4_ios_title_rect(SDL_Rect *rect, int pixelWidth, int pixelHeight) {
    if (!g_overlay || g_kb_shown || CGRectIsEmpty(g_overlay.bounds)) return 0;
    CGRect title = zu4TitleFrame(g_overlay.bounds, g_overlay.safeAreaInsets);
    CGFloat sx = pixelWidth / g_overlay.bounds.size.width;
    CGFloat sy = pixelHeight / g_overlay.bounds.size.height;
    *rect = (SDL_Rect){(int)(title.origin.x * sx), (int)(title.origin.y * sy),
                      (int)(title.size.width * sx), (int)(title.size.height * sy)};
    return 1;
}

int zu4_ios_world_rect(SDL_Rect *rect, int pixelWidth, int pixelHeight, const char *status) {
    BOOL combat = zu4_mobile_combat_active();
    BOOL dungeon = zu4_mobile_dungeon_active();
    BOOL overhead = dungeon && zu4_mobile_dungeon_top_down();
    BOOL panelVisible = zu4_topic_panel_is_visible() || zu4_map_panel_visible();
    if (panelVisible || !zu4_mobile_can_repeat_movement()) [g_btn_target stopMoving];
    static char lastWorldStatus[256] = {};
    if (status) snprintf(lastWorldStatus, sizeof(lastWorldStatus), "%s", status);
    else if (zu4_mobile_map_visible() && lastWorldStatus[0]) status = lastWorldStatus;
    else lastWorldStatus[0] = 0;
    BOOL gameplay = status != NULL || combat;
    zu4_update_dungeon_dpad(dungeon, overhead);
    if (g_world_touch) {
        g_world_touch.frame = zu4MapFrame(g_overlay.bounds, g_overlay.safeAreaInsets);
        zu4_layout_world_touch_targets();
        g_world_touch.hidden = !gameplay || combat || dungeon || panelVisible || !zu4_mobile_world_taps_enabled();
        g_world_touch.userInteractionEnabled = !g_world_touch.hidden;
    }
    if (g_combat_touch) {
        g_combat_touch.frame = zu4MapFrame(g_overlay.bounds, g_overlay.safeAreaInsets);
        zu4_update_combat_targets(gameplay && combat && !panelVisible);
    }
    g_dpad.hidden = (!gameplay || panelVisible) && !zu4_topic_panel_direction_active();
    UIButton *repeat = (UIButton *)[g_dpad viewWithTag:ZU4_TAG_REPEAT_ATTACK];
    char repeatName[64] = {};
    BOOL canRepeat = combat && zu4_mobile_combat_repeat_target(repeatName, sizeof(repeatName));
    repeat.hidden = !combat || panelVisible;
    repeat.enabled = canRepeat;
    repeat.alpha = canRepeat ? 1.0 : 0.35;
    repeat.accessibilityValue = canRepeat ? [NSString stringWithUTF8String:repeatName] : @"No valid previous attack";
    for (UIView *view in g_overlay.subviews) {
        if (![view isKindOfClass:UIButton.class]) continue;
        UIButton *button = (UIButton *)view;
        // These controls are created with compact keyboard symbols and later
        // receive word labels such as Spells and Continue. Reset the font so
        // the former symbols do not leave those two labels oversized.
        button.titleLabel.font = [UIFont boldSystemFontOfSize:15];
        button.titleLabel.numberOfLines = 1;
        button.titleLabel.textAlignment = NSTextAlignmentCenter;
        button.enabled = YES;
        button.alpha = 1.0;
        button.hidden = panelVisible || !gameplay;
        if (button.tag == ZU4_TAG_MAP)
            button.hidden = panelVisible || !gameplay || combat ||
                !zu4_mobile_exploration_map_enabled() || !zu4_update_minimap_button(button);
        if (combat && gameplay && button.tag == SDLK_RETURN) {
            BOOL selected = zu4_mobile_combat_target_prepared();
            [button setTitle:@"Clear" forState:UIControlStateNormal];
            button.accessibilityLabel = @"Clear combat target";
            button.accessibilityHint = @"Returns Attack to directional targeting.";
            button.accessibilityValue = nil;
            button.enabled = selected;
            button.alpha = selected ? 1.0 : 0.35;
        } else if (dungeon && gameplay && button.tag == SDLK_RETURN) {
            [button setTitle:overhead ? @"3D View" : @"Overhead" forState:UIControlStateNormal];
            button.accessibilityLabel = overhead ? @"Switch to first-person view" : @"Switch to overhead view";
            button.accessibilityValue = overhead ? @"Overhead view active" : @"First-person view active";
            button.accessibilityHint = @"Changes only the dungeon presentation and does not use a turn.";
        } else if (!gameplay && !panelVisible && button.tag == SDLK_RETURN) {
            button.hidden = NO;
            [button setTitle:@"Skip" forState:UIControlStateNormal];
            button.accessibilityLabel = @"Skip introduction";
            button.accessibilityValue = nil;
        } else if (gameplay && button.tag == SDLK_RETURN) {
            [button setTitle:@"Continue" forState:UIControlStateNormal];
            button.accessibilityLabel = @"Continue or confirm";
            button.accessibilityHint = @"Advances text or confirms the current prompt.";
            button.accessibilityValue = nil;
        }
        if (button.tag == ZU4_TAG_TALK) {
            NSString *title = combat ? @"Attack" : (dungeon ? @"Search" : @"Talk");
            NSString *targetName = nil;
            if (combat) {
                for (int i = 0; i < zu4_mobile_combat_target_count(); ++i) {
                    Zu4MobileCombatTarget target = {};
                    if (zu4_mobile_combat_target_at(i, &target) && target.selected) {
                        targetName = [NSString stringWithUTF8String:target.name];
                        break;
                    }
                }
                if (targetName.length) {
                    title = [NSString stringWithFormat:@"Attack\n%@", targetName];
                    button.titleLabel.numberOfLines = 2;
                    button.titleLabel.font = [UIFont boldSystemFontOfSize:12];
                }
            }
            [button setTitle:title forState:UIControlStateNormal];
            button.accessibilityLabel = combat ? @"Attack" : title;
            button.accessibilityValue = targetName;
            button.accessibilityHint = combat
                ? (zu4_mobile_combat_target_selected()
                    ? @"Attacks the selected target and uses this character’s turn."
                    : @"Choose an attack direction, or select a combatant first.") :
                (dungeon ? @"Searches the current dungeon cell using the original turn rules." :
                 @"Choose a direction toward the person you want to speak with.");
        }
        if (button.tag == ZU4_TAG_ENTER) {
            if (combat) {
                BOOL available = zu4_mobile_combat_target_count() > 0;
                [button setTitle:@"Prev\nTarget" forState:UIControlStateNormal];
                button.titleLabel.numberOfLines = 2;
                button.titleLabel.textAlignment = NSTextAlignmentCenter;
                button.titleLabel.font = [UIFont boldSystemFontOfSize:12];
                button.accessibilityLabel = @"Previous target";
                button.accessibilityHint = @"Selects the previous attackable combatant without using a turn.";
                button.enabled = available;
                button.alpha = available ? 1.0 : 0.35;
                continue;
            }
            char contextLabel[32] = {};
            BOOL available = !combat && zu4_mobile_context_action(contextLabel, sizeof(contextLabel));
            NSString *title = contextLabel[0] ? [NSString stringWithUTF8String:contextLabel] : @"Interact";
            [button setTitle:title forState:UIControlStateNormal];
            button.accessibilityLabel = title;
            button.enabled = available;
            button.alpha = available ? 1.0 : 0.35;
        }
        if (button.tag == ZU4_TAG_JOURNAL) {
            if (combat) {
                BOOL available = zu4_mobile_combat_target_count() > 0;
                [button setTitle:@"Next\nTarget" forState:UIControlStateNormal];
                button.titleLabel.numberOfLines = 2;
                button.titleLabel.textAlignment = NSTextAlignmentCenter;
                button.titleLabel.font = [UIFont boldSystemFontOfSize:12];
                button.accessibilityLabel = @"Next target";
                button.accessibilityHint = @"Selects the next attackable combatant without using a turn.";
                button.enabled = available;
                button.alpha = available ? 1.0 : 0.35;
                continue;
            }
            button.enabled = YES;
            button.alpha = 1.0;
            NSString *title = dungeon ? @"Torch" : @"Journal";
            [button setTitle:title forState:UIControlStateNormal];
            button.accessibilityLabel = title;
            button.accessibilityHint = dungeon
                ? @"Lights one carried torch using the original dungeon rules."
                : @"Opens the journal.";
        }
        if (button.tag == SDLK_SPACE) {
            BOOL exploring = (status || combat) && !g_kb_shown;
            [button setTitle:exploring ? @"Wait" : @"Spc" forState:UIControlStateNormal];
            button.accessibilityLabel = exploring ? (combat ? @"Pass this character’s turn" : @"Wait one turn") : @"Space";
        }
        if (button.tag == SDLK_ESCAPE || button.tag == ZU4_TAG_MENU) {
            BOOL exploring = (status || combat) && !g_kb_shown;
            button.tag = exploring ? ZU4_TAG_MENU : SDLK_ESCAPE;
            [button setTitle:exploring ? @"Menu" : @"Esc" forState:UIControlStateNormal];
            button.accessibilityLabel = exploring ? @"Pause menu" : @"Cancel";
        }
        if (button.tag == ZU4_TAG_KEYBOARD || button.tag == ZU4_TAG_CAST) {
            BOOL casting = (status || combat) && !g_kb_shown;
            button.tag = casting ? ZU4_TAG_CAST : ZU4_TAG_KEYBOARD;
            [button setTitle:casting ? @"Spells" : @"⌨" forState:UIControlStateNormal];
            button.accessibilityLabel = casting ? @"Spellbook" : @"Keyboard";
        }
    }
    if (!g_overlay || g_kb_shown || !status) {
        g_world_status.hidden = YES;
        g_world_log.hidden = YES;
        g_party_roster.hidden = YES;
        return 0;
    }
    if (!g_world_status) {
        g_world_status = [[UILabel alloc] init];
        g_world_status.numberOfLines = 2;
        g_world_status.font = [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
        g_world_status.adjustsFontForContentSizeCategory = YES;
        g_world_status.textColor = UIColor.whiteColor;
        [g_overlay addSubview:g_world_status];
        g_world_log = [[UILabel alloc] init];
        g_world_log.numberOfLines = 6;
        g_world_log.font = [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
        g_world_log.adjustsFontForContentSizeCategory = YES;
        g_world_log.textColor = [UIColor colorWithWhite:0.8 alpha:1];
        [g_overlay addSubview:g_world_log];
    }
    g_world_status.hidden = NO;
    g_world_log.hidden = NO;
    NSArray *rawLines = [g_recent_messages componentsSeparatedByString:@"\n"];
    NSMutableArray<NSString *> *lines = [NSMutableArray array];
    for (NSString *line in rawLines) {
        NSString *trimmed = [line stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet];
        if (trimmed.length) [lines addObject:trimmed];
    }
    g_world_status.text = [NSString stringWithUTF8String:status];
    CGRect b = g_overlay.bounds;
    UIEdgeInsets safe = g_overlay.safeAreaInsets;
    CGFloat width = b.size.width - safe.left - safe.right;
    CGFloat height = b.size.height - safe.top - safe.bottom;
    BOOL portrait = b.size.height > b.size.width;
    if (portrait && dungeon) {
        UIFont *compactDungeonStatus = [UIFont systemFontOfSize:13 weight:UIFontWeightRegular];
        g_world_status.font = [[UIFontMetrics metricsForTextStyle:UIFontTextStyleSubheadline]
            scaledFontForFont:compactDungeonStatus];
    } else {
        g_world_status.font = portrait ? [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline]
                                       : [UIFont systemFontOfSize:12 weight:UIFontWeightSemibold];
    }
    g_world_status.numberOfLines = portrait ? (combat ? 3 : 2) : 7;
    if (!portrait) g_world_status.text = [g_world_status.text stringByReplacingOccurrencesOfString:@"  •  " withString:@"\n"];
    CGRect map = zu4MapFrame(b, safe);
    CGFloat utilityX = MAX(CGRectGetMaxX(map) + 12, CGRectGetMaxX(b) - safe.right - 176);
    CGFloat utilityWidth = CGRectGetMaxX(b) - safe.right - utilityX;
    if (!portrait && settings.flipControls) {
        utilityX = safe.left + 92;
        utilityWidth = CGRectGetMinX(map) - utilityX - 8 + 84;
    }
    CGFloat side = map.size.width, x = map.origin.x, y = map.origin.y;
    g_world_status.frame = portrait ? CGRectMake(safe.left + 12, safe.top + 8, width - 108, combat ? 56 : 52)
        : CGRectMake(utilityX, safe.top + 8, MAX(0.0, utilityWidth - 84.0), 80);
    zu4_update_party_roster(portrait, b, safe, map, !panelVisible);
    g_world_log.hidden = panelVisible;
    // Ignore trailing/structural blank lines so they never consume the recent
    // activity capacity. Longer entries word-wrap into the three visual lines.
    NSUInteger compactStart = lines.count > 3 ? lines.count - 3 : 0;
    g_world_log.text = [[lines subarrayWithRange:NSMakeRange(compactStart, lines.count - compactStart)] componentsJoinedByString:@"\n"];
    g_world_log.numberOfLines = 3;
    g_world_log.lineBreakMode = NSLineBreakByWordWrapping;
    g_world_log.font = [UIFont systemFontOfSize:12 weight:UIFontWeightRegular];
    g_world_log.backgroundColor = [UIColor colorWithWhite:0 alpha:0.62];
    g_world_log.layer.cornerRadius = 5.0;
    g_world_log.layer.masksToBounds = YES;
    CGFloat logWidth = floor((map.size.width - 12.0) * 0.5);
    g_world_log.frame = CGRectMake(CGRectGetMinX(map) + 6, CGRectGetMaxY(map) - 60,
                                   logWidth, 54);
    CGFloat sx = pixelWidth / b.size.width, sy = pixelHeight / b.size.height;
    *rect = (SDL_Rect){(int)(x * sx), (int)(y * sy), (int)(side * sx), (int)(side * sy)};
    return 1;
}

void zu4_ios_message(const char *text) {
    if (!g_recent_messages) g_recent_messages = [[NSMutableString alloc] init];
    for (const unsigned char *p = (const unsigned char *)text; *p; ++p) {
        if (*p == '\b') {
            if (g_recent_messages.length) [g_recent_messages deleteCharactersInRange:NSMakeRange(g_recent_messages.length - 1, 1)];
        } else if (*p == '\n' || (*p >= 32 && *p < 127)) {
            [g_recent_messages appendFormat:@"%c", *p];
        }
    }
    if (g_recent_messages.length > 2048)
        [g_recent_messages deleteCharactersInRange:NSMakeRange(0, g_recent_messages.length - 2048)];
}
