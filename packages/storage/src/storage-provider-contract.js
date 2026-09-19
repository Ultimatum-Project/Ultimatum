(function installStorageProviderContract(global) {
  const REQUIRED_OPERATIONS = Object.freeze(["beginTransaction", "read", "stat", "list", "estimate"]);

  function assertStorageProvider(provider) {
    if (!provider || typeof provider.id !== "string" || !provider.id)
      throw new TypeError("A storage provider must have a stable id.");
    if (!provider.capabilities || typeof provider.capabilities !== "object")
      throw new TypeError("A storage provider must declare capabilities.");
    for (const operation of REQUIRED_OPERATIONS) {
      if (typeof provider[operation] !== "function")
        throw new TypeError(`Storage provider ${provider.id} is missing ${operation}().`);
    }
    return provider;
  }

  const exported = {REQUIRED_OPERATIONS, assertStorageProvider};
  if (typeof module !== "undefined" && module.exports) module.exports = exported;
  else global.UltimatumStorageProviderContract = Object.freeze(exported);
})(typeof window !== "undefined" ? window : globalThis);
