const test = require("node:test");
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const crypto = require("node:crypto");
const vm = require("node:vm");
const game = require("../dist/game-data.js");

function fixture() {
  const names = new WeakMap();
  const entries = Object.entries(game.manifest.files).map(([name,rule]) => {
    const bytes = new Uint8Array(rule.size);
    if (name.endsWith(".TLK")) for(let start=0;start<bytes.length;start+=288) bytes[start+5]=65;
    names.set(bytes.buffer,name);
    return {name:`wrapper/u4/${name.toLowerCase()}`, data:bytes.buffer};
  });
  return {entries, hash:async bytes=>game.manifest.files[names.get(bytes.buffer)].sha256};
}

test("the compatibility profile follows every configured DOS map, dialogue and EGA image", () => {
  const conf = path.resolve(__dirname,"../../../vendor/ultima4-ios/conf");
  const maps = fs.readFileSync(path.join(conf,"maps.xml"),"utf8");
  const images = fs.readFileSync(path.join(conf,"graphics.xml"),"utf8").match(/<imageset name="EGA">([\s\S]*?)<\/imageset>/)[1];
  const required = new Set(["AVATAR.EXE","TITLE.EXE",...Array.from(maps.matchAll(/(?:fname|tlk_fname)="([^"]+)"/g),m=>m[1].toUpperCase()),...Array.from(images.matchAll(/filename="([^/\"]+)"/g),m=>m[1].toUpperCase())]);
  assert.deepEqual(Object.keys(game.manifest.files).sort(),[...required].sort());
  assert.equal(required.size,103);
});

test("verified packages normalize names and exclude unrelated files and embedded saves", async () => {
  const {entries,hash} = fixture();
  const report = await game.validate([...entries,{name:"PARTY.SAV",data:new ArrayBuffer(502)},{name:"private-notes.txt",data:new ArrayBuffer(10)}],hash);
  assert.equal(report.profile,"u4-dos-english-ega-v1");
  assert.equal(report.files.length,103);
  assert.equal(report.verification.ignored,2);
  assert.ok(report.files.every(file=>file.name===file.name.toUpperCase() && /^[a-f0-9]{64}$/.test(file.sha256)));
});

test("installation selection preserves folders without mixing or guessing", async()=>{
  const {entries,hash}=fixture();
  const base=entries.map(entry=>({...entry,name:entry.name.replace("wrapper/u4/","ultima4/")}));
  const upgrade=base.filter(entry=>!entry.name.endsWith("avatar.exe")).map(entry=>({...entry,name:entry.name.replace("ultima4/","ultima4/upgrade/")}));
  const report=await game.validate([...base,...upgrade],hash);
  assert.equal(report.files.length,103);
  assert.equal(report.verification.directory,"ULTIMA4");
  assert.equal(report.verification.otherDirectories,1);
  await assert.rejects(game.validate([...base,...base.map(entry=>({...entry,name:entry.name.replace("ultima4/","backup/")}))],hash),/Multiple complete/);
  await assert.rejects(game.validate(base.map((entry,i)=>({...entry,name:entry.name.replace("ultima4/",i%2 ? "part-a/" : "part-b/")})),hash),/Missing/);
  assert.throws(()=>game.select([...base,{...base[0],name:"ULTIMA4/./ABACUS.EGA"}]),/More than one/);
});

test("1.01 is a complete reviewed profile, not an arbitrary per-file allowlist",async()=>{
  const {entries}=fixture();
  const revised=game.manifest.profiles[1];
  assert.equal(revised.id,"u4-dos-english-ega-1.01-v1");
  assert.equal(Object.keys(revised.files).length,103);
  assert.equal(Object.keys(revised.files).filter(name=>revised.files[name].sha256!==game.manifest.files[name].sha256).length,6);
  let index=0;
  const report=await game.validate(entries,async()=>Object.values(revised.files)[index++].sha256);
  assert.equal(report.profile,revised.id);
  index=0;
  await assert.rejects(game.validate(entries,async()=>{
    const name=Object.keys(game.manifest.files)[index++];
    return (name==="COVE.TLK" ? revised.files : game.manifest.files)[name].sha256;
  }),/mixed, modified/);
});

test("imports reject incomplete sets, wrong versions and truncated data", async () => {
  const {entries,hash} = fixture();
  await assert.rejects(game.validate(entries.slice(1),hash),/Missing/);
  await assert.rejects(game.validate(entries,async()=>"0".repeat(64)),/not a supported version/);
  const changed = entries.map(entry=>({...entry})); changed[0].data=new ArrayBuffer(5);
  await assert.rejects(game.validate(changed,hash),/unsupported size/);
});

