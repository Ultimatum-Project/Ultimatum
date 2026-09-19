const canvas = document.querySelector("#worldCanvas");
const engineCanvas = document.querySelector("#engineCanvas");
const worldContext = canvas.getContext("2d", { alpha: false });
const ui = Object.fromEntries([...document.querySelectorAll("[id]")].map(element => [element.id, element]));
const cloudConfigured=Boolean(window.UltimatumCloudConfig?.url&&window.UltimatumCloudConfig?.key);
const runtimeModule = () => document.ultimatumRuntimeModule || window.UltimatumRuntimeModule || window.Module;
const engine = new window.UltimatumEngineSession(new window.UltimatumEngineClient(runtimeModule));
const sessionOrchestrator = new window.UltimatumSessionOrchestrator({
  session: engine,
  lease: new window.UltimatumWebSessionLease({name:"ultimatum-ultima4-default-session-v1"}),
  preflight: () => {
    if (!engine.hasGameData()) throw Error("Verified Ultima IV game data is required before starting a session.");
  },
  mount: () => waitForRuntimeFilesystem(),
  checkpoint: reason => secureActiveAdventure(reason),
});
window.ultimatumSessionDiagnostics = () => sessionOrchestrator.diagnostics();
const diagnostics = new window.UltimatumDiagnosticsCollector({
  product:{id:"ultimatum-web",version:"20260918-phase1-06"},
  port:{gameId:"ultima4",portId:"xu4",version:"0.1.0",engineId:"xu4",engineVersion:"1.0-git"},
});
sessionOrchestrator.onProgress(event => {
  document.documentElement.dataset.sessionState = event.state;
  const failed=event.state==="recoverable-error"||event.state==="fatal-error";
  diagnostics.record({component:"session",severity:event.state==="fatal-error"?"error":event.state==="recoverable-error"?"warning":"info",code:failed?`session.${event.state}`:"session.state-change",details:failed?{state:event.state,phase:event.detail?.phase,"error-name":event.detail?.name}:{state:event.state,"previous-state":event.previousState}});
});
const library = new window.UltimatumIndexedDbLibraryStore();
const experimentalOpfs = window.UltimatumOpfsStorageProvider?.isSupported() ? new window.UltimatumOpfsStorageProvider() : null;
window.ultimatumStorageProviders = Object.freeze({default:library,experimentalOpfs});
window.ultimatumStorageDiagnostics = async () => Object.freeze({
  defaultProvider:library.id,
  experimentalOpfs:Object.freeze({supported:Boolean(experimentalOpfs),selected:false,id:experimentalOpfs?.id || null}),
  estimate:await library.estimate(),
  migrationPerformed:false,
});
const settingsRegistry = new window.UltimatumSettingsRegistry({manifestUrl:"settings.manifest.json"});
const controlRegistry = new window.UltimatumControlRegistry({manifestUrl:"controls.manifest.json"});
window.ultimatumSettingsDiagnostics = () => settingsRegistry.project(engine.snapshot(),"web");
window.ultimatumControlDiagnostics = () => controlRegistry.inspect("web");
window.ultimatumDiagnostics = async () => {
  const [storage,settings,controls]=await Promise.all([window.ultimatumStorageDiagnostics(),window.ultimatumSettingsDiagnostics(),window.ultimatumControlDiagnostics()]);
  const session=sessionOrchestrator.diagnostics();
  return diagnostics.bundle({
    host:{kind:"web",online:navigator.onLine,"storage-api":Boolean(navigator.storage)},
    session:{state:session.state,"lease-supported":session.leaseSupported,"lease-acquired":session.leaseAcquired},
    storage:{provider:storage.defaultProvider,"opfs-supported":storage.experimentalOpfs.supported,"opfs-selected":storage.experimentalOpfs.selected,"usage-bytes":storage.estimate?.usage,"quota-bytes":storage.estimate?.quota,"migration-performed":storage.migrationPerformed},
    settings:{"schema-version":settings.schemaVersion,"settings-version":settings.settingsVersion,"unavailable-count":settings.unavailable.length,"migration-performed":settings.migrationPerformed},
    controls:{"schema-version":controls.schemaVersion,"actions-version":controls.actionsVersion,"action-count":controls.actionCount,"verified-profile-count":controls.profiles.length,"migration-performed":controls.migrationPerformed},
  });
};
const gameDataImporter = new window.UltimaIVImportAdapter({
  gameData: window.UltimatumGameData,
  extractZip: (buffer, maxBytes) => engine.extractGameZip(buffer, maxBytes),
  maxZipBytes: 128 * 1024 * 1024,
});
const installationOrchestrator = new window.UltimatumInstallationOrchestrator({
  adapter: gameDataImporter,
  library,
  stageRuntime: async verified => {
    await waitForRuntimeFilesystem();
    if (engineStarted) return null;
    const rollback = engine.replaceGameFiles(verified.files);
    return {rollback,commit:()=>rollback.commit()};
  },
});
const adventures = new window.UltimatumAdventureUI(engine, {
  start: restoreSave => startEngine(restoreSave),
  isStarted: () => engineStarted,
  state: () => latestState,
  toast: message => toast(message),
  openData: () => openDataLibrary(),
  focus: () => canvas.focus({preventScroll:true}),
  gameData: () => localGameDataForCloud(),
  installGameData: text => installCloudGameData(text),
  whenRuntimeReady: () => waitForRuntimeFilesystem(),
});
const journalUI = new window.UltimatumJournalUI(engine, {
  toast: message=>toast(message),persist:()=>adventures.persistJournal(),
  legacyHistory:()=>conversationHistory,
  resume:()=>render(engine.snapshot()),
});
const modalBackground = [...document.querySelectorAll(".topbar, .command-bar, .mode-command-bar, .mobile-control-deck, #sidePanel")];
const mobileLayout = window.matchMedia("(max-width: 650px), (max-width: 980px) and (max-height: 500px)");
const desktopLayout = window.matchMedia("(min-width: 981px)");
const conversationDock = ui.conversationForm.closest(".conversation-dock");
const dockHome = document.createComment("Conversation dock mobile position");
conversationDock.before(dockHome);
function positionPromptDock() {
  if (desktopLayout.matches) {
    ui.desktopPromptHost.append(conversationDock);
    if (latestState.prompt?.id || latestState.interactionActive) selectPanel("conversationsPanel");
  }
  else dockHome.after(conversationDock);
  modalBackground.forEach(region => { region.inert = !desktopLayout.matches && Boolean(latestState.prompt?.id || latestState.interactionActive); });
}
desktopLayout.addEventListener("change", () => { positionPromptDock(); render(latestState); });
let engineReady = false;
let engineStarted = false;
let runtimeReady = false;
let runtimeFilesystemResolve;
const runtimeFilesystemReady = new Promise(resolve => { runtimeFilesystemResolve = resolve; });
let latestState = { ready: false, inputMode: "loading", messages: [], party: [], spells: [] };
let toastTimer;
let panelTrigger = null;
let pendingSpell = null;
let conversationExit = null;
let selectedPartyMember = null;
let partyDetailSignature = "";
let storageRevision = 0;
let libraryRequest = 0;
let recoveryDownloadRequest = 0;
let storageFlushPending = false;
let storageDirty = false;
let lifecycleSecuring = null;
const MAX_ZIP_BYTES = 128 * 1024 * 1024;
const LEGACY_SCREEN = { width: 320, height: 200 };
const WORLD_VIEW = { x: 8, y: 8, width: 176, height: 176 };
const ENGINE_BUILD = "20260917-mobile8";
const FEEDBACK_INSTALLATION_KEY = "ultimatum-feedback-installation-v1";
const worldFrame = worldContext.createImageData(WORLD_VIEW.width, WORLD_VIEW.height);
let screenPixels = 0;
let conversationHistory = [];
let conversationHistoryRevision = 0;
let conversationHistoryRenderKey = "";
let recordedConversationPrompt = "";
let conversationSession = null;
let currentExplorationMap = null;
let selectedMapCell = null;

const GAMEPLAY_ACTION = Object.freeze({
  dungeonSearch: 12,
  dungeonTorch: 13,
  dungeonView: 14,
  combatTarget: 15,
  combatCycle: 16,
  combatClear: 17,
  combatRepeat: 20,
});

async function waitForRuntimeFilesystem() {
  if (engine.module?.FS) return engine.module.FS;
  await runtimeFilesystemReady;
  if (!engine.module?.FS) throw Error("The game engine did not finish preparing its local storage. Reload and try again.");
  return engine.module.FS;
}

worldContext.imageSmoothingEnabled = false;

function copyWorldPixels() {
  if (!runtimeReady) return false;
  if (!screenPixels) screenPixels = engine.screenPixels();
  if (!screenPixels) return false;

  const source = engine.heap;
  const sourceStride = LEGACY_SCREEN.width * 4;
  const rowBytes = WORLD_VIEW.width * 4;
  const firstPixel = screenPixels + ((WORLD_VIEW.y * LEGACY_SCREEN.width + WORLD_VIEW.x) * 4);
  for (let row = 0; row < WORLD_VIEW.height; row += 1) {
    const sourceStart = firstPixel + row * sourceStride;
    worldFrame.data.set(source.subarray(sourceStart, sourceStart + rowBytes), row * rowBytes);
  }
  worldContext.putImageData(worldFrame, 0, 0);
  return true;
}

