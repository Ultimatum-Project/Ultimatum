// Repackage authored UI only while preserving the previously verified engine
// and its matching source snapshot. Useful during concurrent native work.
import {readFile,writeFile,mkdir} from "node:fs/promises";
import {createHash} from "node:crypto";
import path from "node:path";
import {fileURLToPath} from "node:url";
import {cloudEnvironment,renderCloudConfig} from "./cloud-environments.mjs";
const site = path.resolve(path.dirname(fileURLToPath(import.meta.url)),"..");
const web = path.resolve(site,"../web");
const audience = process.argv.includes("--test") ? "test" : "public";
const build = path.join(site,audience === "test" ? "build-test" : "build");
const hash = bytes => createHash("sha256").update(bytes).digest("hex");
const assets = JSON.parse(await readFile(path.join(build,"play/engine/assets.json"),"utf8"));
const source = JSON.parse(await readFile(path.join(build,"source-files.json"),"utf8"));
// Refuse to refresh a partial, missing or modified public release. In
// particular, do not silently attach current native sources to an older engine.
for (const file of source.files) {
  if (!file.path.startsWith("source/") || file.path.includes("..")) throw Error("Unsafe source manifest path.");
  const bytes = await readFile(path.join(build,file.path));
  if (bytes.length !== file.size || hash(bytes) !== file.sha256) throw Error("The matching-source snapshot has changed. Run a full build instead.");
}
for (const chunk of assets.chunks) {
  if (!/^assets-[a-f0-9]+-\d+\.bin$/.test(chunk.name)) throw Error("Unsafe engine chunk name.");
  const bytes = await readFile(path.join(build,"play/engine",chunk.name));
  if (bytes.length !== chunk.size || hash(bytes) !== chunk.sha256) throw Error("The public engine asset package has changed.");
}
for (const name of [assets.code,assets.wasm]) {
  if (!/^engine-[a-f0-9]+\.(js|wasm)$/.test(name)) throw Error("Unsafe engine filename.");
  const bytes = await readFile(path.join(build,"play/engine",name));
  if (!name.includes(hash(bytes).slice(0,16))) throw Error("The content-addressed engine has changed.");
}
const files = ["index.html","app.css","app.js","engine-client.js","adventure-store.js","adventure-ui.js","journal-ui.js","cloud-sdk.js","cloud-sdk.js.LEGAL.txt","cloud-config.js","cloud-client.js","cloud-ui.js","cloud.css","game-data.js","game-data-manifest.js"];
for (const name of files) {
  const original = await readFile(path.join(web,"dist",name));
  let publicBytes = original;
  if (name === "index.html") publicBytes = Buffer.from(original.toString().replace('<script src="app.js','<script src="public-loader.js"></script>\n    <script src="app.js'));
  if (name === "app.css") publicBytes = Buffer.from(original.toString().replace(/url\("assets\/[^"\n]+"\)/g,"none"));
  if (name === "cloud-config.js") publicBytes = Buffer.from(renderCloudConfig(audience));
  await writeFile(path.join(build,"play",name),publicBytes);
  const relative = `source/clients/web/dist/${name}`;
  await writeFile(path.join(build,relative),original);
  let entry = source.files.find(file => file.path === relative);
  if (!entry) {entry={path:relative};source.files.push(entry);}
  Object.assign(entry,{size:original.length,sha256:hash(original)});
}
// Refresh the authored homepage alongside the play shell. This remains a
// local package update; deployment is a separate, explicit step.
for(const name of ["index.html","styles.css"]) {
  const bytes=await readFile(path.join(site,"dist",name));
  await writeFile(path.join(build,name),bytes);
  const relative=`source/clients/site/dist/${name}`;
  await writeFile(path.join(build,relative),bytes);
  let entry=source.files.find(file=>file.path===relative);
  if(!entry){entry={path:relative};source.files.push(entry);}
  Object.assign(entry,{size:bytes.length,sha256:hash(bytes)});
}
for(const name of ["index.html","admin.css","admin.js"]) {
  const bytes=await readFile(path.join(site,"dist/admin",name));
  await mkdir(path.join(build,"admin"),{recursive:true});
  await writeFile(path.join(build,"admin",name),bytes);
  const relative=`source/clients/site/dist/admin/${name}`;
  await mkdir(path.dirname(path.join(build,relative)),{recursive:true});
  await writeFile(path.join(build,relative),bytes);
  let entry=source.files.find(file=>file.path===relative);
  if(!entry){entry={path:relative};source.files.push(entry);}
  Object.assign(entry,{size:bytes.length,sha256:hash(bytes)});
}
// Include the exact SDK build inputs alongside the refreshed authored shell.
for(const name of ["src/cloud-sdk-entry.js","scripts/build-cloud.mjs","package.json","package-lock.json"]) {
  const bytes=await readFile(path.join(web,name));
  const relative=`source/clients/web/${name}`;
  await mkdir(path.dirname(path.join(build,relative)),{recursive:true});
  await writeFile(path.join(build,relative),bytes);
  let entry=source.files.find(file=>file.path===relative);
  if(!entry){entry={path:relative};source.files.push(entry);}
  Object.assign(entry,{size:bytes.length,sha256:hash(bytes)});
}
const info=JSON.parse(await readFile(path.join(build,"build-info.json"),"utf8"));
info.cloudProject=cloudEnvironment(audience).project;
await writeFile(path.join(build,"build-info.json"),JSON.stringify(info));
await writeFile(path.join(build,"source-files.json"),JSON.stringify(source,null,2));
console.log("Public web shell refreshed. Compiled engine, engine assets and native source snapshot are unchanged.");