test("unsafe paths, duplicate basenames and oversized selections fail before reading", () => {
  assert.throws(()=>game.select([{name:"../AVATAR.EXE",size:100}]),/unsafe/);
  assert.throws(()=>game.select([{name:"C:\\u4\\AVATAR.EXE",size:100}]),/unsafe/);
  assert.throws(()=>game.select([{name:"/AVATAR.EXE",size:100}]),/unsafe/);
  assert.throws(()=>game.select([{name:"a/AVATAR.EXE",size:100},{name:"b/avatar.exe",size:100}]),/More than one/);
  assert.throws(()=>game.select([{name:"AVATAR.EXE",size:game.MAX_FILE_BYTES+1}]),/too large/);
  assert.throws(()=>game.select(Array(2049).fill({name:"readme.txt",size:1})),/2,048/);
  assert.deepEqual(game.select([{name:"huge-unrelated-video.mp4",size:2**40}]),[]);
});

test("dialogue bounds and city coordinates are checked independently of hash recognition", async () => {
  let {entries,hash} = fixture();
  const talk=entries.find(entry=>entry.name.endsWith("britain.tlk")); new Uint8Array(talk.data).fill(65);
  await assert.rejects(game.validate(entries,hash),/damaged conversation/);
  ({entries,hash}=fixture());
  const city=entries.find(entry=>entry.name.endsWith("britain.ult")); new Uint8Array(city.data)[1056]=255;
  await assert.rejects(game.validate(entries,hash),/invalid person/);
});

test("LAN SHA-256 fallback matches independent hashes including padding boundaries", async () => {
  const sandbox={window:{UltimatumGameDataManifest:game.manifest},Uint8Array,Uint32Array,DataView,Set,Map};
  vm.runInNewContext(fs.readFileSync(path.join(__dirname,"../dist/game-data.js"),"utf8"),sandbox);
  for(const length of [0,3,55,56,63,64,65,1000,65536]) {
    const bytes=crypto.randomBytes(length);
    assert.equal(await sandbox.window.UltimatumGameData.digest(bytes),crypto.createHash("sha256").update(bytes).digest("hex"));
  }
});

test("the actual development data matches the reviewed profile when installed", {skip:!fs.existsSync(path.join(__dirname,"../.cache/game-data/AVATAR.EXE"))}, async () => {
  const entries=Object.keys(game.manifest.files).map(name=>({name,data:fs.readFileSync(path.join(__dirname,"../.cache/game-data",name))}));
  const report=await game.validate(entries);
  assert.equal(report.files.length,103);
  const fixes={"COVE.TLK":[[4032,6]],"LCB.TLK":[[2592,6]],"MINOC.TLK":[[1728,6]],"SERPENT.ULT":[[1276,2],[1277,2]],"SKARA.TLK":[[0,6]],"YEW.TLK":[[2539,78]]};
  for(const entry of entries) for(const [offset,value] of fixes[entry.name] || []) entry.data[offset]=value;
  const revised=await game.validate(entries);
  assert.equal(revised.profile,"u4-dos-english-ega-1.01-v1");
  assert.ok(revised.files.every(file=>file.sha256===crypto.createHash("sha256").update(new Uint8Array(file.data)).digest("hex")));
});

test("VGA imports reject arbitrary ZIPs before replacing the overlay", async()=>{
  await assert.rejects(game.validateVga(new ArrayBuffer(20)),/not the verified/);
  await assert.rejects(game.validateVga(new ArrayBuffer(game.VGA_ARCHIVE.size)),/not the verified/);
  const filename=path.join(__dirname,"../.cache/u4upgrad.zip");
  if(fs.existsSync(filename)) await game.validateVga(fs.readFileSync(filename));
});

test("private cloud packages contain only the reviewed game files and reject tampering", async()=>{
  const {entries,hash}=fixture();
  const verified=await game.validate(entries,hash),text=game.encodePackage(verified);
  const value=JSON.parse(text);
  assert.equal(value.format,"ultimatum-game-data");
  assert.equal(value.files.length,103);
  assert.deepEqual(value.files.map(file=>file.name),Object.keys(game.manifest.files));
  assert.ok(value.files.every(file=>typeof file.data==="string"&&file.size>0));
  value.files[0].data=value.files[0].data.slice(0,-4)+"AAAA";
  await assert.rejects(game.decodePackage(JSON.stringify(value)),/integrity check|unsupported size/);
});