function copyEngineFrame() {
  const width = latestState.ready ? WORLD_VIEW.width : LEGACY_SCREEN.width;
  const height = latestState.ready ? WORLD_VIEW.height : desktopLayout.matches ? LEGACY_SCREEN.height : LEGACY_SCREEN.width;
  if (canvas.width !== width || canvas.height !== height) {
    canvas.width = width;
    canvas.height = height;
    worldContext.imageSmoothingEnabled = false;
  }
  canvas.classList.toggle("intro-view", !latestState.ready);
  if (latestState.ready) {
    copyWorldPixels();
  } else if (engineStarted) {
    worldContext.fillStyle = "#05080c";
    worldContext.fillRect(0, 0, canvas.width, canvas.height);
    const top = Math.floor((canvas.height - LEGACY_SCREEN.height) / 2);
    worldContext.drawImage(engineCanvas, 0, 0, engineCanvas.width, engineCanvas.height, 0, top, LEGACY_SCREEN.width, LEGACY_SCREEN.height);
  }

  requestAnimationFrame(copyEngineFrame);
}

requestAnimationFrame(copyEngineFrame);

function setImportStatus(message, state = "") {
  ui.importStatus.textContent = message;
  ui.importStatus.dataset.state = state;
  for (const input of [ui.gameFolderInput,ui.gameZipInput,ui.gameUrlInput,...ui.gameUrlForm.querySelectorAll("button")]) input.disabled = state === "busy";
}


async function startEngine(restoreSave) {
  if (engineStarted || !engine.hasGameData()) return false;
  conversationHistory = [];
  try {
    const saved = JSON.parse(runtimeModule().FS.readFile("/home/web_user/.xu4/conversations.json", {encoding:"utf8"}));
    if (restoreSave && Array.isArray(saved)) conversationHistory = saved.filter(entry => entry && typeof entry.text === "string" && entry.text.length <= 8192 && typeof entry.location === "string" && entry.location.length <= 128 && typeof entry.session === "string" && entry.session.length <= 128 && typeof entry.speaker === "string" && entry.speaker.length <= 128 && Number.isSafeInteger(entry.moves)).slice(-500);
  } catch {}
  conversationHistoryRevision++;
  renderConversationHistory();
  engineStarted = true;
  ui.engineStatus.querySelector("span").textContent = restoreSave ? "Restoring the party…" : "Opening the Book of History…";
  try {
    await sessionOrchestrator.start({sessionOptions:{restoreSave},listener:render,onError:handleEngineReadError,interval:160});
  } catch (error) {
    engineStarted = false;
    console.error(error);
    ui.engineStatus.querySelector("span").textContent = error.name === "SessionLeaseUnavailableError"
      ? "This game is already active in another tab."
      : "The game session could not be started.";
    toast(error.message);
    return false;
  }
  return true;
}

function showDataDialog(required = false) {
  ui.dataDialog.classList.toggle("required", required);
  ui.closeDataButton.hidden = required;
  if (!ui.dataDialog.open) ui.dataDialog.showModal();
}

function openAccountFromData() {
  const required=ui.dataDialog.classList.contains("required");
  ui.dataDialog.close();
  const returnFromAccount=async()=>{
    if(required&&engine.hasGameData())await adventures.prepare();
    else showDataDialog(required);
  };
  if(!adventures.openAccount(returnFromAccount))showDataDialog(required);
}

async function activateGamePackage(record, description) {
  setImportStatus("Checking the complete DOS/EGA game data…", "busy");
  const {verified} = await installationOrchestrator.install(record);
  const notes = ` Verified ${verified.verification.label} data.${verified.verification.otherDirectories ? ` Using ${verified.verification.directory || "the ZIP root"}; other folders were not imported.` : ""}`;
  if (engineStarted) {
    setImportStatus(`${description} is saved in this browser. Reload to begin with it.${notes}`, "success");
    ui.reloadDataButton.hidden = false;
    return;
  }
  setImportStatus(`${description} is ready.${notes} Continue a saved game or start a new one.`, "success");
  ui.dataDialog.close();
  await adventures.prepare();
}

async function localGameDataForCloud() {
  const saved = await library.get("source-data");
  if (!saved) return {available:false};
  if (saved.kind === "zip") await waitForRuntimeFilesystem();
  const {verified} = await gameDataImporter.prepare(saved);
  return {available:true,profile:verified.profile,label:verified.verification.label,logicalBytes:verified.files.reduce((sum,file)=>sum+file.data.byteLength,0),text:window.UltimatumGameData.encodePackage(verified)};
}

async function installCloudGameData(text) {
  await waitForRuntimeFilesystem();
  await installationOrchestrator.installPackage(text);
  if (engineStarted) {
    ui.reloadDataButton.hidden = false;
    return {reloadRequired:true};
  }
  return {reloadRequired:false};
}

async function fetchZip(url) {
  const parsed = new URL(url);
  if (!["http:", "https:"].includes(parsed.protocol)) throw new Error("Use an HTTP or HTTPS address.");
  const response = await fetch(parsed.href, { mode: "cors" });
  if (!response.ok) throw new Error(`The ZIP server returned ${response.status}.`);
  const declaredSize = Number(response.headers.get("content-length") || 0);
  if (declaredSize > MAX_ZIP_BYTES) throw new Error("That ZIP is larger than the 128 MB browser import limit.");
  if (!response.body) {
    const buffer = await response.arrayBuffer();
    if (buffer.byteLength > MAX_ZIP_BYTES) throw Error("That ZIP is larger than the 128 MB browser import limit.");
    return buffer;
  }
  const reader = response.body.getReader(), chunks = [];
  let size = 0;
  while (true) {
    const {done, value} = await reader.read();
    if (done) break;
    size += value.length;
    if (size > MAX_ZIP_BYTES) {await reader.cancel(); throw Error("That ZIP is larger than the 128 MB browser import limit.");}
    chunks.push(value);
  }
  const bytes = new Uint8Array(size);
  let offset = 0;
  for (const chunk of chunks) {bytes.set(chunk, offset); offset += chunk.length;}
  return bytes.buffer;
}

async function prepareRuntime() {
  runtimeReady = true;
  // A failed legacy restore is not permission to migrate the bundled seed
  // over the player's existing browser adventure.
  await engine.initializeSaveStorage();
  ui.adventureData.disabled = false;
  ui.engineStatus.querySelector("span").textContent = "Opening the saved game…";
  try {
    const [libraryEntry, savedVga] = await Promise.all([library.inspect("ultima4","xu4"), library.get("optional-overlay")]);
    const savedGame = libraryEntry.sourceData;
    const restoreGameSave = !engine.hasSave();
    if (savedGame) {
      const entries = savedGame.kind === "zip" ? engine.extractGameZip(savedGame.data, MAX_ZIP_BYTES) : savedGame.files;
      const {verified} = await gameDataImporter.prepare(savedGame);
      engine.replaceGameFiles(verified.files).commit();
      // Existing classic saves are independent of the game-data library.
      // Legacy embedded saves migrate only if no working save exists.
      if (restoreGameSave && !savedGame.version) {
        const saveFiles = Object.fromEntries(entries.filter(file => ["PARTY.SAV", "MONSTERS.SAV", "OUTMONST.SAV", "DNGMAP.SAV"].includes(file.name.toUpperCase())).map(file => [file.name.toLowerCase(),new Uint8Array(file.data)]));
        if (saveFiles["party.sav"] && saveFiles["monsters.sav"]) {engine.validateAdventure(saveFiles); engine.writeAdventureFiles(saveFiles);}
      }
    }
    if (restoreGameSave && savedGame) await engine.persistSaves();
    if (savedVga) {await window.UltimatumGameData.validateVga(savedVga); engine.installZip(savedVga, 2, MAX_ZIP_BYTES);}
  } catch (error) {
    console.warn("Could not restore the local game library", error);
    setImportStatus(`${error.message} Select compatible game data to continue. Your saves are unchanged.`, "error");
  }
  const accountRequested=new URLSearchParams(location.search).get("account")==="1";
  if (engine.hasGameData()) {
    await adventures.prepare();
    if(accountRequested)adventures.openAccount();
  }
  else {
    ui.engineStatus.querySelector("span").textContent = "Choose thy local Ultima IV game data to continue.";
    if (ui.importStatus.dataset.state !== "error") setImportStatus("Choose a game folder or ZIP. Nothing will be uploaded to us.");
    const showRequiredData=()=>showDataDialog(true);
    if(accountRequested) {
      if(!adventures.openAccount(showRequiredData))showRequiredData();
    } else if(!adventures.deferUntilAccountClose(showRequiredData))showRequiredData();
  }
}

function sendKey(key) {
  if (!engineStarted) return toast("Continue a saved game or start a new one first.");
  engine.sendKey(key);
  canvas.focus({ preventScroll: true });
}

function openGameMenu() {
  if (!engineReady || !engine.openMenu()) return toast("Finish the current choice before opening Menu.");
  closeMobilePanel();
}

async function flushMenuStorage() {
  if (storageFlushPending) { storageDirty = true; return; }
  storageFlushPending = true;
  try {
    do { storageDirty = false; await engine.persistSaves(); } while (storageDirty);
  } catch (error) {
    toast("Browser storage did not finish saving your change. Keep this tab open and try Save again.");
    console.error(error);
  } finally { storageFlushPending = false; }
}

function sendText(value) {
  const message = value.trim();
  if (!message || (!engineReady && !latestState.prompt?.id)) return false;
  const accepted = latestState.prompt?.id
    ? engine.submitAnswer(message, latestState.prompt.id)
    : engine.submitPrompt(message);
  if (!accepted) {
    toast("The engine is not awaiting a written answer.");
    return false;
  }
  recordConversationReply(message);
  canvas.focus({ preventScroll: true });
  return true;
}

function endConversation(promptId = latestState.prompt?.id) {
  if (!engineReady) return false;
  const accepted = promptId
    ? engine.submitAnswer("bye", promptId)
    : engine.endConversation();
  if (!accepted) {
    toast("The conversation has already moved on.");
    return false;
  }
  conversationExit = { requestedAt: performance.now() };
  recordConversationReply("Goodbye");
  ui.conversationInput.disabled = true;
  ui.conversationSubmit.disabled = true;
  ui.topicRow.querySelectorAll("button").forEach(button => { button.disabled = true; });
  canvas.focus({ preventScroll: true });
  return true;
}

