(function installImportAdapter(global) {
  function aborted(signal) {
    if (signal?.aborted) throw signal.reason instanceof Error ? signal.reason : new DOMException("The import was cancelled.", "AbortError");
  }

  class UltimaIVImportAdapter {
    constructor({gameData, extractZip, maxZipBytes}) {
      if (!gameData?.validate || typeof extractZip !== "function") throw new TypeError("Ultima IV import validation and ZIP extraction are required.");
      this.id = "ultima4-web-compat-v1";
      this.gameId = "ultima4";
      this.gameData = gameData;
      this.extractZip = extractZip;
      this.maxZipBytes = maxZipBytes;
    }

    select(entries, options = {}) {
      aborted(options.signal);
      return this.gameData.select(entries);
    }

    match(verified) {
      return {
        gameId: this.gameId,
        editionId: "dos-english-ega",
        profileId: verified.profile,
        confidence: "exact",
        evidence: {label:verified.verification.label,fileCount:verified.files.length},
        verified,
      };
    }

    inventory(source, options = {}) {
      aborted(options.signal);
      const entries = source?.kind === "zip" ? this.extractZip(source.data, this.maxZipBytes) : source?.files;
      if (!Array.isArray(entries)) throw new TypeError("Choose a game folder or supported ZIP.");
      return entries.map((entry, index) => ({
        id: `entry-${index + 1}`,
        path: entry.name,
        size: entry.size ?? entry.data?.byteLength,
        entry,
      }));
    }

    async detect(inventory, options = {}) {
      aborted(options.signal);
      options.reportProgress?.({phase:"detect",completed:0,total:1});
      const verified = await this.gameData.validate(inventory.map(item => item.entry));
      aborted(options.signal);
      options.reportProgress?.({phase:"detect",completed:1,total:1});
      return [this.match(verified)];
    }

    async createInstallPlan(match, options = {}) {
      aborted(options.signal);
      if (match?.gameId !== this.gameId || !match.verified) throw new TypeError("A verified Ultima IV detection result is required.");
      const logicalBytes = match.verified.files.reduce((sum, file) => sum + file.data.byteLength, 0);
      return Object.freeze({
        contractVersion: 1,
        adapterId: this.id,
        gameId: this.gameId,
        editionId: match.editionId,
        profileId: match.profileId,
        logicalBytes,
        destination: Object.freeze({role:"source-data",mount:"/ultima4"}),
        publication: "atomic-record-with-runtime-rollback",
        privacy: "local-only-unless-user-uploads",
      });
    }

    async prepare(source, options = {}) {
      const inventory = this.inventory(source, options);
      const [match] = await this.detect(inventory, options);
      const plan = await this.createInstallPlan(match, options);
      return {inventory,match,plan,verified:match.verified};
    }

    async preparePackage(text, options = {}) {
      aborted(options.signal);
      options.reportProgress?.({phase:"verify-package",completed:0,total:1});
      const verified = await this.gameData.decodePackage(text);
      aborted(options.signal);
      const match = this.match(verified);
      const plan = await this.createInstallPlan(match, options);
      options.reportProgress?.({phase:"verify-package",completed:1,total:1});
      return {match,plan,verified};
    }
  }

  if (typeof module !== "undefined" && module.exports) module.exports = {UltimaIVImportAdapter};
  else global.UltimaIVImportAdapter = UltimaIVImportAdapter;
})(typeof window !== "undefined" ? window : globalThis);
