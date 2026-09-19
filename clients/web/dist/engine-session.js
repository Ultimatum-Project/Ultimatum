(function installEngineSession(global) {
  const intentHandlers = {
    "input.key": (client, value) => client.sendKey(value.key),
    "input.text": (client, value) => client.sendText(value.text),
    "prompt.submit": (client, value) => client.submitPrompt(value.text),
    "prompt.answer": (client, value) => client.submitAnswer(value.text, value.promptId),
    "prompt.option": (client, value) => client.submitOption(value.key),
    "interaction.primary": client => client.activatePrimaryAction(),
    "gameplay.action": (client, value) => client.gameplayAction(value.action, value.parameter ?? 0),
    "world.tap": (client, value) => client.tapWorld(value.x, value.y),
    "menu.open": client => client.openMenu(),
  };

  function requireValue(condition, message) {
    if (!condition) throw new TypeError(`Invalid engine intent: ${message}`);
  }

  class EngineSession {
    constructor(client, options = {}) {
      requireValue(client && typeof client.snapshot === "function", "an engine client is required");
      this.client = client;
      this.contractVersion = "1";
      this.sessionId = options.sessionId || `web-${Date.now()}-${Math.random().toString(16).slice(2)}`;
      this.startMode = "handoff";
      this.intentIds = new Set();
      this.lastSequence = -1;
      this.dispatchTail = Promise.resolve();
      this.subscription = null;
      this.paused = false;
      this.closed = false;
      this.started = false;
      this.lastSnapshot = {contractVersion:1, ready:false, inputMode:"loading", capabilities:{}, prompt:{kind:"loading", id:0}};
      return new Proxy(this, {
        get(target, property, receiver) {
          if (Reflect.has(target, property)) {
            const value = Reflect.get(target, property, receiver);
            return typeof value === "function" ? value.bind(target) : value;
          }
          const value = target.client[property];
          return typeof value === "function" ? value.bind(target.client) : value;
        },
      });
    }

    start(options = {}) {
      requireValue(!this.closed, "the session is closed");
      const restoreSave = typeof options === "boolean" ? options : Boolean(options.restoreSave);
      const result = this.client.start(restoreSave);
      this.started = true;
      return result;
    }

    getSnapshot() {
      if (!this.started) return this.lastSnapshot;
      const snapshot = this.client.snapshot();
      if (snapshot && snapshot.contractVersion !== 1)
        throw new Error(`Unsupported engine contract version: ${snapshot.contractVersion ?? "missing"}`);
      if (snapshot) this.lastSnapshot = snapshot;
      return snapshot || this.lastSnapshot;
    }

    snapshot() { return this.getSnapshot(); }

    subscribe(listener, onError = error => console.error(error), interval = 160) {
      requireValue(typeof listener === "function", "a snapshot listener is required");
      this.subscription = {listener, onError, interval};
      if (!this.paused && !this.closed) this.client.startPolling(listener, onError, interval);
      return () => {
        this.client.stopPolling();
        if (this.subscription?.listener === listener) this.subscription = null;
      };
    }

    startPolling(listener, onError, interval = 160) { this.subscribe(listener, onError, interval); }
    stopPolling() { this.client.stopPolling(); }

    dispatch(envelope) {
      const run = async () => {
        requireValue(envelope && typeof envelope === "object", "expected an envelope");
        requireValue(typeof envelope.intentId === "string" && envelope.intentId.length > 0 && envelope.intentId.length <= 128, "intentId is required");
        requireValue(Number.isSafeInteger(envelope.sequence) && envelope.sequence >= 0, "sequence must be a non-negative integer");
        requireValue(typeof envelope.kind === "string" && intentHandlers[envelope.kind], `unsupported kind ${envelope.kind ?? "missing"}`);
        requireValue(!this.intentIds.has(envelope.intentId), `duplicate intentId ${envelope.intentId}`);
        requireValue(envelope.sequence > this.lastSequence, `stale sequence ${envelope.sequence}`);
        const snapshot = this.getSnapshot();
        if (envelope.expectedPromptGeneration !== undefined)
          requireValue(snapshot?.prompt?.id === envelope.expectedPromptGeneration, "prompt generation changed");
        if (envelope.expectedStateRevision !== undefined)
          requireValue(snapshot?.stateRevision === envelope.expectedStateRevision, "state revision changed or is unavailable");
        this.intentIds.add(envelope.intentId);
        this.lastSequence = envelope.sequence;
        const value = intentHandlers[envelope.kind](this.client, envelope.parameters || {});
        return {intentId: envelope.intentId, sequence: envelope.sequence, accepted: value !== false, value};
      };
      const result = this.dispatchTail.then(run, run);
      this.dispatchTail = result.catch(() => undefined);
      return result;
    }

    // Keep the current synchronous boolean result while callers migrate to the
    // eventual asynchronous platform checkpoint result.
    requestCheckpoint() { return Boolean(this.client.requestCheckpoint()); }

    async pause() {
      this.paused = true;
      this.client.stopPolling();
    }

    async resume() {
      requireValue(!this.closed, "the session is closed");
      this.paused = false;
      if (this.subscription) this.client.startPolling(this.subscription.listener, this.subscription.onError, this.subscription.interval);
    }

    async quiesce() {
      await this.client.persistSaves();
      return {flushed: true};
    }

    async shutdown() {
      this.closed = true;
      this.started = false;
      this.client.stopPolling();
      await this.client.persistSaves();
    }
  }

  global.UltimatumEngineSession = EngineSession;
})(window);
