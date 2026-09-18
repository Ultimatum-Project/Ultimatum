#import <UIKit/UIKit.h>
#import <CommonCrypto/CommonDigest.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game_data_bootstrap.h"
#include "miniz.h"
#include "u4file.h"

static NSString *const Zu4GameDataURL = @"https://ultima.thatfleminggent.com/ultima4.zip";
static NSString *const Zu4GameDataInfoURL = @"https://ultima.thatfleminggent.com/u4download.html";
static NSString *const Zu4GameDataSHA256 = @"94aa748cfa1d0e7aa2e518abebb994f3c18acf7edb78c3bd37cd0a4404e6ba74";

static BOOL Zu4ArchiveContainsRequiredFiles(NSURL *url)
{
    mz_zip_archive archive = {};
    if (!mz_zip_reader_init_file(&archive, url.fileSystemRepresentation, 0)) return NO;
    BOOL avatar = NO, title = NO, charset = NO;
    mz_uint count = mz_zip_reader_get_num_files(&archive);
    for (mz_uint index = 0; index < count; ++index) {
        mz_zip_archive_file_stat stat = {};
        if (!mz_zip_reader_file_stat(&archive, index, &stat)) continue;
        NSString *rawName = [NSString stringWithUTF8String:stat.m_filename];
        NSString *safeName = rawName != nil ? rawName : @"";
        NSString *name = safeName.lastPathComponent.uppercaseString;
        if ([name isEqualToString:@"AVATAR.EXE"]) avatar = YES;
        if ([name isEqualToString:@"TITLE.EXE"]) title = YES;
        if ([name isEqualToString:@"CHARSET.EGA"]) charset = YES;
    }
    BOOL valid = avatar && title && charset && mz_zip_validate_archive(&archive, 0);
    mz_zip_reader_end(&archive);
    return valid;
}

static NSString *Zu4SHA256(NSURL *url)
{
    NSInputStream *stream = [NSInputStream inputStreamWithURL:url];
    if (!stream) return nil;
    CC_SHA256_CTX context;
    CC_SHA256_Init(&context);
    [stream open];
    uint8_t buffer[32 * 1024];
    for (;;) {
        NSInteger count = [stream read:buffer maxLength:sizeof(buffer)];
        if (count < 0) { [stream close]; return nil; }
        if (count == 0) break;
        CC_SHA256_Update(&context, buffer, (CC_LONG)count);
    }
    [stream close];
    unsigned char digest[CC_SHA256_DIGEST_LENGTH];
    CC_SHA256_Final(digest, &context);
    NSMutableString *result = [NSMutableString stringWithCapacity:CC_SHA256_DIGEST_LENGTH * 2];
    for (NSUInteger i = 0; i < CC_SHA256_DIGEST_LENGTH; ++i) [result appendFormat:@"%02x", digest[i]];
    return result;
}

static NSURL *Zu4GameDataDirectory(void)
{
    NSFileManager *manager = NSFileManager.defaultManager;
    NSURL *support = [manager URLForDirectory:NSApplicationSupportDirectory
                                     inDomain:NSUserDomainMask
                            appropriateForURL:nil
                                       create:YES
                                        error:nil];
    return [support URLByAppendingPathComponent:@"UltimatumU4/GameData" isDirectory:YES];
}

static NSURL *Zu4CloudGameDataDirectory(void)
{
    return [[Zu4GameDataDirectory() URLByAppendingPathComponent:@"Cloud" isDirectory:YES]
            URLByAppendingPathComponent:@"current" isDirectory:YES];
}

static BOOL Zu4UseCloudDirectory(void)
{
    NSURL *directory = Zu4CloudGameDataDirectory();
    NSFileManager *manager = NSFileManager.defaultManager;
    BOOL directoryFlag = NO;
    if (![manager fileExistsAtPath:directory.path isDirectory:&directoryFlag] || !directoryFlag) return NO;
    for (NSString *name in @[@".ultimatum-cloud-data", @"AVATAR.EXE", @"TITLE.EXE", @"CHARSET.EGA"])
        if (![manager fileExistsAtPath:[directory URLByAppendingPathComponent:name].path]) return NO;
    zu4_set_game_data_path(directory.fileSystemRepresentation);
    return YES;
}

