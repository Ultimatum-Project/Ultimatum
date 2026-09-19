const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const test = require("node:test");
const vm = require("node:vm");
const {pathToFileURL} = require("node:url");

const repo = path.resolve(__dirname, "../../..");
const web = path.resolve(__dirname, "..");

test("the Ultima IV port descriptor passes the executable adapter contract", async () => {
  const manifest = JSON.parse(fs.readFileSync(path.join(repo,"ports/ultima-iv/port.manifest.json"),"utf8"));
  const validator = await import(pathToFileURL(path.join(repo,"packages/adapter-sdk/src/validate-port-descriptor.mjs")));
  assert.equal(validator.validatePortDescriptor(manifest),manifest);
  assert.equal(manifest.gameId,"ultima4");
  assert.equal(manifest.portId,"xu4");
  assert.equal(manifest.saveSchemaVersion,1);
  assert.equal(manifest.capabilities.saves,"managed");
  assert.ok(fs.existsSync(path.join(repo,manifest.settingsManifest)));
  assert.ok(fs.existsSync(path.join(repo,manifest.controlsManifest)));
  assert.ok(fs.existsSync(path.join(repo,manifest.diagnosticsManifest)));
});

test("the Ultima IV catalog entry passes the shared catalog contract", async () => {
  const catalog=JSON.parse(fs.readFileSync(path.join(repo,"ports/ultima-iv/catalog-entry.json"),"utf8"));
  const port=JSON.parse(fs.readFileSync(path.join(repo,"ports/ultima-iv/port.manifest.json"),"utf8"));
  const validator=await import(pathToFileURL(path.join(repo,"packages/catalog/src/validate-catalog-entry.mjs")));
  assert.equal(validator.validateCatalogEntry(catalog,{portDescriptor:port}),catalog);
  assert.equal(catalog.slug,"ultima-iv");
  assert.equal(catalog.editions[0].acquisition.mode,"bring-your-own-data");
  assert.deepEqual(catalog.artwork,[],"unreviewed artwork is not invented by the catalog");
  assert.throws(()=>validator.validateCatalogEntry({...catalog,gameId:"different"},{portDescriptor:port}),/disagrees/);
  assert.throws(()=>validator.validateCatalogEntry({...catalog,port:{...catalog.port,descriptor:"../outside.json"}}),/safe repository path/);
});

test("Ultima IV settings are typed, scoped, profile-complete and descriptor-versioned",async()=>{
  const port=JSON.parse(fs.readFileSync(path.join(repo,"ports/ultima-iv/port.manifest.json"),"utf8"));
  const manifest=JSON.parse(fs.readFileSync(path.join(repo,"ports/ultima-iv/settings.manifest.json"),"utf8"));
  const validator=await import(pathToFileURL(path.join(repo,"packages/settings/src/validate-settings-manifest.mjs")));
  assert.equal(validator.validateSettingsManifest(manifest,{portDescriptor:port}),manifest);
  assert.equal(manifest.profiles.find(profile=>profile.recommended).id,"ultimatum");
  assert.deepEqual(manifest.profiles.find(profile=>profile.id==="classic").values,{
    "presentation.filter-movement-messages":false,"graphics.theme":"ega","navigation.exploration-map":false,"navigation.player-map-pins":false,
  });
  for(const setting of manifest.settings.filter(setting=>setting.scope==="device-host"))assert.equal(setting.portable,false,`${setting.id} cannot enter portable saves`);
  assert.throws(()=>validator.validateSettingsManifest({...manifest,settings:manifest.settings.map(setting=>setting.id==="audio.music-volume"?{...setting,default:11}:setting)},{portDescriptor:port}),/default is invalid/);
  assert.throws(()=>validator.validateSettingsManifest({...manifest,settings:manifest.settings.map(setting=>setting.id==="controls.handedness"?{...setting,portable:true}:setting)},{portDescriptor:port}),/must not be portable/);
});

