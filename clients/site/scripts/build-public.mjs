import {readFile, writeFile, mkdir, cp, readdir, stat, rename, rm} from "node:fs/promises";
import {createHash} from "node:crypto";
import {spawnSync} from "node:child_process";
import path from "node:path";
import {fileURLToPath} from "node:url";
import {cloudEnvironment, renderCloudConfig} from "./cloud-environments.mjs";

const site = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const repo = path.resolve(site, "../..");
const web = path.join(repo,"clients/web");
const cache = path.join(site,".cache");
const engine = path.join(cache,"engine");
const staging = path.join(cache,"public-staging");
const hash = bytes => createHash("sha256").update(bytes).digest("hex");
// The graphics-only archive produced from the checksum-pinned 1.3 download.
// Never allow the original patcher's executables into either distribution mode.
const graphicsSHA256 = "656281a611a58489a1f009e8a5bec8968a1b4c91202ea0c5ce5e4bdcec503225";

export function packageMetadata(script) {
  const match = script.match(/loadPackage\((\{(?:"files"|files):.*?(?:"remote_package_size"|remote_package_size):\d+\})\)/);
  if (!match) throw Error("Cannot inspect the engine preload manifest; refusing a public build.");
  const metadata = JSON.parse(match[1].replace(/([,{])([a-zA-Z_]\w*):/g,'$1"$2":'));
  for (const file of metadata.files) {
    const soundtrack = /^\/music\/hurin\/[^/]+\.(ogg|txt|xml)$/i.test(file.filename);
    if (!/^\/(conf|graphics|sound)\//.test(file.filename) && file.filename !== "/u4upgrad.zip" && !soundtrack) throw Error(`Forbidden public preload: ${file.filename}`);
  }
  return metadata;
}

export function inspectPublicAssets(metadata, data) {
  if (data.length !== metadata.remote_package_size) throw Error("Engine assets do not match the compiled preload manifest.");
  const overlays = metadata.files.filter(file => file.filename === "/u4upgrad.zip");
  if (overlays.length !== 1) throw Error("Public build requires exactly one verified graphics-only VGA overlay.");
  const {start,end} = overlays[0];
  if (!Number.isInteger(start) || !Number.isInteger(end) || start < 0 || end <= start || end > data.length ||
      hash(data.subarray(start,end)) !== graphicsSHA256) throw Error("Unverified public VGA overlay; refusing a public build.");
}

async function copyDirectory(from, to, filter = () => true) {
  await mkdir(to,{recursive:true});
  for (const entry of await readdir(from,{withFileTypes:true})) {
    if (!filter(entry)) continue;
    const source = path.join(from,entry.name), target = path.join(to,entry.name);
    if (entry.isDirectory()) await copyDirectory(source,target,filter);
    else if (entry.isFile()) await cp(source,target);
  }
}

export async function build(options = {}) {
  const testBuild = typeof options === "object" && Boolean(options.qaDebug);
  const debugTools = testBuild || (typeof options === "object" && Boolean(options.debugTools));
  const audience = testBuild ? "test" : "public";
  const cloud = cloudEnvironment(audience);
  const output = path.join(site,testBuild ? "build-test" : "build");
  await mkdir(cache,{recursive:true});
  const compiled = spawnSync(process.execPath,[path.join(web,"scripts/run-shell.mjs"),"build-engine.sh"], {
    cwd:web, stdio:"inherit", env:{...process.env,
      ULTIMATUM_BUNDLE_U4_DATA:"OFF", ULTIMATUM_WEB_DEBUG_TOOLS:debugTools ? "ON" : "OFF",
      ULTIMATUM_BUNDLE_MUSIC:"ON",
      ULTIMATUM_WEB_BUILD_DIRECTORY:path.join(cache,"engine-build"), ULTIMATUM_WEB_OUTPUT_DIRECTORY:engine}
  });
  if (compiled.status !== 0) throw Error("The public engine build failed.");
  const config = await readFile(path.join(cache,"engine-build/CMakeCache.txt"),"utf8");
  for (const flag of ["ULTIMATUM_BUNDLE_U4_DATA","ULTIMATUM_WEB_RUNTIME_TESTS","ULTIMATUM_WEB_DIAGNOSTICS"]) {
    if (!config.includes(`${flag}:BOOL=OFF`)) throw Error(`Public build must disable ${flag}.`);
  }
  if (!config.includes(`ULTIMATUM_WEB_DEBUG_TOOLS:BOOL=${debugTools ? "ON" : "OFF"}`)) throw Error("Debug tools do not match the selected distribution mode.");
  if (!config.includes("ULTIMATUM_BUNDLE_MUSIC:BOOL=ON")) throw Error("Public builds must include the xu4 soundtrack.");
  const script = (await readFile(path.join(engine,"ultimatum-engine.js"),"utf8")).replaceAll(path.join(engine,"ultimatum-engine.data"),"ultimatum-engine.data");
  const metadata = packageMetadata(script);
  if (metadata.files.filter(file=>/\/music\/hurin\/.*\.ogg$/i.test(file.filename)).length !== 9) throw Error("Public builds require the complete nine-track xu4 soundtrack.");
  const data = await readFile(path.join(engine,"ultimatum-engine.data"));
  inspectPublicAssets(metadata,data);
  // This is a fixed, generated staging directory—not a user-selected target.
  await rm(staging,{recursive:true,force:true});
  await mkdir(staging,{recursive:true});
  await copyDirectory(path.join(site,"dist"),staging);
  await writeFile(path.join(staging,"build-info.json"),JSON.stringify({soundtrack:"xu4",qaDebug:debugTools,cloudProject:cloud.project}));
  const play = path.join(staging,"play");
  await mkdir(path.join(play,"engine"),{recursive:true});
  for (const name of ["index.html","app.css","app.js","engine-client.js","engine-session.js","library-store.js","import-adapter.js","save-store.js","adventure-ui.js","journal-ui.js","cloud-sdk.js","cloud-sdk.js.LEGAL.txt","cloud-config.js","cloud-client.js","cloud-ui.js","cloud.css","game-data.js","game-data-manifest.js","manifest.webmanifest","sw.js"]) await cp(path.join(web,"dist",name),path.join(play,name));
  await writeFile(path.join(play,"cloud-config.js"),renderCloudConfig(audience));
  // Old decorative sprite URLs are unnecessary in the engine-backed UI and
  // should not cause game-data images to be copied into the public shell.
  const css = (await readFile(path.join(play,"app.css"),"utf8")).replace(/url\("assets\/[^"\n]+"\)/g,"none");
  await writeFile(path.join(play,"app.css"),css);
  let html = await readFile(path.join(play,"index.html"),"utf8");
  html = html.replace('<script src="app.js', '<script src="public-loader.js"></script>\n    <script src="app.js');
  await writeFile(path.join(play,"index.html"),html);
  await cp(path.join(site,"dist/public-loader.js"),path.join(play,"public-loader.js"));
  const wasmBytes = await readFile(path.join(engine,"ultimatum-engine.wasm"));
  const code = `engine-${hash(script).slice(0,16)}.js`, wasm = `engine-${hash(wasmBytes).slice(0,16)}.wasm`;
  await writeFile(path.join(play,"engine",code),script);
  await writeFile(path.join(play,"engine",wasm),wasmBytes);
  const id = hash(data).slice(0,16), chunks = [];
  for (let start=0,index=0; start<data.length; start+=20*1024*1024,index++) {
    const bytes = data.subarray(start,Math.min(start+20*1024*1024,data.length));
    const name = `assets-${id}-${index}.bin`;
    chunks.push({name,size:bytes.length,sha256:hash(bytes)});
    await writeFile(path.join(play,"engine",name),bytes);
  }
  await writeFile(path.join(play,"engine/assets.json"),JSON.stringify({version:1,size:data.length,code,wasm,chunks}));
  const source = path.join(staging,"source");
  const licenses = path.join(staging,"licenses");
  await mkdir(source,{recursive:true});
  await mkdir(licenses,{recursive:true});
  await cp(path.join(repo,".env.example"),path.join(source,".env.example"));
  for (const directory of ["packages/adapter-sdk","packages/import-framework","packages/save-store","packages/semantic-session","packages/storage","ports/ultima-iv"])
    await copyDirectory(path.join(repo,directory),path.join(source,directory));
  await cp(path.join(web,".cache/deps/libxml2/Copyright"),path.join(licenses,"libxml2.txt"));
  await cp(path.join(web,".cache/ports/SDL2/LICENSE.txt"),path.join(licenses,"SDL2.txt"));
  const zlibRoot = path.join(web,".cache/emscripten/ports/zlib");
  const zlibVersions = (await readdir(zlibRoot,{withFileTypes:true})).filter(entry=>entry.isDirectory() && /^zlib-/.test(entry.name));
  if (zlibVersions.length !== 1) throw Error("Cannot identify the bundled zlib license; refusing a public build.");
  await cp(path.join(zlibRoot,zlibVersions[0].name,"LICENSE"),path.join(licenses,"zlib.txt"));
  for (const directory of ["src","deps","conf","graphics","sound"]) await copyDirectory(path.join(repo,"vendor/ultima4-ios",directory),path.join(source,"vendor/ultima4-ios",directory));
  // The web build invokes this sibling script; include it for reproducibility.
  await mkdir(path.join(source,"vendor/ultima4-ios/ios"),{recursive:true});
  for (const name of ["prepare-vga-upgrade.sh"])
    await cp(path.join(repo,"vendor/ultima4-ios/ios",name),path.join(source,"vendor/ultima4-ios/ios",name));
  const vgaCredits = await readFile(path.join(web,".cache/assets/ultima-iv-vga-readme.txt"));
  await writeFile(path.join(licenses,"ultima-iv-vga.txt"),vgaCredits);
  for (const name of ["COPYING","AUTHORS","README.md","README.old"]) await cp(path.join(repo,"vendor/ultima4-ios",name),path.join(source,"vendor/ultima4-ios",name));
  for (const directory of ["toolchain","scripts","src","dist"]) await copyDirectory(path.join(web,directory),path.join(source,"clients/web",directory),entry=>entry.name!=="engine" && entry.name!=="assets");
  for (const name of ["CMakeLists.txt","README.md","package.json","package-lock.json"]) await cp(path.join(web,name),path.join(source,"clients/web",name));
  for (const directory of ["dist","scripts"]) await copyDirectory(path.join(site,directory),path.join(source,"clients/site",directory));
  for (const name of ["package.json","wrangler.jsonc"]) await cp(path.join(site,name),path.join(source,"clients/site",name));
  await writeFile(path.join(staging,"source.html"),`<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Engine source — Ultimatum Project</title><link rel="stylesheet" href="styles.css"></head><body><main class="page"><section class="project-section"><div><p class="eyebrow">OPEN-SOURCE ENGINE</p><h1>Under the hood.</h1><p>This web client uses the xu4 engine, licensed under GNU GPL version 2 or later.</p><p><a href="source/vendor/ultima4-ios/COPYING">Engine license</a> · <a href="source/vendor/ultima4-ios/AUTHORS">Contributors</a> · <a href="source/clients/web/README.md">Web build instructions</a> · <a href="source-files.json">Source file list</a></p><p>The corresponding engine source and included engine assets for this build are available at the paths in that list. Original Ultima IV DOS data and the optional VGA archive are not included.</p><p><a href="./">Back to Ultimatum Project</a></p></div></section></main></body></html>`);
  const files = [];
  const credits = (await readFile(path.join(staging,"source.html"),"utf8")).replace(
    "Original Ultima IV DOS data and the optional VGA archive are not included.",
    `Original Ultima IV DOS data is not included. The graphics-only VGA overlay is included with <a href="licenses/ultima-iv-vga.txt">original author credits and redistribution terms</a>. The soundtrack supplied with xu4 is included; alternate soundtrack packs are not distributed. Music payloads are not copied into the downloadable source snapshot.`);
  await writeFile(path.join(staging,"source.html"),credits.replace('<p><a href="./">Back', '<p>Bundled library licenses: <a href="licenses/SDL2.txt">SDL2</a> · <a href="licenses/libxml2.txt">libxml2</a> · <a href="licenses/zlib.txt">zlib</a>.</p><p><a href="./">Back'));
  async function inspect(directory) {
    for (const entry of await readdir(directory,{withFileTypes:true})) {
      const filename=path.join(directory,entry.name);
      if (entry.isDirectory()) await inspect(filename);
      else if (entry.isFile()) {
        const size=(await stat(filename)).size;
        if (size>25*1024*1024) throw Error(`Cloudflare asset exceeds 25 MiB: ${filename}`);
        const relative=path.relative(staging,filename).replaceAll(path.sep,"/");
        if (relative.startsWith("source/")) files.push({path:relative,size,sha256:hash(await readFile(filename))});
      } else throw Error("Symlinks are not allowed in the public build.");
    }
  }
  await inspect(staging);
  await writeFile(path.join(staging,"source-files.json"),JSON.stringify({version:1,files},null,2));
  // Publish locally only after the entire build passes inspection.
  await rm(output,{recursive:true,force:true});
  await rename(staging,output);
  const label = testBuild ? "QA debug build with xu4 soundtrack" : `${debugTools ? "Early-access debug" : "Public"} release with xu4 soundtrack`;
  console.log(`${label} prepared in ${output}; ${chunks.length} engine asset chunks. Nothing has been deployed.`);
}
if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  await build({qaDebug:process.argv.includes("--qa-debug"),debugTools:process.argv.includes("--debug-tools")});
}
