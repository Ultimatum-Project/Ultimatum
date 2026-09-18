// Opt-in fixture: native UIKit + running engine, never linked into device builds.
#import <UIKit/UIKit.h>
#include "adventure_package.h"
#include "adventure_transfer.h"
#include "context.h"
#include "event.h"
#include "game.h"
#include "settings.h"
#include "save_slots.h"
#include "topic_panel.h"
#include "topicjournal.h"
#include <cstdio>
#include <cstdlib>
extern "C" void zu4_mobile_menu(void);
extern int gameSave(void);
static void check(bool ok,const char *text){fprintf(stderr,"%s %s\n",ok?"PASS":"FAIL",text);fflush(stderr);if(!ok)abort();}
static UIView *panel(){for(UIWindow *w in UIApplication.sharedApplication.windows)for(UIView *v in w.subviews)if([v isKindOfClass:NSClassFromString(@"Zu4TopicPanel")]&&v.userInteractionEnabled)return v;return nil;}
static UIButton *button(UIView *v,UIView *owner,NSInteger tag){if([v isKindOfClass:UIButton.class]&&v.tag==tag&&[[(UIButton *)v allTargets] containsObject:owner])return (UIButton *)v;for(UIView *child in v.subviews){UIButton *found=button(child,owner,tag);if(found)return found;}return nil;}
static UIViewController *presented(){for(UIWindow *w in UIApplication.sharedApplication.windows){UIViewController *v=w.rootViewController;while(v.presentedViewController)v=v.presentedViewController;if([v isKindOfClass:UIDocumentPickerViewController.class]||[v isKindOfClass:UIActivityViewController.class])return v;}return nil;}
static NSString *fixture;
static std::string before,currentImported;
static unsigned moves;static int food;
static bool titleMode;
static bool menuMode;
static const char *menuActions[]={"backups","back","resume"};
static const char *titleActions[]={"backups","import","__cancel","back","back"};
static const char *actions[]={"backups","import","__cancel","import","__bad","back","import","__good","slot-2","back","import","__good","slot-2","back","import","__good","slot-2","replace","back","export","slot-2","__share","back","resume"};
static void step(size_t index,int attempt=0){
    const size_t count=menuMode?sizeof(menuActions)/sizeof(menuActions[0]):titleMode?sizeof(titleActions)/sizeof(titleActions[0]):sizeof(actions)/sizeof(actions[0]);
    if(index==count)return;
    NSString *action=[NSString stringWithUTF8String:menuMode?menuActions[index]:titleMode?titleActions[index]:actions[index]];
    if([action hasPrefix:@"__"]){
        UIViewController *v=presented();
        if(!v){if(attempt>=300)check(false,"system file/share controller opens");dispatch_after(dispatch_time(DISPATCH_TIME_NOW,50*NSEC_PER_MSEC),dispatch_get_main_queue(),^{step(index,attempt+1);});return;}
        check(zu4_adventure_transfer_is_visible(),"file transfer owns paused input");
        int wind=c->windCounter;game->paused=false;game->timerFired();game->paused=true;
        check(wind==c->windCounter&&moves==c->saveGame->moves&&food==c->saveGame->food,"system sheet pauses world and resources");
        if([v isKindOfClass:UIDocumentPickerViewController.class]){
            UIDocumentPickerViewController *picker=(UIDocumentPickerViewController *)v;
            if([action isEqual:@"__cancel"])[picker.delegate documentPickerWasCancelled:picker];
            else {
                NSString *path=[action isEqual:@"__bad"]?[fixture stringByAppendingString:@".bad"]:fixture;
                if([action isEqual:@"__bad"])[@"{}" writeToFile:path atomically:YES encoding:NSUTF8StringEncoding error:nil];
                [picker.delegate documentPicker:picker didPickDocumentsAtURLs:@[[NSURL fileURLWithPath:path]]];
            }
        } else {
            check([action isEqual:@"__share"],"export opens actual iOS share sheet");
            NSURL *dir=[NSURL fileURLWithPath:NSTemporaryDirectory()];
            NSArray *dirs=[NSFileManager.defaultManager contentsOfDirectoryAtURL:dir includingPropertiesForKeys:nil options:0 error:nil];
            NSData *exported=nil;
            for(NSURL *candidate in dirs)if([candidate.lastPathComponent hasPrefix:@"ultimatum-share-"])exported=[NSData dataWithContentsOfURL:[candidate URLByAppendingPathComponent:@"Ultimatum-Slot-2.u4save"]]?:exported;
            AdventurePackage::Bundle bundle;std::string error;
            check(exported&&AdventurePackage::decode(std::string((const char *)exported.bytes,exported.length),bundle,error),"share sheet contains valid portable adventure");
            check(bundle.files.count("journal-notebook.dat")&&bundle.files.count("conversations.json")&&bundle.files.count("map-pins.dat"),"export includes notebook, transcript, and maps");
            UIActivityViewController *share=(UIActivityViewController *)v;
            share.completionWithItemsHandler(nil,NO,nil,nil);
        }
    } else {
        UIView *v=panel();NSArray *keys=[v valueForKey:@"keywords"];NSInteger tag=[keys indexOfObject:action];
        if(!v||tag==NSNotFound){if(attempt>=300)check(false,"expected native action appears");dispatch_after(dispatch_time(DISPATCH_TIME_NOW,50*NSEC_PER_MSEC),dispatch_get_main_queue(),^{step(index,attempt+1);});return;}
        [v.window layoutIfNeeded];UIButton *tap=button(v,v,tag);check(tap!=nil,"native transfer button found");
        check(tap.bounds.size.height>=44,"transfer touch target is at least 44 points");
        check(CGRectContainsRect(UIEdgeInsetsInsetRect(v.bounds,v.safeAreaInsets),[tap convertRect:tap.bounds toView:v]),"transfer action fits panel safe area");
        std::string root=SaveSlots::snapshotRoot(zu4_settings_ptr()->path,2);
        if(index==6)check(SaveSnapshot::current(root)==before,"invalid backup leaves destination unchanged");
        if(index==8){check(![keys containsObject:@"slot-1"],"playing slot cannot be overwritten");}
        if(index==9){currentImported=SaveSnapshot::current(root);check(!currentImported.empty()&&currentImported!=before,"validated Files import installs independent slot");}
        if(index==14)check(SaveSnapshot::current(root)==currentImported,"replacement confirmation Back preserves checkpoint");
        if(index==18)check(SaveSnapshot::previous(root)==currentImported,"replacement keeps previous recovery checkpoint");
        [tap sendActionsForControlEvents:UIControlEventTouchUpInside];
    }
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW,250*NSEC_PER_MSEC),dispatch_get_main_queue(),^{step(index+1);});
}
void zu4_run_adventure_runtime_tests(){
    check([NSBundle.mainBundle.bundleIdentifier isEqual:@"org.ultimatumproject.tests.package"],"isolated adventure test bundle");
    game->paused=true;moves=c->saveGame->moves;food=c->saveGame->food;
    if(getenv("ZU4_PACKAGE_PORTRAIT")){
        [UIDevice.currentDevice setValue:@(UIDeviceOrientationPortrait) forKey:@"orientation"];
        [UIViewController attemptRotationToDeviceOrientation];
    }
    if(getenv("ZU4_PACKAGE_TITLE")){
        titleMode=true;int active=gameActiveSaveSlot();
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW,100*NSEC_PER_MSEC),dispatch_get_main_queue(),^{step(0);});
        check(gameChooseSaveSlotForJourney()==SAVE_SLOT_SELECTION_CANCELLED,"Journey title slot picker exposes backups and Files cancellation");
        check(gameActiveSaveSlot()==active&&!zu4_adventure_transfer_is_visible(),"title picker cancellation preserves active slot and releases system sheet");
        fprintf(stderr,"ADVENTURE TITLE COMPLETE\n");fflush(stderr);return;
    }
    if(getenv("ZU4_PACKAGE_MENU")){
        menuMode=true;
        if(!getenv("ZU4_PACKAGE_MENU_HOLD"))dispatch_after(dispatch_time(DISPATCH_TIME_NOW,100*NSEC_PER_MSEC),dispatch_get_main_queue(),^{step(0);});
        zu4_mobile_menu();
        check(eventHandler->getController()==game&&moves==c->saveGame->moves&&food==c->saveGame->food,"pause menu Backups and Resume return to game without spending resources");
        fprintf(stderr,"ADVENTURE MENU COMPLETE\n");fflush(stderr);return;
    }
    if(getenv("ZU4_PACKAGE_RELOAD")){
        check(gameActiveSaveSlot()==2&&c->saveGame->moves==9471,"actual engine relaunch loads imported adventure");
        TopicJournal journal;check(journal.load(gameSaveDirectory()+"topics.txt")&&!journal.entries().empty(),"imported journal loads in native engine");
        std::string old=gameSaveDirectory();AdventurePackage::Bundle beforeSave,afterSave;std::string error;
        check(AdventurePackage::readCheckpoint(old,beforeSave,error),"imported checkpoint readable before native save");
        check(gameSave()!=0,"actual native engine saves imported adventure");
        check(AdventurePackage::readCheckpoint(gameSaveDirectory(),afterSave,error),"native checkpoint passes package validation");
        check(beforeSave.files.at("journal-notebook.dat")==afterSave.files.at("journal-notebook.dat")&&beforeSave.files.at("conversations.json")==afterSave.files.at("conversations.json"),"native save preserves imported notes, bookmarks, and web transcript");
        fprintf(stderr,"ADVENTURE RELOAD COMPLETE\n");fflush(stderr);return;
    }
    fixture=[[[NSFileManager.defaultManager URLsForDirectory:NSDocumentDirectory inDomains:NSUserDomainMask] firstObject].path stringByAppendingPathComponent:@"world.u4save"];
    before=SaveSnapshot::current(SaveSlots::snapshotRoot(zu4_settings_ptr()->path,2));
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW,100*NSEC_PER_MSEC),dispatch_get_main_queue(),^{step(0);});
    zu4_mobile_menu();
    check(eventHandler->getController()==game&&!zu4_adventure_transfer_is_visible(),"cancellation and Back return input to main controls");
    check(gameActiveSaveSlot()==1&&moves==c->saveGame->moves&&food==c->saveGame->food,"import/export leaves active adventure unchanged");
    check(SaveSlots::select(zu4_settings_ptr()->path,2),"select imported disposable adventure for relaunch test");
    fprintf(stderr,"ADVENTURE RUNTIME COMPLETE\n");fflush(stderr);
}