test("Ultima IV action descriptors and verified desktop/touch profiles conform",async()=>{
  const port=JSON.parse(fs.readFileSync(path.join(repo,"ports/ultima-iv/port.manifest.json"),"utf8"));
  const manifest=JSON.parse(fs.readFileSync(path.join(repo,"ports/ultima-iv/controls.manifest.json"),"utf8"));
  const validator=await import(pathToFileURL(path.join(repo,"packages/input-system/src/validate-control-manifest.mjs")));
  assert.equal(validator.validateControlManifest(manifest,{portDescriptor:port}),manifest);
  assert.deepEqual(manifest.profiles.map(profile=>profile.device).sort(),["keyboard","touch"]);
  assert.ok(!manifest.profiles.some(profile=>profile.device==="controller"),"unimplemented controller support is not advertised");
  const desktop=manifest.profiles.find(profile=>profile.id==="desktop-standard");
  assert.ok(desktop.bindings.some(binding=>binding.actionId==="action.search"&&binding.input.id==="KeyS"&&binding.input.modifiers?.includes("Shift")));
  const duplicate={...manifest,profiles:manifest.profiles.map(profile=>profile.id!=="desktop-standard"?profile:{...profile,bindings:[...profile.bindings,{actionId:"action.wait",input:{kind:"key",id:"Space"}}]})};
  assert.throws(()=>validator.validateControlManifest(duplicate,{portDescriptor:port}),/duplicate physical binding/);
});

test("browser registries project engine settings and expose control profiles without migration",async()=>{
  const settingsManifest=JSON.parse(fs.readFileSync(path.join(repo,"ports/ultima-iv/settings.manifest.json"),"utf8"));
  const controlsManifest=JSON.parse(fs.readFileSync(path.join(repo,"ports/ultima-iv/controls.manifest.json"),"utf8"));
  const {SettingsRegistry}=require(path.join(repo,"packages/settings/src/settings-registry.js"));
  const {ControlRegistry}=require(path.join(repo,"packages/input-system/src/control-registry.js"));
  const response=value=>async function(){assert.equal(this,globalThis,"browser host methods stay bound to their global object");return{ok:true,json:async()=>value};};
  const settings=new SettingsRegistry({manifestUrl:"settings.manifest.json",fetch:response(settingsManifest)});
  const projected=await settings.project({preferences:{profile:"assisted",filterMovementMessages:true,bumpInteractions:false,directInteractions:true,tapToWalk:false},video:"ega",capabilities:{explorationMap:true,mapPins:false},audio:{musicVolume:6,effectsVolume:4}},"web");
  assert.equal(projected.values["experience.profile"],"assisted");
  assert.equal(projected.values["graphics.theme"],"ega");
  assert.equal(projected.values["audio.music-volume"],6);
  assert.ok(!projected.unavailable.length);
  assert.equal(projected.migrationPerformed,false);
  const controls=await new ControlRegistry({manifestUrl:"controls.manifest.json",fetch:response(controlsManifest)}).inspect("web");
  assert.deepEqual(controls.profiles.map(profile=>profile.id).sort(),["desktop-standard","touch-standard"]);
  assert.equal(controls.selectionPersistence,"library-device-host");
  assert.equal(controls.migrationPerformed,false);
});

test("structured diagnostics validate port codes and redact support snapshots by construction",async()=>{
  const port=JSON.parse(fs.readFileSync(path.join(repo,"ports/ultima-iv/port.manifest.json"),"utf8"));
  const manifest=JSON.parse(fs.readFileSync(path.join(repo,"ports/ultima-iv/diagnostics.manifest.json"),"utf8"));
  const validator=await import(pathToFileURL(path.join(repo,"packages/diagnostics/src/validate-diagnostics-manifest.mjs")));
  assert.equal(validator.validateDiagnosticsManifest(manifest,{portDescriptor:port}),manifest);
  assert.throws(()=>validator.validateDiagnosticsManifest({...manifest,events:[...manifest.events,manifest.events[0]]},{portDescriptor:port}),/duplicate/);
  const {DiagnosticsCollector}=require(path.join(repo,"packages/diagnostics/src/diagnostics.js"));
  let tick=0;
  const collector=new DiagnosticsCollector({product:{id:"ultimatum-web",version:"test"},port:{gameId:"ultima4",portId:"xu4",version:"0.1.0",engineId:"xu4",engineVersion:"test"},maxEvents:2,sessionId:"test-session",now:()=>`2026-09-18T00:00:0${tick++}Z`});
  collector.record({component:"session",code:"session.state-change",details:{state:"loading",path:"C:/Users/name/save",message:"private text"}});
  collector.record({component:"session",code:"session.state-change",details:{state:"running",input:"ArrowUp"}});
  collector.record({component:"session",severity:"warning",code:"session.recoverable-error",details:{phase:"flushing",token:"secret"}});
  const bundle=collector.bundle({host:{kind:"web",online:true},save:{name:"Avatar"},notes:"typed words are unsafe"});
  assert.equal(bundle.events.length,2,"diagnostic history is bounded");
  assert.equal(bundle.privacy.localOnly,true);
  assert.equal(bundle.privacy.sensitiveArtifactsIncluded,false);
  assert.deepEqual(bundle.sections.host,{kind:"web",online:true});
  assert.ok(!Object.hasOwn(bundle.sections,"save"));
  assert.equal(bundle.sections.notes,"[redacted]");
  assert.ok(bundle.events.every(event=>!Object.hasOwn(event.details,"input")&&!Object.hasOwn(event.details,"token")&&!Object.hasOwn(event.details,"path")&&!Object.hasOwn(event.details,"message")));
  assert.throws(()=>collector.record({component:"session",code:"invalid code"}),/code is invalid/);
});

