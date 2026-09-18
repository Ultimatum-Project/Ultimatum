#import <Foundation/Foundation.h>
#include "adventure_package.h"
#include "save_store_contract.h"
#include "save_slots.h"
#include "save_validation.h"
#include "topicjournal.h"
#include "journal_notebook.h"
#include "mobile_map_pins.h"
#include "mobile_map_discoveries.h"
#include "mobile_dungeon_exploration.h"
#include <zlib.h>
#include <cerrno>
#include <cmath>
#include <algorithm>

namespace AdventurePackage {
namespace {
const size_t maxBytes = 8 * 1024 * 1024;
const std::vector<std::string> names = {"party.sav","monsters.sav","outmonst.sav","dngmap.sav","topics.txt","journal-notebook.dat","explored-map.dat","map-pins.dat","map-discoveries.dat","explored-dungeons.dat","conversations.json"};
NSString *const cloudLinksKey=@"UltimatumCloudAdventureLinks-v2";
bool fail(std::string &error, const char *message) { error=message; return false; }
NSString *string(const std::string &text) { return [[NSString alloc] initWithBytes:text.data() length:text.size() encoding:NSUTF8StringEncoding]; }
std::string utf8(NSString *text) { const char *bytes=text.UTF8String;return bytes ? std::string(bytes,[text lengthOfBytesUsingEncoding:NSUTF8StringEncoding]) : ""; }
bool integer(id value, double minimum, double maximum) {
    return [value isKindOfClass:NSNumber.class] && CFGetTypeID((__bridge CFTypeRef)value)!=CFBooleanGetTypeID() &&
        std::isfinite([value doubleValue]) && floor([value doubleValue])==[value doubleValue] && [value doubleValue]>=minimum && [value doubleValue]<=maximum;
}
NSString *checksum(NSData *data) { return [NSString stringWithFormat:@"%08x",(unsigned)crc32(0,(const Bytef *)data.bytes,(uInt)data.length)]; }
bool limits(const Files &files, std::string &error) {
    size_t total=0;
    for(const auto &file:files) {
        if(std::find(names.begin(),names.end(),file.first)==names.end() || file.second.empty() || file.second.size()>maxBytes-total)
            return fail(error,"The backup contains unknown, empty, or oversized adventure files.");
        total+=file.second.size();
    }
    return files.count("party.sav") && files.count("monsters.sav") ? true : fail(error,"The backup needs party.sav and monsters.sav.");
}
bool readFile(const std::string &path, std::vector<unsigned char> &bytes, bool &present) {
    struct stat info; present=false;
    if(lstat(path.c_str(),&info)!=0) return errno==ENOENT;
    present=true;
    if(!S_ISREG(info.st_mode) || info.st_size<=0 || info.st_size>(off_t)maxBytes) return false;
    int fd=open(path.c_str(),O_RDONLY | O_NOFOLLOW);if(fd<0)return false;
    bytes.resize((size_t)info.st_size);size_t offset=0;bool ok=true;
    while(offset<bytes.size()) {
        ssize_t count=read(fd,bytes.data()+offset,bytes.size()-offset);
        if(count<0 && errno==EINTR)continue;
        if(count<=0){ok=false;break;}offset+=(size_t)count;
    }
    unsigned char extra; if(ok)ok=read(fd,&extra,1)==0;
    if(close(fd)!=0)ok=false;return ok;
}
bool writeFiles(const std::string &directory, const Files &files) {
    for(const auto &file:files) {
        int fd=open((directory+file.first).c_str(),O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW,0600);if(fd<0)return false;
        size_t offset=0;bool ok=true;
        while(offset<file.second.size()) {
            ssize_t count=write(fd,file.second.data()+offset,file.second.size()-offset);
            if(count<0 && errno==EINTR)continue;
            if(count<=0){ok=false;break;}offset+=(size_t)count;
        }
        if(close(fd)!=0)ok=false;if(!ok)return false;
    }
    return true;
}
void discard(const std::string &directory, const Files &files) {
    for(const auto &file:files)unlink((directory+file.first).c_str());rmdir(directory.c_str());
}
bool directoryValid(const std::string &directory) {
    if(!SaveValidation::checkpointFiles(directory))return false;
    SaveGame save={};FILE *party=fopen((directory+"party.sav").c_str(),"rb");
    bool valid=party&&saveGameRead(&save,party);if(party&&fclose(party)!=0)valid=false;
    // These values otherwise reach tile assertions or the moon initialization
    // loop before the player can return to a menu.
    if(!valid || !((save.transport>=0x10&&save.transport<=0x15)||save.transport==0x18||save.transport==0x1f) ||
       save.trammelphase>7 || save.feluccaphase>7 || save.orientation>3)return false;
    for(unsigned i=0;i<save.members;++i)if(save.players[i].status!=STAT_GOOD&&save.players[i].status!=STAT_POISONED&&save.players[i].status!=STAT_SLEEPING&&save.players[i].status!=STAT_DEAD)return false;
    TopicJournal journal;struct stat info;
    if(stat((directory+"topics.txt").c_str(),&info)==0) { if(!journal.load(directory+"topics.txt"))return false; }
    else if(errno!=ENOENT)return false;
    JournalNotebook notebook;
    if(!notebook.load(directory+"journal-notebook.dat") || !notebook.validFor(journal) ||
       !MobileMapPins().load(directory+"map-pins.dat",256,256) || !MobileMapDiscoveries().load(directory+"map-discoveries.dat",256,256))return false;
    std::vector<MobileDungeonExploration::Shape> shapes;for(int map=17;map<=24;++map)shapes.push_back({map,8,8,8});
    if(!MobileDungeonExploration().load(directory+"explored-dungeons.dat",shapes))return false;
    FILE *explored=fopen((directory+"explored-map.dat").c_str(),"rb");
    if(!explored)return errno==ENOENT;
    char header[64]={};bool ok=fgets(header,sizeof(header),explored) && std::string(header)=="ZU4-EXPLORED-MAP-1\n";
    for(int i=0;ok && i<256*256;++i){int cell=fgetc(explored);ok=cell==0 || cell==1;}
    ok=ok && fgetc(explored)==EOF && !ferror(explored);if(fclose(explored)!=0)ok=false;return ok;
}
std::vector<std::string> required(const Files &files) { std::vector<std::string> out;for(const auto &file:files)out.push_back(file.first);return out; }
}
bool validate(const Files &files, std::string &error) {
    if(!limits(files,error))return false;
    std::string pattern=std::string(NSTemporaryDirectory().fileSystemRepresentation)+"ultimatum-validate-XXXXXX";
    std::vector<char> buffer(pattern.begin(),pattern.end());buffer.push_back(0);
    char *temporary=mkdtemp(buffer.data());if(!temporary)return fail(error,"Could not stage the adventure for validation.");
    const std::string directory=std::string(temporary)+"/";
    bool ok=writeFiles(directory,files) && directoryValid(directory);discard(directory,files);
    return ok ? true : fail(error,"The adventure is damaged or contains incompatible journal/map data. Your slots are unchanged.");
}
bool readCheckpoint(const std::string &directory, Bundle &bundle, std::string &error) {
    Bundle loaded;
    for(const auto &name:names) {
        std::vector<unsigned char> bytes;bool present;
        if(!readFile(directory+name,bytes,present))return fail(error,"The checkpoint contains an unreadable adventure file.");
        if(present)loaded.files.emplace(name,std::move(bytes));
    }
    if(!limits(loaded.files,error) || !directoryValid(directory))return fail(error,"This checkpoint could not be validated. Use save recovery before exporting it.");
    SaveGame save={};FILE *party=fopen((directory+"party.sav").c_str(),"rb");
    bool ok=party && saveGameRead(&save,party);if(party && fclose(party)!=0)ok=false;
    if(!ok)return fail(error,"The checkpoint party could not be read.");
    loaded.label=save.players[0].name;
    struct stat info;if(stat((directory+"party.sav").c_str(),&info)==0)loaded.savedAt=(double)info.st_mtime*1000;
    bundle=std::move(loaded);return true;
}
bool encode(const Bundle &bundle, std::string &json, std::string &error) {
    @autoreleasepool {
        if(!validate(bundle.files,error))return false;
        NSMutableArray *files=[NSMutableArray array];
        for(const auto &name:names) {
            auto found=bundle.files.find(name);if(found==bundle.files.end())continue;
            NSData *data=[NSData dataWithBytes:found->second.data() length:found->second.size()];
            [files addObject:@{@"name":string(name),@"size":@(data.length),@"crc32":checksum(data),@"data":[data base64EncodedStringWithOptions:0]}];
        }
        NSData *encoded=[NSJSONSerialization dataWithJSONObject:@{@"format":@"ultimatum-adventure",@"version":@1,@"game":@"ultima4",@"engine":@"xu4",@"label":string(bundle.label) ?: @"Adventure",@"savedAt":@(std::isfinite(bundle.savedAt) ? bundle.savedAt : 0),@"files":files} options:0 error:nil];
        if(!encoded || encoded.length>maxBytes*2)return fail(error,"The adventure package could not be encoded.");
        json.assign((const char *)encoded.bytes,encoded.length);return true;
    }
}
bool decode(const std::string &json, Bundle &bundle, std::string &error) {
    @autoreleasepool {
        if(json.empty() || json.size()>maxBytes*2)return fail(error,"Choose a .u4save backup of at most 16 MB.");
        id object=[NSJSONSerialization JSONObjectWithData:[NSData dataWithBytes:json.data() length:json.size()] options:0 error:nil];
        if(![object isKindOfClass:NSDictionary.class])return fail(error,"That file is not an Ultimatum adventure backup.");
        NSDictionary *package=object;
        if(![package[@"format"] isEqual:@"ultimatum-adventure"] || !integer(package[@"version"],1,1) || ![package[@"game"] isEqual:@"ultima4"] || ![package[@"engine"] isEqual:@"xu4"] || ![package[@"files"] isKindOfClass:NSArray.class])return fail(error,"Unsupported adventure format, version, game, or engine.");
        Bundle loaded;size_t total=0;
        if([package[@"files"] count]>names.size())return fail(error,"The backup contains too many adventure files.");
        for(id value in package[@"files"]) {
            if(![value isKindOfClass:NSDictionary.class])return fail(error,"Invalid adventure file record.");
            NSDictionary *file=value;
            if(![file[@"name"] isKindOfClass:NSString.class] || ![file[@"data"] isKindOfClass:NSString.class] || ![file[@"crc32"] isKindOfClass:NSString.class] || !integer(file[@"size"],1,maxBytes-total))return fail(error,"Invalid or oversized adventure file record.");
            std::string name=utf8(file[@"name"]);
            if(std::find(names.begin(),names.end(),name)==names.end() || loaded.files.count(name))return fail(error,"The backup contains unknown paths or duplicate files.");
            NSData *bytes=[[NSData alloc] initWithBase64EncodedString:file[@"data"] options:0];
            if(!bytes || bytes.length!=[file[@"size"] unsignedLongLongValue] || ![checksum(bytes) isEqual:file[@"crc32"]])return fail(error,"An adventure file failed its size or checksum check.");
            total+=bytes.length;const auto *begin=(const unsigned char *)bytes.bytes;
            loaded.files.emplace(name,std::vector<unsigned char>(begin,begin+bytes.length));
        }
        if([package[@"label"] isKindOfClass:NSString.class])loaded.label=utf8(package[@"label"]);
        if(loaded.label.size()>160)loaded.label="Imported adventure";
        if(integer(package[@"savedAt"],0,9007199254740991.0))loaded.savedAt=[package[@"savedAt"] doubleValue];
        if(!validate(loaded.files,error))return false;bundle=std::move(loaded);return true;
    }
}
bool install(const std::string &base, int slot, const Bundle &bundle, const std::string &expectedCurrent, std::string &error) {
    if(!SaveSlots::valid(slot) || !validate(bundle.files,error))return fail(error,"The selected slot or adventure is invalid. No slot was replaced.");
    const std::string root=SaveSlots::snapshotRoot(base,slot);
    if(SaveSnapshot::current(root)!=expectedCurrent)return fail(error,"The selected slot changed. Review it again before importing.");
    const std::string generation=SaveSnapshot::begin(root);if(generation.empty())return fail(error,"A new checkpoint could not be created. Your existing adventure is kept.");
    if(!writeFiles(generation,bundle.files) || !directoryValid(generation)) {discard(generation,bundle.files);return fail(error,"The imported checkpoint could not be written and validated.");}
    // Slot 1's pre-snapshot save must acquire a recovery generation before
    // replacing it. Never overwrite/remove the original legacy files.
    if(slot==1 && expectedCurrent.empty() && access((root+"/CURRENT").c_str(),F_OK)!=0 && SaveSlots::occupied(base,1)) {
        Bundle old;
        if(readCheckpoint(base,old,error)) {
            const std::string recovery=SaveSnapshot::begin(root);
            if(recovery.empty() || !writeFiles(recovery,old.files) || !SaveStoreContract::publish(root,recovery,required(old.files),SaveStoreContract::inspect(root).currentGenerationId))return fail(error,"The legacy adventure's recovery copy could not be secured. Import was not published.");
        } else return fail(error,"The legacy adventure needs recovery before it can be replaced safely.");
    }
    if(!SaveStoreContract::publish(root,generation,required(bundle.files),expectedCurrent))
        return fail(error,"Checkpoint publication did not finish. Keep the app open and review the slot; all checkpoint files were kept.");
    // Do not prune on import: retain previous/recovery evidence, including
    // generations behind a damaged old pointer. Ordinary successful saves
    // continue to apply the existing retention policy.
    return true;
}
bool copyWebTranscript(const std::string &source, const std::string &destination) {
    if(source==destination)return true;
    std::vector<unsigned char> bytes;bool present;
    if(!readFile(source+"conversations.json",bytes,present))return false;
    return !present || writeFiles(destination,{{"conversations.json",std::move(bytes)}});
}
bool cloudSlots(const std::string &base, int activeSlot, std::string &json, std::string &error) {
    @autoreleasepool {
        NSDictionary *links=[NSUserDefaults.standardUserDefaults dictionaryForKey:cloudLinksKey] ?: @{};
        NSMutableArray *slots=[NSMutableArray array];
        for(int slot=1;slot<=SaveSlots::SLOT_COUNT;++slot) {
            const std::string root=SaveSlots::snapshotRoot(base,slot),current=SaveSnapshot::current(root);
            NSMutableDictionary *record=[@{@"slot":@(slot),@"label":@"Empty",@"active":@(slot==activeSlot),
              @"fingerprint":current.empty() ? (id)NSNull.null : string(current)} mutableCopy];
            std::string directory=current;
            if(directory.empty() && access((root+"/CURRENT").c_str(),F_OK)!=0)directory=SaveSlots::fallbackDirectory(base,slot);
            Bundle bundle;std::string text,localError;
            if(!directory.empty() && readCheckpoint(directory,bundle,localError) && encode(bundle,text,localError)) {
                record[@"text"]=string(text);record[@"label"]=string(bundle.label) ?: @"Adventure";
                if(record[@"fingerprint"]==NSNull.null)record[@"fingerprint"]=@"";
                FILE *file=tmpfile();SaveGame save={};
                if(file) {const auto &bytes=bundle.files.at("party.sav");fwrite(bytes.data(),1,bytes.size(),file);rewind(file);
                  if(saveGameRead(&save,file))record[@"summary"]=[NSString stringWithFormat:@"%u moves",save.moves];fclose(file);}
            } else if(SaveSlots::occupied(base,slot))record[@"label"]=@"Recovery needed";
            NSDictionary *cloud=links[[NSString stringWithFormat:@"%d",slot]];
            if([cloud isKindOfClass:NSDictionary.class])record[@"cloud"]=cloud;
            [slots addObject:record];
        }
        NSData *data=[NSJSONSerialization dataWithJSONObject:slots options:0 error:nil];
        if(!data)return fail(error,"Saved slots could not be prepared for Accounts.");
        json.assign((const char *)data.bytes,data.length);return true;
    }
}
bool installCloudResponse(const std::string &base, int activeSlot, const std::string &json, std::string &error) {
    @autoreleasepool {
        if(json.empty() || json.size()>maxBytes*4)return fail(error,"Invalid cloud response.");
        id object=[NSJSONSerialization JSONObjectWithData:[NSData dataWithBytes:json.data() length:json.size()] options:0 error:nil];
        if(![object isKindOfClass:NSDictionary.class] || !integer(object[@"slot"],1,SaveSlots::SLOT_COUNT) ||
           ![object[@"text"] isKindOfClass:NSString.class] ||
           !(object[@"expected"]==NSNull.null || [object[@"expected"] isKindOfClass:NSString.class]))return fail(error,"Invalid cloud destination.");
        int slot=[object[@"slot"] intValue];
        if(slot==activeSlot)return fail(error,"Return to the title before replacing the adventure you are playing.");
        std::string expected=object[@"expected"]==NSNull.null ? "" : utf8(object[@"expected"]);
        NSDictionary *cloud=[object[@"cloud"] isKindOfClass:NSDictionary.class] ? object[@"cloud"] : nil;
        if(cloud && (! [cloud[@"resourceId"] isKindOfClass:NSString.class] || ![cloud[@"revisionId"] isKindOfClass:NSString.class] ||
           ![cloud[@"fingerprint"] isKindOfClass:NSString.class] || ![cloud[@"label"] isKindOfClass:NSString.class]))
            return fail(error,"Invalid cloud link.");
        Bundle bundle;if(!decode(utf8(object[@"text"]),bundle,error) || !install(base,slot,bundle,expected,error))return false;
        if(cloud) {
            NSMutableDictionary *links=[[NSUserDefaults.standardUserDefaults dictionaryForKey:cloudLinksKey] mutableCopy] ?: [NSMutableDictionary dictionary];
            links[[NSString stringWithFormat:@"%d",slot]]=cloud;
            [NSUserDefaults.standardUserDefaults setObject:links forKey:cloudLinksKey];
        }
        return true;
    }
}

void clearCloudLink(int slot) {
    @autoreleasepool {
        NSMutableDictionary *links=[[NSUserDefaults.standardUserDefaults dictionaryForKey:cloudLinksKey] mutableCopy];
        if(!links)return;[links removeObjectForKey:[NSString stringWithFormat:@"%d",slot]];
        [NSUserDefaults.standardUserDefaults setObject:links forKey:cloudLinksKey];
    }
}

}
