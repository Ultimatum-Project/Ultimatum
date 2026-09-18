// Opt-in real engine + UIKit + WKWebView + test Supabase; never in device builds.
#import <UIKit/UIKit.h>
#import <WebKit/WebKit.h>
#include "adventure_package.h"
#include "cloud_accounts.h"
#include "context.h"
#include "event.h"
#include "game.h"
#include "settings.h"
#include "save_slots.h"
#include <cstdio>
#include <cstdlib>
extern "C" void zu4_mobile_menu(void);
static void check(bool ok,const char *text){fprintf(stderr,"%s %s\n",ok?"PASS":"FAIL",text);fflush(stderr);if(!ok)abort();}
static UIView *panel(){for(UIWindow *w in UIApplication.sharedApplication.windows)for(UIView *v in w.subviews)if([v isKindOfClass:NSClassFromString(@"Zu4TopicPanel")]&&v.userInteractionEnabled)return v;return nil;}
static UIButton *button(UIView *v,UIView *owner,NSInteger tag){if([v isKindOfClass:UIButton.class]&&v.tag==tag&&[[(UIButton *)v allTargets] containsObject:owner])return (UIButton *)v;for(UIView *child in v.subviews){UIButton *found=button(child,owner,tag);if(found)return found;}return nil;}
static UIViewController *accounts(){for(UIWindow *w in UIApplication.sharedApplication.windows){UIViewController *v=w.rootViewController;while(v.presentedViewController)v=v.presentedViewController;if([v isKindOfClass:NSClassFromString(@"Zu4CloudAccounts")])return v;}return nil;}
static unsigned moves;static int food;static std::string previous;
static bool cancellation;
static NSString *script() {
    NSURL *docs=[[NSFileManager.defaultManager URLsForDirectory:NSDocumentDirectory inDomains:NSUserDomainMask] firstObject];
    NSString *fixture=[NSString stringWithContentsOfURL:[docs URLByAppendingPathComponent:@"cloud-fixture.json"] encoding:NSUTF8StringEncoding error:nil];check(fixture!=nil,"test-only seeded identities available");
    NSString *body=@";window.cloudTestState='running';void(async()=>{const p=window.ultimatumCloudPanel,r=p.root;const a=(v,m)=>{if(!v)throw Error(m)};const wait=async(fn)=>{const start=Date.now();while(Date.now()-start<30000){if(fn())return;await new Promise(x=>setTimeout(x,100))}throw Error('UI timeout')};const b=(root,t)=>[...root.querySelectorAll('button')].find(x=>x.textContent===t);const tap=async(x)=>{await wait(()=>x&&!x.disabled);x.click()};p.cloud.sendCode=async()=>{};const signin=async(i)=>{r.querySelector('input[name=email]').value=i.email;r.querySelector('form').requestSubmit();await wait(()=>!p.busy&&!r.querySelector('.cloud-code').hidden);r.querySelector('input[name=code]').value=i.code;await tap(b(r,'Verify code'));await wait(()=>!p.busy&&r.querySelectorAll('.cloud-slot').length===3)};await signin(identities[1]);a((await p.cloud.heads()).length===0,'Other account saw web saves');await tap(r.querySelector('[data-cloud=signout]'));await wait(()=>!p.busy);p.lastCodeAt=0;await signin(identities[0]);a((await p.cloud.heads()).some(x=>x.slot===2&&x.revision),'Web cloud checkpoint missing');a([...r.querySelector('.cloud-slot select').options].find(x=>x.value==='1').disabled===false,'Upload of saved active checkpoint unexpectedly blocked');a([...r.querySelectorAll('.cloud-slot')[1].querySelectorAll('select')[1].options].find(x=>x.value==='1').disabled,'Playing native slot replace allowed');const s=r.querySelectorAll('.cloud-slot')[1];s.querySelectorAll('select')[1].value='2';await tap(b(s,'Download latest checkpoint'));await wait(()=>!p.busy&&p.confirmation);await tap(r.querySelector('[data-cloud=cancel]'));a(!p.confirmation,'Download cancel trapped');await tap(b(s,'Download latest checkpoint'));await wait(()=>!p.busy&&p.confirmation);window.cloudTestState='ready-to-install';await tap(r.querySelector('[data-cloud=confirm]'))})().catch(e=>{window.cloudTestState='failed: '+e.message});";
    return [@"const identities=" stringByAppendingString:[fixture stringByAppendingString:body]];
}
static void step(size_t index,int attempt=0) {
    const char *actions[]={"backups","cloud","__accounts","back","back","resume"};
    if(index==6)return;
    if(index==2) {
      UIViewController *v=accounts();if(!v){if(attempt>600)check(false,"native Accounts opens");dispatch_after(dispatch_time(DISPATCH_TIME_NOW,50*NSEC_PER_MSEC),dispatch_get_main_queue(),^{step(index,attempt+1);});return;}
      WKWebView *web=[v valueForKey:@"web"];
      [web evaluateJavaScript:@"Boolean(window.ultimatumCloudPanel && !window.ultimatumCloudPanel.busy)" completionHandler:^(id value,NSError *error){
        if(![value boolValue]){if(attempt>600)check(false,"packaged account UI loads");dispatch_after(dispatch_time(DISPATCH_TIME_NOW,50*NSEC_PER_MSEC),dispatch_get_main_queue(),^{step(index,attempt+1);});return;}
        check(zu4_cloud_accounts_is_visible(),"native account bridge owns paused input");
        int wind=c->windCounter;game->paused=false;game->timerFired();game->paused=true;check(wind==c->windCounter&&moves==c->saveGame->moves&&food==c->saveGame->food,"Accounts pauses world and resources");
        if(cancellation){[web evaluateJavaScript:@"document.querySelectorAll('.cloud-slot').length===3" completionHandler:^(id restored,NSError *failure){check([restored boolValue],"Keychain restores signed-in account after app relaunch");}];[v performSelector:NSSelectorFromString(@"close")];dispatch_after(dispatch_time(DISPATCH_TIME_NOW,300*NSEC_PER_MSEC),dispatch_get_main_queue(),^{step(4);});return;}
        [web evaluateJavaScript:script() completionHandler:^(id result,NSError *scriptError){check(scriptError==nil,"native real-code cloud runtime script starts");}];
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW,500*NSEC_PER_MSEC),dispatch_get_main_queue(),^{step(3);});
      }];return;
    }
    UIView *v=panel();NSArray *keys=[v valueForKey:@"keywords"];NSString *action=[NSString stringWithUTF8String:actions[index]];NSInteger tag=[keys indexOfObject:action];
    if(!v||tag==NSNotFound){
      UIViewController *account=accounts();WKWebView *web=[account valueForKey:@"web"];
      if(web)[web evaluateJavaScript:@"window.cloudTestState || ''" completionHandler:^(id value,NSError *error){if([value isKindOfClass:NSString.class]&&[value hasPrefix:@"failed:"]){fprintf(stderr,"%s\n",[value UTF8String]);check(false,"native cloud script");}}];
      if(attempt>1200)check(false,"expected native return action appears");dispatch_after(dispatch_time(DISPATCH_TIME_NOW,50*NSEC_PER_MSEC),dispatch_get_main_queue(),^{step(index,attempt+1);});return;
    }
    UIButton *tap=button(v,v,tag);check(tap!=nil&&tap.bounds.size.height>=44,"native action has a 44-point target");
    [tap sendActionsForControlEvents:UIControlEventTouchUpInside];
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW,250*NSEC_PER_MSEC),dispatch_get_main_queue(),^{step(index+1);});
}
void zu4_run_cloud_runtime_tests() {
    check([NSBundle.mainBundle.bundleIdentifier isEqual:@"org.ultimatumproject.tests.cloud"],"isolated cloud test app");
    game->paused=true;moves=c->saveGame->moves;food=c->saveGame->food;
    cancellation=getenv("ZU4_CLOUD_CANCEL")!=NULL;
    if(getenv("ZU4_CLOUD_PORTRAIT")){[UIDevice.currentDevice setValue:@(UIDeviceOrientationPortrait) forKey:@"orientation"];[UIViewController attemptRotationToDeviceOrientation];}
    NSURL *docs=[[NSFileManager.defaultManager URLsForDirectory:NSDocumentDirectory inDomains:NSUserDomainMask] firstObject];
    NSData *data=[NSData dataWithContentsOfURL:[docs URLByAppendingPathComponent:@"world.u4save"]];AdventurePackage::Bundle bundle;std::string error;
    check(data&&AdventurePackage::decode(std::string((const char *)data.bytes,data.length),bundle,error),"isolated full-metadata fixture valid");
    const std::string base=zu4_settings_ptr()->path,root=SaveSlots::snapshotRoot(base,2);
    if(SaveSnapshot::current(root).empty())check(AdventurePackage::install(base,2,bundle,"",error),"prepare disposable replacement slot");
    previous=SaveSnapshot::current(root);
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW,100*NSEC_PER_MSEC),dispatch_get_main_queue(),^{step(0);});
    zu4_mobile_menu();
    check(eventHandler->getController()==game&&!zu4_cloud_accounts_is_visible(),"native Accounts returns to main game controls");
    check(moves==c->saveGame->moves&&food==c->saveGame->food&&gameActiveSaveSlot()==1,"cloud actions keep active adventure and resources unchanged");
    if(cancellation)check(SaveSnapshot::current(root)==previous,"Done cancellation changes no checkpoint");
    else {
      check(SaveSnapshot::current(root)!=previous&&SaveSnapshot::previous(root)==previous,"native cloud installation retains local recovery generation");
      AdventurePackage::Bundle installed;check(AdventurePackage::readCheckpoint(SaveSnapshot::current(root),installed,error),"cloud checkpoint installed with native engine validation");
      NSData *expected=[NSData dataWithContentsOfURL:[docs URLByAppendingPathComponent:@"browser.u4save"]];AdventurePackage::Bundle source;
      check(expected&&AdventurePackage::decode(std::string((const char *)expected.bytes,expected.length),source,error),"web source checkpoint valid");
      check(installed.files==source.files,"web cloud → native preserves all binary saves, journal, notes, favorites, maps, and transcript bytes");
    }
    fprintf(stderr,"CLOUD NATIVE RUNTIME COMPLETE\n");fflush(stderr);
}