function submitOption(key) {
  if (!engineReady) return toast("The engine is still waking up.");
  if (!engine.submitOption(key)) return toast("That choice is no longer available.");
  canvas.focus({ preventScroll: true });
}

function activatePrimaryAction() {
  if (!engineReady) return toast("The engine is still waking up.");
  if (!engine.activatePrimaryAction()) return toast("There is nothing to interact with here.");
  canvas.focus({ preventScroll: true });
}

function tapWorld(event) {
  if (!engineReady || latestState.inputMode !== "command" || !latestState.capabilities?.worldTap) return;
  const bounds = canvas.getBoundingClientRect();
  if (!bounds.width || !bounds.height) return;
  const sourceX = (event.clientX - bounds.left) * WORLD_VIEW.width / bounds.width;
  const sourceY = (event.clientY - bounds.top) * WORLD_VIEW.height / bounds.height;
  const offsetX = Math.max(-5, Math.min(5, Math.floor(sourceX / 16) - 5));
  const offsetY = Math.max(-5, Math.min(5, Math.floor(sourceY / 16) - 5));
  if (!offsetX && !offsetY) return toast("Thou art already here. Use Interact for the current tile.");
  if (!engine.tapWorld(offsetX, offsetY)) return toast("Tap walking is unavailable here.");
  canvas.focus({ preventScroll: true });
}

function renderMinimap(state) {
  const pixels = engine.mapPixels(state.minimap);
  const available = Boolean(pixels);
  ui.minimapButton.disabled = ui.mapButton.disabled = !available;
  document.querySelectorAll("[data-open-map]").forEach(button => { button.disabled = !available; });
  if (!available) return;
  const context = ui.minimapCanvas.getContext("2d", {alpha:false});
  context.putImageData(new ImageData(pixels, state.minimap.width, state.minimap.height), 0, 0);
  ui.minimapButton.setAttribute("aria-label", state.minimap.kind === 2 ? "Open explored dungeon floor map" : "Open exploration map");
}

function renderCombatTargets(state) {
  ui.combatOverlay.replaceChildren();
  if (!state.combat?.active || state.inputMode !== "combat") return;
  const canvasBounds = canvas.getBoundingClientRect();
  const stageBounds = ui.worldStage.getBoundingClientRect();
  for (const target of state.combat.targets || []) {
    const button = document.createElement("button");
    button.type = "button";
    button.className = "combat-target";
    button.dataset.combatTarget = String(target.token);
    button.style.left = `${canvasBounds.left - stageBounds.left + (target.x + .5) * canvasBounds.width / 11}px`;
    button.style.top = `${canvasBounds.top - stageBounds.top + (target.y + .5) * canvasBounds.height / 11}px`;
    button.setAttribute("aria-label", `${target.selected ? "Selected target" : "Target"}: ${target.name}, ${target.distance} ${target.distance === 1 ? "tile" : "tiles"} ${target.direction}`);
    button.setAttribute("aria-pressed", String(Boolean(target.selected)));
    ui.combatOverlay.append(button);
  }
}

function renderGameplayMode(state) {
  const combat = Boolean(state.combat?.active);
  const dungeon = Boolean(state.dungeon?.active);
  const interactive = (combat && state.inputMode === "combat") || (dungeon && state.inputMode === "command");
  document.body.dataset.gameMode = combat ? "combat" : dungeon ? "dungeon" : "exploration";
  ui.modeCommandBar.hidden = !(combat || dungeon);
  ui.combatActions.hidden = !combat;
  ui.dungeonActions.hidden = !dungeon || combat;
  ui.modeCommandBar.querySelectorAll("button").forEach(button => { button.disabled = !interactive; });
  if (combat) {
    const active = (state.party || []).find(member => member.active);
    const selected = (state.combat.targets || []).find(target => target.selected);
    ui.modeStatus.textContent = `${active?.name || "Companion"}'s turn · ${active?.weapon || "weapon"}${selected ? ` · Target: ${selected.name}, ${selected.distance} ${selected.distance === 1 ? "tile" : "tiles"} ${selected.direction}` : " · Choose a highlighted target"}`;
    ui.mobileWorldStatus.textContent = `${active?.name || "Companion"}'s turn · ${active?.status || "Ready"}\nHP ${active?.hp ?? "—"}/${active?.maxHp ?? "—"} · Magic ${active?.mp ?? "—"} · ${active?.weapon || "Weapon"}\nTarget: ${selected?.name || "none"}`;
    ui.combatAttack.disabled = !interactive || !state.combat.selected;
    ui.combatClear.disabled = !interactive || !state.combat.prepared;
    ui.combatRepeat.disabled = !interactive || !state.combat.repeatTarget;
    ui.combatRepeat.title = state.combat.repeatTarget ? `Prepare another attack on ${state.combat.repeatTarget}` : "No reachable previous target";
  } else if (dungeon) {
    ui.modeStatus.textContent = `${state.location?.name || "Dungeon"} · Level ${state.dungeon.level} · Facing ${state.dungeon.facing} · ${state.dungeon.light ? `Light ${state.dungeon.light}` : "Dark"} · ${state.dungeon.torches} ${state.dungeon.torches === 1 ? "torch" : "torches"}`;
    ui.mobileWorldStatus.textContent = `${state.location?.name || "Dungeon"} · L${state.dungeon.level} · ${state.dungeon.facing}\n${state.dungeon.light ? `Lit ${state.dungeon.light}` : "Dark"} · ${state.dungeon.torches} ${state.dungeon.torches === 1 ? "torch" : "torches"} · Food ${state.food ?? "—"} · Gold ${state.gold ?? "—"}`;
    ui.dungeonView.textContent = state.dungeon.overhead ? "3D view" : "Overhead";
  } else {
    ui.mobileWorldStatus.textContent = `${state.location?.name || "Britannia"} · Moons ${state.moons?.trammel ?? "—"}/${state.moons?.felucca ?? "—"} · Wind ${state.wind || "—"}\nFood ${state.food ?? "—"} · Gold ${state.gold ?? "—"} · Moves ${state.moves ?? "—"}`;
  }
  const relativeDungeon = dungeon && !combat && !state.dungeon.overhead;
  const labels = relativeDungeon
    ? {north:["Forward","↑"],south:["Back","↓"],west:["Turn left","↶"],east:["Turn right","↷"]}
    : {north:["North","↑"],south:["South","↓"],west:["West","←"],east:["East","→"]};
  for (const [move,[label,symbol]] of Object.entries(labels)) {
    document.querySelectorAll(`.dpad [data-move="${move}"]`).forEach(button => {
      button.textContent = symbol;
      button.setAttribute("aria-label", label);
    });
  }
  renderMobileActions(state, combat, dungeon, interactive);
  renderCombatTargets(state);
}

function setMobileAction(button, label, action, disabled = false) {
  button.textContent = label;
  button.dataset.mobileAction = action;
  button.disabled = Boolean(disabled);
}

function renderMobileActions(state, combat, dungeon, interactive) {
  const blocked = !engineReady || !["command", "combat"].includes(state.inputMode);
  ui.mobileDpadCenter.hidden = !combat;
  if (combat) {
    setMobileAction(ui.mobileDpadCenter, "Repeat attack", "combat-repeat", !interactive || !state.combat.repeatTarget);
    ui.mobileDpadCenter.title = state.combat.repeatTarget ? `Prepare another attack on ${state.combat.repeatTarget}` : "No reachable previous target";
    setMobileAction(ui.mobileAction3, "Prev target", "combat-prev", !interactive || !(state.combat.targets || []).length);
    setMobileAction(ui.mobileAction5, "Next target", "combat-next", !interactive || !(state.combat.targets || []).length);
    setMobileAction(ui.mobileAction6, "Clear", "combat-clear", !interactive || !state.combat.prepared);
    setMobileAction(ui.mobileAction7, "Attack", "combat-attack", !interactive || !state.combat.selected);
    return;
  }
  if (dungeon) {
    setMobileAction(ui.mobileAction3, "Torch", "dungeon-torch", !interactive || state.dungeon.torches < 1);
    setMobileAction(ui.mobileAction5, state.primaryAction?.label || "Interact", "primary", !interactive || !state.primaryAction?.enabled);
    setMobileAction(ui.mobileAction6, state.dungeon.overhead ? "3D view" : "Overhead", "dungeon-view", !interactive);
    setMobileAction(ui.mobileAction7, "Search", "dungeon-search", !interactive);
    return;
  }
  setMobileAction(ui.mobileAction3, "Journal", "journal", blocked);
  setMobileAction(ui.mobileAction5, state.primaryAction?.label || "Interact", "primary", blocked || !state.primaryAction?.enabled);
  setMobileAction(ui.mobileAction6, "Map", "map", blocked || !state.minimap);
  setMobileAction(ui.mobileAction7, "Talk", "talk", blocked);
}

function activateMobileAction(action) {
  if (!action) return;
  const gameplay = (code, parameter = 0) => {
    if (!engine.gameplayAction(code, parameter)) toast("That action is no longer available.");
    canvas.focus({preventScroll:true});
  };
  if (action === "journal") return journalUI.open(ui.mobileAction3);
  if (action === "map") return openExplorationMap();
  if (action === "primary") return activatePrimaryAction();
  if (action === "talk") return sendKey("t".charCodeAt(0));
  if (action === "combat-prev") return gameplay(GAMEPLAY_ACTION.combatCycle, -1);
  if (action === "combat-next") return gameplay(GAMEPLAY_ACTION.combatCycle, 1);
  if (action === "combat-clear") return gameplay(GAMEPLAY_ACTION.combatClear);
  if (action === "combat-repeat") return gameplay(GAMEPLAY_ACTION.combatRepeat);
  if (action === "combat-attack") return sendKey("a".charCodeAt(0));
  if (action === "dungeon-torch") return gameplay(GAMEPLAY_ACTION.dungeonTorch);
  if (action === "dungeon-view") return gameplay(GAMEPLAY_ACTION.dungeonView);
  if (action === "dungeon-search") return gameplay(GAMEPLAY_ACTION.dungeonSearch);
}

