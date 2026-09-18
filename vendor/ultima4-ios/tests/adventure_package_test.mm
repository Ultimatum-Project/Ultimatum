#import <Foundation/Foundation.h>
#include "adventure_package.h"
#include "topicjournal.h"
#include "journal_notebook.h"
#include "mobile_map_pins.h"
#include "mobile_map_discoveries.h"
#include "mobile_dungeon_exploration.h"
#include "save_slots.h"
#include "save_validation.h"
#include <cassert>
#include <fstream>
#include <zlib.h>
using namespace AdventurePackage;
static unsigned checks;
static void check(bool ok){assert(ok);++checks;}
static std::string temporary() { char path[]="/private/tmp/u4-package-test-XXXXXX";check(mkdtemp(path));return std::string(path)+"/"; }
static void writeWorld(const std::string &directory,bool dungeon) {
    SaveGame save={};save.transport=0x1f;save.members=2;save.moves=9471;save.gold=317;save.food=21834;
    save.x=dungeon ? 2 : 100;save.y=dungeon ? 3 : 100;save.dnglevel=dungeon ? 4 : 255;save.location=dungeon ? 17 : 0;
    for(int i=0;i<2;++i){snprintf(save.players[i].name,16,"%s",i ? "Iolo" : "PortableHero");save.players[i].hp=280;save.players[i].hpMax=300;save.players[i].status=STAT_GOOD;}
    save.mixtures[2]=3;save.reagents[REAG_GINSENG]=2;
    FILE *file=fopen((directory+"party.sav").c_str(),"wb");check(file && saveGameWrite(&save,file));check(fclose(file)==0);
    SaveGameMonsterRecord monsters[MONSTERTABLE_SIZE]={};monsters[0].tile=16;monsters[0].x=23;monsters[0].y=42;monsters[0].unused1=17;
    file=fopen((directory+"monsters.sav").c_str(),"wb");check(file && saveGameMonstersWrite(monsters,file));check(fclose(file)==0);
    if(dungeon){file=fopen((directory+"outmonst.sav").c_str(),"wb");check(file && saveGameMonstersWrite(monsters,file));check(fclose(file)==0);std::ofstream(directory+"dngmap.sav",std::ios::binary)<<std::string(512,'\x11');}
}
static Bundle fixture(bool dungeon) {
    const std::string directory=temporary();writeWorld(directory,dungeon);
    TopicJournal journal;journal.observe("Speak of the shrine and virtue.","Teacher in Britain","Teacher","Britain","person","job");check(journal.save(directory+"topics.txt"));
    JournalNotebook notebook;const std::string key=JournalNotebook::key(journal.history()[0]);check(notebook.toggleFavorite(key));check(notebook.setNote(0,key,"Attached \"clue\"\n日本語 café"));check(notebook.setNote(0,"","My personal theory"));check(notebook.save(directory+"journal-notebook.dat"));
    MobileMapPins pins;check(pins.set(23,42,"My pin",256,256));check(pins.save(directory+"map-pins.dat"));
    MobileMapDiscoveries discoveries;check(discoveries.discover(23,42,MobileMapDiscoveries::TOWN,"Britain",256,256));check(discoveries.save(directory+"map-discoveries.dat"));
    MobileDungeonExploration explored;check(explored.reveal(17,2,3,4,8,8,8));check(explored.save(directory+"explored-dungeons.dat"));
    std::string cells(65536,0);cells[42*256+23]=1;std::ofstream(directory+"explored-map.dat",std::ios::binary)<<"ZU4-EXPLORED-MAP-1\n"<<cells;
    std::ofstream(directory+"conversations.json")<<"[{\"text\":\"Earlier web reply\",\"speaker\":\"You\",\"location\":\"Britain\",\"session\":\"fixture\",\"moves\":9471}]";
    Bundle bundle;std::string error;check(readCheckpoint(directory,bundle,error));return bundle;
}
static std::string json(id object) { NSData *data=[NSJSONSerialization dataWithJSONObject:object options:0 error:nil];check(data);return std::string((const char *)data.bytes,data.length); }
static NSMutableDictionary *object(const std::string &text) { return [NSJSONSerialization JSONObjectWithData:[NSData dataWithBytes:text.data() length:text.size()] options:NSJSONReadingMutableContainers error:nil]; }
static void replace(NSMutableDictionary *package,NSString *name,NSData *data) {
    for(NSMutableDictionary *file in package[@"files"])if([file[@"name"] isEqual:name]){file[@"size"]=@(data.length);file[@"crc32"]=[NSString stringWithFormat:@"%08x",(unsigned)crc32(0,(const Bytef *)data.bytes,(uInt)data.length)];file[@"data"]=[data base64EncodedStringWithOptions:0];}
}
int main(int argc,char **argv) { @autoreleasepool {
    std::string error,text;
    if(argc==3 && (std::string(argv[1])=="--fixture" || std::string(argv[1])=="--world-fixture")) {auto bundle=fixture(std::string(argv[1])=="--fixture");check(encode(bundle,text,error));check([[NSData dataWithBytes:text.data() length:text.size()] writeToFile:[NSString stringWithUTF8String:argv[2]] atomically:YES]);return 0;}
    if(argc==4 && std::string(argv[1])=="--roundtrip") {
        NSData *data=[NSData dataWithContentsOfFile:[NSString stringWithUTF8String:argv[2]]];check(data);Bundle bundle;
        check(decode(std::string((const char *)data.bytes,data.length),bundle,error));const std::string base=temporary();check(install(base,2,bundle,"",error));
        Bundle installed;check(readCheckpoint(SaveSnapshot::current(SaveSlots::snapshotRoot(base,2)),installed,error));check(installed.files==bundle.files);check(encode(installed,text,error));
        check([[NSData dataWithBytes:text.data() length:text.size()] writeToFile:[NSString stringWithUTF8String:argv[3]] atomically:YES]);return 0;
    }
    const auto bundle=fixture(true);check(encode(bundle,text,error));Bundle decoded;check(decode(text,decoded,error));check(decoded.files==bundle.files);
    const auto original=object(text);
    for(int mutation=0;mutation<16;++mutation) {
        auto package=object(text);
        switch(mutation){
            case 0:package[@"version"]=@2;break;case 1:package[@"version"]=@YES;break;
            case 2:package[@"game"]=@"ultima3";break;case 3:package[@"engine"]=@"other";break;
            case 4:package[@"files"][0][@"crc32"]=@"00000000";break;
            case 5:package[@"files"][0][@"size"]=@1;break;
            case 6:package[@"files"][0][@"name"]=@"../party.sav";break;
            case 7:[package[@"files"] addObject:package[@"files"][0]];break;
            case 8:package[@"files"][0][@"data"]=@"invalid base64!";break;
            case 9:replace(package,@"journal-notebook.dat",[@"U4NOTEBOOK 1 1\nB \"unknown\"\n" dataUsingEncoding:NSUTF8StringEncoding]);break;
            case 10:replace(package,@"explored-map.dat",[NSMutableData dataWithLength:17]);break;
            case 11:package[@"files"][0][@"name"]=[@"party.sav" stringByAppendingFormat:@"%Cevil",(unichar)0];break;
            default:{
                const auto &bytes=bundle.files.at("party.sav");FILE *file=tmpfile();check(file);
                check(fwrite(bytes.data(),1,bytes.size(),file)==bytes.size());rewind(file);SaveGame save={};check(saveGameRead(&save,file));
                if(mutation==12)save.transport=0;
                if(mutation==13)save.trammelphase=8;
                if(mutation==14)save.orientation=4;
                if(mutation==15)save.players[0].status=(StatusType)'?';
                rewind(file);check(saveGameWrite(&save,file));rewind(file);std::vector<unsigned char> changed(bytes.size());check(fread(changed.data(),1,changed.size(),file)==changed.size());check(fclose(file)==0);
                replace(package,@"party.sav",[NSData dataWithBytes:changed.data() length:changed.size()]);break;
            }
        }
        Bundle unchanged=bundle;check(!decode(json(package),unchanged,error));check(unchanged.files==bundle.files);
    }
    check([original[@"files"] count]==11);
    auto missing=bundle.files;missing.erase("outmonst.sav");check(!validate(missing,error));missing=bundle.files;missing["dngmap.sav"].resize(511);check(!validate(missing,error));
    auto legacy=fixture(false);for(const auto &name:std::vector<std::string>{"topics.txt","journal-notebook.dat","explored-map.dat","map-pins.dat","map-discoveries.dat","explored-dungeons.dat","conversations.json"})legacy.files.erase(name);
    check(encode(legacy,text,error));check(decode(text,decoded,error));
    const std::string base=temporary(),root=SaveSlots::snapshotRoot(base,2);check(SaveSlots::select(base,3));
    check(install(base,2,bundle,"",error));const std::string current=SaveSnapshot::current(root);check(!current.empty());check(SaveSlots::active(base)==3);
    check(!install(base,2,legacy,"stale",error));check(SaveSnapshot::current(root)==current);
    check(install(base,2,legacy,current,error));check(SaveSnapshot::previous(root)==current);
    Bundle recovered;check(readCheckpoint(SaveSnapshot::previous(root),recovered,error));check(recovered.files==bundle.files);
    const std::string legacyBase=temporary();writeWorld(legacyBase,false);check(install(legacyBase,1,bundle,"",error));
    check(!SaveSnapshot::previous(SaveSlots::snapshotRoot(legacyBase,1)).empty());check(access((legacyBase+"party.sav").c_str(),F_OK)==0);
    const std::string transcriptCopy=temporary();check(copyWebTranscript(current,transcriptCopy));
    std::ifstream copied(transcriptCopy+"conversations.json");check(copied.good());
    const std::string linked=temporary();check(symlink((current+"party.sav").c_str(),(linked+"party.sav").c_str())==0);check(!readCheckpoint(linked,recovered,error));
    check(cloudSlots(base,2,text,error));
    NSArray *cloud=[NSJSONSerialization JSONObjectWithData:[NSData dataWithBytes:text.data() length:text.size()] options:0 error:nil];
    check(cloud.count==3);check([cloud[1][@"active"] boolValue]);check([cloud[1][@"text"] isKindOfClass:NSString.class]);
    check(cloud[0][@"fingerprint"]==NSNull.null);
    const std::string cloudBefore=SaveSnapshot::current(root);
    std::string portable;check(encode(bundle,portable,error));
    NSDictionary *response=@{@"slot":@2,@"expected":[NSString stringWithUTF8String:cloudBefore.c_str()],@"text":[NSString stringWithUTF8String:portable.c_str()]};
    check(!installCloudResponse(base,2,json(response),error));check(SaveSnapshot::current(root)==cloudBefore);
    check(!installCloudResponse(base,0,json(@{@"slot":@YES,@"expected":NSNull.null,@"text":@"{}"}),error));
    check(!installCloudResponse(base,0,json(@{@"slot":@2,@"expected":@"stale",@"text":response[@"text"]}),error));check(SaveSnapshot::current(root)==cloudBefore);
    check(installCloudResponse(base,0,json(response),error));check(SaveSnapshot::previous(root)==cloudBefore);
    check(readCheckpoint(SaveSnapshot::current(root),recovered,error));check(recovered.files==bundle.files);
    fprintf(stdout,"Adventure package: %u checks passed\n",checks);return 0;
} }