test("web and native save providers declare the shared SaveStore v1 semantics",async()=>{
  const manifest=JSON.parse(fs.readFileSync(path.join(repo,"ports/ultima-iv/save-store.manifest.json"),"utf8"));
  const validator=await import(pathToFileURL(path.join(repo,"packages/save-store/src/validate-save-store-provider.mjs")));
  assert.equal(validator.validateSaveStoreProvider(manifest),manifest);
  assert.deepEqual(manifest.providers.map(provider=>provider.host).sort(),["native","web"]);
  assert.equal(manifest.portableFormats[0].format,"ultimatum-adventure");
  assert.equal(manifest.portableFormats[0].version,1);
});

test("web and native session providers pass the shared EngineSession declaration",async()=>{
  const manifestPath=path.join(repo,"ports/ultima-iv/engine-session.manifest.json");
  const manifest=JSON.parse(fs.readFileSync(manifestPath,"utf8"));
  const validator=await import(pathToFileURL(path.join(repo,"packages/semantic-session/src/validate-engine-session-provider.mjs")));
  assert.equal(validator.validateEngineSessionProvider(manifest),manifest);
  assert.deepEqual(manifest.providers.map(provider=>provider.host).sort(),["native","web"]);
  assert.throws(()=>validator.validateEngineSessionProvider({...manifest,providers:manifest.providers.map(provider=>
    provider.host==="native" ? {...provider,operations:provider.operations.filter(operation=>operation!=="quiesce")} : provider
  )}),/missing quiesce/);
  assert.throws(()=>validator.validateEngineSessionProvider({...manifest,providers:manifest.providers.map(provider=>
    provider.host==="web" ? {...provider,implementation:"../outside.js"} : provider
  )}),/unsafe implementation path/);
  for(const provider of manifest.providers) {
    assert.ok(fs.existsSync(path.join(repo,provider.implementation)),`${provider.id} implementation exists`);
    assert.ok(fs.existsSync(path.join(repo,provider.activeWiring)),`${provider.id} active wiring exists`);
  }
});

test("snapshot v1 validation is additive but rejects a broken envelope", async () => {
  const validator = await import(pathToFileURL(path.join(repo,"packages/semantic-session/src/validate-engine-snapshot.mjs")));
  const snapshot={contractVersion:1,ready:true,inputMode:"command",capabilities:{menu:true},prompt:{kind:"command",id:4},futureField:true};
  assert.equal(validator.validateEngineSnapshotV1(snapshot),snapshot);
  assert.throws(()=>validator.validateEngineSnapshotV1({...snapshot,contractVersion:2}),/unsupported contractVersion/);
  assert.throws(()=>validator.validateEngineSnapshotV1({...snapshot,prompt:null}),/prompt is required/);
});

test("the compatibility EngineSession serializes and rejects stale intents", async () => {
  const calls=[];
  const client={
    snapshot:()=>({contractVersion:1,ready:true,inputMode:"text",capabilities:{promptInput:true},prompt:{kind:"text",id:7}}),
    submitAnswer:(text,promptId)=>{calls.push([text,promptId]);return true;},
    startPolling(){}, stopPolling(){}, requestCheckpoint:()=>true, persistSaves:()=>Promise.resolve(), start(){},
  };
  const sandbox={window:{},console};
  vm.runInNewContext(fs.readFileSync(path.join(web,"dist/engine-session.js"),"utf8"),sandbox);
  const session=new sandbox.window.UltimatumEngineSession(client,{sessionId:"test-session"});
  assert.equal(session.getSnapshot().inputMode,"loading","pre-start snapshots remain valid and do not enter the engine");
  session.start();
  assert.equal(session.requestCheckpoint(),true,"legacy checkpoint callers keep their synchronous boolean result");
  const accepted=await session.dispatch({intentId:"answer-1",sequence:1,kind:"prompt.answer",parameters:{text:"yes",promptId:7},expectedPromptGeneration:7});
  assert.equal(accepted.accepted,true);
  assert.deepEqual(calls,[["yes",7]]);
  await assert.rejects(()=>session.dispatch({intentId:"answer-1",sequence:2,kind:"prompt.answer",parameters:{text:"no",promptId:7}}),/duplicate intentId/);
  await assert.rejects(()=>session.dispatch({intentId:"answer-2",sequence:1,kind:"prompt.answer",parameters:{text:"no",promptId:7}}),/stale sequence/);
  await assert.rejects(()=>session.dispatch({intentId:"answer-3",sequence:3,kind:"prompt.answer",parameters:{text:"no",promptId:7},expectedPromptGeneration:8}),/prompt generation changed/);
});

