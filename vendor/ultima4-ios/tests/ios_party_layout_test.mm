#import <UIKit/UIKit.h>
#import <QuartzCore/QuartzCore.h>
#import "../ios/topic_panel.mm"
#include "soundtrack.h"
#include <string>
#import <objc/runtime.h>
@interface AudioGeometryPanel : Zu4TopicPanel
@end
@implementation AudioGeometryPanel
- (UIEdgeInsets)safeAreaInsets {
    return self.bounds.size.width > self.bounds.size.height ? UIEdgeInsetsMake(0,59,21,59) : UIEdgeInsetsMake(20,0,0,0);
}
@end
SettingsData settings;
static void testControlGeometry() {
    const CGSize sizes[] = {{320,568},{375,667},{393,852},{402,874},{852,393},{874,402},{768,1024},{1024,768}};
    for (CGSize size : sizes) {
        BOOL portrait = size.height > size.width;
        CGRect bounds = CGRectMake(0, 0, size.width, size.height);
        UIEdgeInsets safe = portrait ? UIEdgeInsetsMake(size.height < 700 ? 20 : 59, 0, size.height < 700 ? 0 : 34, 0)
            : UIEdgeInsetsMake(0, 59, 21, 59);
        CGRect area = UIEdgeInsetsInsetRect(bounds, safe), map = zu4MapFrame(bounds, safe);
        for (int preference = 0; preference < 3; ++preference)
            for (int flip = 0; flip < 2; ++flip) {
                CGRect dpad = zu4DpadFrame(bounds, safe, preference, flip);
                CGFloat button = (dpad.size.width - 10) / 3;
                NSCAssert(button >= 44, @"D-pad target too small");
                NSCAssert(CGRectContainsRect(area, dpad), @"D-pad outside safe area");
                NSCAssert(!CGRectIntersectsRect(map, dpad), @"D-pad covers world");
                CGRect direction = zu4DirectionFrame(bounds, safe, preference, flip);
                NSCAssert(!CGRectIntersectsRect(direction, dpad), @"Direction prompt covers D-pad");
                NSCAssert(CGRectContainsRect(area, direction), @"Direction prompt outside safe area");
                CGRect opposite = zu4DpadFrame(bounds, safe, preference, !flip);
                NSCAssert(dpad.size.width == opposite.size.width, @"Flip changes D-pad size");
                NSCAssert(fabs(dpad.origin.x + opposite.origin.x + dpad.size.width -
                    (CGRectGetMinX(area) + CGRectGetMaxX(area))) < 0.1, @"D-pad not mirrored");
                for (int column = 0; column < 2; ++column)
                    for (int row = 0; row < 4; ++row) {
                        CGRect action = zu4ActionFrame(bounds, safe, flip, column, row);
                        NSCAssert(CGRectContainsRect(area, action), @"Action outside safe area");
                        NSCAssert(!CGRectIntersectsRect(action, dpad), @"D-pad and action overlap");
                        NSCAssert(action.size.width >= 44 && action.size.height >= 44, @"Action target too small");
                    }
            }
    }
    NSLog(@"PASS 48 control geometries: sizes, flip, safe areas, targets, world preservation");
}
static void submit(const char *, int, void *) {}
static void testPauseMenu(UIWindow *window) {
    const char *keys[]={"save","explore","travel","journal","experience","controls","test_tools","backups","extra","resume"};
    const char *labels[]={"Save to Slot 1","Explore","Travel","Journal","Experience","Controls","Debug Tools","Adventure backups — Import / Export","Another future action","Resume"};
    int roles[]={0,0,0,0,0,0,0,0,0,ZU4_TOPIC_CHOICE_DISMISS};
    for(CGSize size : {CGSizeMake(375,667),CGSizeMake(393,852),CGSizeMake(852,393),CGSizeMake(768,1024)})
        for(int actions : {7,8,9}) {
            const char *pageKeys[10],*pageLabels[10];int pageRoles[10]={};
            for(int i=0;i<actions;++i){pageKeys[i]=keys[i];pageLabels[i]=labels[i];}
            pageKeys[actions]=keys[9];pageLabels[actions]=labels[9];pageRoles[actions]=roles[9];
            zu4_topic_panel_show("Adventure paused",pageKeys,pageLabels,pageRoles,actions+1,0,0,0,ZU4_TOPIC_PANEL_COMPACT_MENU,submit,nullptr);
            Zu4TopicPanel *panel=nil;for(UIView *v in window.subviews)if([v isKindOfClass:Zu4TopicPanel.class])panel=(Zu4TopicPanel *)v;
            NSCAssert(panel,@"Pause menu missing");object_setClass(panel,AudioGeometryPanel.class);
            panel.translatesAutoresizingMaskIntoConstraints=YES;[NSLayoutConstraint deactivateConstraints:window.constraints];panel.frame=(CGRect){CGPointZero,size};
            for(int pass=0;pass<8;++pass){[panel setNeedsLayout];[panel layoutIfNeeded];[panel.card layoutIfNeeded];[panel.scrollView layoutIfNeeded];[panel.stack layoutIfNeeded];}
            int visible=0;
            for(UIStackView *row in panel.optionRows)for(UIView *v in row.arrangedSubviews)if([v isKindOfClass:UIButton.class]){
                CGRect frame=[v convertRect:v.bounds toView:panel.scrollView];
                NSCAssert(frame.size.height>=44&&frame.size.width>=44,@"Pause action too small");
                NSCAssert(CGRectContainsRect(panel.scrollView.bounds,frame),@"Pause action clipped");++visible;
            }
            NSCAssert(visible==actions&&!panel.scrollView.scrollEnabled,@"Ordinary pause menu hides an action or scrolls");
            NSLog(@"PASS pause actions=%d size=%@ card=%@",actions,NSStringFromCGSize(size),NSStringFromCGRect(panel.card.frame));
            [panel removeFromSuperview];
        }
}
static void testAudio(UIWindow *window) {
    const char *keys[] = {"music", "effects", "credits", "back"};
    const char *labels[] = {"Music volume\n60%", "Sound effects\n40%", "Music and graphics\nCredits", "Audio Back"};
    const char *volumeKeys[] = {"0","2","4","6","8","10","back"};
    const char *volumeLabels[] = {"Off\nMuted","20%\nVolume","40%\nVolume","60%\nVolume","80%\nVolume","100%\nVolume","Audio Back"};
    int roles[] = {0,0,0,ZU4_TOPIC_CHOICE_BACK};
    int volumeRoles[] = {0,0,0,0,0,0,ZU4_TOPIC_CHOICE_BACK};
    for (int geometry=0; geometry<2; ++geometry) for (int page=0; page<2; ++page) {
        zu4_topic_panel_show(page==0 ? "Audio\n\nPreferences apply to all adventures." : "Music volume",
            page==0 ? keys : volumeKeys,
            page==0 ? labels : volumeLabels,
            page==1 ? volumeRoles : roles, page==1 ? 7 : 4, 0,0,0,
            ZU4_TOPIC_PANEL_DENSE_FULLSCREEN, submit, nullptr);
        Zu4TopicPanel *panel=nil;
        for (UIView *v in window.subviews) if ([v isKindOfClass:Zu4TopicPanel.class]) panel=(Zu4TopicPanel *)v;
        NSCAssert(panel, @"Missing audio panel");
        // Exercise real UIKit constraints at SE portrait and notched landscape
        // sizes without changing device orientation or any engine adventure.
        object_setClass(panel, AudioGeometryPanel.class);
        for(NSLayoutConstraint *constraint in window.constraints.copy)
            if(constraint.firstItem==panel || constraint.secondItem==panel) constraint.active=NO;
        panel.translatesAutoresizingMaskIntoConstraints=YES;
        panel.frame=geometry==0 ? CGRectMake(0,0,375,667) : CGRectMake(0,0,852,393);
        for(int pass=0; pass<5; ++pass){[window layoutIfNeeded];[panel setNeedsLayout];[panel layoutIfNeeded];[panel.card layoutIfNeeded];}
        CGRect safe=UIEdgeInsetsInsetRect(panel.bounds,panel.safeAreaInsets);
        for(UIStackView *row in panel.optionRows) for(UIView *v in row.arrangedSubviews)
            if([v isKindOfClass:UIButton.class]) {
                CGRect frame=[v convertRect:v.bounds toView:panel];
                NSCAssert(v.bounds.size.height>=44 && CGRectContainsRect(safe,frame), @"Audio choice clipped or smaller than 44 points");
            }
        NSCAssert(!panel.scrollView.scrollEnabled && panel.scrollView.contentSize.height<=panel.scrollView.bounds.size.height+1, @"Audio choices need scrolling");
        NSLog(@"PASS audio page %d bounds=%@",page,NSStringFromCGRect(panel.bounds));
        [panel removeFromSuperview];
    }
}
static int favorite(int) { return 1; }
static int noteCount() { return 3; }
static int noteAt(int index, Zu4JournalNote *note) {
    if (index < 0 || index >= 3) return 0;
    note->identifier = index + 1; note->passage = index == 0 ? 0 : -1;
    note->text = "Personal reminder, clearly separated from recorded source words."; return 1;
}
static void testJournal(UIWindow *window) {
    const char *texts[] = {"First encountered passage.", "Second encountered passage."};
    const char *sources[] = {"Iolo in Britain", "Iolo in Britain"};
    const char *speakers[] = {"Iolo", "Iolo"};
    const char *places[] = {"Britain", "Britain"};
    const char *kinds[] = {"person", "person"};
    const char *topics[] = {"Introduction", "job"};
    int indices[] = {0,1};
    Zu4JournalAccess access = {}; access.favorite = favorite; access.noteCount = noteCount; access.noteAt = noteAt;
    zu4_journal_panel_show(texts, sources, speakers, places, kinds, topics, indices, 2, &access);
    Zu4JournalPanel *panel = nil;
    for (UIView *view in window.subviews) if ([view isKindOfClass:Zu4JournalPanel.class]) panel = (Zu4JournalPanel *)view;
    NSCAssert(panel, @"Journal missing");
    for (NSInteger mode = 0; mode < 3; ++mode) {
        panel.journalMode = mode; [panel changeMode];
        for (int pass = 0; pass < 5; ++pass) { [window layoutIfNeeded]; [panel setNeedsLayout]; [panel layoutIfNeeded]; [panel.card layoutIfNeeded]; [panel updateListNavigation]; }
        NSCAssert(panel.modeControl.bounds.size.height >= 44, @"Short journal tabs");
        NSCAssert(CGRectEqualToRect(panel.card.frame, CGRectInset(UIEdgeInsetsInsetRect(panel.bounds, panel.safeAreaInsets), 8, 8)), @"Journal not full-screen");
        for (UIButton *button in panel.modeButtons)
            NSCAssert(button.bounds.size.width >= 44 && button.bounds.size.height >= 44, @"Small journal mode button");
        NSCAssert(panel.previousListButton.bounds.size.width >= 44 && panel.previousListButton.bounds.size.height >= 44, @"Small Previous target");
        NSCAssert(panel.nextListButton.bounds.size.width >= 44 && panel.nextListButton.bounds.size.height >= 44, @"Small Next target");
        if (mode == 2) NSCAssert(panel.addNoteButton.bounds.size.width >= 44 && panel.addNoteButton.bounds.size.height >= 44, @"Small New note target");
        for (NSInteger row = 0; row < [panel visibleListItems].count; ++row) {
            CGRect frame = [panel.tableView rectForRowAtIndexPath:[NSIndexPath indexPathForRow:row inSection:0]];
            NSCAssert(CGRectGetMaxY(frame) <= panel.tableView.bounds.size.height + 0.5, @"Journal list row clipped");
        }
        NSLog(@"PASS journal mode %ld %@ card=%@ list=%@", (long)mode, NSStringFromCGRect(panel.bounds), NSStringFromCGRect(panel.card.frame), NSStringFromCGRect(panel.tableView.frame));
    }
    [panel showSource:[panel sourceForPassage:1]];
    NSCAssert(panel.detailScroll.bounds.size.height >= 44, @"Transcript viewport too short");
    for (UIView *entry in panel.detailStack.arrangedSubviews) if ([entry isKindOfClass:UIStackView.class])
        for (UIView *content in ((UIStackView *)entry).arrangedSubviews) if ([content isKindOfClass:UIStackView.class])
            for (UIView *button in ((UIStackView *)content).arrangedSubviews) if ([button isKindOfClass:UIButton.class]) {
                NSCAssert(button.bounds.size.height >= 44 && button.bounds.size.width >= 44, @"Small passage action");
                NSCAssert(((UIButton *)button).currentImage && !((UIButton *)button).currentTitle.length, @"Passage action is not icon-only");
                NSCAssert(button.accessibilityLabel.length, @"Missing icon accessibility label");
            }
    NSLog(@"PASS journal transcript viewport and passage action targets");
    [panel closeJournal];
}
@interface LayoutDelegate : UIResponder <UIApplicationDelegate>
@property(nonatomic, strong) UIWindow *window;
@end
@implementation LayoutDelegate
- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)options {
    self.window = [[UIWindow alloc] initWithFrame:UIScreen.mainScreen.bounds];
    self.window.rootViewController = [UIViewController new];
    [self.window makeKeyAndVisible];
    dispatch_async(dispatch_get_main_queue(), ^{
        testControlGeometry();
        if (getenv("ZU4_PAUSE_LAYOUT")) { testPauseMenu(self.window); return; }
        if (getenv("ZU4_AUDIO_LAYOUT")) { testAudio(self.window); return; }
        if (getenv("ZU4_JOURNAL_LAYOUT")) { testJournal(self.window); return; }
        const char *keys[] = {"0", "1", "2", "3", "4", "5", "6", "7", "cancel"};
        const char *labels[] = {"Joe · Healthy\nHP 800/800", "Iolo · Healthy\nHP 800/800",
            "Jaana · Healthy\nHP 200/200", "Julia · Healthy\nHP 200/200",
            "Dupre · Healthy\nHP 300/300", "Shamino · Healthy\nHP 200/200",
            "Katrina · Healthy\nHP 100/100", "Geoffrey · Healthy\nHP 200/200", "Cancel"};
        int roles[] = {0,0,0,0,0,0,0,0,ZU4_TOPIC_CHOICE_DISMISS};
        zu4_topic_panel_show("Choose caster", keys, labels, roles, 9, 0, 0, 0,
            ZU4_TOPIC_PANEL_PARTY_SELECTION, submit, nullptr);
        [self.window layoutIfNeeded];
        Zu4TopicPanel *panel = nil;
        for (UIView *view in self.window.subviews)
            if ([view isKindOfClass:Zu4TopicPanel.class]) panel = (Zu4TopicPanel *)view;
        NSCAssert(panel != nil, @"Missing panel");
        for (int pass = 0; pass < 5; ++pass) {
            [panel setNeedsLayout]; [panel layoutIfNeeded];
            [panel.card layoutIfNeeded]; [panel.scrollView layoutIfNeeded]; [panel.stack layoutIfNeeded];
        }
        NSCAssert(!panel.scrollView.scrollEnabled, @"Ordinary party selection scrolls");
        NSInteger count = 0;
        for (UIStackView *row in panel.optionRows)
            for (UIButton *button in row.arrangedSubviews) {
                CGRect frame = [button convertRect:button.bounds toView:panel.scrollView];
                NSLog(@"MEMBER %@ %@", button.currentTitle, NSStringFromCGRect(frame));
                NSCAssert(CGRectGetMaxY(frame) <= panel.scrollView.bounds.size.height + 1, @"Member clipped");
                NSCAssert(frame.size.height >= 44 && frame.size.width >= 44, @"Small target");
                ++count;
            }
        NSCAssert(count == 8, @"Not all eight members visible");
        CGRect title = zu4TitleFrame(panel.bounds, panel.safeAreaInsets);
        CGRect world = zu4MapFrame(panel.bounds, panel.safeAreaInsets);
        if (panel.bounds.size.height > panel.bounds.size.width)
            NSCAssert(CGRectGetMinY(panel.card.frame) >= CGRectGetMaxY(world), @"Ordinary party sheet covers world");
        NSCAssert(fabs(CGRectGetMaxY(title) - CGRectGetMaxY(world)) < 0.1, @"Title not bottom aligned");
        NSLog(@"PASS eight members without scrolling, title=%@ world=%@ card=%@ scroll=%@",
            NSStringFromCGRect(title), NSStringFromCGRect(world), NSStringFromCGRect(panel.card.frame),
            NSStringFromCGRect(panel.scrollView.frame));
        UIGraphicsImageRenderer *renderer = [[UIGraphicsImageRenderer alloc] initWithSize:self.window.bounds.size];
        UIImage *snapshot = [renderer imageWithActions:^(UIGraphicsImageRendererContext *context) {
            [self.window.layer renderInContext:context.CGContext];
        }];
        NSString *path = [NSHomeDirectory() stringByAppendingPathComponent:@"Documents/party-layout.png"];
        [UIImagePNGRepresentation(snapshot) writeToFile:path atomically:YES];
        NSLog(@"SNAPSHOT %@", path);
    });
    return YES;
}
@end
int main(int argc, char **argv) {
    @autoreleasepool { return UIApplicationMain(argc, argv, nil, NSStringFromClass(LayoutDelegate.class)); }
}