function renderMapMarkers(map) {
  ui.mapMarkers.replaceChildren();
  const entries = map.kind === 1 ? [...(latestState.mapDiscoveries || []), ...(latestState.mapPins || []).map(pin => ({...pin,pin:true}))] : [];
  for (const entry of entries) {
    const marker = document.createElement("i");
    marker.className = `map-marker${entry.pin ? " pin" : ""}`;
    marker.style.left = `${(entry.x + .5) / map.width * 100}%`;
    marker.style.top = `${(entry.y + .5) / map.height * 100}%`;
    marker.title = entry.label || entry.name || "Discovered place";
    ui.mapMarkers.append(marker);
  }
  const player = document.createElement("i");
  player.className = "map-marker";
  player.style.left = `${(map.x + .5) / map.width * 100}%`;
  player.style.top = `${(map.y + .5) / map.height * 100}%`;
  ui.mapMarkers.append(player);
}

function openExplorationMap() {
  if (!engineReady) return toast("The engine is still waking up.");
  const map = engine.explorationMap();
  if (!map) return toast("The exploration map is unavailable here.");
  currentExplorationMap = map;
  selectedMapCell = null;
  ui.mapPinEditor.hidden = true;
  ui.explorationMapCanvas.width = map.width;
  ui.explorationMapCanvas.height = map.height;
  ui.explorationMapCanvas.getContext("2d", {alpha:false}).putImageData(new ImageData(map.pixels, map.width, map.height), 0, 0);
  ui.mapTitle.textContent = map.kind === 2 ? `${latestState.location?.name || "Dungeon"} · Level ${latestState.dungeon?.level || 1}` : "Exploration map";
  ui.mapSubtitle.textContent = map.kind === 2 ? "North-up view of this adventure's explored cells on the current floor." : "Only explored terrain and visited places are shown. Select an explored cell to add or edit a pin.";
  ui.mapLegend.textContent = map.kind === 2 ? "Red marks the party. Cyan marks ladders; gold marks rooms and landmarks." : `${(latestState.mapDiscoveries || []).length} discovered places · ${(latestState.mapPins || []).length}/24 pins · red marks the party.`;
  renderMapMarkers(map);
  ui.mapDialog.showModal();
  ui.mapClose.focus({preventScroll:true});
}

function closeExplorationMap() {
  if (!ui.mapDialog.open) return;
  ui.mapDialog.close();
  canvas.focus({preventScroll:true});
}

function chooseMapCell(event) {
  if (!currentExplorationMap || currentExplorationMap.kind !== 1 || !latestState.capabilities?.mapPins) return;
  const bounds = ui.explorationMapCanvas.getBoundingClientRect();
  const x = Math.max(0, Math.min(currentExplorationMap.width - 1, Math.floor((event.clientX - bounds.left) * currentExplorationMap.width / bounds.width)));
  const y = Math.max(0, Math.min(currentExplorationMap.height - 1, Math.floor((event.clientY - bounds.top) * currentExplorationMap.height / bounds.height)));
  selectedMapCell = {x,y};
  const existing = (latestState.mapPins || []).find(pin => pin.x === x && pin.y === y);
  ui.mapPinCoordinates.textContent = `${x}, ${y}`;
  ui.mapPinLabel.value = existing?.label || "";
  ui.mapPinRemove.hidden = !existing;
  ui.mapPinEditor.hidden = false;
  ui.mapPinLabel.focus({preventScroll:true});
}

function toast(message) {
  ui.toast.textContent = message;
  ui.toast.hidden = false;
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => { ui.toast.hidden = true; }, 2600);
}