test("the session orchestrator does not await xu4's long-lived Asyncify handoff",async()=>{
  let polling=false;
  const never=new Promise(()=>{});
  const client={snapshot:()=>({contractVersion:1,ready:true,inputMode:"command",capabilities:{},prompt:{kind:"command",id:0}}),start:()=>never,startPolling:()=>{polling=true;},stopPolling(){},persistSaves:async()=>{},requestCheckpoint:()=>true};
  const sandbox={window:{},console,Promise,Date,Math,Set,Proxy,Reflect,Error,TypeError,Boolean,Number,String};
  vm.runInNewContext(fs.readFileSync(path.join(web,"dist/engine-session.js"),"utf8"),sandbox);
  const session=new sandbox.window.UltimatumEngineSession(client,{sessionId:"asyncify-test"});
  vm.runInNewContext(fs.readFileSync(path.join(repo,"packages/session-orchestrator/src/session-orchestrator.js"),"utf8"),sandbox);
  const orchestrator=new sandbox.window.UltimatumSessionOrchestrator({session});
  await Promise.race([orchestrator.start({listener:()=>{}}),new Promise((_,reject)=>setTimeout(()=>reject(Error("start waited for main-loop exit")),50))]);
  assert.equal(polling,true);
  assert.equal(orchestrator.state,"running");
  assert.equal(session.getSnapshot().ready,true);
});

test("the shared session orchestrator owns launch, lifecycle, intent, flush and cleanup ordering",async()=>{
  const calls=[];
  const snapshot={contractVersion:1,ready:true,inputMode:"command",capabilities:{},prompt:{kind:"command",id:0}};
  const session={
    sessionId:"session-test",start:async options=>calls.push(["start",options.restoreSave]),getSnapshot:()=>snapshot,
    subscribe:listener=>{calls.push(["subscribe"]);listener(snapshot);return()=>calls.push(["unsubscribe"]);},
    dispatch:async envelope=>{calls.push(["dispatch",envelope.intentId]);return{accepted:true};},
    requestCheckpoint:async reason=>{calls.push(["checkpoint",reason]);return{published:true};},
    pause:async reason=>calls.push(["pause",reason]),resume:async reason=>calls.push(["resume",reason]),
    quiesce:async reason=>{calls.push(["flush",reason]);return{flushed:true};},shutdown:async reason=>calls.push(["shutdown",reason]),
  };
  const lease={supported:true,acquire:async()=>{calls.push(["lease-acquire"]);return true;},release:async()=>calls.push(["lease-release"])};
  const sandbox={window:{},console,Date,Error,TypeError,Set,Promise};
  vm.runInNewContext(fs.readFileSync(path.join(repo,"packages/session-orchestrator/src/session-orchestrator.js"),"utf8"),sandbox);
  const Orchestrator=sandbox.window.UltimatumSessionOrchestrator;
  const orchestrator=new Orchestrator({session,lease,preflight:async()=>calls.push(["preflight"]),mount:async()=>calls.push(["mount"]),checkpoint:async reason=>calls.push(["publish",reason])});
  const states=[];orchestrator.onProgress(event=>states.push(event.state));
  await orchestrator.start({sessionOptions:{restoreSave:true},listener:()=>{}});
  assert.deepEqual(states.slice(0,4),["preflighting","mounting","loading","running"]);
  assert.deepEqual(calls.slice(0,5).map(call=>call[0]),["preflight","lease-acquire","mount","start","subscribe"]);
  await orchestrator.dispatch({intentId:"move-1",sequence:1,kind:"input.key",parameters:{key:32}});
  await orchestrator.requestCheckpoint("periodic");
  await orchestrator.suspend("hidden");
  assert.equal(orchestrator.state,"paused");
  assert.deepEqual(states.slice(-4),["paused","quiescing","flushing","paused"]);
  await assert.rejects(()=>orchestrator.dispatch({intentId:"move-2",sequence:2,kind:"input.key",parameters:{key:32}}),/cannot dispatch from paused/);
  await orchestrator.resume("visible");
  await orchestrator.shutdown("title");
  assert.equal(orchestrator.state,"stopped");
  assert.deepEqual(calls.slice(-5).map(call=>call[0]),["pause","publish","unsubscribe","shutdown","lease-release"]);
  assert.equal(orchestrator.diagnostics().leaseAcquired,false);
});

