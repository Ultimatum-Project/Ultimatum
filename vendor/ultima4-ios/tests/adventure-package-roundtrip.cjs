// Execute the actual native codec/slot publisher and browser package adapter.
const fs=require("node:fs"),path=require("node:path"),os=require("node:os"),vm=require("node:vm"),assert=require("node:assert/strict");
const {execFileSync}=require("node:child_process");
const sandbox={Uint8Array,btoa,atob};
vm.runInNewContext(fs.readFileSync(path.resolve(__dirname,"../../../clients/web/dist/adventure-store.js"),"utf8"),sandbox);
const Store=sandbox.UltimatumAdventureStore;
const directory=fs.mkdtempSync(path.join(os.tmpdir(),"ultimatum-package-roundtrip-"));
try {
  for(const kind of ["fixture","world-fixture"]){
    const original=path.join(directory,kind+"-ios.u4save"),web=path.join(directory,kind+"-web.u4save"),returned=path.join(directory,kind+"-returned.u4save");
    execFileSync(process.argv[2],["--"+kind,original]);
    const imported=Store.decode(fs.readFileSync(original,"utf8"));
    assert.ok(imported.files["topics.txt"] && imported.files["journal-notebook.dat"] && imported.files["explored-map.dat"] && imported.files["map-pins.dat"] && imported.files["map-discoveries.dat"] && imported.files["explored-dungeons.dat"] && imported.files["conversations.json"]);
    const note=new TextDecoder().decode(imported.files["journal-notebook.dat"]).replace("My personal theory","Web-side personal note: café 日本語");
    imported.files["journal-notebook.dat"]=new TextEncoder().encode(note);
    imported.files["map-pins.dat"]=new TextEncoder().encode(new TextDecoder().decode(imported.files["map-pins.dat"]).replace("My pin","Web pin"));
    fs.writeFileSync(web,Store.encode({files:imported.files,savedAt:123},"Web returned hero"));
    execFileSync(process.argv[2],["--roundtrip",web,returned]);
    const restored=Store.decode(fs.readFileSync(returned,"utf8"));
    for(const [name,bytes] of Object.entries(imported.files))assert.deepEqual(restored.files[name],bytes,name+" must round-trip byte-for-byte");
    assert.equal(Object.keys(restored.files).length,Object.keys(imported.files).length);
    console.log("Native → web edits → native slot installation → web decode:",kind,"passed");
  }
} finally {fs.rmSync(directory,{recursive:true});}
