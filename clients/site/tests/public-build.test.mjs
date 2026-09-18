import test from "node:test";
import assert from "node:assert/strict";
import {readFile,stat} from "node:fs/promises";
import {createHash} from "node:crypto";
import {packageMetadata,inspectPublicAssets} from "../scripts/build-public.mjs";
import {checkCloudConfig,renderCloudConfig} from "../scripts/cloud-environments.mjs";

test("web packages bind production and QA to separate Supabase projects",()=>{
  assert.equal(checkCloudConfig(renderCloudConfig("public"),"public").project,"ultimatum-prod");
  assert.equal(checkCloudConfig(renderCloudConfig("test"),"test").project,"ultimatum-test");
  assert.throws(()=>checkCloudConfig(renderCloudConfig("test"),"public"),/wrong Supabase/);
});

const buildDirectory=process.env.ULTIMATUM_SITE_BUILD || "build";
const buildRoot=new URL(`../${buildDirectory}/`,import.meta.url);

test("public preload inspection permits only the xu4 soundtrack and refuses game data and saves",()=>{
  const script=files=>`loadPackage({files:${JSON.stringify(files)},remote_package_size:1})`;
  assert.equal(packageMetadata(script([{filename:"/conf/config.xml"}])).remote_package_size,1);
  for(const filename of ["/ultima4/AVATAR.EXE","/home/web_user/.xu4/party.sav","/music/unknown/world.ogg","/u4upgrad.exe"]) assert.throws(()=>packageMetadata(script([{filename}])),/Forbidden public preload/);
  assert.equal(packageMetadata(script([{filename:"/u4upgrad.zip"}])).files.length,1);
  assert.throws(()=>packageMetadata("unexpected loader"),/refusing/);
  assert.throws(()=>packageMetadata(script([{filename:"/music/timemachine/world.ogg"}])),/Forbidden public preload/);
  assert.equal(packageMetadata(script([{filename:"/music/hurin/wanderer.ogg"}])).files.length,1);
});

test("only the exact graphics-only VGA payload is publishable",async()=>{
  const data=await readFile(new URL("../../web/.cache/assets/u4upgrad-graphics.zip",import.meta.url));
  const metadata={files:[{filename:"/u4upgrad.zip",start:0,end:data.length}],remote_package_size:data.length};
  inspectPublicAssets(metadata,data);
  const bad=Buffer.from(data);bad[0]^=1;
  assert.throws(()=>inspectPublicAssets(metadata,bad),/Unverified/);
  assert.throws(()=>inspectPublicAssets({...metadata,files:[]},data),/exactly one/);
  assert.throws(()=>inspectPublicAssets({...metadata,files:[...metadata.files,...metadata.files]},data),/exactly one/);
  assert.throws(()=>inspectPublicAssets({...metadata,files:[{...metadata.files[0],start:-1}]},data),/Unverified/);
});

test("the prepared public engine chunks reconstruct its exact asset package", async()=>{
  const root=buildRoot;
  const manifest=JSON.parse(await readFile(new URL("play/engine/assets.json",root)));
  const info=JSON.parse(await readFile(new URL("build-info.json",root)));
  assert.equal(info.qaDebug,true,"Debug Tools must be enabled for early-access releases");
  assert.equal(info.cloudProject,buildDirectory==="build-test"?"ultimatum-test":"ultimatum-prod");
  let total=0;
  const buffers=[];
  for(const chunk of manifest.chunks) {
    const bytes=await readFile(new URL(`play/engine/${chunk.name}`,root));
    assert.equal(bytes.length,chunk.size);
    assert.ok(bytes.length<=20*1024*1024);
    assert.equal(createHash("sha256").update(bytes).digest("hex"),chunk.sha256);
    total+=bytes.length;
    buffers.push(bytes);
  }
  assert.equal(total,manifest.size);
  const metadata=packageMetadata(await readFile(new URL(`play/engine/${manifest.code}`,root),"utf8"));
  assert.equal(metadata.remote_package_size,total);
  inspectPublicAssets(metadata,Buffer.concat(buffers));
  assert.equal(info.soundtrack,"xu4");
  assert.equal(metadata.files.filter(file=>/\/music\/hurin\/.*\.ogg$/i.test(file.filename)).length,9);
  assert.ok((await stat(new URL(`play/engine/${manifest.wasm}`,root))).size>0);
});

test("public routes include the homepage, real client and matching source, not test hooks or game sprites",async()=>{
  const root=buildRoot;
  const home=await readFile(new URL("index.html",root),"utf8");
  assert.match(home,/Ultimatum Project/); assert.match(home,/href="play\/"/);
  assert.match(home,/FIRST SUPPORTED GAME/);assert.match(home,/No account required/);
  assert.match(home,/Source &amp; credits/);assert.doesNotMatch(home,/Sign in or create account/);
  assert.doesNotMatch(home,/not available yet|local saves only/i);
  const play=await readFile(new URL("play/index.html",root),"utf8");
  checkCloudConfig(await readFile(new URL("play/cloud-config.js",root),"utf8"),buildDirectory==="build-test"?"test":"public");
  assert.match(play,/public-loader.js/); assert.doesNotMatch(play,/runtime-driver|runtime-bootstrap/);
  assert.doesNotMatch(play,/id="vgaZipInput"|Choose U4UPGRAD|Download from source/);
  assert.match(play,/NO PATCH UPLOAD NEEDED/);
  assert.match(play,/id="journalDialog"/);assert.match(play,/journal-ui.js/);
  assert.match(play,/id="feedbackButton"/);assert.match(play,/id="feedbackDialog"/);
  assert.equal((play.match(/data-open-feedback/g)||[]).length,2);
  assert.match(await readFile(new URL("play/journal-ui.js",root),"utf8"),/UltimatumJournalUI/);
  const css=await readFile(new URL("play/app.css",root),"utf8"); assert.doesNotMatch(css,/url\("assets\//);
  const source=JSON.parse(await readFile(new URL("source-files.json",root)));
  assert.match(await readFile(new URL("source/.env.example",root),"utf8"),/ULTIMATUM_SUPABASE_PUBLIC_URL=/);
  assert.ok(source.files.some(file=>file.path.endsWith("src/u4.cpp")));
  assert.ok(source.files.some(file=>file.path.endsWith("toolchain/web_data_archive.cpp")));
  assert.ok(source.files.some(file=>file.path.endsWith("toolchain/web_journal.cpp")));
  assert.ok(source.files.some(file=>file.path.endsWith("src/journal_notebook.cpp")));
  assert.ok(source.files.every(file=>!/(\.sav$|u4upgrad\.zip$|AVATAR\.EXE$|TITLE\.EXE$)/i.test(file.path)));
  assert.ok(source.files.every(file=>!file.path.includes("/music/")),"Bundled music payloads stay outside the downloadable engine source snapshot");
  assert.ok(source.files.some(file=>file.path.endsWith("ios/prepare-vga-upgrade.sh")));
  assert.match(await readFile(new URL("licenses/ultima-iv-vga.txt",root),"utf8"),/Wiltshire/);
  const admin=await readFile(new URL("admin/index.html",root),"utf8");
  assert.match(admin,/Project dashboard/);assert.match(admin,/noindex,nofollow,noarchive/);
  const adminScript=await readFile(new URL("admin/admin.js",root),"utf8");
  assert.match(adminScript,/admin_dashboard/);assert.match(adminScript,/admin_feedback_reports/);
});
