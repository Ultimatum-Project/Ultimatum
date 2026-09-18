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
  assert.equal(session.requestCheckpoint(),true,"legacy checkpoint callers keep their synchronous boolean result");
  const accepted=await session.dispatch({intentId:"answer-1",sequence:1,kind:"prompt.answer",parameters:{text:"yes",promptId:7},expectedPromptGeneration:7});
  assert.equal(accepted.accepted,true);
  assert.deepEqual(calls,[["yes",7]]);
  await assert.rejects(()=>session.dispatch({intentId:"answer-1",sequence:2,kind:"prompt.answer",parameters:{text:"no",promptId:7}}),/duplicate intentId/);
  await assert.rejects(()=>session.dispatch({intentId:"answer-2",sequence:1,kind:"prompt.answer",parameters:{text:"no",promptId:7}}),/stale sequence/);
  await assert.rejects(()=>session.dispatch({intentId:"answer-3",sequence:3,kind:"prompt.answer",parameters:{text:"no",promptId:7},expectedPromptGeneration:8}),/prompt generation changed/);
});

test("the active web shell installs the compatibility session before app startup",()=>{
  const html=fs.readFileSync(path.join(web,"dist/index.html"),"utf8");
  const app=fs.readFileSync(path.join(web,"dist/app.js"),"utf8");
  assert.ok(html.indexOf("engine-client.js") < html.indexOf("engine-session.js"));
  assert.ok(html.indexOf("engine-session.js") < html.indexOf("app.js"));
  assert.match(app,/new window\.UltimatumEngineSession\(new window\.UltimatumEngineClient\(runtimeModule\)\)/);
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
  assert.ok(html.indexOf("library-store.js") < html.indexOf("import-adapter.js"));
  assert.ok(html.indexOf("import-adapter.js") < html.indexOf("app.js"));
  assert.match(app,/new window\.UltimatumIndexedDbLibraryStore\(\)/);
  assert.match(app,/new window\.UltimaIVImportAdapter/);
  assert.match(app,/gameDataImporter\.preparePackage\(text\)/);
  assert.doesNotMatch(app,/indexedDB\.open\(|function libraryGet|function libraryPut/);
});
