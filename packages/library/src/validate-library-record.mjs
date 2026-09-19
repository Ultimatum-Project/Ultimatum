const stableId = /^[a-z0-9]+(?:-[a-z0-9]+)*$/;
const profileId = /^[a-z0-9]+(?:[.-][a-z0-9]+)*$/;
const states = new Set(["not-installed", "importing", "playable", "update-available", "repair-required", "unsupported-host", "compatibility-disabled"]);
const syncStates = new Set(["local-only", "cloud-known", "synced", "pending", "conflict", "error"]);
const sources = new Set(["native", "indexeddb-compatibility", "opfs", "memory"]);

function requireValue(condition, message) {
  if (!condition) throw new TypeError(`Invalid library record: ${message}`);
}

function isObject(value) {
  return value && typeof value === "object" && !Array.isArray(value);
}

function isDateOrNull(value) {
  return value === null || (typeof value === "string" && !Number.isNaN(Date.parse(value)));
}

export function validateLibraryRecord(value) {
  requireValue(isObject(value), "expected an object");
  requireValue(value.schemaVersion === 1, "unsupported schemaVersion");
  for (const name of ["gameId", "portId"])
    requireValue(stableId.test(value[name] || ""), `${name} is not a stable ID`);
  requireValue(states.has(value.state), "state is invalid");

  if (value.installation === null) {
    requireValue(value.state === "not-installed" || value.state === "unsupported-host" || value.state === "compatibility-disabled", `${value.state} requires installation metadata`);
  } else {
    const install = value.installation;
    requireValue(isObject(install), "installation must be an object or null");
    for (const name of ["installId", "editionId", "storageProviderId"])
      requireValue(stableId.test(install[name] || ""), `installation.${name} is invalid`);
    requireValue(profileId.test(install.importProfileId || ""), "installation.importProfileId is invalid");
    requireValue(Number.isSafeInteger(install.logicalBytes) && install.logicalBytes >= 0, "installation.logicalBytes is invalid");
    requireValue(isDateOrNull(install.installedAt) && isDateOrNull(install.validatedAt), "installation timestamps are invalid");
  }

  requireValue(isObject(value.activity) && isDateOrNull(value.activity.lastPlayedAt), "activity is invalid");
  requireValue(Number.isSafeInteger(value.activity.playtimeSeconds) && value.activity.playtimeSeconds >= 0, "activity.playtimeSeconds is invalid");
  requireValue(typeof value.activity.favorite === "boolean", "activity.favorite must be boolean");
  requireValue(isObject(value.selection) && stableId.test(value.selection.engineChannel || ""), "selection.engineChannel is invalid");
  requireValue(value.selection.controlProfileId === null || stableId.test(value.selection.controlProfileId || ""), "selection.controlProfileId is invalid");
  requireValue(isObject(value.sync) && syncStates.has(value.sync.state), "sync.state is invalid");
  requireValue(isObject(value.compatibility) && sources.has(value.compatibility.source), "compatibility.source is invalid");
  requireValue(typeof value.compatibility.requiresMigration === "boolean", "compatibility.requiresMigration must be boolean");
  requireValue(Array.isArray(value.compatibility.reasons) && value.compatibility.reasons.every(reason => typeof reason === "string" && reason.length > 0 && reason.length <= 300), "compatibility.reasons are invalid");
  return value;
}
