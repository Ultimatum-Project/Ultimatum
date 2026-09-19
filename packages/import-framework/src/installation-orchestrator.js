(function installImportFramework(global) {
  const stableId = /^[a-z0-9]+(?:-[a-z0-9]+)*$/;
  const profileId = /^[a-z0-9]+(?:[.-][a-z0-9]+)*$/;

  function aborted(signal) {
    if (signal?.aborted) {
      const reason = signal.reason;
      if (reason instanceof Error) throw reason;
      if (typeof DOMException !== "undefined") throw new DOMException("The import was cancelled.", "AbortError");
      const error = new Error("The import was cancelled."); error.name = "AbortError"; throw error;
    }
  }

  function requireValue(condition, message) {
    if (!condition) throw new TypeError(`Invalid install plan: ${message}`);
  }

  function validateInstallPlan(plan) {
    requireValue(plan && typeof plan === "object" && !Array.isArray(plan), "expected an object");
    requireValue(plan.contractVersion === 1, "unsupported contractVersion");
    for (const name of ["adapterId","gameId","portId","installId","editionId"])
      requireValue(stableId.test(plan[name] || ""), `${name} is invalid`);
    requireValue(profileId.test(plan.profileId || ""), "profileId is invalid");
    requireValue(Number.isSafeInteger(plan.logicalBytes) && plan.logicalBytes >= 0, "logicalBytes is invalid");
    requireValue(plan.destination?.role === "source-data" && typeof plan.destination.mount === "string" && plan.destination.mount.startsWith("/"), "source-data destination is required");
    requireValue(["atomic-record","atomic-record-with-runtime-rollback"].includes(plan.publication), "publication is invalid");
    requireValue(["local-only","local-only-unless-user-uploads","authorized-download"].includes(plan.privacy), "privacy is invalid");
    return plan;
  }

  function createLibraryRecord(plan, providerId, previous, now, source = "memory") {
    const prior = previous && previous.gameId === plan.gameId && previous.portId === plan.portId ? previous : null;
    return Object.freeze({
      schemaVersion:1,
      gameId:plan.gameId,
      portId:plan.portId,
      state:"playable",
      installation:Object.freeze({
        installId:plan.installId,
        editionId:plan.editionId,
        importProfileId:plan.profileId,
        storageProviderId:providerId,
        logicalBytes:plan.logicalBytes,
        installedAt:prior?.installation?.installedAt || now,
        validatedAt:now,
      }),
      activity:prior?.activity || Object.freeze({lastPlayedAt:null,playtimeSeconds:0,favorite:false}),
      selection:prior?.selection || Object.freeze({engineChannel:"stable",controlProfileId:null}),
      sync:prior?.sync || Object.freeze({state:"local-only"}),
      compatibility:Object.freeze({source,requiresMigration:false,reasons:Object.freeze([])}),
    });
  }

  class InstallationOrchestrator {
    constructor({adapter, library, stageRuntime = () => null, clock = () => new Date().toISOString()}) {
      if (!adapter?.prepare || !adapter?.preparePackage) throw new TypeError("An import adapter is required.");
      if (!library?.inspect || !library?.publishInstall || !stableId.test(library.id || "")) throw new TypeError("A transactional library provider is required.");
      if (typeof stageRuntime !== "function" || typeof clock !== "function") throw new TypeError("Invalid installation host services.");
      this.adapter = adapter;
      this.library = library;
      this.stageRuntime = stageRuntime;
      this.clock = clock;
      this.active = false;
      this.state = "idle";
    }

    transition(state, reportProgress, detail = {}) {
      this.state = state;
      reportProgress?.({phase:state,...detail});
    }

    async run(prepare, options = {}) {
      if (this.active) throw new Error("Another installation is already in progress.");
      this.active = true;
      let runtimeStage = null;
      try {
        aborted(options.signal);
        this.transition("preparing",options.reportProgress);
        const prepared = await prepare();
        validateInstallPlan(prepared.plan);
        aborted(options.signal);
        this.transition("staging",options.reportProgress,{logicalBytes:prepared.plan.logicalBytes});
        runtimeStage = await this.stageRuntime(prepared.verified,prepared.plan);
        aborted(options.signal);
        const previous = (await this.library.inspect(prepared.plan.gameId,prepared.plan.portId)).record;
        const record = createLibraryRecord(prepared.plan,this.library.id,previous,this.clock(),this.library.recordSource || "memory");
        this.transition("publishing",options.reportProgress);
        await this.library.publishInstall(prepared.verified,record);
        runtimeStage?.commit?.();
        runtimeStage = null;
        this.transition("complete",options.reportProgress);
        return Object.freeze({...prepared,record});
      } catch (error) {
        runtimeStage?.rollback?.();
        this.transition(error?.name === "AbortError" ? "cancelled" : "failed",options.reportProgress,{error});
        throw error;
      } finally {
        this.active = false;
        if (!["complete","cancelled","failed"].includes(this.state)) this.state = "idle";
      }
    }

    install(source, options = {}) {
      return this.run(() => this.adapter.prepare(source,{signal:options.signal,reportProgress:options.reportAdapterProgress}),options);
    }

    installPackage(text, options = {}) {
      return this.run(() => this.adapter.preparePackage(text,{signal:options.signal,reportProgress:options.reportAdapterProgress}),options);
    }
  }

  const api = {InstallationOrchestrator,validateInstallPlan,createLibraryRecord};
  if (typeof module !== "undefined" && module.exports) module.exports = api;
  else global.UltimatumInstallationOrchestrator = InstallationOrchestrator;
})(typeof window !== "undefined" ? window : globalThis);
