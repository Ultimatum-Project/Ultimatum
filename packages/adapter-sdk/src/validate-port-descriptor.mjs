const id = /^[a-z0-9]+(?:-[a-z0-9]+)*$/;
const profileId = /^[a-z0-9]+(?:[.-][a-z0-9]+)*$/;
const enums = {
  saves: new Set(["none", "opaque-files", "managed"]),
  mods: new Set(["none", "files", "packages", "custom"]),
  multiplayer: new Set(["none", "local", "peer", "server"]),
  threads: new Set(["none", "optional", "required"]),
  pointerLock: new Set(["none", "optional", "required"]),
};

function requireValue(condition, message) {
  if (!condition) throw new TypeError(`Invalid port descriptor: ${message}`);
}

export function validatePortDescriptor(value) {
  requireValue(value && typeof value === "object" && !Array.isArray(value), "expected an object");
  requireValue(value.schemaVersion === 1, "unsupported schemaVersion");
  for (const name of ["gameId", "portId"])
    requireValue(typeof value[name] === "string" && id.test(value[name]), `${name} is not a stable ID`);
  requireValue(typeof value.adapterApiRange === "string" && value.adapterApiRange.length > 0, "adapterApiRange is required");
  requireValue(/^\d+\.\d+\.\d+(?:[-+][0-9A-Za-z.-]+)?$/.test(value.portVersion), "portVersion is not semantic");

  const engine = value.engine;
  requireValue(engine && id.test(engine.id || ""), "engine.id is not a stable ID");
  requireValue(typeof engine.version === "string" && engine.version.length > 0, "engine.version is required");
  for (const name of ["upstreamUrl", "sourceUrl"])
    requireValue(typeof engine[name] === "string" && engine[name].startsWith("https://"), `engine.${name} must use HTTPS`);
  requireValue(typeof engine.licenseId === "string" && engine.licenseId.length > 0, "engine.licenseId is required");

  requireValue(Array.isArray(value.editions) && value.editions.length > 0, "at least one edition is required");
  const editionIds = new Set();
  for (const edition of value.editions) {
    requireValue(edition && id.test(edition.id || ""), "edition.id is not a stable ID");
    requireValue(!editionIds.has(edition.id), `duplicate edition ${edition.id}`);
    editionIds.add(edition.id);
    requireValue(typeof edition.label === "string" && edition.label.length > 0, `edition ${edition.id} needs a label`);
    requireValue(Array.isArray(edition.importProfileIds) && edition.importProfileIds.length > 0 &&
      edition.importProfileIds.every(profile => profileId.test(profile)), `edition ${edition.id} needs stable import profiles`);
  }

  const capabilities = value.capabilities;
  requireValue(capabilities && typeof capabilities === "object", "capabilities are required");
  for (const name of Object.keys(enums))
    requireValue(enums[name].has(capabilities[name]), `unsupported capabilities.${name}`);
  for (const name of ["offline", "cloudSaveEligible", "supportsPause", "supportsSuspend", "supportsMultipleProfiles", "supportsMultipleSurfaces"])
    requireValue(typeof capabilities[name] === "boolean", `capabilities.${name} must be boolean`);
  requireValue(Array.isArray(capabilities.rendering) && capabilities.rendering.length > 0, "rendering capabilities are required");
  requireValue(Array.isArray(capabilities.input) && capabilities.input.length > 0, "input capabilities are required");
  for (const name of ["actionsSchemaVersion", "settingsSchemaVersion", "diagnosticsSchemaVersion", "saveSchemaVersion"])
    requireValue(Number.isSafeInteger(value[name]) && value[name] > 0, `${name} must be a positive integer`);
  for (const name of ["controlsManifest","settingsManifest","diagnosticsManifest"])
    requireValue(typeof value[name]==="string"&&!value[name].startsWith("/")&&!value[name].includes("\\")&&!value[name].split("/").includes("..")&&value[name].endsWith(".json"),`${name} must be a safe repository JSON path`);
  return value;
}