function escapeHtml(value) {
  return String(value ?? "").replace(/[&<>"']/g, character => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[character]));
}

function healthClass(member) {
  if (member.status !== "Good") return member.status.toLowerCase();
  const ratio = member.maxHp ? member.hp / member.maxHp : 0;
  return ratio < 0.3 ? "critical" : ratio < 0.65 ? "wounded" : "healthy";
}

function renderParty(party) {
  if (latestState.inputMode === "player") selectedPartyMember = null;
  ui.partyCount.textContent = `${party.length} ${party.length === 1 ? "member" : "members"}`;
  if (ui.mobilePartyCount) ui.mobilePartyCount.textContent = String(party.length);
  ui.partyList.replaceChildren();
  party.forEach((member, index) => {
    const hp = member.maxHp ? Math.max(0, Math.min(100, member.hp / member.maxHp * 100)) : 0;
    const mp = member.maxMp ? Math.max(0, Math.min(100, member.mp / member.maxMp * 100)) : 0;
    const selectable = latestState.inputMode === "player";
    const card = document.createElement("button");
    card.className = `party-member ${member.active ? "active" : ""}`;
    card.type = "button";
    if (selectable) {
      card.dataset.partyChoice = String(index + 1);
      card.setAttribute("aria-label", `Choose ${member.name}`);
    } else {
      card.dataset.partyMember = String(index);
      card.setAttribute("aria-label", `View ${member.name}'s equipment`);
    }
    card.innerHTML = `
      <div class="member-index">${index + 1}</div>
      <div class="member-main"><header><div><strong>${escapeHtml(member.name)}</strong><span>${escapeHtml(member.class)} · Level ${member.level}</span></div><em class="${healthClass(member)}">${escapeHtml(member.status)}</em></header>
      <div class="member-stat"><span>HP <b>${member.hp}/${member.maxHp}</b></span><i><u style="width:${hp}%"></u></i></div>
      <div class="member-stat magic"><span>MP <b>${member.mp}/${member.maxMp}</b></span><i><u style="width:${mp}%"></u></i></div>
      <footer><span>⚔ ${escapeHtml(member.weapon)}</span><span>◇ ${escapeHtml(member.armor)}</span><span>${member.xp} XP</span></footer></div>`;
    ui.partyList.append(card);
  });
  renderPartyDetail(party);
}

function inventoryItem(category, type) {
  const collection = category === 1 ? latestState.inventory?.weapons : latestState.inventory?.armor;
  return collection?.find(item => item.type === type);
}

function equipmentChoices(member, category) {
  const choices = category === 1 ? member.weaponChoices : member.armorChoices;
  const equipped = category === 1 ? member.weaponType : member.armorType;
  const canEquip = category === 1 ? member.canEquipWeapon : member.canEquipArmor;
  return (choices || []).map(type => {
    const item = inventoryItem(category, type) || { type, name: "Unknown", count: 0 };
    const current = type === equipped;
    return `<button type="button" data-equip-category="${category}" data-equip-type="${type}" ${current || !canEquip ? "disabled" : ""}>
      <span><b>${escapeHtml(item.name)}</b><small>${current ? "Equipped" : type === 0 ? "Unequip" : `${item.count} spare`}</small></span>${current ? "<em>Current</em>" : "<i>Equip</i>"}
    </button>`;
  }).join("");
}

function renderPartyDetail(party) {
  if (selectedPartyMember != null && !party[selectedPartyMember]) selectedPartyMember = null;
  const member = selectedPartyMember == null ? null : party[selectedPartyMember];
  ui.partyHeading.hidden = Boolean(member);
  ui.partyList.hidden = Boolean(member);
  ui.partyDetail.hidden = !member;
  if (!member) {
    partyDetailSignature = "";
    return;
  }
  const markup = `
    <header><span class="eyebrow">${escapeHtml(member.class)} · LEVEL ${member.level}</span><h2>${escapeHtml(member.name)}</h2><p>${escapeHtml(member.status)} · ${member.hp}/${member.maxHp} HP · ${member.mp}/${member.maxMp} MP</p></header>
    <div class="equipment-section"><h3>Weapon</h3><div class="equipment-choices">${equipmentChoices(member, 1)}</div></div>
    <div class="equipment-section"><h3>Armour</h3><div class="equipment-choices">${equipmentChoices(member, 2)}</div></div>`;
  if (markup !== partyDetailSignature) {
    ui.partyDetailContent.innerHTML = markup;
    partyDetailSignature = markup;
  }
}

function renderSpells(spells) {
  const ready = spells.filter(spell => spell.mixtures > 0).length;
  ui.spellCount.textContent = `${ready} prepared`;
  if (ui.mobileSpellCount) ui.mobileSpellCount.textContent = String(ready);
  ui.spellList.replaceChildren();
  spells.forEach(spell => {
    const button = document.createElement("button");
    button.type = "button";
    button.disabled = spell.mixtures < 1 || !spell.available;
    button.dataset.spell = spell.letter;
    button.innerHTML = `<span>${spell.letter}</span><b>${escapeHtml(spell.name)}</b><small>${spell.mixtures} mixed · ${spell.mp} MP</small>`;
    ui.spellList.append(button);
  });
}

function isRepeatedConversationPrompt(message) {
  return /^your interest\s*:\s*$/i.test(message.trim());
}

function readableProse(text) {
  // DOS hard wraps are presentation, not paragraphs. Keep deliberate blank
  // lines while letting HTML wrap each paragraph to the available width.
  return String(text || "").trim().split(/\n\s*\n+/).map(paragraph => paragraph.replace(/\s*\n\s*(?![A-Z][- ](?:[A-Z])|\d+[.)] |[-•] )/g, " ")).join("\n\n");
}

function appendConversationEntry(text, speaker, state = latestState) {
  conversationHistory.push({session:conversationSession, location:(state.location?.name || "Britannia").slice(0,128), moves:state.moves, text:text.slice(0,8192), speaker:(speaker || "Conversation").slice(0,128)});
  conversationHistory = conversationHistory.slice(-500);
  conversationHistoryRevision++;
  try {
    runtimeModule().FS.writeFile("/home/web_user/.xu4/conversations.json", JSON.stringify(conversationHistory));
  } catch { toast("Conversation history could not be recorded. Export your saved game before leaving."); }
  renderConversationHistory();
}

function recordConversationReply(text) {
  if (conversationSession) appendConversationEntry(readableProse(text), "You");
}

function renderConversationHistory(state = latestState) {
  const prompt = state.prompt || {};
  const live = desktopLayout.matches && Boolean(prompt.id || state.interactionActive);
  const context = live ? readableProse(prompt.context) : "";
  const renderKey = `${conversationHistoryRevision}:${desktopLayout.matches}:${live}:${prompt.id}:${context}:${state.peerMap?.requestId || ""}`;
  if (renderKey === conversationHistoryRenderKey) return;
  conversationHistoryRenderKey = renderKey;
  const log = ui.conversationHistory;
  const scrollTop = log.scrollTop;
  const follow = log.scrollHeight - log.clientHeight - scrollTop < 48;
  const activeIndex = context && conversationSession ? conversationHistory.findLastIndex(entry => entry.session === conversationSession && entry.speaker !== "You" && entry.text === context) : -1;
  log.replaceChildren();
  const addEntry = (entry, active = false) => {
    const article = document.createElement("article");
    article.classList.toggle("player-reply", entry.speaker === "You");
    if (active) { article.id = "activeConversationMessage"; article.setAttribute("aria-current", "true"); }
    const label = document.createElement("span");
    label.textContent = `${entry.location} · ${entry.speaker || "Conversation"} · Move ${entry.moves}`;
    const text = document.createElement("p");
    text.textContent = entry.speaker === "You" ? entry.text : entry.text.replace(/\s*Your Interest[:?]?\s*$/i, "").trim();
    article.append(label, text);
    if (active && !ui.peerMapCanvas.hidden) {
      const map = document.createElement("canvas");
      map.width = ui.peerMapCanvas.width; map.height = ui.peerMapCanvas.height;
      map.className = "conversation-peer-map";
      map.setAttribute("aria-label", "Peer map");
      map.getContext("2d").drawImage(ui.peerMapCanvas, 0, 0);
      article.append(map);
    }
    log.append(article);
  };
  if(live && conversationSession)conversationHistory.forEach((entry, index) => {
    if(entry.session===conversationSession)addEntry(entry,index===activeIndex);
  });
  // Creation, menus and other temporary prompts belong in the same stream,
  // but must not be saved as NPC conversation history.
  if (context && activeIndex < 0) addEntry({location:state.location?.name || "Britannia", speaker:prompt.title, moves:state.moves, text:context}, true);
  if (!log.childElementCount) log.innerHTML = '<div class="empty-state">No active conversation. Earlier discoveries and transcripts are in the Journal.</div>';
  if (live && context) conversationDock.setAttribute("aria-describedby", "activeConversationMessage");
  else conversationDock.removeAttribute("aria-describedby");
  log.scrollTop = follow ? log.scrollHeight : scrollTop;
}

function recordConversation(state) {
  const prompt = state.prompt;
  const isConversation = state.ready && state.interactionActive && prompt?.kind !== "menu" && (conversationSession || /conversation|companion|shop|lord british|hawkwind|beggar/i.test(prompt?.title || ""));
  if (!state.interactionActive) conversationSession = null;
  if (!isConversation) return;
  conversationSession ||= `${Date.now()}-${prompt.id}`;
  const text = readableProse(prompt.context);
  const signature = `${conversationSession}:${prompt.id}:${text}`;
  if (!text || recordedConversationPrompt === signature || prompt.submitted) return;
  recordedConversationPrompt = signature;
  const previous = conversationHistory.at(-1);
  if (previous?.session === conversationSession && previous.text === text) return;
  appendConversationEntry(text, prompt.title, state);
}

function renderMessages(messages, inputMode, eventMessages = messages) {
  // The classic renderer uses edge newlines to position its cursor. They are
  // padding, not content, in HTML; preserve all interior paragraph breaks.
  const useful = messages.filter(Boolean).map(readableProse).filter(Boolean);
  const latest = inputMode === "text"
    ? [...useful].reverse().find(message => !isRepeatedConversationPrompt(message)) || useful.at(-1)
    : useful.at(-1);
  ui.latestMessage.textContent = latest || "Awaiting the first command.";
  ui.desktopLatestMessage.textContent = latest || "Awaiting the first command.";
  const mobileLines = eventMessages
    .filter(Boolean)
    .flatMap(message => readableProse(message).split(/\n+/))
    .map(line => line.trim())
    .filter(Boolean)
    .slice(-3);
  ui.mobileWorldLog.replaceChildren(...mobileLines.map(line => {
    const row = document.createElement("span");
    row.textContent = line;
    return row;
  }));
  ui.messageLog.replaceChildren();
  const events=eventMessages.filter(Boolean).map(readableProse).filter(Boolean);
  [...events].reverse().forEach((message, index) => {
    const article = document.createElement("article");
    article.innerHTML = `<span>${index === 0 ? "NOW" : "EARLIER"}</span><p>${escapeHtml(message)}</p>`;
    ui.messageLog.append(article);
  });
  if (!events.length) ui.messageLog.innerHTML = '<div class="empty-state">Recent game actions and feedback will appear here.</div>';
}

function renderPrimaryAction(action) {
  const label = action?.label || "Interact";
  const blockedByPrompt = engineReady && !["command", "combat"].includes(latestState.inputMode);
  const enabled = engineReady && !blockedByPrompt && Boolean(action?.enabled);
  const hints = {
    "Open Chest": "Take what is here",
    "Open Door": "Open the way",
    "Locked Door": "Requires a key",
    Talk: "Speak with thy neighbor",
    Enter: "Go into this place",
    Climb: "Go upward",
    Descend: "Go downward",
    Board: "Use this transport",
    Dismount: "Continue on foot",
    Disembark: "Leave the ship",
    Ascend: "Take to the air",
    Land: "Return to the ground",
  };
  ui.primaryActionLabel.textContent = label;
  ui.primaryActionHint.textContent = enabled ? (hints[label] || "Use what is nearby") : blockedByPrompt ? "Finish the current choice" : "Nothing nearby";
  ui.primaryActionButton.disabled = !enabled;
  ui.primaryActionButton.setAttribute("aria-label", enabled ? label : "Interact unavailable");
}

function renderPromptActions(prompt) {
  const options = prompt?.options || [];
  const signature = JSON.stringify(prompt);
  if (ui.promptActions.dataset.signature === signature) return options.length > 0;
  ui.promptActions.dataset.signature = signature;
  ui.promptActions.replaceChildren();
  for (const option of options) {
    const button = document.createElement("button");
    button.type = "button";
    if (option.value !== undefined) {
      button.dataset.promptValue = option.value;
      button.dataset.promptId = String(prompt.id);
    } else button.dataset.promptKey = String(option.key);
    button.disabled = Boolean(prompt.submitted || option.disabled);
    button.setAttribute("aria-label", option.label);
    button.innerHTML = `${option.symbol ? `<span aria-hidden="true">${escapeHtml(option.symbol)}</span>` : ""}<b>${escapeHtml(option.label)}</b>`;
    ui.promptActions.append(button);
  }
  ui.promptActions.hidden = options.length === 0;
  return options.length > 0;
}

function render(state) {
  latestState = state;
  engineReady = Boolean(state.ready);
  if(state.journalOpen)return; // Keep the suspended reply, draft and focus intact.
  document.querySelectorAll('[data-open-journal]').forEach(button=>{
    button.disabled=!engineReady || state.inputMode==="combat";
    if(button.classList.contains("prompt-journal"))button.hidden=!engineReady || !state.interactionActive;
  });
  if (conversationExit) {
    const elapsed = performance.now() - conversationExit.requestedAt;
    if (["command", "combat"].includes(state.inputMode)) {
      conversationExit = null;
      toast("Conversation ended.");
    } else {
      if (elapsed >= 4000) {
        conversationExit = null;
        toast("The conversation did not close. Try Goodbye again.");
      }
    }
  }
  ui.engineStatus.hidden = engineStarted;
  ui.connectionLabel.textContent = engineReady ? "Live engine" : "Starting engine";
  ui.connectionLight.classList.toggle("connected", engineReady);
  ui.saveButton.disabled = !engineReady;
  ui.menuButton.disabled = ui.mobileMenuButton.disabled = !engineReady || !state.menuAvailable;
  if (state.recoveryDownload?.request > recoveryDownloadRequest) {
    recoveryDownloadRequest = state.recoveryDownload.request;
    const bytes = engine.recoveryBytes(state.recoveryDownload);
    if (bytes) {
      const url = URL.createObjectURL(new Blob([bytes], { type: "application/zip" }));
      const link = document.createElement("a");
      link.href = url; link.download = "ultimatum-debug-recovery.zip";
      document.body.append(link); link.click(); link.remove();
      setTimeout(() => URL.revokeObjectURL(url), 60000);
      toast("Recovery snapshot downloaded. This contains classic saves, not Ultimatum metadata.");
    } else toast("Recovery download did not finish. Try again in Debug Tools → Diagnostics.");
  }
  ui.dataButton.disabled = ui.mobileDataButton.disabled = engineReady && Boolean(state.interactionActive);
  if (engineReady && state.storageRevision > storageRevision) {
    storageRevision = state.storageRevision;
    flushMenuStorage();
  }
  if (engineReady && state.libraryRequest > libraryRequest) {
    libraryRequest = state.libraryRequest;
    openDataLibrary();
  }

  if (state.location) {
    ui.headerLocation.textContent = state.location.name;
    ui.headerContext.textContent = state.location.context;
    ui.locationName.textContent = state.location.name;
    ui.locationContext.textContent = `${state.location.context.toUpperCase()} · ${state.location.x}, ${state.location.y}${state.location.z ? ` · LEVEL ${state.location.z + 1}` : ""}`;
    ui.turnCount.textContent = state.moves.toLocaleString();
    ui.headerTurnCount.textContent = state.moves.toLocaleString();
    ui.windValue.textContent = state.wind;
    ui.resourceValue.textContent = `${state.food} · ${state.gold}`;
  }
  renderMinimap(state);
  renderGameplayMode(state);

  const hasPromptActions = renderPromptActions(state.prompt);
  const semanticPrompt = Boolean(state.prompt?.id || state.interactionActive);
  const acceptsText = semanticPrompt ? Boolean(state.prompt.acceptsText) :
    ["text", "amount", "letter", "player"].includes(state.inputMode) ||
    (state.inputMode === "choice" && !hasPromptActions);
  const conversationDock = ui.conversationForm.closest(".conversation-dock");
  conversationDock.dataset.inputMode = state.inputMode;
  const promptId = String(state.prompt?.id || "");
  if (conversationDock.dataset.promptId !== promptId) {
    ui.conversationInput.value = "";
    ui.latestMessage.scrollTop = 0;
    conversationDock.dataset.promptId = promptId;
    if (desktopLayout.matches && semanticPrompt) selectPanel("conversationsPanel");
    if (state.prompt?.kind === "menu" && !mobileLayout.matches)
      ui.promptActions.querySelector("button:not(:disabled)")?.focus({ preventScroll: true });
  }
  const exitingConversation = Boolean(conversationExit);
  ui.conversationForm.hidden = !acceptsText;
  ui.topicRow.hidden = semanticPrompt || state.inputMode !== "text";
  ui.conversationInput.disabled = exitingConversation || Boolean(state.prompt?.submitted);
  ui.conversationSubmit.disabled = exitingConversation || Boolean(state.prompt?.submitted);
  ui.topicRow.querySelectorAll("button").forEach(button => { button.disabled = exitingConversation; });
  const promptUi = {
    amount: { label: "ENTER AN AMOUNT", placeholder: "Amount", inputMode: "numeric", maxLength: 9, submit: "Enter" },
    choice: { label: "MAKE A CHOICE", placeholder: "Choice", inputMode: "text", maxLength: 1, submit: "Choose" },
    letter: { label: "CHOOSE A LETTER", placeholder: "Letter", inputMode: "text", maxLength: 1, submit: "Choose" },
    player: { label: "CHOOSE A COMPANION", placeholder: "1–8", inputMode: "numeric", maxLength: 1, submit: "Choose" },
    text: { label: "CONVERSATION", placeholder: "Type a topic or answer…", inputMode: "text", maxLength: 32, submit: "Send" },
  }[state.prompt?.kind || state.inputMode];
  ui.promptLabel.textContent = exitingConversation ? "ENDING CONVERSATION" : state.prompt?.title || (state.inputMode === "direction" ? "CHOOSE A DIRECTION" : promptUi?.label || "LATEST MESSAGE");
  if (promptUi) {
    ui.conversationInput.placeholder = state.prompt?.title === "Name your Avatar" ? "Your Avatar’s name" : promptUi.placeholder;
    ui.conversationInput.inputMode = promptUi.inputMode;
    ui.conversationInput.maxLength = state.prompt?.maxLength || promptUi.maxLength;
    ui.conversationSubmit.textContent = state.prompt?.title === "Name your Avatar" ? "Continue" : promptUi.submit;
  }
  conversationDock.classList.toggle("awaiting-input", acceptsText || hasPromptActions);
  conversationDock.classList.toggle("topic-prompt", state.inputMode === "text");
  conversationDock.classList.toggle("option-prompt", hasPromptActions);
  conversationDock.classList.toggle("semantic-prompt", semanticPrompt);
  ui.conversationPlaceholder.hidden = desktopLayout.matches || !semanticPrompt;
  conversationDock.hidden = desktopLayout.matches && !semanticPrompt;
  conversationDock.setAttribute("role", semanticPrompt && !desktopLayout.matches ? "dialog" : "region");
  if (semanticPrompt && !desktopLayout.matches) conversationDock.setAttribute("aria-modal", "true");
  else conversationDock.removeAttribute("aria-modal");
  modalBackground.forEach(region => { region.inert = semanticPrompt && !desktopLayout.matches; });
  conversationDock.classList.toggle("menu-prompt", state.prompt?.kind === "menu");
  conversationDock.classList.toggle("map-prompt", Boolean(state.peerMap));
  ui.peerMapCanvas.hidden = !state.peerMap;
  if (state.peerMap && ui.peerMapCanvas.dataset.promptId !== promptId) {
    const pixels = engine.mapPixels(state.peerMap);
    if (pixels) {
      ui.peerMapCanvas.width = state.peerMap.width;
      ui.peerMapCanvas.height = state.peerMap.height;
      const context = ui.peerMapCanvas.getContext("2d");
      context.putImageData(new ImageData(pixels, state.peerMap.width, state.peerMap.height), 0, 0);
      context.fillStyle = "#ffe09b";
      context.fillRect(Math.max(0, state.peerMap.x - 1), Math.max(0, state.peerMap.y - 1), 3, 3);
      ui.peerMapCanvas.dataset.promptId = promptId;
    }
  }
  if (acceptsText && !mobileLayout.matches && !ui.adventureDialog.open && !ui.dataDialog.open && !conversationDock.contains(document.activeElement)) {
    ui.conversationInput.focus({ preventScroll: true });
  }
  if (pendingSpell && state.inputMode === "letter") {
    const spell = pendingSpell.letter;
    pendingSpell = null;
    engine.submitPrompt(spell);
    closeMobilePanel();
  } else if (pendingSpell && performance.now() - pendingSpell.startedAt > 4000) {
    pendingSpell = null;
    toast("Casting did not reach spell selection.");
  }

  ui.videoButton.disabled = !engineReady || !state.vgaAvailable;
  ui.videoButton.textContent = state.vgaAvailable ? `${state.video.toUpperCase()} graphics` : "VGA unavailable";
  canvas.classList.toggle("world-tappable", engineReady && state.inputMode === "command" && Boolean(state.capabilities?.worldTap));
  document.querySelectorAll(".command[data-key]").forEach(button => {
    button.disabled = !engineReady || !["command", "combat"].includes(state.inputMode);
  });
  document.querySelectorAll(".dpad [data-key]").forEach(button => {
    button.disabled = !engineReady || !["command", "combat", "direction"].includes(state.inputMode);
  });
  renderPrimaryAction(state.primaryAction);
  renderParty(state.party || []);
  renderSpells(state.spells || []);
  renderMessages(state.messages || [], state.inputMode, state.events || state.messages || []);
  if (semanticPrompt) ui.latestMessage.textContent = readableProse(state.prompt.context) || "Choose thy reply.";
  recordConversation(state);
  renderConversationHistory(state);
  adventures.observe(state);
}

function handleEngineReadError(error) {
  ui.engineStatus.querySelector("span").textContent = "The engine bridge could not be read.";
  if (!engineStarted) {
    adventures.status("Loading did not finish: " + error.message + " Your existing saves have not been replaced.");
    ui.adventureRetry.hidden = false;
    ui.adventureData.disabled = true;
  }
  console.error(error);
}

function selectPanel(panelId) {
  document.querySelectorAll("[data-panel]").forEach(button => {
    button.setAttribute("aria-selected", String(button.dataset.panel === panelId));
  });
  document.querySelectorAll(".info-panel").forEach(panel => { panel.hidden = panel.id !== panelId; });
}

function openMobilePanel(panelId, trigger) {
  selectPanel(panelId);
  if (!mobileLayout.matches) return;
  panelTrigger = trigger;
  ui.sidePanel.classList.add("mobile-open");
  ui.sheetBackdrop.hidden = false;
  document.body.classList.add("panel-open");
  document.querySelectorAll("[data-open-panel]").forEach(button => {
    button.setAttribute("aria-expanded", String(button.dataset.openPanel === panelId));
  });
  ui.mobileSheetClose.focus({ preventScroll: true });
}

function closeMobilePanel() {
  if (!ui.sidePanel.classList.contains("mobile-open")) return false;
  ui.sidePanel.classList.remove("mobile-open");
  ui.sheetBackdrop.hidden = true;
  document.body.classList.remove("panel-open");
  document.querySelectorAll("[data-open-panel]").forEach(button => button.setAttribute("aria-expanded", "false"));
  panelTrigger?.focus({ preventScroll: true });
  panelTrigger = null;
  return true;
}

document.querySelectorAll("[data-key]").forEach(button => button.addEventListener("click", () => sendKey(button.dataset.key)));
ui.primaryActionButton.addEventListener("click", activatePrimaryAction);
canvas.addEventListener("click", tapWorld);
ui.combatOverlay.addEventListener("click", event => {
  const target = event.target.closest("[data-combat-target]");
  if (target && engine.gameplayAction(GAMEPLAY_ACTION.combatTarget, Number(target.dataset.combatTarget))) canvas.focus({preventScroll:true});
});
ui.modeCommandBar.addEventListener("click", event => {
  const button = event.target.closest("[data-gameplay-action]");
  if (!button || button.disabled) return;
  if (!engine.gameplayAction(Number(button.dataset.gameplayAction), Number(button.dataset.parameter || 0))) toast("That action is no longer available.");
  canvas.focus({preventScroll:true});
});
ui.combatAttack.addEventListener("click", () => sendKey("a".charCodeAt(0)));
document.querySelector(".mobile-action-grid").addEventListener("click", event => {
  const button = event.target.closest("[data-mobile-action]");
  if (button && !button.disabled) activateMobileAction(button.dataset.mobileAction);
});
ui.mobileDpadCenter.addEventListener("click", () => {
  if (!ui.mobileDpadCenter.disabled) activateMobileAction(ui.mobileDpadCenter.dataset.mobileAction);
});
document.querySelectorAll("[data-open-map]").forEach(button => button.addEventListener("click", openExplorationMap));
ui.mapButton.addEventListener("click", openExplorationMap);
ui.minimapButton.addEventListener("click", openExplorationMap);
ui.mapClose.addEventListener("click", closeExplorationMap);
ui.mapDialog.addEventListener("cancel", event => { event.preventDefault(); closeExplorationMap(); });
ui.explorationMapCanvas.addEventListener("click", chooseMapCell);
ui.mapPinCancel.addEventListener("click", () => { selectedMapCell = null; ui.mapPinEditor.hidden = true; ui.explorationMapCanvas.focus({preventScroll:true}); });
ui.mapPinSave.addEventListener("click", () => {
  const label = ui.mapPinLabel.value.trim();
  if (!selectedMapCell || !label) return toast("Name this pin before saving it.");
  if (!engine.setMapPin(selectedMapCell.x, selectedMapCell.y, label)) return toast("Pins can only be placed on explored terrain, up to 24 per adventure.");
  const pins = (latestState.mapPins || []).filter(pin => pin.x !== selectedMapCell.x || pin.y !== selectedMapCell.y);
  latestState.mapPins = [...pins, {...selectedMapCell,label}];
  renderMapMarkers(currentExplorationMap);
  ui.mapPinEditor.hidden = true;
  if (latestState.canSave) engine.saveAdventure();
  toast("Map pin saved with this adventure.");
});
ui.mapPinRemove.addEventListener("click", () => {
  if (!selectedMapCell || !engine.removeMapPin(selectedMapCell.x, selectedMapCell.y)) return toast("That pin is no longer available.");
  latestState.mapPins = (latestState.mapPins || []).filter(pin => pin.x !== selectedMapCell.x || pin.y !== selectedMapCell.y);
  renderMapMarkers(currentExplorationMap);
  ui.mapPinEditor.hidden = true;
  if (latestState.canSave) engine.saveAdventure();
  toast("Map pin removed.");
});

ui.conversationForm.addEventListener("submit", event => {
  event.preventDefault();
  const topic = event.submitter?.dataset.topic;
  const accepted = topic === "bye" ? endConversation() : sendText(topic || ui.conversationInput.value);
  if (accepted) {
    ui.conversationInput.value = "";
    event.submitter?.blur();
  }
});

ui.promptActions.addEventListener("click", event => {
  const answer = event.target.closest("[data-prompt-value]");
  if (answer && !answer.disabled) {
    if (answer.dataset.promptValue === "bye") endConversation(answer.dataset.promptId);
    else if (!engine.submitAnswer(answer.dataset.promptValue, answer.dataset.promptId)) toast("That choice is no longer available.");
    else recordConversationReply(answer.textContent.trim());
    canvas.focus({ preventScroll: true });
    return;
  }
  const key = event.target.closest("[data-prompt-key]")?.dataset.promptKey;
  if (key) submitOption(key);
});

ui.spellList.addEventListener("click", event => {
  const spell = event.target.closest("[data-spell]")?.dataset.spell;
  if (!spell) return;
  if (latestState.inputMode === "letter") {
    engine.submitPrompt(spell);
    closeMobilePanel();
    return;
  }
  if (!["command", "combat"].includes(latestState.inputMode)) return toast("Finish the current action before casting.");
  pendingSpell = { letter: spell, startedAt: performance.now() };
  engine.sendKey("c".charCodeAt(0));
});

ui.partyList.addEventListener("click", event => {
  const choice = event.target.closest("[data-party-choice]")?.dataset.partyChoice;
  if (choice) {
    sendText(choice);
    closeMobilePanel();
    return;
  }
  const member = event.target.closest("[data-party-member]")?.dataset.partyMember;
  if (member == null) return;
  selectedPartyMember = Number(member);
  renderParty(latestState.party || []);
});

ui.partyBackButton.addEventListener("click", () => {
  selectedPartyMember = null;
  renderParty(latestState.party || []);
});

ui.partyDetail.addEventListener("click", event => {
  const button = event.target.closest("[data-equip-type]");
  if (!button || selectedPartyMember == null) return;
  const result = engine.equip(selectedPartyMember, Number(button.dataset.equipCategory), Number(button.dataset.equipType));
  if (result === 1) return;
  toast(result === -4 ? "None of that equipment remains." : result === -5 ? "That companion cannot use this equipment." : "Equipment cannot be changed now.");
});

document.querySelector(".tabs").addEventListener("click", event => {
  const tab = event.target.closest("[data-panel]");
  if (!tab) return;
  selectPanel(tab.dataset.panel);
});

document.querySelectorAll("[data-open-panel]").forEach(button => {
  button.addEventListener("click", () => openMobilePanel(button.dataset.openPanel, button));
});
ui.mobileSheetClose.addEventListener("click", closeMobilePanel);
ui.sheetBackdrop.addEventListener("click", closeMobilePanel);
mobileLayout.addEventListener("change", event => { if (!event.matches) closeMobilePanel(); });

ui.saveButton.addEventListener("click", async () => {
  if (!engineReady) return toast("The engine is still waking up.");
  const saved = engine.saveAdventure();
  if (!saved) return toast("Thou canst not save here.");
  if (saved < 0) return toast("The game could not be saved.");
  ui.saveButton.disabled = true;
  try {
    adventures.saveRevision = engine.snapshot().saveRevision;
    await adventures.capture();
  } catch (error) {
    console.error(error);
    toast("Save storage failed. Export your checkpoint from Saved Games before leaving.");
  } finally {
    ui.saveButton.disabled = false;
  }
});

async function secureActiveAdventure() {
  if (latestState.canSave && adventures.activeSlot && !adventures.pendingCheckpoint) {
    const saved = engine.saveAdventure();
    if (saved > 0) {
      const secured = engine.snapshot();
      adventures.saveRevision = secured.saveRevision;
      adventures.lastSavedMoves = secured.moves;
      await adventures.capture("");
    }
  }
  await adventures.saveQueue.catch(() => {});
}

async function secureLifecycle(reason, shutdown = false) {
  if (!engineReady) return;
  if (lifecycleSecuring) {
    if (shutdown) return lifecycleSecuring.then(() => secureLifecycle(reason, true));
    return lifecycleSecuring;
  }
  const operation = shutdown ? sessionOrchestrator.shutdown(reason) : sessionOrchestrator.suspend(reason);
  lifecycleSecuring = operation.then(result => {
    sessionStorage.setItem("ultimatum-last-lifecycle-save-v1", JSON.stringify({reason,at:Date.now(),moves:latestState.moves || 0}));
    return result;
  }).catch(error => {
    console.error(`Could not secure browser lifecycle state (${reason})`,error);
  }).finally(() => {
    lifecycleSecuring = null;
  });
  return lifecycleSecuring;
}

function resumeLifecycle(reason) {
  if (!engineStarted) return;
  sessionOrchestrator.resume(reason).then(() => render(engine.snapshot())).catch(error => {
    console.error(`Could not resume browser lifecycle state (${reason})`,error);
  });
}

document.addEventListener("visibilitychange", () => {
  if (document.visibilityState === "hidden") secureLifecycle("hidden");
  else resumeLifecycle("visible");
});
window.addEventListener("pagehide", event => {
  if (!event.persisted) sessionOrchestrator.releaseLease().catch(error => console.error("Could not release the outgoing session lease",error));
  secureLifecycle("pagehide", !event.persisted);
});
window.addEventListener("pageshow", event => {
  if (event.persisted && engineStarted) {
    resumeLifecycle("pageshow");
    toast("Game resumed.");
  }
});
document.addEventListener("freeze", () => { secureLifecycle("freeze"); });
document.addEventListener("resume", () => { resumeLifecycle("resume"); });
setInterval(() => {
  if (!document.hidden && engineReady && latestState.canSave && adventures.activeSlot &&
      !adventures.pendingCheckpoint && latestState.moves !== adventures.lastSavedMoves) {
    sessionOrchestrator.requestCheckpoint("periodic").catch(error => console.error("Automatic checkpoint request failed",error));
  }
},15000);
document.addEventListener("pointerdown", () => { navigator.storage?.persist?.().catch(()=>{}); }, {once:true,capture:true});

ui.videoButton.addEventListener("click", () => {
  const nextVideo = latestState.video === "vga" ? 0 : 1;
  const changed = engine.setVideo(nextVideo);
  toast(changed ? `${nextVideo ? "VGA" : "EGA"} graphics enabled.` : "That graphics set is not available.");
});

function openDataLibrary() {
  if (!runtimeReady) return toast("The engine is still preparing. Please wait.");
  closeMobilePanel();
  setImportStatus(engineStarted ? "A new selection will be stored locally and used after reloading." : "Choose a game folder or ZIP.");
  showDataDialog(!engineStarted && !engine.hasGameData());
}

function feedbackInstallationId() {
  try {
    let id = localStorage.getItem(FEEDBACK_INSTALLATION_KEY);
    if (!/^[0-9a-f-]{36}$/i.test(id || "")) {
      id = crypto.randomUUID();
      localStorage.setItem(FEEDBACK_INSTALLATION_KEY, id);
    }
    return id;
  } catch { return crypto.randomUUID(); }
}

function setFeedbackStatus(message, state = "") {
  ui.feedbackStatus.textContent = message;
  ui.feedbackStatus.dataset.state = state;
}

let feedbackReturnDialog = null;
function openFeedback() {
  closeMobilePanel();
  setFeedbackStatus("");
  feedbackReturnDialog = [ui.adventureDialog, ui.dataDialog].find(dialog => dialog.open) || null;
  feedbackReturnDialog?.close();
  if (!ui.feedbackDialog.open) ui.feedbackDialog.showModal();
  ui.feedbackMessage.focus();
}

function closeFeedback() {
  ui.feedbackDialog.close();
  const dialog = feedbackReturnDialog;
  feedbackReturnDialog = null;
  if (dialog && !dialog.open) dialog.showModal();
}

document.querySelectorAll("#feedbackButton, [data-open-feedback]").forEach(button => button.addEventListener("click", openFeedback));
if(!cloudConfigured)document.querySelectorAll("#feedbackButton, [data-open-feedback], #dataAccount").forEach(button=>button.hidden=true);
ui.closeFeedbackButton.addEventListener("click", closeFeedback);
ui.cancelFeedbackButton.addEventListener("click", closeFeedback);
ui.feedbackDialog.addEventListener("cancel", event => { event.preventDefault(); closeFeedback(); });
ui.feedbackMessage.addEventListener("input", () => { ui.feedbackLength.textContent = ui.feedbackMessage.value.length; });
ui.feedbackForm.addEventListener("submit", async event => {
  event.preventDefault();
  if (ui.feedbackWebsite.value) return ui.feedbackDialog.close();
  const message = ui.feedbackMessage.value.trim();
  if (message.length < 20) return setFeedbackStatus("Please add a little more detail so we can act on it.", "error");
  ui.sendFeedbackButton.disabled = true;
  ui.cancelFeedbackButton.disabled = true;
  setFeedbackStatus("Sending…");
  try {
    const context = ui.feedbackDiagnostics.checked ? {
      page: location.pathname,
      build: ENGINE_BUILD,
      platform: "web",
      viewport: `${innerWidth}×${innerHeight}`,
      user_agent: navigator.userAgent.slice(0, 512),
    } : {};
    await adventures.cloudUI.cloud.submitFeedback({
      id: crypto.randomUUID(),
      installationId: feedbackInstallationId(),
      kind: new FormData(ui.feedbackForm).get("feedbackKind"),
      message,
      email: ui.feedbackEmail.value.trim() || null,
      context,
    });
    ui.feedbackMessage.value = "";
    ui.feedbackLength.textContent = "0";
    setFeedbackStatus("Thank you — your report was sent.", "success");
  } catch (error) {
    setFeedbackStatus(error.message || "The report could not be sent. Please try again or use email instead.", "error");
  } finally {
    ui.sendFeedbackButton.disabled = false;
    ui.cancelFeedbackButton.disabled = false;
  }
});

ui.dataButton.addEventListener("click", openDataLibrary);
ui.mobileDataButton.addEventListener("click", openDataLibrary);
ui.menuButton.addEventListener("click", openGameMenu);
ui.mobileMenuButton.addEventListener("click", openGameMenu);
ui.closeDataButton.addEventListener("click", () => ui.dataDialog.close());
ui.dataAccount.addEventListener("click", openAccountFromData);
ui.dataDialog.addEventListener("cancel", event => {
  if (!engineStarted && (!runtimeReady || !engine.hasGameData())) event.preventDefault();
});
ui.reloadDataButton.addEventListener("click", () => window.location.reload());

ui.gameFolderInput.addEventListener("change", async () => {
  const selected = [...ui.gameFolderInput.files];
  if (!selected.length) return;
  setImportStatus(`Reading ${selected.length} files…`, "busy");
  try {
    const candidates = gameDataImporter.select(selected.map(file => ({name:file.webkitRelativePath || file.name, size:file.size, file})));
    const files = await Promise.all(candidates.map(async entry => ({name:entry.name, data:await entry.file.arrayBuffer()})));
    await activateGamePackage({ kind: "files", files }, `The ${ui.gameFolderInput.files[0].webkitRelativePath.split("/")[0] || "selected"} folder`);
  } catch (error) {
    setImportStatus(error.message, "error");
  } finally {
    ui.gameFolderInput.value = "";
  }
});

ui.gameZipInput.addEventListener("change", async () => {
  const file = ui.gameZipInput.files[0];
  if (!file) return;
  setImportStatus(`Opening ${file.name}…`, "busy");
  try {
    if (file.size > MAX_ZIP_BYTES) throw Error("That ZIP is larger than the 128 MB browser import limit.");
    await activateGamePackage({ kind: "zip", data: await file.arrayBuffer() }, file.name);
  } catch (error) {
    setImportStatus(error.message, "error");
  } finally {
    ui.gameZipInput.value = "";
  }
});

ui.gameUrlForm.addEventListener("submit", async event => {
  event.preventDefault();
  setImportStatus("Fetching the ZIP directly into this browser…", "busy");
  try {
    const data = await fetchZip(ui.gameUrlInput.value);
    await activateGamePackage({ kind: "zip", data }, new URL(ui.gameUrlInput.value).pathname.split("/").at(-1) || "Remote ZIP");
  } catch (error) {
    const corsHelp = error instanceof TypeError ? " The source may block cross-site browser downloads; download it normally and choose the ZIP from this device instead." : "";
    setImportStatus(`${error.message}${corsHelp}`, "error");
  }
});

window.addEventListener("keydown", event => {
  if(ui.journalDialog.open || ui.mapDialog.open || ui.feedbackDialog.open)return;
  if (ui.adventureDialog.open || ui.dataDialog.open) return;
  if (!desktopLayout.matches && event.key === "Tab" && (latestState.prompt?.id || latestState.interactionActive)) {
    const controls = [...document.querySelectorAll(".conversation-dock button:not(:disabled), .conversation-dock input:not(:disabled)")].filter(control => control.getClientRects().length);
    const current = controls.indexOf(document.activeElement);
    if (controls.length && (current === -1 || (!event.shiftKey && current === controls.length - 1) || (event.shiftKey && current === 0))) {
      event.preventDefault();
      controls[event.shiftKey ? controls.length - 1 : 0].focus({ preventScroll: true });
    }
    return;
  }
  if (event.target.closest("input, textarea, select, [contenteditable='true']") || event.isComposing || event.metaKey || event.ctrlKey || event.altKey) return;
  if (event.target.matches("button") && ["Enter", " "].includes(event.key)) {
    // SDL suppresses the browser's native Enter/Space button activation.
    // Activate the focused HTML control explicitly, once, through its normal
    // click path rather than letting a compatibility key leak into the game.
    event.preventDefault();
    if (!event.repeat && !event.target.disabled) event.target.click();
    return;
  }
  if (event.key === "Escape" && closeMobilePanel()) {
    event.preventDefault();
    return;
  }
  const special = { ArrowUp: 1001, ArrowDown: 1002, ArrowLeft: 1003, ArrowRight: 1004, Escape: 27, Enter: 13 };
  const movement = !event.shiftKey && ["command", "combat", "direction"].includes(latestState.inputMode) ? {w:1001, s:1002, a:1003, d:1004}[event.key.toLowerCase()] : null;
  // The legacy command controller matches lowercase letters literally. Shift
  // is our escape hatch from WASD, not a request to send an uppercase command.
  const classicCommand = event.shiftKey && ["command", "combat"].includes(latestState.inputMode) && /^[wasd]$/i.test(event.key) ? event.key.toLowerCase().charCodeAt(0) : null;
  const key = movement ?? classicCommand ?? special[event.key] ?? (event.key.length === 1 ? event.key.charCodeAt(0) : null);
  if (key === null) return;
  event.preventDefault();
  sendKey(key);
});

positionPromptDock();

const configuredModule = window.UltimatumModuleConfig = window.Module = {
  canvas: engineCanvas,
  noInitialRun: true,
  locateFile(path) {
    return path.startsWith("ultimatum-engine.") ? `engine/${path}?v=${ENGINE_BUILD}` : path;
  },
  setStatus(text) {
    if (/downloading data/i.test(text)) ui.engineStatus.querySelector("span").textContent = "Downloading engine data (42 MB)…";
    else if (/running/i.test(text)) ui.engineStatus.querySelector("span").textContent = "Starting the WebAssembly engine…";
  },
  printErr(text) {
    console.error(text);
    if (/error|abort|requires/i.test(text)) ui.engineStatus.querySelector("span").textContent = text;
  },
  onRuntimeInitialized() {
    // Newer Emscripten shells may keep their `Module` binding script-local
    // after async initialization. Preserve the initialized object for the
    // platform client and lifecycle/test bridges that intentionally resolve
    // it through window.Module.
    const initializedModule = this?.ccall ? this : window.Module;
    document.ultimatumRuntimeModule = initializedModule;
    window.UltimatumRuntimeModule = window.Module = initializedModule;
    runtimeFilesystemResolve(initializedModule.FS);
    prepareRuntime().catch(handleEngineReadError);
  },
  onAbort(reason) {
    ui.engineStatus.querySelector("span").textContent = `The engine stopped: ${reason}`;
  },
};

async function loadEngine() {
  if (window.UltimatumPublicLoader) {
    ui.engineStatus.querySelector("span").textContent = "Downloading the game engine…";
    await window.UltimatumPublicLoader.prepare(configuredModule, message => {ui.engineStatus.querySelector("span").textContent = message;});
  }
  const engineScript = document.createElement("script");
  engineScript.src = window.UltimatumPublicLoader?.script || `engine/ultimatum-engine.js?v=${ENGINE_BUILD}`;
  engineScript.async = true;
  engineScript.onerror = () => { ui.engineStatus.querySelector("span").textContent = "The game engine could not be downloaded. Please reload to try again."; };
  document.body.append(engineScript);
}
loadEngine().catch(error => {ui.engineStatus.querySelector("span").textContent = `${error.message} Reload to try again.`;});
if ("serviceWorker" in navigator && location.protocol === "https:") {
  navigator.serviceWorker.register("sw.js",{scope:"./"}).catch(error => console.warn("Offline shell registration failed",error));
}
