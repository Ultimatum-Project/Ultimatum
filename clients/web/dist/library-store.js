(function installLibraryStore(global) {
  const DEFAULT_MAPPING = Object.freeze({
    providerId: "browser-indexeddb-library-v1",
    database: "ultimatum-local-library-v1",
    version: 1,
    objectStore: "assets",
    records: Object.freeze({
      "source-data": "game",
      "optional-overlay": "vga",
      "install-record": "install-ultima4-default",
    }),
    engineMounts: Object.freeze({
      "source-data": "/ultima4",
      "profile-data": "/home/web_user/.xu4",
    }),
  });

  function byteLength(value) {
    if (value instanceof ArrayBuffer) return value.byteLength;
    if (ArrayBuffer.isView(value)) return value.byteLength;
    return 0;
  }

  function logicalByteLength(value, seen = new Set()) {
    const direct = byteLength(value);
    if (direct) return direct;
    if (!value || typeof value !== "object" || seen.has(value)) return 0;
    seen.add(value);
    if (Array.isArray(value)) return value.reduce((total,item)=>total+logicalByteLength(item,seen),0);
    return Object.values(value).reduce((total,item)=>total+logicalByteLength(item,seen),0);
  }

  class IndexedDbStorageTransaction {
    constructor(provider, roles) {
      this.provider = provider;
      this.roles = roles ? new Set(roles) : null;
      this.operations = new Map();
      this.state = "open";
    }

    write(ref, value) {
      if (this.state !== "open") throw new Error(`Storage transaction is ${this.state}.`);
      const role = typeof ref === "string" ? ref : ref?.role;
      this.provider.keyFor(role);
      if (this.roles && !this.roles.has(role)) throw new TypeError(`Storage role ${role} is outside the transaction scope.`);
      this.operations.set(role,{kind:"write",value});
      return this;
    }

    remove(ref) {
      if (this.state !== "open") throw new Error(`Storage transaction is ${this.state}.`);
      const role = typeof ref === "string" ? ref : ref?.role;
      this.provider.keyFor(role);
      if (this.roles && !this.roles.has(role)) throw new TypeError(`Storage role ${role} is outside the transaction scope.`);
      this.operations.set(role,{kind:"remove"});
      return this;
    }

    async commit() {
      if (this.state !== "open") throw new Error(`Storage transaction is ${this.state}.`);
      this.state = "committing";
      const database = await this.provider.open();
      try {
        await new Promise((resolve,reject)=>{
          const transaction=database.transaction(this.provider.mapping.objectStore,"readwrite");
          const store=transaction.objectStore(this.provider.mapping.objectStore);
          for(const [role,operation] of this.operations) {
            if(operation.kind==="write")store.put(operation.value,this.provider.keyFor(role));
            else store.delete(this.provider.keyFor(role));
          }
          transaction.oncomplete=resolve;
          transaction.onerror=()=>reject(transaction.error);
          transaction.onabort=()=>reject(transaction.error || Error("The game-library transaction was cancelled."));
        });
        this.state="committed";
      } catch(error) { this.state="failed"; throw error; }
      finally { this.operations.clear(); database.close(); }
    }

    async rollback() {
      if(this.state==="committed")throw new Error("A committed storage transaction cannot be rolled back.");
      this.operations.clear();this.state="rolled-back";
    }
  }

  function baseRecord(state, installation, reasons = []) {
    return Object.freeze({
      schemaVersion: 1,
      gameId: "ultima4",
      portId: "xu4",
      state,
      installation,
      activity: Object.freeze({lastPlayedAt:null,playtimeSeconds:0,favorite:false}),
      selection: Object.freeze({engineChannel:"stable",controlProfileId:null}),
      sync: Object.freeze({state:"local-only"}),
      compatibility: Object.freeze({
        source: "indexeddb-compatibility",
        requiresMigration: false,
        reasons: Object.freeze([...reasons]),
      }),
    });
  }

  function projectUltimaIVRecord(sourceData, providerId) {
    if (sourceData == null) return baseRecord("not-installed", null);
    const modern = sourceData.kind === "files" && sourceData.version === 1 && sourceData.game === "ultima4" &&
      typeof sourceData.profile === "string" && Array.isArray(sourceData.files);
    const legacy = sourceData.kind === "zip" && sourceData.data != null;
    if (!modern && !legacy) {
      return baseRecord("repair-required", Object.freeze({
        installId:"ultima4-default", editionId:"dos-english-ega", importProfileId:"unrecognized",
        storageProviderId:providerId, logicalBytes:0, installedAt:null, validatedAt:null,
      }), ["The existing game-data record is not a recognized Ultima IV installation."]);
    }
    const files = modern ? sourceData.files : [];
    const logicalBytes = modern ? files.reduce((total,file)=>total+byteLength(file?.data),0) : byteLength(sourceData.data);
    const profile = modern ? sourceData.profile : "legacy-browser-import";
    return baseRecord("playable", Object.freeze({
      installId:"ultima4-default", editionId:"dos-english-ega", importProfileId:profile,
      storageProviderId:providerId, logicalBytes, installedAt:null, validatedAt:null,
    }));
  }

  class IndexedDbLibraryStore {
    constructor(indexedDB = global.indexedDB, mapping = DEFAULT_MAPPING) {
      if (!indexedDB?.open) throw new TypeError("IndexedDB is required for the browser library store.");
      this.indexedDB = indexedDB;
      this.mapping = mapping;
      this.id = mapping.providerId;
      this.recordSource = "indexeddb-compatibility";
      this.capabilities = Object.freeze({transactions:true,atomicTransactions:true,stagedPublication:true,rollbackOnFailure:true,byteStorage:false,structuredClone:true,atomicRecordPublication:true,durable:true,experimental:false});
    }

    open() {
      return new Promise((resolve, reject) => {
        const request = this.indexedDB.open(this.mapping.database, this.mapping.version);
        request.onupgradeneeded = () => {
          if (!request.result.objectStoreNames.contains(this.mapping.objectStore))
            request.result.createObjectStore(this.mapping.objectStore);
        };
        request.onsuccess = () => resolve(request.result);
        request.onerror = () => reject(request.error);
      });
    }

    keyFor(role) {
      const key = this.mapping.records[role];
      if (!key) throw new TypeError(`Unknown library storage role: ${role}`);
      return key;
    }

    async get(role) {
      const database = await this.open();
      return new Promise((resolve, reject) => {
        const request = database.transaction(this.mapping.objectStore).objectStore(this.mapping.objectStore).get(this.keyFor(role));
        request.onsuccess = () => { database.close(); resolve(request.result); };
        request.onerror = () => { database.close(); reject(request.error); };
      });
    }

    async put(role, value) {
      const database = await this.open();
      return new Promise((resolve, reject) => {
        const transaction = database.transaction(this.mapping.objectStore, "readwrite");
        transaction.objectStore(this.mapping.objectStore).put(value, this.keyFor(role));
        transaction.oncomplete = () => { database.close(); resolve(); };
        transaction.onerror = () => { database.close(); reject(transaction.error); };
        transaction.onabort = () => { database.close(); reject(transaction.error || Error("The game-library transaction was cancelled.")); };
      });
    }

    async beginTransaction(scope = null) {
      await this.open().then(database=>database.close());
      const roles = Array.isArray(scope) ? scope : scope?.roles;
      return new IndexedDbStorageTransaction(this,roles || null);
    }

    read(ref) { return this.get(typeof ref === "string" ? ref : ref?.role); }

    async stat(ref) {
      const role=typeof ref === "string" ? ref : ref?.role;
      const value=await this.get(role);
      return value == null ? null : Object.freeze({role,kind:"record",size:logicalByteLength(value),lastModified:null});
    }

    async *list(scope = null) {
      const roles = Array.isArray(scope) ? scope : scope?.roles || Object.keys(this.mapping.records);
      const values=await this.readRoles(roles);
      for(const role of roles) if(values[role] != null) yield Object.freeze({role,kind:"record",size:logicalByteLength(values[role]),lastModified:null});
    }

    async estimate() {
      const estimate=typeof global.navigator?.storage?.estimate === "function" ? await global.navigator.storage.estimate() : {};
      return Object.freeze({usage:Number.isFinite(estimate?.usage)?estimate.usage:null,quota:Number.isFinite(estimate?.quota)?estimate.quota:null});
    }

    async readRoles(roles) {
      const database = await this.open();
      return new Promise((resolve, reject) => {
        const transaction = database.transaction(this.mapping.objectStore);
        const store = transaction.objectStore(this.mapping.objectStore);
        const requests = roles.map(role => store.get(this.keyFor(role)));
        transaction.oncomplete = () => { database.close(); resolve(Object.fromEntries(roles.map((role,index)=>[role,requests[index].result]))); };
        transaction.onerror = () => { database.close(); reject(transaction.error); };
        transaction.onabort = () => { database.close(); reject(transaction.error || Error("The game-library read was cancelled.")); };
      });
    }

    async publishInstall(sourceData, record) {
      if (!sourceData || record?.schemaVersion !== 1 || record.gameId !== "ultima4" || record.portId !== "xu4" || record.installation?.storageProviderId !== this.id)
        throw new TypeError("A compatible source-data record and install record are required.");
      const database = await this.open();
      return new Promise((resolve, reject) => {
        const transaction = database.transaction(this.mapping.objectStore,"readwrite");
        const store = transaction.objectStore(this.mapping.objectStore);
        store.put(sourceData,this.keyFor("source-data"));
        store.put(record,this.keyFor("install-record"));
        transaction.oncomplete = () => { database.close(); resolve(); };
        transaction.onerror = () => { database.close(); reject(transaction.error); };
        transaction.onabort = () => { database.close(); reject(transaction.error || Error("The game installation transaction was cancelled.")); };
      });
    }

    async inspect(gameId = "ultima4", portId = "xu4") {
      if (gameId !== "ultima4" || portId !== "xu4") throw new TypeError(`Unsupported compatibility library entry: ${gameId}/${portId}`);
      const records = await this.readRoles(["source-data","install-record"]);
      const sourceData = records["source-data"];
      const stored = records["install-record"];
      const compatible = stored?.schemaVersion === 1 && stored.gameId === gameId && stored.portId === portId &&
        stored.installation?.storageProviderId === this.id;
      const record = compatible ? stored : projectUltimaIVRecord(sourceData,this.id);
      return Object.freeze({record,sourceData});
    }

    physicalLocation(role) {
      return Object.freeze({
        providerId: this.id,
        database: this.mapping.database,
        version: this.mapping.version,
        objectStore: this.mapping.objectStore,
        key: this.keyFor(role),
        engineMount: this.mapping.engineMounts[role] || null,
      });
    }
  }

  IndexedDbLibraryStore.DEFAULT_MAPPING = DEFAULT_MAPPING;
  IndexedDbLibraryStore.projectUltimaIVRecord = projectUltimaIVRecord;
  if (typeof module !== "undefined" && module.exports) module.exports = {IndexedDbLibraryStore,IndexedDbStorageTransaction,DEFAULT_MAPPING,projectUltimaIVRecord};
  else global.UltimatumIndexedDbLibraryStore = IndexedDbLibraryStore;
})(typeof window !== "undefined" ? window : globalThis);
