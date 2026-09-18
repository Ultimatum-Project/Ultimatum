(function(global) {
  global.UltimatumPublicLoader = {
    async prepare(module, status) {
      const response = await fetch("engine/assets.json", {cache:"no-cache"});
      if (!response.ok) throw Error("The engine asset list could not be downloaded.");
      const manifest = await response.json();
      if (manifest.version !== 1 || !Number.isSafeInteger(manifest.size) || manifest.size < 1 || manifest.size > 128 * 1024 * 1024 || !Array.isArray(manifest.chunks) || manifest.chunks.length > 16) throw Error("The engine asset list is invalid.");
      const data = new Uint8Array(manifest.size);
      if (!/^engine-[a-f0-9]{16}\.js$/.test(manifest.code) || !/^engine-[a-f0-9]{16}\.wasm$/.test(manifest.wasm)) throw Error("The engine asset list is invalid.");
      global.UltimatumPublicLoader.script = `engine/${manifest.code}`;
      const locate = module.locateFile;
      module.locateFile = (name, prefix) => name.endsWith(".wasm") ? `engine/${manifest.wasm}` : locate(name, prefix);
      let offset = 0;
      for (const chunk of manifest.chunks) {
        if (!/^assets-[a-f0-9]{16}-\d+\.bin$/.test(chunk.name) || !Number.isSafeInteger(chunk.size) || chunk.size < 1 || chunk.size > 20 * 1024 * 1024 || offset + chunk.size > data.length) throw Error("The engine asset list is invalid.");
        const fetched = await fetch(`engine/${chunk.name}`);
        if (!fetched.ok) throw Error("An engine asset could not be downloaded.");
        const bytes = new Uint8Array(await fetched.arrayBuffer());
        if (bytes.length !== chunk.size) throw Error("An engine asset download was incomplete.");
        if (global.crypto?.subtle) {
          const hash = await global.crypto.subtle.digest("SHA-256", bytes);
          const actual = Array.from(new Uint8Array(hash), b=>b.toString(16).padStart(2,"0")).join("");
          if (actual !== chunk.sha256) throw Error("An engine asset failed its integrity check.");
        }
        data.set(bytes, offset); offset += bytes.length;
        status(`Downloading engine assets (${Math.round(offset / data.length * 100)}%)…`);
      }
      if (offset !== data.length) throw Error("The engine asset download was incomplete.");
      module.getPreloadedPackage = (name, size) => {
        if (!/ultimatum-engine\.data(?:\?|$)/.test(name) || size !== data.length) throw Error("The engine and its assets are different versions. Reload to try again.");
        const buffer = data.buffer;
        module.getPreloadedPackage = undefined;
        return buffer;
      };
    }
  };
})(window);
