(function installSessionOrchestrator(global) {
  const ACTIVE_STATES = new Set(["preflighting", "mounting", "loading", "running", "paused", "quiescing", "flushing"]);

  function requireValue(condition, message) {
    if (!condition) throw new TypeError(`Invalid session orchestration: ${message}`);
  }

  class WebSessionLease {
    constructor(options = {}) {
      this.name = options.name || "ultimatum-engine-session";
      this.locks = options.locks === undefined ? global.navigator?.locks : options.locks;
      this.held = false;
      this.supported = Boolean(this.locks?.request);
      this.releaseHeld = null;
      this.request = null;
    }

    async acquire() {
      if (this.held) return true;
      if (!this.supported) {
        this.held = true;
        return true;
      }
      let settle;
      const acquired = new Promise(resolve => { settle = resolve; });
      let release;
      const held = new Promise(resolve => { release = resolve; });
      this.request = this.locks.request(this.name, {mode:"exclusive", ifAvailable:true}, async lock => {
        if (!lock) {
          settle(false);
          return;
        }
        this.held = true;
        this.releaseHeld = release;
        settle(true);
        await held;
        this.held = false;
        this.releaseHeld = null;
      });
      return acquired;
    }

    async release() {
      if (!this.held) return;
      if (!this.supported) {
        this.held = false;
        return;
      }
      this.releaseHeld?.();
      await this.request;
    }
  }

  class SessionOrchestrator {
    constructor(options = {}) {
      const session = options.session;
      for (const operation of ["start", "getSnapshot", "subscribe", "dispatch", "requestCheckpoint", "pause", "resume", "quiesce", "shutdown"])
        requireValue(typeof session?.[operation] === "function", `EngineSession is missing ${operation}`);
      this.contractVersion = 1;
      this.session = session;
      this.sessionId = session.sessionId;
      this.preflight = options.preflight || (() => undefined);
      this.mount = options.mount || (() => undefined);
      this.checkpoint = options.checkpoint || null;
      this.lease = options.lease || null;
      this.state = "idle";
      this.events = [];
      this.listeners = new Set();
      this.operationTail = Promise.resolve();
      this.unsubscribeSnapshot = null;
      this.lastError = null;
      this.leaseAcquired = false;
    }

    onProgress(listener) {
      requireValue(typeof listener === "function", "progress listener must be a function");
      this.listeners.add(listener);
      return () => this.listeners.delete(listener);
    }

    transition(state, detail = null) {
      const previousState = this.state;
      this.state = state;
      const event = Object.freeze({contractVersion:1, sessionId:this.sessionId, state, previousState, at:new Date().toISOString(), detail});
      this.events.push(event);
      if (this.events.length > 100) this.events.shift();
      for (const listener of this.listeners) {
        try { listener(event); } catch (error) { console.error(error); }
      }
      return event;
    }

    diagnostics() {
      return {contractVersion:1, sessionId:this.sessionId, state:this.state, leaseSupported:Boolean(this.lease?.supported), leaseAcquired:this.leaseAcquired, lastError:this.lastError ? {name:this.lastError.name, message:this.lastError.message} : null, events:[...this.events]};
    }

    enqueue(operation) {
      const result = this.operationTail.then(operation, operation);
      this.operationTail = result.catch(() => undefined);
      return result;
    }

    releaseLease() {
      if (!this.leaseAcquired) return Promise.resolve();
      this.leaseAcquired = false;
      return Promise.resolve(this.lease?.release());
    }

    async fail(error, phase) {
      this.lastError = error instanceof Error ? error : new Error(String(error));
      this.transition(error?.recoverable === false ? "fatal-error" : "recoverable-error", {phase, name:this.lastError.name, message:this.lastError.message});
      await this.releaseLease();
    }

    start(options = {}) {
      return this.enqueue(async () => {
        requireValue(this.state === "idle", `cannot start from ${this.state}`);
        try {
          this.transition("preflighting");
          await this.preflight(options);
          if (this.lease) {
            this.leaseAcquired = await this.lease.acquire();
            if (!this.leaseAcquired) {
              const error = new Error("This installation already has an active game session in another tab.");
              error.name = "SessionLeaseUnavailableError";
              throw error;
            }
          }
          this.transition("mounting");
          await this.mount(options);
          this.transition("loading");
          const started = this.session.start(options.sessionOptions || options);
          if (options.listener) this.unsubscribeSnapshot = this.session.subscribe(options.listener, options.onError, options.interval);
          if (this.session.startMode !== "handoff") await started;
          this.transition("running");
          return this.session.getSnapshot();
        } catch (error) {
          await this.fail(error, this.state);
          throw error;
        }
      });
    }

    getSnapshot() { return this.session.getSnapshot(); }

    dispatch(envelope) {
      return this.enqueue(async () => {
        requireValue(this.state === "running", `cannot dispatch from ${this.state}`);
        return this.session.dispatch(envelope);
      });
    }

    requestCheckpoint(reason = "requested") {
      return this.enqueue(async () => {
        requireValue(this.state === "running" || this.state === "paused", `cannot checkpoint from ${this.state}`);
        return this.session.requestCheckpoint(reason);
      });
    }

    suspend(reason = "platform", checkpoint = this.checkpoint) {
      return this.enqueue(async () => {
        if (this.state === "paused") return {flushed:true, alreadyPaused:true};
        requireValue(this.state === "running", `cannot suspend from ${this.state}`);
        try {
          await this.session.pause(reason);
          this.transition("paused", {reason});
          this.transition("quiescing", {reason});
          if (checkpoint) await checkpoint(reason);
          this.transition("flushing", {reason});
          const result = await this.session.quiesce(reason);
          this.transition("paused", {reason, flushed:result?.flushed !== false});
          return result;
        } catch (error) {
          await this.fail(error, "suspend");
          throw error;
        }
      });
    }

    resume(reason = "platform") {
      return this.enqueue(async () => {
        if (this.state === "running") return;
        requireValue(this.state === "paused", `cannot resume from ${this.state}`);
        try {
          await this.session.resume(reason);
          this.transition("running", {reason});
        } catch (error) {
          await this.fail(error, "resume");
          throw error;
        }
      });
    }

    shutdown(reason = "requested") {
      return this.enqueue(async () => {
        if (this.state === "stopped") return;
        requireValue(ACTIVE_STATES.has(this.state) || this.state === "recoverable-error" || this.state === "fatal-error", `cannot shut down from ${this.state}`);
        try {
          if (this.state === "running") await this.session.pause(reason);
          this.transition("quiescing", {reason});
          if (this.checkpoint) await this.checkpoint(reason);
          this.transition("flushing", {reason});
          this.unsubscribeSnapshot?.();
          this.unsubscribeSnapshot = null;
          await this.session.shutdown(reason);
          await this.releaseLease();
          this.transition("stopped", {reason});
        } catch (error) {
          await this.fail(error, "shutdown");
          throw error;
        }
      });
    }
  }

  global.UltimatumWebSessionLease = WebSessionLease;
  global.UltimatumSessionOrchestrator = SessionOrchestrator;
})(typeof window === "undefined" ? globalThis : window);
