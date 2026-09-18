(function (global) {
  "use strict";
  const FILES = ["party.sav", "monsters.sav", "outmonst.sav", "dngmap.sav", "topics.txt", "journal-notebook.dat", "explored-map.dat", "map-pins.dat", "map-discoveries.dat", "explored-dungeons.dat", "conversations.json"];
  const MAX_BYTES = 8 * 1024 * 1024;
  function checksum(bytes) {
    let crc = -1;
    for (const byte of bytes) {
      crc ^= byte;
      for (let i = 0; i < 8; i++) crc = (crc >>> 1) ^ (0xedb88320 & -(crc & 1));
    }
    return ((crc ^ -1) >>> 0).toString(16).padStart(8, "0");
  }
  function validateFiles(files, requireCore = true) {
    if (!files || typeof files !== "object" || Array.isArray(files)) throw Error("Invalid adventure files.");
    let total = 0;
    for (const [name, bytes] of Object.entries(files)) {
      if (!FILES.includes(name) || !(bytes instanceof Uint8Array) || (requireCore && bytes.length === 0)) throw Error("Invalid adventure file: " + name);
      total += bytes.length;
    }
    if (total > MAX_BYTES || !Object.keys(files).length || (requireCore && (!files["party.sav"] || !files["monsters.sav"]))) throw Error("The adventure is incomplete or too large.");
    return files;
  }
  function fingerprint(files) {
    validateFiles(files);
    return digest(files);
  }
  function digest(files) {
    return FILES.filter(name => files[name]).map(name => name + ":" + checksum(files[name])).join("|");
  }
  class AdventureStore {
    constructor(indexedDB = global.indexedDB) { this.indexedDB = indexedDB; this.db = null; }
    async open() {
      if (this.db) return;
      this.db = await new Promise((resolve, reject) => {
        const request = this.indexedDB.open("ultimatum-adventures-v1", 1);
        request.onupgradeneeded = () => request.result.createObjectStore("slots", {keyPath:"slot"});
        request.onsuccess = () => { this.db = request.result; this.db.onversionchange = () => {this.db.close();this.db=null;}; resolve(request.result); };
        request.onerror = () => reject(request.error);
        request.onblocked = () => reject(Error("Close another Ultimatum tab to finish opening save storage."));
      });
    }
    async transact(mode, operation) {
      await this.open();
      return new Promise((resolve, reject) => {
        const transaction = this.db.transaction("slots", mode);
        let result;
        let failure;
        const setResult = value => { result = value; };
        const fail = error => {failure=error;transaction.abort();};
        transaction.oncomplete = () => resolve(result);
        transaction.onabort = transaction.onerror = () => reject(failure || transaction.error || Error("Save storage could not be updated."));
        try { operation(transaction.objectStore("slots"), setResult, transaction, fail); }
        catch(error) { transaction.abort(); reject(error); }
      });
    }
    list() { return this.transact("readonly", (store, done) => {const r=store.getAll();r.onsuccess=()=>done(r.result.filter(record=>record.slot>=1 && record.slot<=3));}); }
    migrate(files, summary) {
      if(files) validateFiles(files,false);
      return this.transact("readwrite", (store, done) => {
        const r=store.get(0);
        r.onsuccess=()=> {
          if(r.result) return;
          const existing=store.get(1);
          existing.onsuccess=()=> {
            if(files && !existing.result) {
              const checkpoint={files,summary,fingerprint:digest(files),savedAt:Date.now(),damaged:Boolean(summary.damaged)};
              store.put({slot:1,label:summary.name || "Existing save",current:checkpoint,previous:null});done(true);
            }
            store.put({slot:0,migrated:true});
          };
        };
      });
    }
    get(slot) { this.checkSlot(slot); return this.transact("readonly", (store, done) => {const r=store.get(slot);r.onsuccess=()=>done(r.result || null);}); }
    wasMigrated() {return this.transact("readonly",(store,done)=>{const r=store.get(0);r.onsuccess=()=>done(Boolean(r.result));});}
    checkSlot(slot) { if (!Number.isInteger(slot) || slot<1 || slot>3) throw Error("Choose a valid saved game."); }
    async commit(slot, files, summary, label, expectedFingerprint, cloud=undefined) {
      this.checkSlot(slot);
      const digest = fingerprint(files);
      const checkpoint = {files, fingerprint:digest, summary, savedAt:Date.now()};
      return this.transact("readwrite", (store, done, transaction, fail) => {
        const r=store.get(slot);
        r.onsuccess=()=> {
          const old=r.result;
          if(expectedFingerprint !== undefined && (old?.current?.fingerprint ?? null) !== expectedFingerprint) {
            fail(Error("This saved game changed in another tab. Export your current progress before reloading."));return;
          }
          // Publishing one record atomically retains the selected generation
          // and its immediate predecessor, matching native recovery semantics.
          const value={slot,label:label ?? old?.label ?? summary?.name ?? "Saved Game",current:checkpoint,
            previous:old?.current?.fingerprint===digest ? old.previous : old?.current || null,
            cloud:cloud===undefined ? old?.cloud || null : cloud};
          store.put(value);done(value);
        };
      });
    }
    async rename(slot, label) {
      this.checkSlot(slot); label=label.trim();
      if (!label || label.length>40) throw Error("Use a name of 1–40 characters.");
      return this.transact("readwrite", (store, done) => {const r=store.get(slot);r.onsuccess=()=>{if(r.result){r.result.label=label;store.put(r.result);done(r.result);}};});
    }
    async setCloudLink(slot, cloud, expectedFingerprint) {
      this.checkSlot(slot);
      return this.transact("readwrite",(store,done,transaction,fail)=>{
        const request=store.get(slot);request.onsuccess=()=>{
          const record=request.result;
          if(!record?.current) return fail(Error("The local saved game is no longer available."));
          if(expectedFingerprint!==undefined && record.current.fingerprint!==expectedFingerprint)
            return fail(Error("This saved game changed before cloud sync finished."));
          record.cloud=cloud ? {...cloud} : null;store.put(record);done(record);
        };
      });
    }
    async updateJournal(slot, files, expectedFingerprint) {
      this.checkSlot(slot);
      if(Object.keys(files).some(name=>!["topics.txt","journal-notebook.dat"].includes(name))) throw Error("Only journal metadata can be updated here.");
      return this.transact("readwrite",(store,done,transaction,fail)=>{
        const request=store.get(slot);request.onsuccess=()=>{
          try {
          const record=request.result;
          if(!record?.current || record.current.fingerprint!==expectedFingerprint) return fail(Error("This saved game changed in another tab. Reload its latest checkpoint before editing the journal."));
          const updated={...record.current.files,...files};
          record.current={...record.current,files:updated,fingerprint:fingerprint(updated)};
          // Editing notes must never replace the previous gameplay checkpoint.
          store.put(record);done(record);
          } catch(error) { fail(error); }
        };
      });
    }
    remove(slot, expectedFingerprint) {
      this.checkSlot(slot);
      return this.transact("readwrite",(store,done,transaction,fail)=>{
        const r=store.get(slot);r.onsuccess=()=>{
          if(expectedFingerprint!==undefined && (r.result?.current?.fingerprint ?? null)!==expectedFingerprint) return fail(Error("This saved game changed in another tab. Review its latest checkpoint before deleting."));
          store.delete(slot);
        };
      });
    }
    restorePrevious(slot, expectedCurrent, expectedPrevious) {
      this.checkSlot(slot);
      return this.transact("readwrite", (store, done, transaction, fail) => {
        const r=store.get(slot);r.onsuccess=()=>{
          const record=r.result;
          if(expectedCurrent!==undefined && (record?.current?.fingerprint!==expectedCurrent || record?.previous?.fingerprint!==expectedPrevious)) return fail(Error("This saved game changed in another tab. Review its recovery checkpoint again."));
          if(!record?.previous){transaction.abort();return;}
          [record.current,record.previous]=[record.previous,record.current];store.put(record);done(record);
        };
      });
    }
    static encode(checkpoint, label) {
      // Quarantined legacy fragments can be backed up, but decode/engine
      // validation still refuses to load an incomplete or damaged adventure.
      validateFiles(checkpoint.files,!checkpoint.damaged);
      const files=Object.entries(checkpoint.files).map(([name, bytes])=>{
        let binary="";
        for(let offset=0;offset<bytes.length;offset+=8192) binary+=String.fromCharCode(...bytes.subarray(offset,offset+8192));
        return {name,size:bytes.length,crc32:checksum(bytes),data:global.btoa(binary)};
      });
      return JSON.stringify({format:"ultimatum-adventure",version:1,game:"ultima4",engine:"xu4",label,savedAt:checkpoint.savedAt,files});
    }
    static decode(text) {
      if (typeof text!=="string" || text.length>MAX_BYTES*2) throw Error("That saved-game backup is too large.");
      const packageData=JSON.parse(text);
      if(packageData.format!=="ultimatum-adventure" || packageData.version!==1 || packageData.game!=="ultima4" || packageData.engine!=="xu4" || !Array.isArray(packageData.files)) throw Error("Unsupported saved-game backup.");
      const files=Object.create(null);
      for(const file of packageData.files){
        if(!FILES.includes(file.name) || files[file.name] || typeof file.data!=="string") throw Error("Unexpected or duplicate saved-game file.");
        const bytes=Uint8Array.from(global.atob(file.data),c=>c.charCodeAt(0));
        if(bytes.length!==file.size || checksum(bytes)!==file.crc32) throw Error("The saved-game backup is damaged: " + file.name);
        files[file.name]=bytes;
      }
      validateFiles(files);
      return {files,label:typeof packageData.label==="string" ? packageData.label.slice(0,40) : "Imported save"};
    }
  }
  AdventureStore.FILES=FILES;
  AdventureStore.fingerprint=fingerprint;
  global.UltimatumAdventureStore=AdventureStore;
})(typeof window === "undefined" ? globalThis : window);
