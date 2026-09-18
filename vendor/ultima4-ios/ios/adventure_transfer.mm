#import <UIKit/UIKit.h>
#include "adventure_transfer.h"

@interface Zu4AdventureTransfer : NSObject <UIDocumentPickerDelegate, UIAdaptivePresentationControllerDelegate>
@property(nonatomic) Zu4AdventureTransferResult result;
@property(nonatomic) void *context;
@property(nonatomic) BOOL finished;
@property(nonatomic,strong) UIViewController *controller;
@property(nonatomic,strong) NSURL *temporaryDirectory;
- (void)finish:(int)outcome data:(NSData *)data error:(NSString *)error;
@end
static Zu4AdventureTransfer *activeTransfer;
@implementation Zu4AdventureTransfer
- (void)finish:(int)outcome data:(NSData *)data error:(NSString *)error {
    if(self.finished)return;self.finished=YES;
    void (^complete)(void)=^{
        Zu4AdventureTransferResult result=self.result;void *context=self.context;
        self.result=NULL;self.context=NULL;
        if([self.controller isKindOfClass:UIActivityViewController.class])
            ((UIActivityViewController *)self.controller).completionWithItemsHandler=nil;
        self.controller=nil;
        if(activeTransfer==self)activeTransfer=nil;
        if(result)result(outcome,(const unsigned char *)data.bytes,data.length,error.UTF8String,context);
        if(self.temporaryDirectory)[NSFileManager.defaultManager removeItemAtURL:self.temporaryDirectory error:nil];
    };
    if(self.controller.presentingViewController)[self.controller dismissViewControllerAnimated:NO completion:complete];else complete();
}
- (void)documentPickerWasCancelled:(UIDocumentPickerViewController *)controller { [self finish:0 data:nil error:nil]; }
- (void)presentationControllerDidDismiss:(UIPresentationController *)controller { [self finish:0 data:nil error:nil]; }
- (void)documentPicker:(UIDocumentPickerViewController *)controller didPickDocumentsAtURLs:(NSArray<NSURL *> *)urls {
    if(urls.count!=1){[self finish:-1 data:nil error:@"Choose one .u4save adventure backup."];return;}
    NSURL *url=urls.firstObject;
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED,0),^{
        BOOL scoped=[url startAccessingSecurityScopedResource];
        __block NSData *data=nil;__block NSString *message=nil;
        NSError *coordinationError=nil;
        NSFileCoordinator *coordinator=[[NSFileCoordinator alloc] initWithFilePresenter:nil];
        [coordinator coordinateReadingItemAtURL:url options:0 error:&coordinationError byAccessor:^(NSURL *readURL){
            NSNumber *size=nil;NSError *readError=nil;
            if(![readURL getResourceValue:&size forKey:NSURLFileSizeKey error:&readError] || size.unsignedLongLongValue>16*1024*1024)message=@"Choose an adventure backup of at most 16 MB.";
            else {data=[NSData dataWithContentsOfURL:readURL options:NSDataReadingUncached error:&readError];if(!data)message=@"The selected backup could not be read. Try downloading it in Files first.";}
        }];
        if(scoped)[url stopAccessingSecurityScopedResource];
        if(coordinationError){data=nil;message=@"Files could not provide that backup. Try downloading it locally first.";}
        if(data.length>16*1024*1024){data=nil;message=@"Choose an adventure backup of at most 16 MB.";}
        dispatch_async(dispatch_get_main_queue(),^{[self finish:data ? 1 : -1 data:data error:message ?: @"The backup could not be read."];});
    });
}
@end

static UIViewController *presenter() {
    for(UIWindow *window in UIApplication.sharedApplication.windows) {
        if(window.hidden || !window.rootViewController)continue;
        UIViewController *controller=window.rootViewController;
        while(controller.presentedViewController)controller=controller.presentedViewController;
        if(controller.view.window)return controller;
    }
    return nil;
}
static Zu4AdventureTransfer *begin(Zu4AdventureTransferResult result, void *context) {
    if(activeTransfer){if(result)result(-1,NULL,0,"Another file transfer is already open.",context);return nil;}
    Zu4AdventureTransfer *transfer=[[Zu4AdventureTransfer alloc] init];transfer.result=result;transfer.context=context;
    activeTransfer=transfer;return transfer;
}
void zu4_adventure_import_show(Zu4AdventureTransferResult result, void *context) {
    void (^show)(void)=^{
        Zu4AdventureTransfer *transfer=begin(result,context);if(!transfer)return;
        UIViewController *parent=presenter();if(!parent){[transfer finish:-1 data:nil error:@"Files could not open. Return to the app and try again."];return;}
        UIDocumentPickerViewController *picker=[[UIDocumentPickerViewController alloc] initWithDocumentTypes:@[@"com.ultimatum.adventure",@"public.json"] inMode:UIDocumentPickerModeOpen];
        picker.allowsMultipleSelection=NO;picker.delegate=transfer;transfer.controller=picker;
        [parent presentViewController:picker animated:YES completion:nil];picker.presentationController.delegate=transfer;
    };
    if(NSThread.isMainThread)show();else dispatch_async(dispatch_get_main_queue(),show);
}
void zu4_adventure_export_show(const char *json, size_t count, int slot, Zu4AdventureTransferResult result, void *context) {
    NSData *data=[NSData dataWithBytes:json length:count];
    void (^show)(void)=^{
        Zu4AdventureTransfer *transfer=begin(result,context);if(!transfer)return;
        UIViewController *parent=presenter();if(!parent){[transfer finish:-1 data:nil error:@"The share sheet could not open. Return to the app and try again."];return;}
        NSURL *directory=[NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:[@"ultimatum-share-" stringByAppendingString:NSUUID.UUID.UUIDString]] isDirectory:YES];
        NSURL *file=[directory URLByAppendingPathComponent:[NSString stringWithFormat:@"Ultimatum-Slot-%d.u4save",slot]];
        transfer.temporaryDirectory=directory;
        if(![NSFileManager.defaultManager createDirectoryAtURL:directory withIntermediateDirectories:NO attributes:nil error:nil] || ![data writeToURL:file options:NSDataWritingAtomic error:nil]){[transfer finish:-1 data:nil error:@"The backup file could not be prepared. Your save is unchanged."];return;}
        UIActivityViewController *share=[[UIActivityViewController alloc] initWithActivityItems:@[file] applicationActivities:nil];
        transfer.controller=share;
        share.completionWithItemsHandler=^(UIActivityType type,BOOL completed,NSArray *items,NSError *error){[transfer finish:error ? -1 : (completed ? 1 : 0) data:nil error:error ? @"The backup was not shared. Try again or choose Save to Files." : nil];};
        share.popoverPresentationController.sourceView=parent.view;
        share.popoverPresentationController.sourceRect=CGRectMake(CGRectGetMidX(parent.view.bounds),CGRectGetMaxY(parent.view.bounds)-44,1,1);
        [parent presentViewController:share animated:YES completion:nil];share.presentationController.delegate=transfer;
    };
    if(NSThread.isMainThread)show();else dispatch_async(dispatch_get_main_queue(),show);
}
int zu4_adventure_transfer_is_visible() { return activeTransfer!=nil; }