static NSDictionary *Zu4Entry(NSString *name, NSData *data)
{
    return @{ @"name": name.uppercaseString,
              @"data": [data base64EncodedStringWithOptions:0] };
}

static NSArray *Zu4EntriesFromDirectory(NSURL *directory)
{
    NSMutableArray *entries = [NSMutableArray array];
    NSArray *urls = [NSFileManager.defaultManager contentsOfDirectoryAtURL:directory
        includingPropertiesForKeys:@[NSURLIsRegularFileKey, NSURLFileSizeKey] options:0 error:nil];
    NSUInteger total = 0;
    for (NSURL *url in urls) {
        if ([url.lastPathComponent hasPrefix:@"."]) continue;
        NSNumber *regular = nil, *size = nil;
        [url getResourceValue:&regular forKey:NSURLIsRegularFileKey error:nil];
        [url getResourceValue:&size forKey:NSURLFileSizeKey error:nil];
        if (!regular.boolValue || size.unsignedIntegerValue == 0 || size.unsignedIntegerValue > 2 * 1024 * 1024) continue;
        NSData *data = [NSData dataWithContentsOfURL:url options:NSDataReadingMappedIfSafe error:nil];
        if (!data || total + data.length > 32 * 1024 * 1024) return nil;
        total += data.length; [entries addObject:Zu4Entry(url.lastPathComponent, data)];
    }
    return entries;
}

static NSArray *Zu4EntriesFromArchive(NSURL *url)
{
    if (![Zu4SHA256(url) isEqualToString:Zu4GameDataSHA256]) return nil;
    mz_zip_archive archive = {};
    if (!mz_zip_reader_init_file(&archive, url.fileSystemRepresentation, 0)) return nil;
    NSMutableArray *entries = [NSMutableArray array]; NSUInteger total = 0;
    for (mz_uint index = 0; index < mz_zip_reader_get_num_files(&archive); ++index) {
        mz_zip_archive_file_stat stat = {};
        if (!mz_zip_reader_file_stat(&archive, index, &stat) || stat.m_is_directory || !stat.m_uncomp_size || stat.m_uncomp_size > 2 * 1024 * 1024) continue;
        NSString *raw = [NSString stringWithUTF8String:stat.m_filename]; NSString *name = raw.lastPathComponent.uppercaseString;
        if (!name.length || [name hasSuffix:@".SAV"]) continue;
        size_t size = 0; void *bytes = mz_zip_reader_extract_to_heap(&archive, index, &size, 0);
        if (!bytes || total + size > 32 * 1024 * 1024) { if (bytes) mz_free(bytes); [entries removeAllObjects]; break; }
        NSData *data = [NSData dataWithBytes:bytes length:size]; mz_free(bytes); total += size;
        [entries addObject:Zu4Entry(name, data)];
    }
    mz_zip_reader_end(&archive); return entries.count ? entries : nil;
}

char *zu4_ios_game_data_entries_json(void)
{
    @autoreleasepool {
        NSArray *files = Zu4UseCloudDirectory() ? Zu4EntriesFromDirectory(Zu4CloudGameDataDirectory()) :
            Zu4EntriesFromArchive([Zu4GameDataDirectory() URLByAppendingPathComponent:@"ultima4.zip"]);
        if (!files.count) return NULL;
        NSData *json = [NSJSONSerialization dataWithJSONObject:@{@"available":@YES,@"files":files} options:0 error:nil];
        if (!json) return NULL; char *result = (char *)malloc(json.length + 1); if (!result) return NULL;
        memcpy(result, json.bytes, json.length); result[json.length] = '\0'; return result;
    }
}

static void Zu4SetInstallError(char *buffer, size_t size, NSString *message)
{
    if (!buffer || !size) return; snprintf(buffer, size, "%s", message.UTF8String ?: "Game data could not be installed.");
}

