(function installOpfsStorageProvider(global) {
  const PROVIDER_DIRECTORY = "ultimatum-experimental-v1";
  const textEncoder = new TextEncoder();

  function asPath(value, label = "storage reference") {
    const path = typeof value === "string" ? value : value?.path;
    if (typeof path !== "string" || !path || path.startsWith("/") || path.endsWith("/") || path.includes("\\") || path.includes("\0"))
      throw new TypeError(`Invalid ${label}.`);
    const segments = path.split("/");
    if (segments.some(segment => !segment || segment === "." || segment === ".." || segment.startsWith(".")))
      throw new TypeError(`Invalid ${label}.`);
    return Object.freeze({path:segments.join("/"), segments:Object.freeze(segments)});
  }

  function asScope(value) {
    if (value == null || value === "") return Object.freeze({path:"",segments:Object.freeze([])});
    return asPath(typeof value === "string" ? value : value?.path, "storage scope");
  }

  function bytes(value) {
    if (value instanceof Uint8Array) return value.slice();
    if (value instanceof ArrayBuffer) return new Uint8Array(value.slice(0));
    if (ArrayBuffer.isView(value)) return new Uint8Array(value.buffer.slice(value.byteOffset,value.byteOffset+value.byteLength));
    if (typeof value === "string") return textEncoder.encode(value);
    throw new TypeError("Storage writes require bytes or text.");
  }

  function notFound(error) {
    return error?.name === "NotFoundError" || /not found/i.test(error?.message || "");
  }

  class OpfsStorageTransaction {
    constructor(provider, scope) {
      this.provider = provider;
      this.scope = asScope(scope);
      this.operations = new Map();
      this.state = "open";
    }

    check(ref) {
      if (this.state !== "open") throw new Error(`Storage transaction is ${this.state}.`);
      const normalized = asPath(ref);
      if (this.scope.path && normalized.path !== this.scope.path && !normalized.path.startsWith(`${this.scope.path}/`))
        throw new TypeError(`Storage reference is outside transaction scope ${this.scope.path}.`);
      return normalized;
    }

    write(ref, value) {
      const normalized = this.check(ref);
      this.operations.set(normalized.path,{kind:"write",ref:normalized,data:bytes(value)});
      return this;
    }

    remove(ref) {
      const normalized = this.check(ref);
      this.operations.set(normalized.path,{kind:"remove",ref:normalized});
      return this;
    }

    async commit() {
      if (this.state !== "open") throw new Error(`Storage transaction is ${this.state}.`);
      this.state = "committing";
      try {
        await this.provider._enqueueCommit([...this.operations.values()]);
        this.state = "committed";
      } catch (error) {
        this.state = "failed";
        throw error;
      } finally {
        this.operations.clear();
      }
    }

    async rollback() {
      if (this.state === "committed") throw new Error("A committed storage transaction cannot be rolled back.");
      this.operations.clear();
      this.state = "rolled-back";
    }
  }

  class OpfsStorageProvider {
    constructor(options = {}) {
      const manager = options.storageManager || global.navigator?.storage;
      if (typeof manager?.getDirectory !== "function") throw new TypeError("OPFS is not available in this browser.");
      this.storageManager = manager;
      this.directoryName = options.directoryName || PROVIDER_DIRECTORY;
      asPath(this.directoryName,"provider directory");
      this.id = "browser-opfs-experimental-v1";
      this.capabilities = Object.freeze({
        transactions:true,
        atomicTransactions:false,
        stagedPublication:true,
        rollbackOnFailure:true,
        byteStorage:true,
        durable:true,
        experimental:true,
      });
      this._rootPromise = null;
      this._commitTail = Promise.resolve();
    }

    static isSupported(storageManager = global.navigator?.storage) {
      return typeof storageManager?.getDirectory === "function";
    }

    async _root() {
      if (!this._rootPromise) this._rootPromise = this.storageManager.getDirectory().then(root => root.getDirectoryHandle(this.directoryName,{create:true}));
      return this._rootPromise;
    }

    async _parent(ref, create) {
      let directory = await this._root();
      for (const segment of ref.segments.slice(0,-1)) directory = await directory.getDirectoryHandle(segment,{create});
      return directory;
    }

    async _read(ref) {
      const normalized = asPath(ref);
      try {
        const parent = await this._parent(normalized,false);
        const handle = await parent.getFileHandle(normalized.segments.at(-1),{create:false});
        return new Uint8Array(await (await handle.getFile()).arrayBuffer());
      } catch (error) {
        if (notFound(error)) return null;
        throw error;
      }
    }

    async read(ref) {
      const result = await this._read(ref);
      if (result === null) {
        const error = new Error(`Storage entry not found: ${asPath(ref).path}`);
        error.name = "NotFoundError";
        throw error;
      }
      return result;
    }

    async stat(ref) {
      const normalized = asPath(ref);
      try {
        const parent = await this._parent(normalized,false);
        const handle = await parent.getFileHandle(normalized.segments.at(-1),{create:false});
        const file = await handle.getFile();
        return Object.freeze({path:normalized.path,kind:"file",size:file.size,lastModified:file.lastModified || null});
      } catch (error) {
        if (notFound(error)) return null;
        throw error;
      }
    }

    async *_walk(directory, prefix) {
      for await (const [name,handle] of directory.entries()) {
        if (name.startsWith(".")) continue;
        const path = prefix ? `${prefix}/${name}` : name;
        if (handle.kind === "directory") yield* this._walk(handle,path);
        else {
          const file = await handle.getFile();
          yield Object.freeze({path,kind:"file",size:file.size,lastModified:file.lastModified || null});
        }
      }
    }

    async *list(scope = "") {
      const normalized = asScope(scope);
      let directory = await this._root();
      try {
        for (const segment of normalized.segments) directory = await directory.getDirectoryHandle(segment,{create:false});
      } catch (error) {
        if (notFound(error)) return;
        throw error;
      }
      yield* this._walk(directory,normalized.path);
    }

    async estimate() {
      const estimate = typeof this.storageManager.estimate === "function" ? await this.storageManager.estimate() : {};
      return Object.freeze({usage:Number.isFinite(estimate?.usage) ? estimate.usage : null,quota:Number.isFinite(estimate?.quota) ? estimate.quota : null});
    }

    async beginTransaction(scope = "") {
      await this._root();
      return new OpfsStorageTransaction(this,scope);
    }

    async _write(ref, data) {
      const parent = await this._parent(ref,true);
      const handle = await parent.getFileHandle(ref.segments.at(-1),{create:true});
      const writable = await handle.createWritable();
      try { await writable.write(data); await writable.close(); }
      catch (error) { try { await writable.abort?.(); } catch {} throw error; }
    }

    async _remove(ref) {
      try { await (await this._parent(ref,false)).removeEntry(ref.segments.at(-1)); }
      catch (error) { if (!notFound(error)) throw error; }
    }

    async _commit(operations) {
      const before = new Map();
      for (const operation of operations) before.set(operation.ref.path,await this._read(operation.ref));
      const applied = [];
      try {
        for (const operation of operations) {
          applied.push(operation);
          if (operation.kind === "write") await this._write(operation.ref,operation.data);
          else await this._remove(operation.ref);
        }
      } catch (error) {
        const rollbackErrors = [];
        for (const operation of applied.reverse()) {
          try {
            const original = before.get(operation.ref.path);
            if (original === null) await this._remove(operation.ref);
            else await this._write(operation.ref,original);
          } catch (rollbackError) { rollbackErrors.push(rollbackError); }
        }
        if (rollbackErrors.length) throw new AggregateError([error,...rollbackErrors],"OPFS publication and rollback failed.");
        throw error;
      }
    }

    _enqueueCommit(operations) {
      const run=this._commitTail.then(()=>this._commit(operations));
      this._commitTail=run.catch(()=>{});
      return run;
    }
  }

  OpfsStorageProvider.PROVIDER_DIRECTORY = PROVIDER_DIRECTORY;
  OpfsStorageProvider.asPath = asPath;
  const exported = {OpfsStorageProvider,OpfsStorageTransaction,asPath};
  if (typeof module !== "undefined" && module.exports) module.exports = exported;
  else global.UltimatumOpfsStorageProvider = OpfsStorageProvider;
})(typeof window !== "undefined" ? window : globalThis);
