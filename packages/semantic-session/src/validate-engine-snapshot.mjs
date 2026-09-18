function requireValue(condition, message) {
  if (!condition) throw new TypeError(`Invalid engine snapshot: ${message}`);
}

export function validateEngineSnapshotV1(value) {
  requireValue(value && typeof value === "object" && !Array.isArray(value), "expected an object");
  requireValue(value.contractVersion === 1, `unsupported contractVersion ${value.contractVersion ?? "missing"}`);
  requireValue(typeof value.ready === "boolean", "ready must be boolean");
  requireValue(typeof value.inputMode === "string" && value.inputMode.length > 0, "inputMode is required");
  requireValue(value.capabilities && typeof value.capabilities === "object" && !Array.isArray(value.capabilities), "capabilities are required");
  requireValue(value.prompt && typeof value.prompt === "object" && !Array.isArray(value.prompt), "prompt is required");
  requireValue(typeof value.prompt.kind === "string", "prompt.kind is required");
  if (value.prompt.id !== undefined)
    requireValue(Number.isSafeInteger(value.prompt.id) && value.prompt.id >= 0, "prompt.id must be a non-negative integer");
  return value;
}