int zu4_ios_install_game_data_package(const char *json, char *errorBuffer, size_t errorSize)
{
    @autoreleasepool {
        if (!json || strlen(json) > 16 * 1024 * 1024) { Zu4SetInstallError(errorBuffer,errorSize,@"Invalid cloud game data."); return 0; }
        NSData *input = [NSData dataWithBytes:json length:strlen(json)];
        NSDictionary *package = [NSJSONSerialization JSONObjectWithData:input options:0 error:nil]; NSArray *files = package[@"files"];
        if (![package[@"format"] isEqual:@"ultimatum-game-data"] || ![package[@"version"] isEqual:@1] || ![package[@"game"] isEqual:@"ultima4"] || ![files isKindOfClass:NSArray.class] || files.count != 103) {
            Zu4SetInstallError(errorBuffer,errorSize,@"Unsupported cloud game-data package."); return 0;
        }
        NSFileManager *manager = NSFileManager.defaultManager; NSURL *current = Zu4CloudGameDataDirectory(); NSURL *root = current.URLByDeletingLastPathComponent;
        NSURL *stage = [root URLByAppendingPathComponent:[[NSUUID UUID].UUIDString stringByAppendingString:@".install"] isDirectory:YES];
        NSError *error = nil; if (![manager createDirectoryAtURL:stage withIntermediateDirectories:YES attributes:nil error:&error]) { Zu4SetInstallError(errorBuffer,errorSize,error.localizedDescription); return 0; }
        NSMutableSet *names = [NSMutableSet set]; NSUInteger total = 0; BOOL required[3] = {NO,NO,NO};
        for (NSDictionary *file in files) {
            NSString *name = file[@"name"], *encoded = file[@"data"], *declaredHash = file[@"sha256"]; NSNumber *declaredSize = file[@"size"];
            NSCharacterSet *safe = [NSCharacterSet characterSetWithCharactersInString:@"ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._-"];
            if (![name isKindOfClass:NSString.class] || !name.length || name.length>64 || ![name isEqual:name.lastPathComponent] || ![name isEqual:name.uppercaseString] || [name rangeOfCharacterFromSet:safe.invertedSet].location!=NSNotFound || [names containsObject:name] || ![encoded isKindOfClass:NSString.class] || ![declaredHash isKindOfClass:NSString.class] || ![declaredSize isKindOfClass:NSNumber.class]) { error=[NSError errorWithDomain:@"UltimatumU4.GameData" code:10 userInfo:@{NSLocalizedDescriptionKey:@"A cloud game-data file is invalid."}]; break; }
            NSData *data = [[NSData alloc] initWithBase64EncodedString:encoded options:0];
            if (!data.length || data.length!=declaredSize.unsignedIntegerValue || data.length>2*1024*1024 || total+data.length>32*1024*1024) { error=[NSError errorWithDomain:@"UltimatumU4.GameData" code:11 userInfo:@{NSLocalizedDescriptionKey:@"A cloud game-data file has an invalid size."}]; break; }
            unsigned char digest[CC_SHA256_DIGEST_LENGTH]; CC_SHA256(data.bytes,(CC_LONG)data.length,digest); NSMutableString *hash=[NSMutableString stringWithCapacity:64]; for(NSUInteger i=0;i<sizeof(digest);++i)[hash appendFormat:@"%02x",digest[i]];
            if (![hash isEqual:declaredHash]) { error=[NSError errorWithDomain:@"UltimatumU4.GameData" code:12 userInfo:@{NSLocalizedDescriptionKey:@"A cloud game-data file failed its integrity check."}]; break; }
            if (![data writeToURL:[stage URLByAppendingPathComponent:name] options:NSDataWritingAtomic error:&error]) break;
            [names addObject:name]; total+=data.length; if([name isEqual:@"AVATAR.EXE"])required[0]=YES;if([name isEqual:@"TITLE.EXE"])required[1]=YES;if([name isEqual:@"CHARSET.EGA"])required[2]=YES;
        }
        if (!error && !(required[0]&&required[1]&&required[2])) error=[NSError errorWithDomain:@"UltimatumU4.GameData" code:13 userInfo:@{NSLocalizedDescriptionKey:@"The cloud game data is incomplete."}];
        if (!error) [@"validated" writeToURL:[stage URLByAppendingPathComponent:@".ultimatum-cloud-data"] atomically:YES encoding:NSUTF8StringEncoding error:&error];
        NSURL *backup=[root URLByAppendingPathComponent:@"previous" isDirectory:YES];
        if (!error) { [manager removeItemAtURL:backup error:nil]; if ([manager fileExistsAtPath:current.path] && ![manager moveItemAtURL:current toURL:backup error:&error]) {} }
        if (!error && ![manager moveItemAtURL:stage toURL:current error:&error]) { if([manager fileExistsAtPath:backup.path])[manager moveItemAtURL:backup toURL:current error:nil]; }
        if (error) { [manager removeItemAtURL:stage error:nil]; Zu4SetInstallError(errorBuffer,errorSize,error.localizedDescription); return 0; }
        [manager removeItemAtURL:backup error:nil]; [current setResourceValue:@YES forKey:NSURLIsExcludedFromBackupKey error:nil]; return 1;
    }
}

