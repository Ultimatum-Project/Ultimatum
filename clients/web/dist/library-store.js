(function installLibraryStore(global) {
  const DEFAULT_MAPPING = Object.freeze({
    providerId: "browser-indexeddb-library-v1",
    database: "ultimatum-local-library-v1",
    version: 1,
    objectStore: "assets",
    records: Object.freeze({
      "source-data": "game",
      "optional-overlay": "vga",
    }),
    engineMounts: Object.freeze({
      "source-data": "/ultima4",
      "profile-data": "/home/web_user/.xu4",
    }),
  });

  class IndexedDbLibraryStore {
    constructor(indexedDB = global.indexedDB, mapping = DEFAULT_MAPPING) {
      if (!indexedDB?.open) throw new TypeError("IndexedDB is required for the browser library store.");
      this.indexedDB = indexedDB;
      this.mapping = mapping;
      this.id = mapping.providerId;
      this.capabilities = Object.freeze({transactions:true,structuredClone:true,atomicRecordPublication:true});
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
  if (typeof module !== "undefined" && module.exports) module.exports = {IndexedDbLibraryStore,DEFAULT_MAPPING};
  else global.UltimatumIndexedDbLibraryStore = IndexedDbLibraryStore;
})(typeof window !== "undefined" ? window : globalThis);
