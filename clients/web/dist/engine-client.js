(function installEngineClient(global) {
  class EngineClient {
    constructor(moduleProvider) {
      this.moduleProvider = moduleProvider;
      this.pollTimer = null;
      this.saveStorageMounted = false;
    }

    get module() {
      return this.moduleProvider();
    }

    get heap() {
      return this.module?.HEAPU8 ?? null;
    }

    call(name, returnType = null, argTypes = [], args = []) {
      if (!this.module?.ccall) return null;
      return this.module.ccall(name, returnType, argTypes, args);
    }

    pathExists(path) {
      try {
        this.module.FS.stat(path);
        return true;
      } catch {
        return false;
      }
    }

    ensureDirectory(path) {
      try {
        this.module.FS.mkdir(path);
      } catch (error) {
        // Optimized Emscripten ErrnoError objects may have no message.
        // Check the filesystem with its supported public metadata API rather
        // than interpreting diagnostics from optimized runtime errors.
        const fs = this.module.FS;
        try { if (fs.isDir(fs.stat(path).mode)) return; } catch {}
        throw error;
      }
    }

    syncSaves(populate = false) {
      return new Promise((resolve, reject) => {
        this.module.FS.syncfs(populate, error => error ? reject(error) : resolve());
      });
    }

    async initializeSaveStorage() {
      if (this.saveStorageMounted) return;
      const savePath = "/home/web_user/.xu4";
      this.ensureDirectory("/home/web_user");
      this.ensureDirectory(savePath);
      const seedFiles = new Map();
      for (const name of ["party.sav", "monsters.sav", "outmonst.sav", "dngmap.sav"]) {
        try { seedFiles.set(name, this.module.FS.readFile(`${savePath}/${name}`)); } catch {}
      }
      this.module.FS.mount(this.module.IDBFS, {}, savePath);
      this.saveStorageMounted = true;
      await this.syncSaves(true);
      if (!this.hasSave() && seedFiles.has("party.sav")) {
        for (const [name, data] of seedFiles) this.module.FS.writeFile(`${savePath}/${name}`, data);
        await this.syncSaves(false);
      }
    }

    persistSaves() {
      return this.saveStorageMounted ? this.syncSaves(false) : Promise.resolve();
    }

    installLooseFiles(files, installSaves = true) {
      this.ensureDirectory("/ultima4");
      this.ensureDirectory("/home/web_user");
      this.ensureDirectory("/home/web_user/.xu4");
      for (const file of files) {
        const name = file.name.split(/[\\/]/).at(-1).toUpperCase();
        if (!name) continue;
        const bytes = new Uint8Array(file.data);
        this.module.FS.writeFile(`/ultima4/${name}`, bytes);
        if (installSaves && ["PARTY.SAV", "MONSTERS.SAV", "OUTMONST.SAV", "DNGMAP.SAV"].includes(name)) {
          this.module.FS.writeFile(`/home/web_user/.xu4/${name.toLowerCase()}`, bytes);
        }
      }
    }

    installZip(buffer, kind, maxBytes) {
      const bytes = new Uint8Array(buffer);
      if (bytes.byteLength > maxBytes) throw new Error("That ZIP is larger than the 128 MB browser import limit.");
      const pointer = this.module._malloc(bytes.byteLength);
      if (!pointer) throw new Error("The browser could not reserve enough memory for that ZIP.");
      try {
        this.module.HEAPU8.set(bytes, pointer);
        const result = this.call("zu4_web_install_zip", "number", ["number", "number", "number"], [pointer, bytes.byteLength, kind]);
        if (result < 1) throw new Error(kind === 2 ? "The VGA overlay could not be installed." : "No usable files could be extracted from that ZIP.");
        return result;
      } finally {
        this.module._free(pointer);
      }
    }

    extractGameZip(buffer, maxBytes) {
      const bytes = new Uint8Array(buffer);
      if (!bytes.length || bytes.length > maxBytes) throw Error("Choose a ZIP smaller than the 128 MB browser import limit.");
      const fs = this.module.FS;
      this.ensureDirectory("/game-data-check");
      const walk = (directory,visit) => {
        for (const name of fs.readdir(directory)) if (name !== "." && name !== "..") {
          const path=`${directory}/${name}`;
          if (fs.isDir(fs.stat(path).mode)) {walk(path,visit);visit(path,true);}
          else visit(path,false);
        }
      };
      const clear = () => walk("/game-data-check",(path,directory)=>directory ? fs.rmdir(path) : fs.unlink(path));
      clear();
      const pointer = this.module._malloc(bytes.length);
      if (!pointer) throw Error("The browser could not reserve enough memory for that ZIP.");
      try {
        this.module.HEAPU8.set(bytes, pointer);
        const result = this.call("zu4_web_extract_game_zip", "number", ["number", "number"], [pointer, bytes.length]);
        const errors = {"-1":"The ZIP exceeds the import limit.", "-2":"That file is not a readable ZIP.", "-3":"The ZIP contains too many files or too much unpacked game data.", "-4":"The ZIP contains an unsafe file path.", "-5":"The ZIP contains duplicate game filenames. Select one installation.", "-6":"A game file is empty, encrypted, unsupported, or too large.", "-7":"The ZIP has a damaged game file (checksum or extraction failure).", "-8":"The browser could not stage the game data."};
        if (result < 1) throw Error(errors[result] || "No Ultima IV files were found in that ZIP.");
        const entries=[];
        walk("/game-data-check",(path,directory)=>{if (!directory) entries.push({name:path.slice("/game-data-check/".length),data:fs.readFile(path).slice().buffer});});
        return entries;
      } finally {this.module._free(pointer); clear();}
    }

    replaceGameFiles(files) {
      const fs = this.module.FS;
      this.ensureDirectory("/ultima4");
      const stage = "/game-data-install", backup = "/game-data-previous";
      const clear = path => {
        if (!this.pathExists(path)) return;
        for (const name of fs.readdir(path)) if (name !== "." && name !== "..") fs.unlink(`${path}/${name}`);
        fs.rmdir(path);
      };
      clear(stage); clear(backup);
      fs.mkdir(stage);
      try {for (const file of files) fs.writeFile(`${stage}/${file.name}`, new Uint8Array(file.data));}
      catch(error) {clear(stage); throw error;}
      fs.rename("/ultima4",backup);
      try {fs.rename(stage,"/ultima4");}
      catch(error) {fs.rename(backup,"/ultima4"); clear(stage); throw error;}
      const rollback = () => {clear("/ultima4"); fs.rename(backup,"/ultima4");};
      rollback.commit = () => clear(backup);
      return rollback;
    }

    hasGameData() {
      return Boolean(this.call("zu4_web_has_game_data", "number"));
    }

    hasSave() {
      try {
        this.module.FS.readFile("/home/web_user/.xu4/party.sav");
        return true;
      } catch {
        return false;
      }
    }

    readAdventureFiles(directory = "/home/web_user/.xu4/") {
      const files = Object.create(null);
      for (const name of global.UltimatumSaveStore.FILES) {
        try {files[name]=this.module.FS.readFile(directory + name).slice();}
        catch(error) {if(this.pathExists(directory + name)) throw error;}
      }
      return files;
    }

    writeAdventureFiles(files, directory = "/home/web_user/.xu4/") {
      this.ensureDirectory(directory.replace(/\/$/, ""));
      // Only adventure-owned files are replaced. Settings and other files survive.
      for (const name of global.UltimatumSaveStore.FILES) {
        try {this.module.FS.unlink(directory + name);}
        catch(error) {if(this.pathExists(directory + name)) throw error;}
        if (files[name]) this.module.FS.writeFile(directory + name, files[name]);
      }
    }

    validateAdventure(files) {
      global.UltimatumSaveStore.fingerprint(files);
      this.writeAdventureFiles(files, "/adventure-check/");
      const raw=this.call("zu4_web_save_info", "string", ["string"], ["/adventure-check/"]);
      const info=raw && JSON.parse(raw);
      if (!info) throw Error("The adventure files are invalid or incomplete. Your existing saves have not been replaced.");
      return info;
    }

    requestCheckpoint() {
      return Boolean(this.call("zu4_web_request_checkpoint", "number"));
    }

    start(restoreSave) {
      this.call("zu4_web_configure_input");
      this.module.callMain(restoreSave ? ["--skip-intro"] : []);
    }

    snapshot() {
      const raw = this.call("zu4_web_snapshot_json", "string");
      if (!raw) return null;
      const state = JSON.parse(raw);
      if (state.contractVersion !== 1) {
        throw new Error(`Unsupported engine contract version: ${state.contractVersion ?? "missing"}`);
      }
      return state;
    }

    startPolling(onState, onError, interval = 160) {
      this.stopPolling();
      const poll = () => {
        try {
          const state = this.snapshot();
          if (state) onState(state);
        } catch (error) {
          onError(error);
        }
      };
      poll();
      this.pollTimer = setInterval(poll, interval);
    }

    stopPolling() {
      if (this.pollTimer) clearInterval(this.pollTimer);
      this.pollTimer = null;
    }

    sendKey(key) {
      this.call("zu4_web_send_key", null, ["number"], [Number(key)]);
    }

    sendText(text) {
      this.call("zu4_web_send_text", null, ["string"], [text]);
    }

    submitPrompt(text) {
      return Boolean(this.call("zu4_web_submit_prompt", "number", ["string"], [text]));
    }

    submitAnswer(text, promptId) {
      return Boolean(this.call("zu4_web_submit_answer", "number", ["string", "number"], [text, Number(promptId)]));
    }

    endConversation() {
      return Boolean(this.call("zu4_web_end_conversation", "number"));
    }

    submitOption(key) {
      return Boolean(this.call("zu4_web_submit_option", "number", ["number"], [Number(key)]));
    }

    equip(memberIndex, category, type) {
      return this.call("zu4_web_equip", "number", ["number", "number", "number"], [memberIndex, category, type]);
    }

    activatePrimaryAction() {
      return Boolean(this.call("zu4_web_activate_primary_action", "number"));
    }

    gameplayAction(action, parameter = 0) {
      return Boolean(this.call("zu4_web_gameplay_action", "number", ["number", "number"], [Number(action), Number(parameter)]));
    }

    explorationMap() {
      const kind = this.call("zu4_web_prepare_map", "number");
      if (!kind) return null;
      const map = {
        kind,
        pixels: this.call("zu4_web_map_pixels", "number"),
        width: this.call("zu4_web_map_width", "number"),
        height: this.call("zu4_web_map_height", "number"),
        x: this.call("zu4_web_map_player_x", "number"),
        y: this.call("zu4_web_map_player_y", "number"),
      };
      const pixels = this.mapPixels(map);
      return pixels ? {...map, pixels} : null;
    }

    setMapPin(x, y, label) {
      return Boolean(this.call("zu4_web_set_map_pin", "number", ["number", "number", "string"], [Number(x), Number(y), String(label)]));
    }

    removeMapPin(x, y) {
      return Boolean(this.call("zu4_web_remove_map_pin", "number", ["number", "number"], [Number(x), Number(y)]));
    }

    openMenu() {
      return Boolean(this.call("zu4_web_open_menu", "number"));
    }
    openJournal() { return Boolean(this.call("zu4_web_journal_open","number")); }
    closeJournal() { return Boolean(this.call("zu4_web_journal_close","number")); }
    journal() { return JSON.parse(this.call("zu4_web_journal_json","string") || "null"); }
    reloadJournal() { this.call("zu4_web_journal_reload"); }
    favoritePassage(index) { return Boolean(this.call("zu4_web_journal_favorite","number",["number"],[index])); }
    saveJournalNote(index,id,text) { return Boolean(this.call("zu4_web_journal_note","number",["number","string","string"],[index,String(id),text])); }
    deleteJournalNote(id) { return Boolean(this.call("zu4_web_journal_delete","number",["string"],[String(id)])); }

    mapPixels(map) {
      if (!map || !this.heap || !Number.isSafeInteger(map.width) || !Number.isSafeInteger(map.height) ||
          !Number.isSafeInteger(map.pixels) || map.width < 1 || map.height < 1) return null;
      const length = map.width * map.height * 4;
      if (map.pixels < 1 || map.pixels + length > this.heap.length) return null;
      return new Uint8ClampedArray(this.heap.slice(map.pixels, map.pixels + length));
    }

    recoveryBytes(download) {
      if (!download || !this.heap || !Number.isSafeInteger(download.bytes) || !Number.isSafeInteger(download.size) ||
          download.bytes < 1 || download.size < 1 || download.bytes + download.size > this.heap.length) return null;
      return this.heap.slice(download.bytes, download.bytes + download.size);
    }

    saveAdventure() {
      return this.call("zu4_web_save_adventure", "number");
    }

    tapWorld(offsetX, offsetY) {
      return Boolean(this.call("zu4_web_tap_world", "number", ["number", "number"], [offsetX, offsetY]));
    }

    setVideo(videoType) {
      return Boolean(this.call("zu4_web_set_video", "number", ["number"], [videoType]));
    }

    screenPixels() {
      return this.call("zu4_web_screen_pixels", "number") || 0;
    }
  }

  global.UltimatumEngineClient = EngineClient;
})(window);
