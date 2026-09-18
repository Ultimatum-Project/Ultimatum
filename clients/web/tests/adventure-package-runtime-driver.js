// Disposable loopback origin: actual engine + Journal UI + native package.
(async function () {
  const report=document.createElement("pre");report.id="adventurePackageReport";
  report.style.cssText="position:fixed;top:0;left:0;z-index:10000;background:black;color:#9f9;font:12px monospace;pointer-events:none";
  document.body.append(report);
  const results=[];
  const show=(status,error)=>report.textContent=JSON.stringify({status,error,results});
  const assert=(ok,text)=>{if(!ok)throw Error(text);};
  const pass=text=>{results.push(text);show("RUNNING");};
  const wait=async(fn,text)=>{const end=Date.now()+30000;while(Date.now()<end){if(await fn())return;await new Promise(r=>setTimeout(r,80));}throw Error("Timed out: "+text);};
  const click=selector=>{const control=document.querySelector(selector);assert(control&&!control.disabled,"Control unavailable: "+selector);control.click();};
  show("RUNNING");
  try {
    await wait(()=>adventures.ready,"real engine at title");
    const native=await (await fetch("/native.u4save")).text();
    const transfer=new DataTransfer();transfer.items.add(new File([native],"native.u4save",{type:"application/json"}));
    adventures.importSlot=2;adventures.ui.adventureImport.files=transfer.files;
    adventures.ui.adventureImport.dispatchEvent(new Event("change"));
    await wait(async()=>!adventures.busy&&await adventures.store.get(2),"native import through web file handler");
    pass("Actual web engine validates and imports native .u4save into independent slot");
    click('[data-adventure-action="continue"][data-slot="2"]');
    await wait(()=>latestState?.ready&&engine.snapshot().inputMode==="command"&&adventures.activeSlot===2,"Continue imported adventure");
    assert(engine.snapshot().moves===9471,"Imported moves differ");
    click('[data-open-journal]:not(:disabled)');
    click('[data-journal-mode="clues"]');click('[data-journal-source]');
    const u=journalUI.ui;
    assert(document.querySelector('[data-journal-bookmark][aria-pressed="true"]'),"Native bookmark not visible");
    assert(u.journalContent.textContent.includes('Attached "clue"\n日本語 café'),"Native attached Unicode note not visible");
    click('[data-journal-mode="notes"]');
    assert(u.journalContent.textContent.includes("My personal theory"),"Native standalone note not visible");
    pass("Real web Continue and Journal UI restore native bookmark and Unicode/standalone notes");
    click('#journalNewNote');u.journalNoteText.value="Web round trip ✓";u.journalEditor.requestSubmit(u.journalDone);
    await wait(()=>!journalUI.busy,"web note mutation");
    assert(u.journalContent.textContent.includes("Web round trip ✓"),"Web note did not persist");
    click('#journalClose');
    ui.saveButton.click();await wait(()=>!ui.saveButton.disabled&&!adventures.pendingCheckpoint,"native engine checkpoint");await adventures.saveQueue;
    let exported;
    adventures.download=text=>{exported=text;};
    await adventures.action("export",2);
    assert(typeof exported==="string","Web export did not produce backup");
    const text=exported;
    const decoded=UltimatumSaveStore.decode(text);
    assert(new TextDecoder().decode(decoded.files["journal-notebook.dat"]).includes("Web round trip ✓"),"Export lost web note");
    assert(decoded.files["conversations.json"],"Export lost conversation log");
    assert((await fetch("/browser.u4save",{method:"POST",body:text})).ok,"Local test artifact was not recorded");
    pass("Web note edit, actual game save, and portable export preserve native metadata for return to iOS");
    show("PASS");
  } catch(error){console.error(error);show("FAIL",error.message);}
})();