static BOOL Zu4UseArchiveAtURL(NSURL *archiveURL)
{
    if (![Zu4SHA256(archiveURL) isEqualToString:Zu4GameDataSHA256] ||
        !Zu4ArchiveContainsRequiredFiles(archiveURL)) return NO;
    zu4_set_game_data_path(archiveURL.URLByDeletingLastPathComponent.fileSystemRepresentation);
    return YES;
}

@interface Zu4GameDataViewController : UIViewController
@property(nonatomic, strong) UILabel *titleLabel;
@property(nonatomic, strong) UILabel *bodyLabel;
@property(nonatomic, strong) UILabel *statusLabel;
@property(nonatomic, strong) UIButton *downloadButton;
@property(nonatomic, strong) UIButton *sourceButton;
@property(nonatomic, strong) UIProgressView *progressView;
@property(nonatomic, copy) void (^completion)(void);
@property(nonatomic, strong) NSURLSessionDownloadTask *task;
@end

@implementation Zu4GameDataViewController

- (void)viewDidLoad
{
    [super viewDidLoad];
    self.view.backgroundColor = [UIColor colorWithRed:0.035 green:0.045 blue:0.075 alpha:1.0];

    self.titleLabel = [[UILabel alloc] init];
    self.titleLabel.text = @"Ultimatum U4";
    self.titleLabel.textColor = [UIColor colorWithRed:0.95 green:0.78 blue:0.31 alpha:1.0];
    self.titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleLargeTitle];
    self.titleLabel.adjustsFontForContentSizeCategory = YES;
    self.titleLabel.textAlignment = NSTextAlignmentCenter;

    self.bodyLabel = [[UILabel alloc] init];
    self.bodyLabel.text = @"The open-source game engine is installed. To play, download the original Ultima IV DOS game files separately from an authorized Ultima Dragons mirror.\n\nThe 517 KB archive is stored only on this device and verified before use.";
    self.bodyLabel.textColor = UIColor.whiteColor;
    self.bodyLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    self.bodyLabel.adjustsFontForContentSizeCategory = YES;
    self.bodyLabel.numberOfLines = 0;
    self.bodyLabel.textAlignment = NSTextAlignmentCenter;

    self.statusLabel = [[UILabel alloc] init];
    self.statusLabel.textColor = [UIColor colorWithWhite:0.78 alpha:1.0];
    self.statusLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    self.statusLabel.adjustsFontForContentSizeCategory = YES;
    self.statusLabel.numberOfLines = 0;
    self.statusLabel.textAlignment = NSTextAlignmentCenter;

    self.progressView = [[UIProgressView alloc] initWithProgressViewStyle:UIProgressViewStyleDefault];
    self.progressView.hidden = YES;

    self.downloadButton = [UIButton buttonWithType:UIButtonTypeSystem];
    [self.downloadButton setTitle:@"Download Game Files" forState:UIControlStateNormal];
    self.downloadButton.titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
    self.downloadButton.titleLabel.adjustsFontForContentSizeCategory = YES;
    [self.downloadButton setTitleColor:UIColor.blackColor forState:UIControlStateNormal];
    self.downloadButton.backgroundColor = [UIColor colorWithRed:0.95 green:0.78 blue:0.31 alpha:1.0];
    self.downloadButton.layer.cornerRadius = 12.0;
    self.downloadButton.accessibilityHint = @"Downloads and verifies the original DOS data, then starts the game.";
    [self.downloadButton addTarget:self action:@selector(download:) forControlEvents:UIControlEventTouchUpInside];

    self.sourceButton = [UIButton buttonWithType:UIButtonTypeSystem];
    [self.sourceButton setTitle:@"About the source and rights" forState:UIControlStateNormal];
    self.sourceButton.titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    self.sourceButton.titleLabel.adjustsFontForContentSizeCategory = YES;
    [self.sourceButton addTarget:self action:@selector(openSourceInformation:) forControlEvents:UIControlEventTouchUpInside];

    for (UIView *view in @[self.titleLabel, self.bodyLabel, self.statusLabel, self.progressView,
                           self.downloadButton, self.sourceButton]) {
        view.translatesAutoresizingMaskIntoConstraints = NO;
        [self.view addSubview:view];
    }

    UILayoutGuide *safe = self.view.safeAreaLayoutGuide;
    NSLayoutConstraint *verticalPosition = [self.titleLabel.centerYAnchor
        constraintEqualToAnchor:safe.centerYAnchor constant:-170];
    verticalPosition.priority = UILayoutPriorityDefaultHigh;
    [NSLayoutConstraint activateConstraints:@[
        [self.titleLabel.leadingAnchor constraintGreaterThanOrEqualToAnchor:safe.leadingAnchor constant:24],
        [self.titleLabel.trailingAnchor constraintLessThanOrEqualToAnchor:safe.trailingAnchor constant:-24],
        [self.titleLabel.centerXAnchor constraintEqualToAnchor:safe.centerXAnchor],
        [self.bodyLabel.topAnchor constraintEqualToAnchor:self.titleLabel.bottomAnchor constant:24],
        [self.bodyLabel.leadingAnchor constraintEqualToAnchor:safe.leadingAnchor constant:32],
        [self.bodyLabel.trailingAnchor constraintEqualToAnchor:safe.trailingAnchor constant:-32],
        [self.progressView.topAnchor constraintEqualToAnchor:self.bodyLabel.bottomAnchor constant:28],
        [self.progressView.leadingAnchor constraintEqualToAnchor:self.downloadButton.leadingAnchor],
        [self.progressView.trailingAnchor constraintEqualToAnchor:self.downloadButton.trailingAnchor],
        [self.statusLabel.topAnchor constraintEqualToAnchor:self.progressView.bottomAnchor constant:12],
        [self.statusLabel.leadingAnchor constraintEqualToAnchor:safe.leadingAnchor constant:32],
        [self.statusLabel.trailingAnchor constraintEqualToAnchor:safe.trailingAnchor constant:-32],
        [self.downloadButton.topAnchor constraintEqualToAnchor:self.statusLabel.bottomAnchor constant:20],
        [self.downloadButton.centerXAnchor constraintEqualToAnchor:safe.centerXAnchor],
        [self.downloadButton.widthAnchor constraintGreaterThanOrEqualToConstant:230],
        [self.downloadButton.heightAnchor constraintGreaterThanOrEqualToConstant:52],
        [self.sourceButton.topAnchor constraintEqualToAnchor:self.downloadButton.bottomAnchor constant:16],
        [self.sourceButton.centerXAnchor constraintEqualToAnchor:safe.centerXAnchor],
        [self.sourceButton.heightAnchor constraintGreaterThanOrEqualToConstant:44],
        [self.sourceButton.bottomAnchor constraintLessThanOrEqualToAnchor:safe.bottomAnchor constant:-16],
        [self.titleLabel.topAnchor constraintGreaterThanOrEqualToAnchor:safe.topAnchor constant:20],
        verticalPosition
    ]];
}

