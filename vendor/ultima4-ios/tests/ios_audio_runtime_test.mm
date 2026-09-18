// Opt-in isolated simulator fixture; never included in physical-device builds.
#import <UIKit/UIKit.h>
#include "context.h"
#include "game.h"
#include "settings.h"
#include "music.h"
#include "soundtrack.h"
#include "topic_panel.h"
#include <cstdio>
#include <cstdlib>
extern "C" void zu4_audio_runtime_menu();
static void check(bool good, const char *text) {
    fprintf(stderr, "%s %s\n", good ? "PASS" : "FAIL", text); fflush(stderr);
    if (!good) abort();
}
static UIView *panel() {
    for (UIWindow *w in UIApplication.sharedApplication.windows)
        for (UIView *v in w.subviews)
            if ([v isKindOfClass:NSClassFromString(@"Zu4TopicPanel")] && v.userInteractionEnabled) return v;
    return nil;
}
static UIButton *button(UIView *root, UIView *target, NSInteger tag) {
    if ([root isKindOfClass:UIButton.class] && root.tag == tag &&
        [[(UIButton *)root allTargets] containsObject:target]) return (UIButton *)root;
    for (UIView *v in root.subviews) { UIButton *found = button(v, target, tag); if (found) return found; }
    return nil;
}
static const char *actions[] = {"music", "0", "music", "6", "effects", "4", "credits", "back", "back"};
static unsigned moves; static int food;
static void step(int index, int attempt = 0) {
    if (index == sizeof(actions)/sizeof(actions[0])) return;
    UIView *view = panel();
    if (!view) {
        check(attempt < 100, "native audio panel becomes available");
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 50*NSEC_PER_MSEC), dispatch_get_main_queue(), ^{ step(index, attempt+1); }); return;
    }
    [view.window layoutIfNeeded]; [view layoutIfNeeded];
    NSArray *keys = [view valueForKey:@"keywords"];
    NSInteger tag = [keys indexOfObject:[NSString stringWithUTF8String:actions[index]]];
    check(tag != NSNotFound, "expected native audio action is present");
    UIButton *tap = button(view, view, tag); check(tap != nil, "real native button target found");
    CGRect frame = [tap convertRect:tap.bounds toView:view];
    CGRect safe = UIEdgeInsetsInsetRect(view.bounds, view.safeAreaInsets);
    check(CGRectContainsRect(safe, frame) && tap.bounds.size.height >= 44, "audio action fits safe area with 44-point target");
    if (index == 2) check(!zu4_music_is_enabled() && settings.musicVol == 0, "native volume Off mutes engine");
    if (index == 4) check(zu4_music_is_enabled() && settings.musicVol == 6, "native volume resumes context at 60 percent");
    if (index == 6) check(settings.soundVol == 4, "native SFX setting applies independently");
    if (index == 7) check([(UIScrollView *)[view valueForKey:@"scrollView"] isScrollEnabled], "long attribution text can scroll with fixed Back");
    int wind = c->windCounter; game->paused = false; game->timerFired(); game->paused = true;
    check(c->windCounter == wind && c->saveGame->moves == moves && c->saveGame->food == food, "audio reading does not advance world or spend resources");
    [tap sendActionsForControlEvents:UIControlEventTouchUpInside];
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 180*NSEC_PER_MSEC), dispatch_get_main_queue(), ^{ step(index+1); });
}
void zu4_run_audio_runtime_tests() {
    check([NSBundle.mainBundle.bundleIdentifier isEqualToString:@"org.ultimatumproject.tests.audio"], "isolated audio test adventure");
    game->paused = true; moves = c->saveGame->moves; food = c->saveGame->food;
    if (getenv("ZU4_AUDIO_LANDSCAPE")) {
        [UIDevice.currentDevice setValue:@(UIDeviceOrientationLandscapeLeft) forKey:@"orientation"];
        [UIViewController attemptRotationToDeviceOrientation];
    }
    if (getenv("ZU4_AUDIO_RELOAD")) {
        check(settings.soundtrack == ZU4_SOUNDTRACK_HURIN && settings.musicVol == 6 && settings.soundVol == 4,
              "audio preferences survive actual engine relaunch");
        fprintf(stderr, "AUDIO RELOAD COMPLETE\n"); fflush(stderr); return;
    }
    check(ZU4_SOUNDTRACK_COUNT == 1 && zu4_music_pack_available(ZU4_SOUNDTRACK_HURIN), "only the xu4 soundtrack is installed");
    for (int song = 1; song < TRACK_MAX; ++song) {
        zu4_music_play(song); check(zu4_music_select_pack(ZU4_SOUNDTRACK_HURIN), "actual engine decodes the xu4 soundtrack");
        check(zu4_music_current_track() == song, "all nine track contexts remain available");
    }
    check(zu4_music_select_pack(ZU4_SOUNDTRACK_HURIN), "restore xu4 soundtrack");
    zu4_music_play(TRACK_OUTSIDE);
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 100*NSEC_PER_MSEC), dispatch_get_main_queue(), ^{ step(0); });
    zu4_audio_runtime_menu();
    check(!panel(), "audio Back releases the active native input panel");
    check(c->saveGame->moves == moves && c->saveGame->food == food, "audio flow leaves isolated adventure untouched");
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 200*NSEC_PER_MSEC), dispatch_get_main_queue(), ^{
        check(!zu4_topic_panel_is_visible(), "retiring audio panel clears and returns to main engine controls");
        fprintf(stderr, "AUDIO RUNTIME COMPLETE\n"); fflush(stderr);
    });
}
