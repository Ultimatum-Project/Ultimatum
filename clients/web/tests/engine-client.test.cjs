const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const test = require("node:test");
const vm = require("node:vm");
const os = require("node:os");
const { execFileSync } = require("node:child_process");

const root = path.resolve(__dirname, "..");
const read = relative => fs.readFileSync(path.join(root, relative), "utf8");

test("HTML messages trim classic cursor padding but preserve paragraphs", () => {
  const articles = [];
  const mobileLines = [];
  const sandbox = {
    ui: {
      latestMessage: {textContent:""},
      desktopLatestMessage: {textContent:""},
      mobileWorldLog: {replaceChildren(...rows){mobileLines.splice(0,mobileLines.length,...rows.map(row=>row.textContent));}},
      messageLog: {replaceChildren(){articles.length=0;}, append(article){articles.push(article);}},
    },
    document: {createElement(){return {innerHTML:"",textContent:""};}},
  };
  const app = read("dist/app.js");
  const escape = app.slice(app.indexOf("function escapeHtml("), app.indexOf("function healthClass("));
  const messages = app.slice(app.indexOf("function isRepeatedConversationPrompt("), app.indexOf("function renderPrimaryAction("));
  vm.runInNewContext(escape + messages,sandbox);
  const raw = ["\nOpened!\n", "\n\r\n  ", "\nFirst paragraph.\n\nSecond paragraph.\n"];
  sandbox.renderMessages(raw,"command");
  assert.equal(sandbox.ui.latestMessage.textContent,"First paragraph.\n\nSecond paragraph.");
  assert.equal(articles.length,2);
  assert.match(articles[0].innerHTML,/<p>First paragraph\.\n\nSecond paragraph\.<\/p>/);
  assert.match(articles[1].innerHTML,/<p>Opened!<\/p>/);
  assert.deepEqual(mobileLines,["Opened!","First paragraph.","Second paragraph."]);
  assert.equal(raw[0],"\nOpened!\n", "Raw engine history must not be modified");
  sandbox.renderMessages(["\nName: <Iolo>\n", "\nYour Interest:\n"],"text");
  assert.equal(sandbox.ui.latestMessage.textContent,"Name: <Iolo>");
  assert.match(articles[1].innerHTML,/&lt;Iolo&gt;/);
  sandbox.renderMessages(["\n \r\n"],"command");
  assert.equal(sandbox.ui.latestMessage.textContent,"Awaiting the first command.");
  assert.equal(articles.length,0);
  sandbox.renderMessages(["I bear greetings\nfrom the fair\ncity of\nMoonglow.\n\nDost thou seek\nan inn or\nhealing?"],"text");
  assert.equal(sandbox.ui.latestMessage.textContent,"I bear greetings from the fair city of Moonglow.\n\nDost thou seek an inn or healing?");
  assert.equal(sandbox.ui.desktopLatestMessage.textContent,sandbox.ui.latestMessage.textContent);
  assert.equal(sandbox.readableProse("We have:\nB-Staff\nC-Dagger\n\nYour Interest?"),"We have:\nB-Staff\nC-Dagger\n\nYour Interest?");
  sandbox.renderMessages(["Revealed NPC prose"],"text",["Opened!"]);
  assert.equal(sandbox.ui.latestMessage.textContent,"Revealed NPC prose");
  assert.equal(articles.length,1);assert.match(articles[0].innerHTML,/Opened!/);
  assert.doesNotMatch(articles[0].innerHTML,/NPC prose/);
});

