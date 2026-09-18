#import <UIKit/UIKit.h>
#import <WebKit/WebKit.h>
#import <Security/Security.h>
#include "cloud_accounts.h"
#include "adventure_package.h"
#include "game_data_bootstrap.h"
#include "cloud_config.h"

@interface Zu4CloudAccounts : UIViewController <WKScriptMessageHandler, WKNavigationDelegate, UIAdaptivePresentationControllerDelegate>
@property(nonatomic,strong) WKWebView *web;
@property(nonatomic,strong) NSArray *slots;
@property(nonatomic) Zu4AdventureTransferResult result;
@property(nonatomic) void *context;
@property(nonatomic) BOOL finished;
@property(nonatomic) BOOL backgroundSync;
@end
static Zu4CloudAccounts *activeAccounts;
static NSString *const sessionAccount=ZU4_CLOUD_SESSION_ACCOUNT_NS;
static NSString *const cloudLinksKey=@"UltimatumCloudAdventureLinks-v2";
static NSDictionary *keychainQuery() {
    return @{(__bridge id)kSecClass:(__bridge id)kSecClassGenericPassword,
      (__bridge id)kSecAttrService:[NSBundle.mainBundle.bundleIdentifier stringByAppendingString:@".ultimatum-account"],
      (__bridge id)kSecAttrAccount:sessionAccount};
}
@implementation Zu4CloudAccounts
- (void)viewDidLoad {
    [super viewDidLoad];self.view.backgroundColor=UIColor.blackColor;
    WKWebViewConfiguration *config=[[WKWebViewConfiguration alloc] init];
    config.websiteDataStore=WKWebsiteDataStore.nonPersistentDataStore;
    [config.userContentController addScriptMessageHandler:self name:@"ultimatumCloud"];
    if(self.backgroundSync)[config.userContentController addUserScript:[[WKUserScript alloc] initWithSource:@"window.ultimatumBackgroundSync=true" injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:YES]];
    self.web=[[WKWebView alloc] initWithFrame:CGRectZero configuration:config];self.web.navigationDelegate=self;
    self.web.translatesAutoresizingMaskIntoConstraints=NO;[self.view addSubview:self.web];
    UIButton *done=[UIButton buttonWithType:UIButtonTypeSystem];[done setTitle:@"Done" forState:UIControlStateNormal];
    done.titleLabel.font=[UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];done.translatesAutoresizingMaskIntoConstraints=NO;
    [done addTarget:self action:@selector(close) forControlEvents:UIControlEventTouchUpInside];if(!self.backgroundSync)[self.view addSubview:done];
    UILayoutGuide *safe=self.view.safeAreaLayoutGuide;
    if(self.backgroundSync)[NSLayoutConstraint activateConstraints:@[[self.web.topAnchor constraintEqualToAnchor:safe.topAnchor],[self.web.leadingAnchor constraintEqualToAnchor:safe.leadingAnchor],[self.web.trailingAnchor constraintEqualToAnchor:safe.trailingAnchor],[self.web.bottomAnchor constraintEqualToAnchor:safe.bottomAnchor]]];
    else [NSLayoutConstraint activateConstraints:@[[done.topAnchor constraintEqualToAnchor:safe.topAnchor],[done.trailingAnchor constraintEqualToAnchor:safe.trailingAnchor constant:-16],[done.heightAnchor constraintGreaterThanOrEqualToConstant:44],[done.widthAnchor constraintGreaterThanOrEqualToConstant:64],
      [self.web.topAnchor constraintEqualToAnchor:done.bottomAnchor],[self.web.leadingAnchor constraintEqualToAnchor:safe.leadingAnchor],[self.web.trailingAnchor constraintEqualToAnchor:safe.trailingAnchor],[self.web.bottomAnchor constraintEqualToAnchor:safe.bottomAnchor]]];
    NSURL *directory=[NSBundle.mainBundle.bundleURL URLByAppendingPathComponent:@"accounts" isDirectory:YES];
    [self.web loadFileURL:[directory URLByAppendingPathComponent:@"cloud.html"] allowingReadAccessToURL:directory];
    if(self.backgroundSync){
      __weak Zu4CloudAccounts *weakSelf=self;
      dispatch_after(dispatch_time(DISPATCH_TIME_NOW,30*NSEC_PER_SEC),dispatch_get_main_queue(),^{
        Zu4CloudAccounts *strongSelf=weakSelf;if(strongSelf&&!strongSelf.finished)[strongSelf close];
      });
    }
}
- (void)finish:(int)outcome data:(NSData *)data error:(NSString *)error {
    if(self.finished)return;self.finished=YES;
    [self.web.configuration.userContentController removeScriptMessageHandlerForName:@"ultimatumCloud"];
    [self.web stopLoading];self.web.navigationDelegate=nil;
    void (^complete)(void)=^{
      Zu4AdventureTransferResult callback=self.result;void *context=self.context;self.result=NULL;self.context=NULL;
      if(activeAccounts==self)activeAccounts=nil;
      if(callback)callback(outcome,(const unsigned char *)data.bytes,data.length,error.UTF8String,context);
    };
    if(self.presentingViewController)[self dismissViewControllerAnimated:NO completion:complete];
    else if(self.parentViewController){[self willMoveToParentViewController:nil];[self.view removeFromSuperview];[self removeFromParentViewController];complete();}
    else complete();
}
- (void)close {[self finish:0 data:nil error:nil];}
- (void)presentationControllerDidDismiss:(UIPresentationController *)controller {[self close];}
- (void)reply:(NSNumber *)identifier value:(id)value error:(NSString *)error {
    if(self.finished)return;
    NSData *json=[NSJSONSerialization dataWithJSONObject:@[identifier ?: @0,value ?: NSNull.null,error ?: NSNull.null] options:0 error:nil];
    NSString *args=[[NSString alloc] initWithData:json encoding:NSUTF8StringEncoding];
    if(args)[self.web evaluateJavaScript:[NSString stringWithFormat:@"window.ultimatumCloudReply(...%@)",args] completionHandler:nil];
}
- (void)userContentController:(WKUserContentController *)controller didReceiveScriptMessage:(WKScriptMessage *)message {
    if(self.finished || !message.frameInfo.mainFrame || ![message.body isKindOfClass:NSDictionary.class])return;
    NSDictionary *body=message.body;NSNumber *identifier=body[@"id"];NSString *action=body[@"action"];
    if(![identifier isKindOfClass:NSNumber.class] || ![action isKindOfClass:NSString.class])return;
    if([action isEqual:@"close"]){[self close];return;}
    if([action isEqual:@"syncComplete"]){[self close];return;}
    if([action isEqual:@"list"]){[self reply:identifier value:self.slots error:nil];return;}
    if([action isEqual:@"setLink"]) {
      NSNumber *slot=body[@"slot"];NSDictionary *cloud=[body[@"cloud"] isKindOfClass:NSDictionary.class] ? body[@"cloud"] : nil;
      id expected=body[@"expected"];NSMutableDictionary *local=nil;
      for(NSDictionary *record in self.slots)if([record[@"slot"] isEqual:slot])local=[record mutableCopy];
      if(![slot isKindOfClass:NSNumber.class] || slot.intValue<1 || slot.intValue>3 || !cloud ||
         ![cloud[@"resourceId"] isKindOfClass:NSString.class] || ![cloud[@"revisionId"] isKindOfClass:NSString.class] ||
         ![cloud[@"fingerprint"] isKindOfClass:NSString.class] || ![cloud[@"label"] isKindOfClass:NSString.class] ||
         ![(local[@"fingerprint"] ?: NSNull.null) isEqual:(expected ?: NSNull.null)]) {
        [self reply:identifier value:nil error:@"The local adventure changed before cloud sync finished."];return;
      }
      NSMutableDictionary *links=[[NSUserDefaults.standardUserDefaults dictionaryForKey:cloudLinksKey] mutableCopy] ?: [NSMutableDictionary dictionary];
      links[slot.stringValue]=cloud;[NSUserDefaults.standardUserDefaults setObject:links forKey:cloudLinksKey];local[@"cloud"]=cloud;
      NSMutableArray *updated=[self.slots mutableCopy];for(NSUInteger i=0;i<updated.count;++i)if([updated[i][@"slot"] isEqual:slot])updated[i]=local;self.slots=updated;
      [self reply:identifier value:local error:nil];return;
    }
    if([@[@"getSession",@"setSession",@"removeSession"] containsObject:action]) {
      if(![body[@"key"] isEqual:sessionAccount]){[self reply:identifier value:nil error:@"Invalid account storage key."];return;}
      NSMutableDictionary *query=[keychainQuery() mutableCopy];OSStatus status=errSecSuccess;
      if([action isEqual:@"getSession"]) {
        query[(__bridge id)kSecReturnData]=@YES;CFTypeRef raw=NULL;status=SecItemCopyMatching((__bridge CFDictionaryRef)query,&raw);
        NSData *data=CFBridgingRelease(raw);
        [self reply:identifier value:data ? [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding] : nil error:(status==errSecSuccess || status==errSecItemNotFound) ? nil : @"Unlock the device to restore your account."];return;
      }
      if([action isEqual:@"removeSession"])status=SecItemDelete((__bridge CFDictionaryRef)query);
      else {
        NSString *value=body[@"value"];if(![value isKindOfClass:NSString.class] || value.length>65536){[self reply:identifier value:nil error:@"Invalid account session."];return;}
        NSData *data=[value dataUsingEncoding:NSUTF8StringEncoding];
        NSDictionary *attributes=@{(__bridge id)kSecValueData:data,(__bridge id)kSecAttrAccessible:(__bridge id)kSecAttrAccessibleWhenUnlockedThisDeviceOnly};
        status=SecItemUpdate((__bridge CFDictionaryRef)query,(__bridge CFDictionaryRef)attributes);
        if(status==errSecItemNotFound){[query addEntriesFromDictionary:attributes];status=SecItemAdd((__bridge CFDictionaryRef)query,NULL);}
      }
      [self reply:identifier value:nil error:(status==errSecSuccess || status==errSecItemNotFound) ? nil : @"The account session could not be saved securely. Local adventures are safe."];return;
    }
    if([action isEqual:@"gameData"]){
      char *json=zu4_ios_game_data_entries_json();
      if(!json){[self reply:identifier value:@{@"available":@NO} error:nil];return;}
      NSData *data=[NSData dataWithBytesNoCopy:json length:strlen(json) freeWhenDone:YES];
      NSDictionary *value=[NSJSONSerialization JSONObjectWithData:data options:0 error:nil];
      [self reply:identifier value:value ?: @{ @"available":@NO } error:nil];return;
    }
    if([action isEqual:@"installGameData"]){
      NSString *package=body[@"text"];
      if(![package isKindOfClass:NSString.class] || [package lengthOfBytesUsingEncoding:NSUTF8StringEncoding]>16*1024*1024){[self reply:identifier value:nil error:@"Invalid cloud game data."];return;}
      char installError[512]={};
      if(!zu4_ios_install_game_data_package(package.UTF8String,installError,sizeof(installError))){[self reply:identifier value:nil error:[NSString stringWithUTF8String:installError] ?: @"Game data could not be installed."];return;}
      [self reply:identifier value:@{@"reloadRequired":@YES} error:nil];return;
    }
    NSString *text=body[@"text"];if(![text isKindOfClass:NSString.class] || [text lengthOfBytesUsingEncoding:NSUTF8StringEncoding]>16*1024*1024){[self reply:identifier value:nil error:@"Invalid cloud checkpoint."];return;}
    AdventurePackage::Bundle bundle;std::string error;
    if(!AdventurePackage::decode(text.UTF8String,bundle,error)){[self reply:identifier value:nil error:[NSString stringWithUTF8String:error.c_str()]];return;}
    if([action isEqual:@"validate"]){[self reply:identifier value:@"validated saved checkpoint" error:nil];return;}
    if([action isEqual:@"install"]) {
      NSNumber *slot=body[@"slot"];id expected=body[@"expected"];
      if(![slot isKindOfClass:NSNumber.class] || slot.intValue<1 || slot.intValue>3){[self reply:identifier value:nil error:@"Choose Slot 1, 2 or 3."];return;}
      NSDictionary *local=nil;for(NSDictionary *record in self.slots)if([record[@"slot"] isEqual:slot])local=record;
      if([local[@"active"] boolValue] || ![(local[@"fingerprint"] ?: NSNull.null) isEqual:(expected ?: NSNull.null)]){[self reply:identifier value:nil error:@"Return to the title before replacing a playing or changed adventure."];return;}
      NSDictionary *cloud=[body[@"cloud"] isKindOfClass:NSDictionary.class] ? body[@"cloud"] : nil;
      NSData *data=[NSJSONSerialization dataWithJSONObject:@{@"slot":slot,@"text":text,@"expected":expected ?: NSNull.null,@"cloud":cloud ?: NSNull.null} options:0 error:nil];
      [self finish:1 data:data error:nil];return;
    }
    [self reply:identifier value:nil error:@"Unknown account action."];
}
- (void)webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)action decisionHandler:(void (^)(WKNavigationActionPolicy))handler {
    NSURL *url=action.request.URL;NSURL *root=[NSBundle.mainBundle.bundleURL URLByAppendingPathComponent:@"accounts" isDirectory:YES];
    // Auth stays inside the trusted packaged UI. Remote pages never gain the bridge.
    BOOL allowed=url.isFileURL && [url.path hasPrefix:[root.path stringByAppendingString:@"/"]];
    handler(allowed ? WKNavigationActionPolicyAllow : WKNavigationActionPolicyCancel);
}
- (void)webView:(WKWebView *)webView didFailProvisionalNavigation:(WKNavigation *)navigation withError:(NSError *)error {
    if(error.code!=NSURLErrorCancelled)[self finish:-1 data:nil error:@"Accounts could not open. Local play and saves are still available."];
}
@end
void zu4_cloud_accounts_show(const char *slotsJSON,size_t count,Zu4AdventureTransferResult result,void *context) {
    NSData *data=[NSData dataWithBytes:slotsJSON length:count];
    dispatch_async(dispatch_get_main_queue(),^{
      if(activeAccounts){if(result)result(-1,NULL,0,"Accounts are already open.",context);return;}
      NSArray *slots=[NSJSONSerialization JSONObjectWithData:data options:0 error:nil];
      if(![slots isKindOfClass:NSArray.class]){if(result)result(-1,NULL,0,"Saved slots could not be prepared.",context);return;}
      UIViewController *parent=nil;
      for(UIWindow *window in UIApplication.sharedApplication.windows)if(!window.hidden && window.rootViewController){parent=window.rootViewController;while(parent.presentedViewController)parent=parent.presentedViewController;if(parent.view.window)break;}
      if(!parent){if(result)result(-1,NULL,0,"Accounts could not open.",context);return;}
      Zu4CloudAccounts *accounts=[[Zu4CloudAccounts alloc] init];accounts.slots=slots;accounts.result=result;accounts.context=context;accounts.modalPresentationStyle=UIModalPresentationFullScreen;
      activeAccounts=accounts;[parent presentViewController:accounts animated:YES completion:nil];accounts.presentationController.delegate=accounts;
    });
}
void zu4_cloud_accounts_sync(const char *slotsJSON,size_t count,Zu4AdventureTransferResult result,void *context) {
    OSStatus sessionStatus=SecItemCopyMatching((__bridge CFDictionaryRef)keychainQuery(),NULL);
    if(sessionStatus!=errSecSuccess){if(result)result(0,NULL,0,NULL,context);return;}
    NSData *data=[NSData dataWithBytes:slotsJSON length:count];
    dispatch_async(dispatch_get_main_queue(),^{
      if(activeAccounts){if(result)result(0,NULL,0,NULL,context);return;}
      NSArray *slots=[NSJSONSerialization JSONObjectWithData:data options:0 error:nil];
      if(![slots isKindOfClass:NSArray.class]){if(result)result(-1,NULL,0,"Saved slots could not be prepared.",context);return;}
      UIViewController *parent=nil;
      for(UIWindow *window in UIApplication.sharedApplication.windows)if(!window.hidden&&window.rootViewController){parent=window.rootViewController;while(parent.presentedViewController)parent=parent.presentedViewController;if(parent.view.window)break;}
      if(!parent){if(result)result(-1,NULL,0,"Account sync could not start.",context);return;}
      Zu4CloudAccounts *accounts=[[Zu4CloudAccounts alloc] init];accounts.slots=slots;accounts.result=result;accounts.context=context;accounts.backgroundSync=YES;
      activeAccounts=accounts;[parent addChildViewController:accounts];accounts.view.frame=CGRectMake(0,0,1,1);accounts.view.alpha=0.01;[parent.view addSubview:accounts.view];[accounts didMoveToParentViewController:parent];
    });
}
int zu4_cloud_accounts_is_visible(void){return activeAccounts!=nil;}
