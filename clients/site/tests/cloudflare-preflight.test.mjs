import test from "node:test";
import assert from "node:assert/strict";
import {readFile} from "node:fs/promises";
import {checkReleaseInfo, checkAsset} from "../scripts/cloudflare-preflight.mjs";

test("Cloudflare deployment requires only the xu4 soundtrack and Debug Tools for both early-access audiences", () => {
  checkReleaseInfo({soundtrack:"xu4",qaDebug:true,cloudProject:"ultimatum-prod"});
  checkReleaseInfo({soundtrack:"xu4",qaDebug:true,cloudProject:"ultimatum-test"},"test");
  for (const info of [{soundtrack:"none",qaDebug:true,cloudProject:"ultimatum-prod"}, {}, {soundtrack:"four-packs",qaDebug:true,cloudProject:"ultimatum-prod"}])
    assert.throws(() => checkReleaseInfo(info), /only the xu4 soundtrack/);
  assert.throws(() => checkReleaseInfo({soundtrack:"xu4",qaDebug:false,cloudProject:"ultimatum-prod"}), /required in the current early-access release/);
  assert.throws(() => checkReleaseInfo({soundtrack:"xu4",qaDebug:false,cloudProject:"ultimatum-test"},"test"), /required in the current early-access release/);
  assert.throws(() => checkReleaseInfo({soundtrack:"xu4",cloudProject:"ultimatum-prod"}), /Missing/);
  assert.throws(() => checkReleaseInfo({soundtrack:"xu4",qaDebug:true,cloudProject:"ultimatum-prod"},"anything"), /Unknown/);
  assert.throws(() => checkReleaseInfo({soundtrack:"xu4",qaDebug:true,cloudProject:"ultimatum-test"}), /wrong Supabase project/);
});
test("staging has an isolated Worker, package and no unprotected fallback hostname", async () => {
  const raw = await readFile(new URL("../wrangler.test.jsonc", import.meta.url), "utf8");
  const config = JSON.parse(raw.replace(/^\s*\/\/.*$/gm, ""));
  assert.equal(config.name, "ultimatum-project-test");
  assert.equal(config.assets.directory, "./build-test");
  assert.equal(config.workers_dev, false);
  assert.equal(config.preview_urls, false);
  assert.deepEqual(config.routes, [{pattern:"test.ultimatumproject.com", custom_domain:true}]);
});
test("production uses only the apex custom domain", async () => {
  const config = JSON.parse(await readFile(new URL("../wrangler.jsonc", import.meta.url), "utf8"));
  assert.equal(config.name, "ultimatum-project");
  assert.equal(config.assets.directory, "./build");
  assert.equal(config.workers_dev, false);
  assert.equal(config.preview_urls, false);
  assert.deepEqual(config.routes, [{pattern:"ultimatumproject.com", custom_domain:true}]);
});
test("deployment excludes private files and respects Cloudflare asset limits", () => {
  for (const name of [".env", "source/.git/config", "play/party.sav", "backups/slot.u4save", "AVATAR.EXE", "runtime-driver.js", "play/node_modules/file.js"])
    assert.throws(() => checkAsset(name, 10), /Private or runtime-fixture/);
  checkAsset("source/.env.example",1024);
  assert.throws(() => checkAsset("large.bin", 25 * 1024 * 1024 + 1), /25 MiB/);
  checkAsset("play/engine/assets-1234-0.bin", 20 * 1024 * 1024);
  checkAsset("source/vendor/ultima4-ios/src/debug.cpp", 1024); // Cheats are a product decision, not private data.
});