test("Journal is engine-paused and browser notebook edits never flush through Asyncify",()=>{
  const journal=read("toolchain/web_journal.cpp"),bridge=read("toolchain/web_bridge.cpp");
  assert.match(journal,/\+\+webInteractionDepth\(\);eventHandler->pushController/);
  assert.match(journal,/eventHandler->popController\(\);overlay.reset\(\);--webInteractionDepth/);
  assert.match(journal,/prompt->submitted/);assert.match(journal,/CTX_COMBAT/);
  for(const name of ["open","close","reload","json","favorite","note","delete"]) assert.match(journal,new RegExp("zu4_web_journal_"+name));
  assert.match(bridge,/if \(webJournalIsOpen\(\)\)/);
  const notebook=fs.readFileSync(path.resolve(root,"../../vendor/ultima4-ios/src/journal_notebook.cpp"),"utf8");
  assert.match(notebook,/#ifndef ZU4_WEB\s+if \(ok\) ok = fsync\(fd\) == 0;/);
  assert.match(read("dist/app.js"),/if\(state.journalOpen\)return/);
});

test("the C++ bridge exports live state and input functions", () => {
  const bridge = read("toolchain/web_bridge.cpp");
  for (const name of ["zu4_web_ready", "zu4_web_has_game_data", "zu4_web_install_zip", "zu4_web_screen_pixels", "zu4_web_snapshot_json", "zu4_web_send_key", "zu4_web_send_text", "zu4_web_submit_prompt", "zu4_web_end_conversation", "zu4_web_submit_option", "zu4_web_equip", "zu4_web_set_video", "zu4_web_activate_primary_action", "zu4_web_save_adventure", "zu4_web_tap_world"]) {
    assert.match(bridge, new RegExp(`EMSCRIPTEN_KEEPALIVE[^\\n]+${name}`));
  }
  assert.match(bridge, /primaryAction/);
  assert.match(bridge, /uniqueAdjacentAction/);
  assert.match(bridge, /zu4_mobile_adjacent_interaction/);
  assert.match(bridge, /find_first_not_of/);
  assert.doesNotMatch(bridge, /PendingInput|pendingInput|notifyKeyPressed/);
  assert.match(bridge, /enqueueKey\(ch\)/);
  assert.match(bridge, /return zu4_web_submit_prompt\("bye"\)/);
  assert.match(bridge, /enqueueAction\(ZU4_WEB_ACTION_CONTEXT\)/);
  assert.match(bridge, /enqueueAction\(ZU4_WEB_ACTION_ADJACENT, adjacent\.token\)/);
  assert.match(bridge, /zu4_web_dispatch_action/);
  assert.doesNotMatch(bridge, /zu4_mobile_adjacent_interaction\(adjacent\.token\)/);
  const eventLoop = read("../../vendor/ultima4-ios/src/event_sdl.cpp");
  assert.match(eventLoop, /zu4_web_dispatch_action\(event\.user\.code/);
  assert.match(eventLoop, /event\.user\.code > ZU4_WEB_ACTION_GAMEPLAY_BASE/);
  assert.match(bridge, /dynamic_cast<ReadIntController/);
  assert.match(bridge, /const bool installSaves = kind == 1/);
  assert.match(bridge, /c->party->size\(\)/);
  assert.match(bridge, /combat \? combat->getCurrentPlayer\(\) == member/);
  assert.match(bridge, /spellCheckPrerequisites/);
  assert.match(bridge, /webInteractionDepth\(\) \? "busy" : "command"/);
  assert.match(bridge, /interactionActive/);
  assert.match(bridge,/SDL_SetHint\(SDL_HINT_EMSCRIPTEN_KEYBOARD_ELEMENT, "#engineCanvas"\)/);
});

test("tap routes start and continue only on the SDL-owned engine loop", () => {
  const bridge = read("toolchain/web_bridge.cpp");
  const game = read("../../vendor/ultima4-ios/src/game.cpp");
  const loop = read("../../vendor/ultima4-ios/src/event_sdl.cpp");
  const entry = bridge.match(/int zu4_web_tap_world\([^]*?\n\}/)[0];
  assert.match(entry, /zu4_mobile_capture_walk/);
  assert.match(entry, /enqueueAction\(ZU4_WEB_ACTION_WALK_START/);
  assert.doesNotMatch(entry, /zu4_mobile_world_tap|zu4_web_walk_start\(/);
  assert.match(bridge, /static_cast<uint32_t>\(parameter\) == walkRequestGeneration/);
  const timeout = game.match(/static void mobileWalkTimeout\([^]*?\n\}/)[0];
  assert.match(timeout, /ZU4_WEB_ACTION_WALK_STEP/);
  assert.match(timeout, /SDL_PushEvent/);
  assert.doesNotMatch(timeout, /mobileWalkStep\(/);
  for (const action of ["ZU4_WEB_ACTION_WALK_START", "ZU4_WEB_ACTION_WALK_STEP"])
    assert.match(loop, new RegExp(action));
});

test("the web shell is wired to the engine canvas and real panels", () => {
  const html = read("dist/index.html");
  const app = read("dist/app.js");
  const engineClient = read("dist/engine-client.js");
  const css = read("dist/app.css");
  assert.match(html, /canvas id="worldCanvas" width="176" height="176"/);
  assert.match(html, /canvas id="engineCanvas"[^>]+width="320" height="200"/);
  assert.match(html, /id="partyPanel"/);
  assert.match(html, /id="partyDetail"/);
  assert.match(html, /id="spellsPanel"/);
  assert.match(html, /id="conversationForm"/);
  assert.match(html, /id="promptActions"/);
  assert.match(html, /id="gameFolderInput"/);
  assert.match(html, /id="gameZipInput"/);
  assert.match(html, /id="gameUrlForm"/);
  assert.match(html, /id="adventureAccount"[^>]*>Sign in or create account</);
  assert.match(html, /id="dataAccount"[^>]*>Sign in or create account</);
  assert.match(html, /Continue a saved game or start a new one/);
  assert.match(html, /id="primaryActionButton"/);
  assert.match(html, /class="mobile-control-deck"/);
  assert.match(html, /id="mobileDpadCenter"[\s\S]*hidden>Repeat attack<\/button>/);
  assert.match(html, /id="mobileWorldLog" class="mobile-world-log"/);
  assert.match(html, /id="mobileWorldStatus"/);
  assert.match(html, /id="mobileAction7"/);
  assert.match(html, /id="sheetBackdrop"/);
  assert.match(html, /class="dpad" role="group" aria-label="Movement D-pad"/);
  assert.match(html, /data-key="1002" data-move="south"/);
  assert.ok(html.indexOf("engine-client.js") < html.indexOf("engine-session.js"));
  assert.ok(html.indexOf("engine-session.js") < html.indexOf("app.js"));
  assert.match(app, /engine\/ultimatum-engine\.js/);
  assert.match(app, /engine\/\$\{path\}\?v=\$\{ENGINE_BUILD\}/);
  assert.match(app, /Downloading engine data \(42 MB\)/);
  assert.match(app, /locateFile\(path\)/);
  assert.match(engineClient, /zu4_web_snapshot_json/);
  assert.match(engineClient, /zu4_web_send_text/);
  assert.match(engineClient, /zu4_web_submit_prompt/);
  assert.match(engineClient, /zu4_web_end_conversation/);
  assert.match(engineClient, /zu4_web_submit_option/);
  assert.match(engineClient, /zu4_web_equip/);
  assert.match(engineClient, /FS\.mount\(this\.module\.IDBFS/);
  assert.doesNotMatch(engineClient, /analyzePath/);
  assert.doesNotMatch(read("dist/journal-ui.js"), /analyzePath/);
  assert.match(app, /await engine\.persistSaves\(\)/);
  assert.match(app, /await installationOrchestrator\.install\(record\)/);
  assert.match(app, /new URLSearchParams\(location\.search\).*account/);
  assert.match(app, /adventures\.deferUntilAccountClose\(showRequiredData\)/);
  assert.match(app, /ui\.dataAccount\.addEventListener\("click", openAccountFromData\)/);
  assert.match(app, /saved\.kind === "zip"\) await waitForRuntimeFilesystem\(\)/);
  assert.match(app, /runtimeFilesystemResolve\(initializedModule\.FS\)/);
  const adventureUi = read("dist/adventure-ui.js");
  assert.match(adventureUi, /await this\.hooks\.whenRuntimeReady\?\.\(\)/);
  assert.match(adventureUi, /deferUntilAccountClose\(callback\)/);
  assert.match(adventureUi, /close:\(\)=>this\.closeAccount\(\)/);
  assert.match(adventureUi, /else if\(!this\.cloudUI\.installing\)this\.closeAccount\(\)/);
  assert.match(adventureUi, /"Saved Games"/);
  assert.match(adventureUi, /const firstEmpty=/);
  assert.doesNotMatch(adventureUi, /heading\.textContent=`Slot/);
  assert.match(app, /engine\.replaceGameFiles\(verified\.files\)/);
  assert.match(app, /restoreGameSave && !savedGame\.version/);
  assert.match(app, /pendingSpell\.letter/);
  assert.doesNotMatch(app, /setTimeout\(\(\) => sendKey\(spell/);
  assert.match(app, /dataset\.partyChoice = String\(index \+ 1\)/);
  assert.match(app, /renderPromptActions\(state\.prompt\)/);
  assert.match(app, /state\.prompt\?\.id \|\| state\.interactionActive/);
  assert.match(app, /!blockedByPrompt && Boolean\(action\?\.enabled\)/);
  assert.match(app, /classList\.toggle\("topic-prompt"/);
  assert.match(html, /type="submit" form="conversationForm" data-topic="health"/);
  assert.match(app, /event\.submitter\?\.dataset\.topic/);
  assert.match(app, /topic === "bye" \? endConversation\(\)/);
  assert.match(app, /\["command", "combat"\]\.includes\(state\.inputMode\)/);
  assert.match(app, /ENDING CONVERSATION/);
  assert.match(app, /isRepeatedConversationPrompt/);
  assert.match(app, /find\(message => !isRepeatedConversationPrompt\(message\)\)/);
  assert.match(app, /canvas\.focus\(\{ preventScroll: true \}\);[\s\S]*return true;/);
  assert.match(app, /Finish the current choice/);
  assert.match(app, /equipmentChoices\(member, 1\)/);
  assert.match(engineClient, /zu4_web_activate_primary_action/);
  assert.match(engineClient, /zu4_web_tap_world/);
  assert.match(read("dist/library-store.js"), /indexedDB\.open|this\.indexedDB\.open/);
  assert.match(app, /fetchZip/);
  assert.match(app, /WORLD_VIEW = \{ x: 8, y: 8, width: 176, height: 176 \}/);
  assert.match(app, /canvas: engineCanvas/);
  assert.match(engineClient, /zu4_web_screen_pixels/);
  assert.match(app, /worldContext\.putImageData\(worldFrame, 0, 0\)/);
  assert.match(app, /canvas\.addEventListener\("click", tapWorld\)/);
  assert.match(app, /Math\.floor\(sourceX \/ 16\) - 5/);
  assert.match(css, /button, canvas\s*\{[\s\S]*touch-action: manipulation;/);
  assert.match(css, /\.side-panel\.mobile-open/);
  assert.match(css, /\.mobile-world-log\s*\{[\s\S]*position: absolute;[\s\S]*pointer-events: none;/);
  assert.match(css, /\.mobile-action-grid\s*\{[\s\S]*grid-template-rows: repeat\(4,/);
  assert.match(app, /slice\(-3\)/);
  assert.match(app, /"dungeon-search"/);
  assert.match(css, /\.conversation-dock\.topic-prompt\s*\{[\s\S]*height: 158px;/);
  assert.match(css, /orientation: landscape/);
  assert.match(css, /\.dpad \[data-move="south"\]\s*\{[^}]*grid-row: 3/);
  assert.match(css, /grid-template-rows: repeat\(3, 44px\)/);
  assert.match(css, /transform: translateY\(21px\)/);
  assert.match(css, /\.command-bar\s*\{[^}]*min-height: 100px;[^}]*grid-template-rows: repeat\(2, 48px\)/);
});

test("EngineClient owns the UI-to-Wasm protocol", () => {
  const calls = [];
  const module = {
    ccall(name, returnType, argTypes, args) {
      calls.push({ name, returnType, argTypes, args });
      if (name === "zu4_web_snapshot_json") return '{"contractVersion":1,"ready":true,"inputMode":"command"}';
      if (name === "zu4_web_has_game_data") return 1;
      if (name === "zu4_web_set_video") return 1;
      return 0;
    },
    HEAPU8: new Uint8Array(8),
  };
  const sandbox = { window: {} };
  vm.runInNewContext(read("dist/engine-client.js"), sandbox);
  const client = new sandbox.window.UltimatumEngineClient(() => module);

  assert.equal(client.hasGameData(), true);
  assert.equal(client.snapshot().contractVersion, 1);
  assert.equal(client.snapshot().inputMode, "command");
  assert.equal(client.setVideo(1), true);
  assert.equal(client.activatePrimaryAction(), false);
  assert.equal(calls.at(-1).name, "zu4_web_activate_primary_action");
  assert.equal(client.tapWorld(-2, 3), false);
  assert.equal(calls.at(-1).name, "zu4_web_tap_world");
  assert.deepEqual(Array.from(calls.at(-1).args), [-2, 3]);
  assert.equal(client.submitPrompt("yes"), false);
  assert.equal(calls.at(-1).name, "zu4_web_submit_prompt");
  assert.deepEqual(Array.from(calls.at(-1).args), ["yes"]);
  assert.equal(client.endConversation(), false);
  assert.equal(calls.at(-1).name, "zu4_web_end_conversation");
  assert.equal(client.submitAnswer("12", 42), false);
  assert.equal(calls.at(-1).name, "zu4_web_submit_answer");
  assert.deepEqual(Array.from(calls.at(-1).args), ["12", 42]);
  assert.equal(client.submitOption(1001), false);
  assert.equal(calls.at(-1).name, "zu4_web_submit_option");
  assert.deepEqual(Array.from(calls.at(-1).args), [1001]);
  assert.equal(client.equip(2, 1, 6), 0);
  assert.equal(calls.at(-1).name, "zu4_web_equip");
  assert.deepEqual(Array.from(calls.at(-1).args), [2, 1, 6]);
  assert.equal(client.saveAdventure(), 0);
  assert.equal(calls.at(-1).name, "zu4_web_save_adventure");
  client.sendKey(1001);
  assert.equal(calls.at(-1).name, "zu4_web_send_key");
  assert.equal(calls.at(-1).args.length, 1);
  assert.equal(calls.at(-1).args[0], 1001);
});

test("special conversation prompts execute atomic validated answers", () => {
  const directory = fs.mkdtempSync(path.join(os.tmpdir(), "ultimatum-web-prompt-"));
  const executable = path.join(directory, `web-prompt-test${process.platform === "win32" ? ".exe" : ""}`);
  try {
    const source = path.join(__dirname, "web-prompt.test.cpp");
    if (process.platform === "win32") {
      execFileSync("cl.exe", ["/nologo", "/std:c++14", "/EHsc", `/I${path.join(root, "toolchain")}`, `/I${path.join(root, "../../vendor/ultima4-ios/src")}`, source, `/Fe:${executable}`, `/Fo:${path.join(directory, "web-prompt-test.obj")}`], {cwd:directory});
    } else {
      execFileSync("c++", ["-std=c++14", "-I", path.join(root, "toolchain"), "-I", path.join(root, "../../vendor/ultima4-ios/src"), source, "-o", executable]);
    }
    assert.match(execFileSync(executable, { encoding: "utf8" }), /duplicate submissions passed/);
  } finally {
    fs.rmSync(directory, { recursive: true, force: true });
  }
  const bridge = read("toolchain/web_bridge.cpp");
  assert.match(bridge, /prompt->generation != generation/);
  assert.match(bridge, /enqueueAction\(ZU4_WEB_ACTION_PROMPT, prompt->generation\)/);
  assert.match(bridge, /prompt->generation == parameter/);
  assert.ok(bridge.includes('case \'\\r\': out << "\\\\r"; break;'), "Continuation values must survive JSON serialization");
});

test("EngineClient mounts durable saves without losing a bundled first-run journey", async () => {
  const files = new Map([["/home/web_user/.xu4/party.sav", new Uint8Array([1, 2, 3])]]);
  const syncCalls = [];
  const module = {
    IDBFS: {},
    FS: {
      mkdir() {},
      mount(type, options, path) {
        assert.equal(type, module.IDBFS);
        assert.equal(path, "/home/web_user/.xu4");
      },
      readFile(path) {
        if (!files.has(path)) throw new Error("missing");
        return files.get(path);
      },
      writeFile(path, data) { files.set(path, data); },
      syncfs(populate, callback) {
        syncCalls.push(populate);
        if (populate) files.clear();
        callback(null);
      },
    },
  };
  const sandbox = { window: {} };
  vm.runInNewContext(read("dist/engine-client.js"), sandbox);
  const client = new sandbox.window.UltimatumEngineClient(() => module);

  await client.initializeSaveStorage();
  assert.deepEqual(syncCalls, [true, false]);
  assert.deepEqual(Array.from(files.get("/home/web_user/.xu4/party.sav")), [1, 2, 3]);
});

test("optimized Emscripten errors do not prevent mounting existing save directories", () => {
  const sandbox={window:{}};
  const module={FS:{mkdir(){throw {errno:20};},stat(){return {mode:0x4000};},isDir(mode){return mode===0x4000;}}};
  vm.runInNewContext(read("dist/engine-client.js"),sandbox);
  const client=new sandbox.window.UltimatumEngineClient(()=>module);
  assert.doesNotThrow(()=>client.ensureDirectory("/home/web_user/.xu4"));
  assert.equal(client.pathExists("/home/web_user/.xu4"),true);
  module.FS.isDir=()=>false;
  assert.throws(()=>client.ensureDirectory("/home/web_user/.xu4"));
  module.FS.stat=()=>{throw {errno:44};};
  assert.equal(client.pathExists("/missing"),false);
});

test("signed-in account game-data inspection waits for the runtime filesystem", async () => {
  const app=read("dist/app.js");
  const waitSource=app.match(/async function waitForRuntimeFilesystem\(\) \{[\s\S]*?^\}/m)[0];
  const gameDataSource=app.match(/async function localGameDataForCloud\(\) \{[\s\S]*?^\}/m)[0];
  let extracted=false;
  const engine={module:null,extractGameZip(){extracted=true;return [{name:"AVATAR.EXE",data:new ArrayBuffer(3)}];}};
  const sandbox={
    engine,
    library:{get:async()=>({kind:"zip",data:new ArrayBuffer(4)})},
    gameDataImporter:{prepare:async saved=>({verified:{profile:"ultima4-dos",verification:{label:"English DOS\/EGA"},files:engine.extractGameZip(saved.data)}})},
    MAX_ZIP_BYTES:128*1024*1024,
    window:{UltimatumGameData:{
      encodePackage:()=>"verified-package",
    }},
  };
  vm.runInNewContext(`
    let runtimeFilesystemResolve;
    const runtimeFilesystemReady=new Promise(resolve=>{runtimeFilesystemResolve=resolve;});
    ${waitSource}
    ${gameDataSource}
    this.inspect=localGameDataForCloud;
    this.initialize=()=>{engine.module={FS:{}};runtimeFilesystemResolve(engine.module.FS);};
  `,sandbox);
  const pending=sandbox.inspect();
  await new Promise(resolve=>setImmediate(resolve));
  assert.equal(extracted,false,"Account inspection touched the ZIP before the filesystem initialized");
  sandbox.initialize();
  const result=await pending;
  assert.equal(extracted,true);
  assert.equal(result.available,true);
  assert.equal(result.text,"verified-package");
});

test("north-up web dungeons route arrow input through cardinal movement", () => {
  const bridge=read("toolchain/web_bridge.cpp");
  const game=read("../../vendor/ultima4-ios/src/game.cpp");
  assert.match(bridge,/zu4_mobile_dungeon_top_down\(\)[\s\S]+ZU4_MOBILE_ACTION_MOVE/);
  assert.match(game,/if \(zu4_mobile_dungeon_top_down\(\)\)[\s\S]+orientation = dir;[\s\S]+notifyKeyPressed\(U4_UP\)/);
  assert.doesNotMatch(game,/ifdef ZU4_IOS[\s\S]{0,80}if \(zu4_mobile_dungeon_top_down\(\)/);
  assert.match(read("dist/app.js"),/dungeon && !combat && !state\.dungeon\.overhead/);
});

test("the web engine uses mobile-safe rendering and audio defaults", () => {
  const engine = read("../../vendor/ultima4-ios/src/u4.cpp");
  const music = read("../../vendor/ultima4-ios/src/music.c");
  assert.match(engine, /#ifdef ZU4_WEB[\s\S]+settings\.scale = 1;/);
  assert.match(engine, /settings\.musicVol > 3/);
  assert.match(music, /#ifdef ZU4_WEB[\s\S]+spec->samples = 4096;/);
});

test("the local engine build preloads saves and supports the optional VGA overlay", () => {
  const cmake = read("CMakeLists.txt");
  assert.match(cmake, /\/home\/web_user\/\.xu4/);
  assert.match(cmake, /ULTIMATUM_U4_UPGRADE/);
  assert.match(cmake, /@\/u4upgrad\.zip/);
  assert.match(cmake, /-lidbfs\.js/);
  assert.match(cmake, /--pre-js=\$\{CMAKE_CURRENT_SOURCE_DIR\}\/toolchain\/web_module_config\.js/);
  assert.match(read("toolchain/web_module_config.js"), /globalThis\.Module/);
});

test("Menu and Experience use engine-owned prompts and explicit capabilities", () => {
  const bridge = read("toolchain/web_bridge.cpp");
  const menu = read("toolchain/web_menu.cpp");
  assert.match(bridge, /EMSCRIPTEN_KEEPALIVE int zu4_web_open_menu/);
  assert.match(bridge, /menuQueued \|\| !webMenuAvailable/);
  assert.match(bridge, /enqueueAction\(ZU4_WEB_ACTION_MENU\)/);
  assert.match(menu, /WebInteractionScope interaction/);
  assert.match(menu, /zu4_mobile_cancel_walk\(\)/);
  assert.match(menu, /zu4_experience_set_profile/);
  assert.match(menu, /zu4_experience_restore_defaults/);
  assert.match(menu, /screenApplyVideoType/);
  assert.match(menu, /zu4_settings_write/);
  assert.match(menu, /zu4_experience_set_exploration_map/);
  assert.match(menu, /zu4_experience_set_map_pins/);
  assert.match(menu, /#ifdef ZU4_WEB_DEBUG_TOOLS/);
  assert.match(menu, /prepareDebugChange/);
  assert.match(menu, /Confirm destructive test action/);
  assert.match(read("CMakeLists.txt"), /option\(ULTIMATUM_WEB_DEBUG_TOOLS[^\n]+OFF\)/);
  assert.match(read("dist/index.html"), /id="mobileMenuButton"/);
  assert.match(read("dist/index.html"), /id="menuButton"/);
  assert.match(read("dist/app.js"), /option.disabled/);
});

test("EngineClient copies Peer pixels and rejects out-of-bounds maps", () => {
  const calls=[];
  const heap=new Uint8Array([0,1,2,3,255,8,9,10,255]);
  const module={HEAPU8:heap,ccall(name){calls.push(name);return 1;}};
  const sandbox={window:{}};
  vm.runInNewContext(read("dist/engine-client.js"),sandbox);
  const client=new sandbox.window.UltimatumEngineClient(()=>module);
  assert.equal(client.openMenu(),true);
  assert.equal(calls.at(-1),"zu4_web_open_menu");
  assert.deepEqual(Array.from(client.mapPixels({pixels:1,width:2,height:1})),[1,2,3,255,8,9,10,255]);
  assert.equal(client.mapPixels({pixels:2,width:2,height:1}),null);
  assert.equal(client.mapPixels({pixels:0,width:1,height:1}),null);
  assert.equal(client.mapPixels({pixels:1,width:0,height:1}),null);
  assert.equal(client.mapPixels({pixels:1,width:1.5,height:1}),null);
  assert.equal(client.mapPixels({pixels:NaN,width:1,height:1}),null);
  assert.deepEqual(Array.from(client.recoveryBytes({bytes:1,size:4})),[1,2,3,255]);
  assert.equal(client.recoveryBytes({bytes:1,size:100}),null);
  const pixels=client.mapPixels({pixels:1,width:1,height:1});
  pixels[0]=99;
  assert.equal(heap[1],1,"Rendering must not mutate the WASM map buffer");
});

test("EngineClient exposes semantic combat, dungeon and exploration map operations", () => {
  const calls=[];
  const heap=new Uint8Array(128);
  heap.set([10,20,30,255],32);
  const values={zu4_web_prepare_map:1,zu4_web_map_pixels:32,zu4_web_map_width:1,zu4_web_map_height:1,zu4_web_map_player_x:7,zu4_web_map_player_y:9};
  const module={HEAPU8:heap,ccall(name,_returnType,_argTypes,args){calls.push({name,args});return values[name] ?? 1;}};
  const sandbox={window:{}};
  vm.runInNewContext(read("dist/engine-client.js"),sandbox);
  const client=new sandbox.window.UltimatumEngineClient(()=>module);
  assert.equal(client.gameplayAction(16,-1),true);
  assert.equal(calls.at(-1).name,"zu4_web_gameplay_action");
  assert.deepEqual(Array.from(calls.at(-1).args),[16,-1]);
  const map=client.explorationMap();
  assert.equal(map.kind,1); assert.equal(map.x,7); assert.equal(map.y,9);
  assert.deepEqual(Array.from(map.pixels),[10,20,30,255]);
  assert.equal(client.setMapPin(7,9,"Britain"),true);
  assert.equal(calls.at(-1).name,"zu4_web_set_map_pin");
  assert.deepEqual(Array.from(calls.at(-1).args),[7,9,"Britain"]);
  assert.equal(client.removeMapPin(7,9),true);
  assert.equal(calls.at(-1).name,"zu4_web_remove_map_pin");
  assert.deepEqual(Array.from(calls.at(-1).args),[7,9]);
});

test("the web shell protects active adventures across backgrounding and tab reloads", () => {
  const app=read("dist/app.js"), adventures=read("dist/adventure-ui.js"), html=read("dist/index.html");
  assert.match(app,/secureLifecycle\("hidden"\)/);
  assert.match(app,/sessionOrchestrator\.releaseLease\(\)/);
  assert.match(app,/secureLifecycle\("pagehide", !event\.persisted\)/);
  assert.match(app,/secureLifecycle\("freeze"\)/);
  assert.match(app,/event\.persisted/);
  assert.match(app,/navigator\.storage\?\.persist/);
  assert.match(app,/serviceWorker\.register\("sw\.js"/);
  assert.match(app,/if\(!cloudConfigured\).*#dataAccount/,"Unconfigured clones must hide cloud-only actions");
  assert.match(adventures,/ultimatum-active-session-v1/);
  assert.match(adventures,/UltimatumCloudConfig\?\.url&&global\.UltimatumCloudConfig\?\.key/,"Account clients must not initialize without local configuration");
  assert.match(adventures,/Restored the latest safe checkpoint after the tab reloaded/);
  assert.match(html,/manifest\.webmanifest/);
  const serviceWorker=read("dist/sw.js");
  assert.match(serviceWorker,/ultimatum-web-shell-/);
  assert.match(serviceWorker,/path !== "\.\/"/,"The navigation fallback must not turn every same-origin response into a cached shell asset");
});

test("generated engine artifacts contain the public bridge exports", () => {
  const generated = read("dist/engine/ultimatum-engine.js");
  assert.match(generated,/zu4_web_open_menu/);
  assert.doesNotMatch(generated,/zu4_web_test_prepare|zu4_web_test_generation|zu4_web_test_objects/);
  for (const name of ["zu4_web_install_zip", "zu4_web_has_game_data", "zu4_web_screen_pixels", "zu4_web_snapshot_json", "zu4_web_send_key", "zu4_web_send_text", "zu4_web_submit_prompt", "zu4_web_end_conversation", "zu4_web_submit_option", "zu4_web_equip", "zu4_web_set_video", "zu4_web_activate_primary_action", "zu4_web_save_adventure", "zu4_web_tap_world", "zu4_web_gameplay_action", "zu4_web_prepare_map", "zu4_web_set_map_pin", "zu4_web_remove_map_pin"]) {
    assert.match(generated, new RegExp(name));
  }
  assert.ok(fs.statSync(path.join(root, "dist/engine/ultimatum-engine.wasm")).size > 1_000_000);
});