- (void)openSourceInformation:(id)sender
{
    NSURL *url = [NSURL URLWithString:Zu4GameDataInfoURL];
    if (url) [UIApplication.sharedApplication openURL:url options:@{} completionHandler:nil];
}

- (void)download:(id)sender
{
    if (self.task) return;
    self.downloadButton.enabled = NO;
    self.downloadButton.alpha = 0.55;
    self.progressView.hidden = NO;
    self.progressView.progress = 0.12;
    self.statusLabel.text = @"Downloading…";
    NSURL *url = [NSURL URLWithString:Zu4GameDataURL];
    __weak Zu4GameDataViewController *weakSelf = self;
    self.task = [NSURLSession.sharedSession downloadTaskWithURL:url completionHandler:
        ^(NSURL *temporaryURL, NSURLResponse *response, NSError *downloadError) {
        Zu4GameDataViewController *strongSelf = weakSelf;
        if (!strongSelf) return;
        NSError *error = downloadError;
        NSHTTPURLResponse *http = [response isKindOfClass:NSHTTPURLResponse.class] ? (id)response : nil;
        if (!error && http.statusCode != 200) {
            error = [NSError errorWithDomain:@"UltimatumU4.GameData" code:http.statusCode
                                    userInfo:@{NSLocalizedDescriptionKey: @"The download server returned an unexpected response."}];
        }
        NSURL *directory = Zu4GameDataDirectory();
        NSURL *destination = [directory URLByAppendingPathComponent:@"ultima4.zip"];
        NSFileManager *manager = NSFileManager.defaultManager;
        if (!error && ![manager createDirectoryAtURL:directory withIntermediateDirectories:YES attributes:nil error:&error]) {}
        if (!error && ![Zu4SHA256(temporaryURL) isEqualToString:Zu4GameDataSHA256]) {
            error = [NSError errorWithDomain:@"UltimatumU4.GameData" code:2
                                    userInfo:@{NSLocalizedDescriptionKey: @"The downloaded file did not match the trusted archive."}];
        }
        if (!error && !Zu4ArchiveContainsRequiredFiles(temporaryURL)) {
            error = [NSError errorWithDomain:@"UltimatumU4.GameData" code:3
                                    userInfo:@{NSLocalizedDescriptionKey: @"The archive is incomplete or damaged."}];
        }
        if (!error) {
            [manager removeItemAtURL:destination error:nil];
            if (![manager moveItemAtURL:temporaryURL toURL:destination error:&error]) {}
        }
        if (!error) [destination setResourceValue:@YES forKey:NSURLIsExcludedFromBackupKey error:nil];

        dispatch_async(dispatch_get_main_queue(), ^{
            strongSelf.task = nil;
            if (error || !Zu4UseArchiveAtURL(destination)) {
                strongSelf.progressView.hidden = YES;
                strongSelf.statusLabel.text = error.localizedDescription ?: @"The game files could not be verified. Please try again.";
                strongSelf.downloadButton.enabled = YES;
                strongSelf.downloadButton.alpha = 1.0;
                UIAccessibilityPostNotification(UIAccessibilityAnnouncementNotification, strongSelf.statusLabel.text);
                return;
            }
            strongSelf.progressView.progress = 1.0;
            strongSelf.statusLabel.text = @"Verified. Starting Ultimatum U4…";
            UIAccessibilityPostNotification(UIAccessibilityAnnouncementNotification, strongSelf.statusLabel.text);
            if (strongSelf.completion) strongSelf.completion();
        });
    }];
    [self.task resume];
}

@end

int zu4_ios_prepare_game_data(void)
{
    if (Zu4UseCloudDirectory()) return 1;
    NSURL *directory = Zu4GameDataDirectory();
    NSURL *archive = [directory URLByAppendingPathComponent:@"ultima4.zip"];
    if (Zu4UseArchiveAtURL(archive)) return 1;

    [NSFileManager.defaultManager removeItemAtURL:archive error:nil];
    __block BOOL finished = NO;
    Zu4GameDataViewController *controller = [[Zu4GameDataViewController alloc] init];
    UIWindow *window = [[UIWindow alloc] initWithFrame:UIScreen.mainScreen.bounds];
    window.windowLevel = UIWindowLevelNormal + 2.0;
    window.rootViewController = controller;
    [window makeKeyAndVisible];
    controller.completion = ^{
        finished = YES;
    };

    if ([NSProcessInfo.processInfo.environment[@"ZU4_IOS_AUTO_DOWNLOAD"] boolValue]) {
        [controller download:nil];
    }
    while (!finished) {
        @autoreleasepool {
            CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.1, true);
        }
    }
    window.hidden = YES;
    window.rootViewController = nil;
    return 1;
}
