const id = /^[a-z0-9]+(?:-[a-z0-9]+)*$/;
const repositoryPath = /^(?!\/)(?!.*(?:^|\/)\.\.(?:\/|$))[A-Za-z0-9._/-]+$/;
const operations = new Set(["start","getSnapshot","subscribe","dispatch","requestCheckpoint","pause","resume","quiesce","shutdown"]);
const semanticOperations = [...operations];
const lifecycleOperations = ["requestCheckpoint","pause","resume","quiesce","shutdown"];

function requireValue(condition, message) {
  if (!condition) throw new TypeError(`Invalid engine-session provider: ${message}`);
}

export function validateEngineSessionProvider(value) {
  requireValue(value && typeof value === "object" && !Array.isArray(value), "expected an object");
  requireValue(value.contractVersion === 1, "unsupported contractVersion");
  requireValue(id.test(value.gameId || ""), "gameId is not a stable ID");
  requireValue(value.snapshotSchema === "engine-snapshot-v1", "snapshotSchema must remain engine-snapshot-v1");
  requireValue(Array.isArray(value.providers) && value.providers.length > 0, "at least one provider is required");
  const providerIds = new Set();
  for (const provider of value.providers) {
    requireValue(provider && typeof provider === "object" && !Array.isArray(provider), "provider must be an object");
    requireValue(id.test(provider.id || ""), "provider.id is not stable");
    requireValue(!providerIds.has(provider.id), `duplicate provider ${provider.id}`);
    providerIds.add(provider.id);
    requireValue(["web","native"].includes(provider.host), `unsupported host ${provider.host}`);
    requireValue(["semantic","lifecycle-compatibility"].includes(provider.surface), `unsupported surface ${provider.surface}`);
    requireValue(repositoryPath.test(provider.implementation || ""), `${provider.id} has an unsafe implementation path`);
    requireValue(repositoryPath.test(provider.activeWiring || ""), `${provider.id} has an unsafe wiring path`);
    requireValue(typeof provider.entryPoint === "string" && provider.entryPoint.length > 0, `${provider.id} needs an entry point`);
    requireValue(["promise-serialized-main-thread","serialized-sdl-event-loop"].includes(provider.executionModel), `${provider.id} has an unsupported execution model`);
    requireValue(Array.isArray(provider.operations) && provider.operations.length > 0, `${provider.id} needs operations`);
    requireValue(new Set(provider.operations).size === provider.operations.length, `${provider.id} has duplicate operations`);
    requireValue(provider.operations.every(operation=>operations.has(operation)), `${provider.id} has an unknown operation`);
    const required = provider.surface === "semantic" ? semanticOperations : lifecycleOperations;
    for (const operation of required)
      requireValue(provider.operations.includes(operation), `${provider.id} is missing ${operation}`);
    if (provider.surface === "semantic")
      requireValue(provider.host === "web", "the current semantic compatibility provider is web-only");
    if (provider.surface === "lifecycle-compatibility")
      requireValue(provider.host === "native", "the current lifecycle compatibility provider is native-only");
  }
  for (const semantic of ["serializedExecution","lifecycleActionsOnEngineThread","additiveSnapshots"])
    requireValue(value.semantics?.[semantic] === true, `${semantic} must be guaranteed`);
  return value;
}
