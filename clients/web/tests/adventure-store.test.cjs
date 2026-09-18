const assert=require("node:assert/strict");
const fs=require("node:fs");
const vm=require("node:vm");
const path=require("node:path");
const test=require("node:test");
const sandbox={Uint8Array,btoa,atob};
vm.runInNewContext(fs.readFileSync(path.join(__dirname,"../dist/adventure-store.js"),"utf8"),sandbox);
const Store=sandbox.UltimatumAdventureStore;
const files={"party.sav":new Uint8Array([1,2,3]),"monsters.sav":new Uint8Array([4,5]),"topics.txt":new Uint8Array([65,10]),"journal-notebook.dat":new TextEncoder().encode('U4NOTEBOOK 1 2\nN 1 "" "A personal note"\n'),"conversations.json":new Uint8Array([91,93])};
test("Adventure packages preserve classic saves and optional metadata byte-for-byte",()=>{
  const text=Store.encode({files,savedAt:123},"Test journey");
  const decoded=Store.decode(text);
  assert.equal(decoded.label,"Test journey");
  for(const name of Object.keys(files)) assert.deepEqual(decoded.files[name],files[name]);
  assert.equal(Store.fingerprint(decoded.files),Store.fingerprint(files));
  const packageData=JSON.parse(text);
  assert.equal(packageData.version,1);assert.equal(packageData.game,"ultima4");
});
test("Packages reject corruption, unknown paths, duplicates, missing files and unknown versions",()=>{
  const original=JSON.parse(Store.encode({files,savedAt:123},"Test"));
  for(const mutate of [
    p=>p.files[0].crc32="00000000",
    p=>p.files[0].size++,
    p=>p.files[0].name="../party.sav",
    p=>p.files.push(p.files[0]),
    p=>p.files=p.files.filter(f=>f.name!=="monsters.sav"),
    p=>p.version=2,
    p=>p.game="ultima5",
    p=>p.engine="unknown",
    p=>p.files[0].data="not base64!",
  ]) {const copy=structuredClone(original);mutate(copy);assert.throws(()=>Store.decode(JSON.stringify(copy)));}
});
test("CRC32 uses the standard portable checksum and enforces payload limits",()=>{
  const encoded=JSON.parse(Store.encode({files:{...files,"topics.txt":new TextEncoder().encode("123456789")}},"Test"));
  assert.equal(encoded.files.find(f=>f.name==="topics.txt").crc32,"cbf43926");
  assert.throws(()=>Store.fingerprint({...files,"topics.txt":new Uint8Array(8*1024*1024)}));
  assert.throws(()=>Store.decode("x".repeat(16*1024*1024+1)));
  assert.throws(()=>new Store().checkSlot(0));assert.throws(()=>new Store().checkSlot(4));
});
test("Quarantined legacy fragments can be exported but cannot be imported as playable saves",()=>{
  const checkpoint={files:{"party.sav":files["party.sav"]},damaged:true};
  const backup=Store.encode(checkpoint,"Needs recovery");
  assert.equal(JSON.parse(backup).files[0].name,"party.sav");
  assert.throws(()=>Store.decode(backup));
  assert.throws(()=>Store.encode({...checkpoint,damaged:false},"Incomplete"));
  const emptyBackup=Store.encode({files:{"party.sav":new Uint8Array()},damaged:true},"Interrupted legacy write");
  assert.equal(JSON.parse(emptyBackup).files[0].size,0);
  assert.throws(()=>Store.decode(emptyBackup));
});

// Simulate one IndexedDB transaction; abort must never publish its staged record.
function journalStore(record) {
  const store=new Store();let durable=structuredClone(record);
  store.transact=(mode,operation)=>new Promise((resolve,reject)=>{
    let staged,result;
    const objectStore={get(){const request={result:structuredClone(durable)};queueMicrotask(()=>request.onsuccess());return request;},put(value){staged=structuredClone(value);}};
    operation(objectStore,value=>{result=value;durable=staged;resolve(result);},{abort(){}},reject);
  });
  return {store,current:()=>durable};
}
test("Journal metadata updates preserve gameplay and previous recovery generations",async()=>{
  const record={slot:1,label:"Hero",current:{files,fingerprint:Store.fingerprint(files),summary:{moves:123},savedAt:456},previous:{fingerprint:"recovery",files:{"party.sav":new Uint8Array([9])}}};
  const {store,current}=journalStore(record);
  const metadata={"journal-notebook.dat":new TextEncoder().encode('U4NOTEBOOK 1 2\nN 1 "" "Changed note"\n')};
  await store.updateJournal(1,metadata,record.current.fingerprint);
  assert.deepEqual(current().previous,record.previous);
  assert.equal(current().current.savedAt,456);assert.deepEqual(current().current.summary,record.current.summary);
  assert.deepEqual(current().current.files["party.sav"],files["party.sav"]);
  assert.notEqual(current().current.fingerprint,record.current.fingerprint);
});
test("Journal updates reject stale slots, gameplay writes, and invalid byte payloads atomically",async()=>{
  const record={slot:1,current:{files,fingerprint:Store.fingerprint(files)},previous:null};
  const {store,current}=journalStore(record);
  await assert.rejects(store.updateJournal(1,{"topics.txt":new Uint8Array([1])},"stale"),/another tab/);
  await assert.rejects(store.updateJournal(1,{"party.sav":new Uint8Array([1])},record.current.fingerprint),/Only journal/);
  await assert.rejects(store.updateJournal(1,{"topics.txt":"invalid"},record.current.fingerprint),/Invalid adventure/);
  assert.deepEqual(current(),record);
});
