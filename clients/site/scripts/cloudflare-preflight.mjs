// Read-only gate for our Cloudflare account deployment. Never uploads files.
import {readFile, readdir} from "node:fs/promises";
import {createHash} from "node:crypto";
import path from "node:path";
import {fileURLToPath} from "node:url";
import {packageMetadata, inspectPublicAssets} from "./build-public.mjs";
import {checkCloudConfig,cloudEnvironment} from "./cloud-environments.mjs";

const sha = bytes => createHash("sha256").update(bytes).digest("hex");
export function checkReleaseInfo(info, audience = "public") {
  if (!["public", "test"].includes(audience)) throw Error("Unknown deployment audience.");
  if (info.soundtrack !== "xu4") throw Error("Public releases must include only the xu4 soundtrack.");
  if (typeof info.qaDebug !== "boolean") throw Error("Missing QA-debug release metadata.");
  if (!info.qaDebug) throw Error("Debug Tools are required in the current early-access release.");
  if (info.cloudProject !== cloudEnvironment(audience).project) throw Error(`${audience} package uses the wrong Supabase project.`);
}
export function checkAsset(relative, size) {
  if (size > 25 * 1024 * 1024) throw Error(`Cloudflare asset exceeds 25 MiB: ${relative}`);
  const documentedEnvironmentTemplate=relative==="source/.env.example";
  if ((!documentedEnvironmentTemplate&&/(^|\/)(\.env(?:\..*)?|\.git|node_modules|\.cache)(\/|$)/i.test(relative)) ||
      /\.(sav|u4save)$/i.test(relative) || /(^|\/)(AVATAR|TITLE)\.EXE$/i.test(relative) ||
      /(^|\/)(runtime-driver|title-runtime-bootstrap|desktop-runtime-driver)\.js$/i.test(relative))
    throw Error(`Private or runtime-fixture asset in deployment: ${relative}`);
}
function safePath(relative) {
  if (typeof relative !== "string" || relative.includes("\\") || relative.split("/").includes("..") || path.isAbsolute(relative))
    throw Error("Unsafe deployment manifest path.");
  return relative;
}
export async function preflight(root, audience = "public") {
  const info = JSON.parse(await readFile(path.join(root, "build-info.json"), "utf8"));
  checkReleaseInfo(info, audience);
  checkCloudConfig(await readFile(path.join(root,"play/cloud-config.js"),"utf8"),audience);
  let count = 0, total = 0;
  async function walk(relative = "") {
    for (const entry of await readdir(path.join(root, relative), {withFileTypes:true})) {
      const name = path.posix.join(relative, entry.name);
      if (entry.isSymbolicLink()) throw Error(`Deployment must not contain symlinks: ${name}`);
      if (entry.isDirectory()) {checkAsset(name, 0); await walk(name);}
      else if (entry.isFile()) {
        const bytes = await readFile(path.join(root, name));
        checkAsset(name, bytes.length); count++; total += bytes.length;
      } else throw Error(`Unsupported deployment entry: ${name}`);
    }
  }
  await walk();
  if (count > 20000) throw Error("Deployment exceeds Cloudflare's free-tier 20,000 static asset limit.");
  for (const name of ["index.html", "404.html", "play/index.html", "admin/index.html", "admin/admin.js", "source.html", "_headers"])
    if (!(await readFile(path.join(root, name))).length) throw Error(`Missing deployment entry: ${name}`);
  const manifest = JSON.parse(await readFile(path.join(root, "play/engine/assets.json"), "utf8"));
  const chunks = [];
  for (const chunk of manifest.chunks) {
    const bytes = await readFile(path.join(root, "play/engine", safePath(chunk.name)));
    if (bytes.length !== chunk.size || sha(bytes) !== chunk.sha256) throw Error("Engine asset checksum mismatch.");
    chunks.push(bytes);
  }
  const script = await readFile(path.join(root, "play/engine", safePath(manifest.code)), "utf8");
  inspectPublicAssets(packageMetadata(script), Buffer.concat(chunks));
  for (const name of [manifest.code, manifest.wasm]) {
    const bytes = await readFile(path.join(root, "play/engine", safePath(name)));
    if (!name.includes(sha(bytes).slice(0,16))) throw Error("Engine code checksum mismatch.");
  }
  const sources = JSON.parse(await readFile(path.join(root, "source-files.json"), "utf8"));
  for (const file of sources.files) {
    if (!safePath(file.path).startsWith("source/")) throw Error("Invalid matching-source manifest.");
    const bytes = await readFile(path.join(root, file.path));
    if (bytes.length !== file.size || sha(bytes) !== file.sha256) throw Error("Matching-source checksum mismatch.");
  }
  return {files:count, bytes:total};
}
if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  const site = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
  const testBuild = process.argv.includes("--test");
  try {
    console.log("Cloudflare package preflight passed:", await preflight(path.join(site, testBuild ? "build-test" : "build"), testBuild ? "test" : "public"));
  }
  catch (error) { console.error(error.message); process.exitCode = 1; }
}
