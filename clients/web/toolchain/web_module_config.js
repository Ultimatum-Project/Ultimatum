// Emscripten emits --pre-js after creating its script-local `Module` binding.
// Reconnect that binding to the object prepared by the platform host so a
// dynamically loaded engine receives its canvas, lifecycle hooks and paths.
if (typeof globalThis !== "undefined" && globalThis.UltimatumModuleConfig) {
  // Keep preload-generated hooks already attached to the shell-owned object,
  // then layer the platform callbacks and canvas configuration onto it.
  Object.assign(Module, globalThis.UltimatumModuleConfig);
  globalThis.Module = Module;
}
