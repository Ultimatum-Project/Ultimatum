const stableId = /^[a-z0-9]+(?:-[a-z0-9]+)*$/;
const language = /^[a-z]{2,3}(?:-[A-Z]{2})?$/;
const repositoryPath = /^(?![A-Za-z]:|\/)(?!.*(?:^|\/)\.\.(?:\/|$))[A-Za-z0-9._/-]+$/;
const availability = new Set(["available", "preview", "hidden", "retired"]);
const acquisitionModes = new Set(["bring-your-own-data", "authorized-download", "bundled"]);
const nativeHosts = new Set(["ios", "macos", "windows", "linux", "android"]);
const artworkRoles = new Set(["cover", "hero", "screenshot", "icon"]);

function requireValue(condition, message) {
  if (!condition) throw new TypeError(`Invalid catalog entry: ${message}`);
}

function isObject(value) {
  return value && typeof value === "object" && !Array.isArray(value);
}

function isLabel(value, maximum = 160) {
  return typeof value === "string" && value.length > 0 && value.length <= maximum;
}

function unique(values) {
  return new Set(values).size === values.length;
}

export function validateCatalogEntry(value, options = {}) {
  requireValue(isObject(value), "expected an object");
  requireValue(value.schemaVersion === 1, "unsupported schemaVersion");
  for (const name of ["gameId", "portId", "slug"])
    requireValue(typeof value[name] === "string" && stableId.test(value[name]), `${name} is not a stable ID`);
  for (const name of ["title", "series", "publisher"])
    requireValue(isLabel(value[name]), `${name} is required`);
  requireValue(Number.isSafeInteger(value.releaseYear) && value.releaseYear >= 1970 && value.releaseYear <= 2100, "releaseYear is invalid");
  requireValue(Array.isArray(value.developers) && value.developers.length > 0 && unique(value.developers) && value.developers.every(item => isLabel(item)), "developers are invalid");
  requireValue(isObject(value.descriptions) && isLabel(value.descriptions.short, 240) && isLabel(value.descriptions.long, 2000), "descriptions are required");
  requireValue(availability.has(value.availability), "availability is invalid");

  requireValue(isObject(value.port), "port reference is required");
  requireValue(repositoryPath.test(value.port.descriptor || ""), "port.descriptor is not a safe repository path");
  requireValue(isLabel(value.port.adapterApiRange, 80), "port.adapterApiRange is required");

  requireValue(Array.isArray(value.editions) && value.editions.length > 0, "at least one edition is required");
  const editionIds = new Set();
  for (const edition of value.editions) {
    requireValue(isObject(edition) && stableId.test(edition.id || ""), "edition.id is not a stable ID");
    requireValue(!editionIds.has(edition.id), `duplicate edition ${edition.id}`);
    editionIds.add(edition.id);
    requireValue(isLabel(edition.label), `edition ${edition.id} needs a label`);
    requireValue(Array.isArray(edition.languages) && edition.languages.length > 0 && unique(edition.languages) && edition.languages.every(code => language.test(code)), `edition ${edition.id} languages are invalid`);
    requireValue(isObject(edition.acquisition) && acquisitionModes.has(edition.acquisition.mode) && isLabel(edition.acquisition.guidance, 500), `edition ${edition.id} acquisition guidance is invalid`);
  }

  requireValue(isObject(value.hostSupport) && isObject(value.hostSupport.web), "hostSupport is required");
  for (const name of ["supported", "offlineAfterInstall"])
    requireValue(typeof value.hostSupport.web[name] === "boolean", `hostSupport.web.${name} must be boolean`);
  requireValue(Array.isArray(value.hostSupport.native) && unique(value.hostSupport.native) && value.hostSupport.native.every(host => nativeHosts.has(host)), "hostSupport.native is invalid");
  requireValue(isObject(value.defaultControlProfiles), "defaultControlProfiles is required");
  for (const name of ["desktop", "touch"])
    requireValue(stableId.test(value.defaultControlProfiles[name] || ""), `defaultControlProfiles.${name} is invalid`);

  requireValue(Array.isArray(value.artwork), "artwork must be an array");
  for (const item of value.artwork)
    requireValue(isObject(item) && artworkRoles.has(item.role) && repositoryPath.test(item.path || "") && isLabel(item.provenance, 500), "artwork needs a role, safe path, and provenance");
  requireValue(Array.isArray(value.knownLimitations) && unique(value.knownLimitations) && value.knownLimitations.every(item => isLabel(item, 500)), "knownLimitations are invalid");
  requireValue(Array.isArray(value.notices) && value.notices.length > 0 && value.notices.every(item => isObject(item) && isLabel(item.label) && repositoryPath.test(item.path || "")), "notices are invalid");

  const port = options.portDescriptor;
  if (port) {
    requireValue(port.gameId === value.gameId, "gameId disagrees with the port descriptor");
    requireValue(port.portId === value.portId, "portId disagrees with the port descriptor");
    requireValue(port.adapterApiRange === value.port.adapterApiRange, "adapterApiRange disagrees with the port descriptor");
    const portEditions = new Set((port.editions || []).map(edition => edition.id));
    for (const editionId of editionIds)
      requireValue(portEditions.has(editionId), `edition ${editionId} is not declared by the port`);
  }
  return value;
}
