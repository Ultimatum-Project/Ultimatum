const id = /^[a-z0-9]+(?:-[a-z0-9]+)*$/;
const requiredSemantics = ["currentAndPrevious","compareAndSwap","atomicPublication","quarantineExport","metadataOnlyTransactions"];

function requireValue(condition, message) {
  if (!condition) throw new TypeError(`Invalid save-store provider: ${message}`);
}

export function validateSaveStoreProvider(value) {
  requireValue(value && typeof value === "object" && !Array.isArray(value), "expected an object");
  requireValue(value.contractVersion === 1, "unsupported contractVersion");
  requireValue(id.test(value.gameId || ""), "gameId is not a stable ID");
  requireValue(Array.isArray(value.providers) && value.providers.length > 0, "at least one provider is required");
  const providerIds = new Set();
  for (const provider of value.providers) {
    requireValue(provider && id.test(provider.id || ""), "provider.id is not stable");
    requireValue(!providerIds.has(provider.id), `duplicate provider ${provider.id}`);
    providerIds.add(provider.id);
    requireValue(["web","native"].includes(provider.host), `unsupported host ${provider.host}`);
    requireValue(typeof provider.physicalModel === "string" && provider.physicalModel.length > 0, `${provider.id} needs a physical model`);
  }
  requireValue(Array.isArray(value.portableFormats) && value.portableFormats.length > 0, "a portable format is required");
  for (const format of value.portableFormats) {
    requireValue(typeof format?.format === "string" && format.format.length > 0, "portable format name is required");
    requireValue(Number.isSafeInteger(format.version) && format.version > 0, "portable format version is invalid");
    requireValue(/^\.[a-z0-9]+$/.test(format.extension || ""), "portable format extension is invalid");
  }
  for (const semantic of requiredSemantics)
    requireValue(value.semantics?.[semantic] === true, `${semantic} must be guaranteed`);
  return value;
}