test("the shared session orchestrator reports a recoverable error when another tab owns the lease",async()=>{
  const session={sessionId:"blocked",start:async()=>{},getSnapshot:()=>({}),subscribe:()=>()=>{},dispatch:async()=>{},requestCheckpoint:async()=>{},pause:async()=>{},resume:async()=>{},quiesce:async()=>({flushed:true}),shutdown:async()=>{}};
  const sandbox={window:{},console,Date,Error,TypeError,Set,Promise};
  vm.runInNewContext(fs.readFileSync(path.join(repo,"packages/session-orchestrator/src/session-orchestrator.js"),"utf8"),sandbox);
  const orchestrator=new sandbox.window.UltimatumSessionOrchestrator({session,lease:{supported:true,acquire:async()=>false,release:async()=>{}}});
  await assert.rejects(()=>orchestrator.start(),error=>error.name==="SessionLeaseUnavailableError");
  assert.equal(orchestrator.state,"recoverable-error");
  assert.match(orchestrator.diagnostics().lastError.message,/another tab/);
});

test("the active web shell installs the compatibility session before app startup",()=>{
  const html=fs.readFileSync(path.join(web,"dist/index.html"),"utf8");
  const app=fs.readFileSync(path.join(web,"dist/app.js"),"utf8");
  assert.ok(html.indexOf("engine-client.js") < html.indexOf("engine-session.js"));
  assert.ok(html.indexOf("engine-session.js") < html.indexOf("session-orchestrator.js"));
  assert.ok(html.indexOf("session-orchestrator.js") < html.indexOf("app.js"));
  assert.match(app,/new window\.UltimatumEngineSession\(new window\.UltimatumEngineClient\(runtimeModule\)\)/);
  assert.match(app,/new window\.UltimatumSessionOrchestrator/);
  assert.match(app,/sessionOrchestrator\.start\(/);
  assert.match(fs.readFileSync(path.join(web,"dist/adventure-ui.js"),"utf8"),/await this\.hooks\.start/);
  assert.match(app,/sessionOrchestrator\.suspend\(reason\)/);
  assert.match(app,/sessionOrchestrator\.resume\(reason\)/);
  assert.match(app,/sessionOrchestrator\.shutdown\(reason\)/);
});

test("declared session providers expose their required operations and active wiring",()=>{
  const manifest=JSON.parse(fs.readFileSync(path.join(repo,"ports/ultima-iv/engine-session.manifest.json"),"utf8"));
  const webProvider=manifest.providers.find(provider=>provider.host==="web");
  const nativeProvider=manifest.providers.find(provider=>provider.host==="native");
  const webImplementation=fs.readFileSync(path.join(repo,webProvider.implementation),"utf8");
  const nativeImplementation=fs.readFileSync(path.join(repo,nativeProvider.implementation),"utf8");
  const nativeWiring=fs.readFileSync(path.join(repo,nativeProvider.activeWiring),"utf8");
  for(const operation of webProvider.operations)
    assert.match(webImplementation,new RegExp(`\\b${operation}\\s*\\(`),`web exposes ${operation}`);
  for(const operation of nativeProvider.operations)
    assert.match(nativeImplementation,new RegExp(`Transition\\s+${operation}\\s*\\(`),`native exposes ${operation}`);
  assert.match(nativeWiring,/#include "native_engine_session\.h"/);
  assert.match(nativeWiring,/nativeEngineSession\.handle\(/);
  assert.match(nativeWiring,/nativeEngineSession\.blocksInput\(/);
  assert.match(nativeWiring,/nativeEngineSession\.shutdown\(/);
});

test("the browser library provider preserves every existing physical identifier",()=>{
  const sandbox={window:{indexedDB:{open(){throw Error("not used");}}},console};
  vm.runInNewContext(fs.readFileSync(path.join(web,"dist/library-store.js"),"utf8"),sandbox);
  const Store=sandbox.window.UltimatumIndexedDbLibraryStore;
  const store=new Store(sandbox.window.indexedDB);
  assert.equal(store.id,"browser-indexeddb-library-v1");
  assert.deepEqual(
    JSON.parse(JSON.stringify(store.physicalLocation("source-data"))),
    {providerId:"browser-indexeddb-library-v1",database:"ultimatum-local-library-v1",version:1,objectStore:"assets",key:"game",engineMount:"/ultima4"},
  );
  assert.equal(store.physicalLocation("optional-overlay").key,"vga");
  assert.throws(()=>store.physicalLocation("unknown"),/Unknown library storage role/);
});

test("IndexedDB compatibility and experimental OPFS providers satisfy one storage contract",async()=>{
  const {assertStorageProvider}=require(path.join(repo,"packages/storage/src/storage-provider-contract.js"));
  const {OpfsStorageProvider}=require(path.join(repo,"packages/storage/src/opfs-storage-provider.js"));
  const libraryModule=require(path.join(web,"dist/library-store.js"));
  assert.equal(assertStorageProvider(new libraryModule.IndexedDbLibraryStore({open(){throw Error("not used");}})).id,"browser-indexeddb-library-v1");

  const missing=()=>Object.assign(Error("Not found"),{name:"NotFoundError"});
  class FileHandle {
    constructor(manager,path){this.kind="file";this.manager=manager;this.path=path;this.data=new Uint8Array();}
    async getFile(){const data=this.data.slice();return{size:data.byteLength,lastModified:1,arrayBuffer:async()=>data.buffer};}
    async createWritable(){let pending;return{write:async value=>{if(this.manager.failPath===this.path)throw Error("injected OPFS failure");pending=new Uint8Array(value).slice();},close:async()=>{this.data=pending;},abort:async()=>{}};}
  }
  class DirectoryHandle {
    constructor(manager,path=""){this.kind="directory";this.manager=manager;this.path=path;this.children=new Map();}
    async getDirectoryHandle(name,{create=false}={}){let value=this.children.get(name);if(!value&&create){value=new DirectoryHandle(this.manager,[this.path,name].filter(Boolean).join("/"));this.children.set(name,value);}if(value?.kind!=="directory")throw missing();return value;}
    async getFileHandle(name,{create=false}={}){let value=this.children.get(name);if(!value&&create){value=new FileHandle(this.manager,[this.path,name].filter(Boolean).join("/"));this.children.set(name,value);}if(value?.kind!=="file")throw missing();return value;}
    async removeEntry(name){if(!this.children.delete(name))throw missing();}
    async *entries(){yield* this.children.entries();}
  }
  const manager={failPath:null,root:null,async getDirectory(){return this.root ||= new DirectoryHandle(this);},async estimate(){return{usage:12,quota:34};}};
  const provider=assertStorageProvider(new OpfsStorageProvider({storageManager:manager}));
  assert.equal(provider.id,"browser-opfs-experimental-v1");
  assert.equal(provider.capabilities.experimental,true);
  assert.equal(provider.capabilities.atomicTransactions,false,"experimental OPFS does not overclaim crash atomicity");
  const tx=await provider.beginTransaction("games/ultima4");
  tx.write("games/ultima4/install.txt","first").write("games/ultima4/profile.bin",new Uint8Array([1,2,3]));
  await tx.commit();
  assert.equal(new TextDecoder().decode(await provider.read("games/ultima4/install.txt")),"first");
  assert.equal((await provider.stat("games/ultima4/profile.bin")).size,3);
  const listed=[];for await(const entry of provider.list("games/ultima4"))listed.push(entry.path);
  assert.deepEqual(listed.sort(),["games/ultima4/install.txt","games/ultima4/profile.bin"]);
  assert.deepEqual(await provider.estimate(),{usage:12,quota:34});
  assert.throws(()=>OpfsStorageProvider.asPath("../escape"),/Invalid storage reference/);

  const failing=await provider.beginTransaction("games/ultima4");
  failing.write("games/ultima4/install.txt","replacement").write("games/ultima4/fail.bin",new Uint8Array([9]));
  manager.failPath="ultimatum-experimental-v1/games/ultima4/fail.bin";
  await assert.rejects(()=>failing.commit(),/injected OPFS failure/);
  manager.failPath=null;
  assert.equal(new TextDecoder().decode(await provider.read("games/ultima4/install.txt")),"first","failed publication restores the previous bytes");
  assert.equal(await provider.stat("games/ultima4/fail.bin"),null);
});

test("the browser provider projects existing installs into local-library v1 without migration",async()=>{
  const libraryModule=require(path.join(web,"dist/library-store.js"));
  const validator=await import(pathToFileURL(path.join(repo,"packages/library/src/validate-library-record.mjs")));
  const missing=libraryModule.projectUltimaIVRecord(undefined,"browser-indexeddb-library-v1");
  assert.equal(validator.validateLibraryRecord(missing),missing);
  assert.equal(missing.state,"not-installed");
  assert.equal(missing.installation,null);

  const installed=libraryModule.projectUltimaIVRecord({
    kind:"files",version:1,game:"ultima4",profile:"u4-dos-english-ega-1.01-v1",
    files:[{name:"AVATAR.EXE",data:new Uint8Array(4).buffer},{name:"TITLE.EXE",data:new Uint8Array(6).buffer}],
  },"browser-indexeddb-library-v1");
  assert.equal(validator.validateLibraryRecord(installed),installed);
  assert.equal(installed.state,"playable");
  assert.equal(installed.installation.logicalBytes,10);
  assert.equal(installed.installation.importProfileId,"u4-dos-english-ega-1.01-v1");
  assert.equal(installed.compatibility.requiresMigration,false);

  const damaged=libraryModule.projectUltimaIVRecord({unexpected:true},"browser-indexeddb-library-v1");
  assert.equal(validator.validateLibraryRecord(damaged),damaged);
  assert.equal(damaged.state,"repair-required");
  assert.match(damaged.compatibility.reasons[0],/not a recognized/);
});

test("the shared installation orchestrator publishes data and its durable record atomically",async()=>{
  const {InstallationOrchestrator}=require(path.join(repo,"packages/import-framework/src/installation-orchestrator.js"));
  const libraryValidator=await import(pathToFileURL(path.join(repo,"packages/library/src/validate-library-record.mjs")));
  const prepared={
    inventory:[{id:"entry-1"}],match:{confidence:"exact"},verified:{kind:"files",files:[]},
    plan:{contractVersion:1,adapterId:"fixture-adapter",gameId:"fixture-game",portId:"fixture-port",installId:"fixture-default",editionId:"fixture-edition",profileId:"fixture-profile-1.01",logicalBytes:42,destination:{role:"source-data",mount:"/fixture"},publication:"atomic-record-with-runtime-rollback",privacy:"local-only"},
  };
  const calls=[];
  const adapter={prepare:async()=>prepared,preparePackage:async()=>prepared};
  const library={
    id:"memory-library",recordSource:"memory",
    inspect:async()=>({record:{schemaVersion:1,gameId:"fixture-game",portId:"fixture-port",state:"not-installed",installation:null,activity:{lastPlayedAt:null,playtimeSeconds:7,favorite:true},selection:{engineChannel:"stable",controlProfileId:null},sync:{state:"local-only"},compatibility:{source:"memory",requiresMigration:false,reasons:[]}}}),
    publishInstall:async(data,record)=>calls.push(["publish",data,record]),
  };
  const orchestrator=new InstallationOrchestrator({adapter,library,clock:()=>"2026-09-18T20:00:00.000Z",stageRuntime:async()=>({commit:()=>calls.push(["commit"]),rollback:()=>calls.push(["rollback"])})});
  const phases=[];
  const result=await orchestrator.install({kind:"files"},{reportProgress:event=>phases.push(event.phase)});
  assert.equal(libraryValidator.validateLibraryRecord(result.record),result.record);
  assert.equal(result.record.installation.importProfileId,"fixture-profile-1.01");
  assert.equal(result.record.installation.logicalBytes,42);
  assert.equal(result.record.activity.favorite,true,"replacement preserves library preferences");
  assert.deepEqual(calls.map(call=>call[0]),["publish","commit"]);
  assert.deepEqual(phases,["preparing","staging","publishing","complete"]);
  assert.equal(orchestrator.active,false);
});

test("the shared installation orchestrator rolls runtime staging back on cancellation or publication failure",async()=>{
  const {InstallationOrchestrator}=require(path.join(repo,"packages/import-framework/src/installation-orchestrator.js"));
  const prepared={verified:{kind:"files",files:[]},plan:{contractVersion:1,adapterId:"fixture-adapter",gameId:"fixture-game",portId:"fixture-port",installId:"fixture-default",editionId:"fixture-edition",profileId:"fixture-profile",logicalBytes:0,destination:{role:"source-data",mount:"/fixture"},publication:"atomic-record-with-runtime-rollback",privacy:"local-only"}};
  const adapter={prepare:async()=>prepared,preparePackage:async()=>prepared};
  let rollbacks=0,commits=0,publishes=0;
  const library={id:"memory-library",recordSource:"memory",inspect:async()=>({record:null}),publishInstall:async()=>{publishes++;throw Error("durable publication failed");}};
  const stageRuntime=async()=>({rollback:()=>rollbacks++,commit:()=>commits++});
  const failed=new InstallationOrchestrator({adapter,library,stageRuntime});
  await assert.rejects(()=>failed.install({kind:"files"}),/publication failed/);
  assert.deepEqual({rollbacks,commits,publishes},{rollbacks:1,commits:0,publishes:1});

  const controller=new AbortController();
  library.publishInstall=async()=>{publishes++;};
  const cancelled=new InstallationOrchestrator({adapter,library,stageRuntime:async()=>{controller.abort();return stageRuntime();}});
  await assert.rejects(()=>cancelled.install({kind:"files"},{signal:controller.signal}),/abort|cancel/i);
  assert.deepEqual({rollbacks,commits,publishes},{rollbacks:2,commits:0,publishes:1},"cancelled staging never publishes");
});

test("the Ultima IV import adapter creates a bounded logical install plan",async()=>{
  const files=[{name:"AVATAR.EXE",data:new ArrayBuffer(4)}];
  const verified={kind:"files",version:1,game:"ultima4",profile:"u4-dos-english-ega-v1",files,verification:{label:"English DOS/EGA"}};
  const sandbox={window:{},console,DOMException};
  vm.runInNewContext(fs.readFileSync(path.join(web,"dist/import-adapter.js"),"utf8"),sandbox);
  const Adapter=sandbox.window.UltimaIVImportAdapter;
  const progress=[];
  const adapter=new Adapter({gameData:{
    select:entries=>entries.slice(0,1),
    validate:async entries=>{assert.equal(entries[0],files[0]);return verified;},
    decodePackage:async text=>{assert.equal(text,"package");return verified;},
  },extractZip:()=>files,maxZipBytes:128});
  const prepared=await adapter.prepare({kind:"files",files},{reportProgress:event=>progress.push(event)});
  assert.equal(prepared.inventory[0].id,"entry-1");
  assert.equal(prepared.match.confidence,"exact");
  assert.equal(prepared.plan.destination.role,"source-data");
  assert.equal(prepared.plan.destination.mount,"/ultima4");
  assert.equal(prepared.plan.portId,"xu4");
  assert.equal(prepared.plan.installId,"ultima4-default");
  assert.equal(prepared.plan.logicalBytes,4);
  assert.deepEqual(progress.map(event=>event.completed),[0,1]);
  assert.equal(adapter.select([...files,{name:"ignored"}]).length,1);
  const packaged=await adapter.preparePackage("package");
  assert.equal(packaged.plan.profileId,"u4-dos-english-ega-v1");
  const controller=new AbortController();controller.abort();
  await assert.rejects(()=>adapter.prepare({kind:"files",files},{signal:controller.signal}),/abort|cancel/i);
});

test("the active web app uses compatibility import and library boundaries",()=>{
  const html=fs.readFileSync(path.join(web,"dist/index.html"),"utf8");
  const app=fs.readFileSync(path.join(web,"dist/app.js"),"utf8");
  assert.ok(html.indexOf("game-data.js") < html.indexOf("library-store.js"));
  assert.ok(html.indexOf("storage-provider-contract.js") < html.indexOf("opfs-storage-provider.js"));
  assert.ok(html.indexOf("opfs-storage-provider.js") < html.indexOf("library-store.js"));
  assert.ok(html.indexOf("library-store.js") < html.indexOf("import-adapter.js"));
  assert.ok(html.indexOf("import-adapter.js") < html.indexOf("installation-orchestrator.js"));
  assert.ok(html.indexOf("installation-orchestrator.js") < html.indexOf("app.js"));
  assert.match(app,/new window\.UltimatumIndexedDbLibraryStore\(\)/);
  assert.match(app,/UltimatumOpfsStorageProvider\?\.isSupported\(\)/);
  assert.match(app,/migrationPerformed:false/);
  assert.match(app,/new window\.UltimatumSettingsRegistry/);
  assert.match(app,/new window\.UltimatumControlRegistry/);
  assert.match(app,/ultimatumSettingsDiagnostics/);
  assert.match(app,/ultimatumControlDiagnostics/);
  assert.match(app,/new window\.UltimatumDiagnosticsCollector/);
  assert.match(app,/ultimatumDiagnostics/);
  assert.match(app,/new window\.UltimatumInstallationOrchestrator/);
  assert.match(app,/library\.inspect\("ultima4","xu4"\)/);
  assert.match(app,/new window\.UltimaIVImportAdapter/);
  assert.match(app,/installationOrchestrator\.installPackage\(text\)/);
  assert.doesNotMatch(app,/library\.put\("source-data"|indexedDB\.open\(|function libraryGet|function libraryPut/);
});
