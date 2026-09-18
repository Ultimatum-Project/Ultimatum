const VERSION = "ultimatum-web-shell-20260918-platform03";
const SHELL = ["./", "index.html", "app.css", "app.js", "engine-client.js", "engine-session.js", "library-store.js", "import-adapter.js", "save-store.js", "adventure-ui.js", "journal-ui.js", "game-data.js", "game-data-manifest.js", "manifest.webmanifest"];

self.addEventListener("install", event => {
  event.waitUntil(caches.open(VERSION).then(cache => cache.addAll(SHELL)).then(() => self.skipWaiting()));
});

self.addEventListener("activate", event => {
  event.waitUntil(caches.keys().then(keys => Promise.all(keys.filter(key => key.startsWith("ultimatum-web-shell-") && key !== VERSION).map(key => caches.delete(key)))).then(() => self.clients.claim()));
});

self.addEventListener("fetch", event => {
  const request = event.request;
  if (request.method !== "GET") return;
  const url = new URL(request.url);
  if (url.origin !== self.location.origin) return;
  const immutableEngineAsset = /\/engine\/(?:engine-|assets-)[^/]+\.(?:js|wasm|bin)$/.test(url.pathname);
  if (immutableEngineAsset) {
    event.respondWith(caches.open(VERSION).then(async cache => {
      const cached = await cache.match(request);
      if (cached) return cached;
      const response = await fetch(request);
      if (response.ok) cache.put(request,response.clone());
      return response;
    }));
    return;
  }
  if (request.mode === "navigate") {
    event.respondWith(fetch(request).then(response => {
      if (response.ok) caches.open(VERSION).then(cache => cache.put("./",response.clone()));
      return response;
    }).catch(() => caches.match("./")));
    return;
  }
  event.respondWith(fetch(request).then(response => {
    const shellAsset = SHELL.some(path => path !== "./" && url.pathname.endsWith(path.replace(/^\.\//,"")));
    if (response.ok && shellAsset)
      caches.open(VERSION).then(cache => cache.put(request,response.clone()));
    return response;
  }).catch(() => caches.match(request)));
});
