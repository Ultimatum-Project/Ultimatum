// Isolated loopback QA uses real UI controls, real IndexedDB and the real WASM engine.
(async function () {
  const report=document.createElement("pre");report.id="runtimeReport";
  report.style.cssText="position:fixed;left:0;top:0;z-index:10000;max-height:140px;max-width:100vw;overflow:auto;background:#000;color:#9f9;font:10px monospace;pointer-events:none";
  document.body.append(report);
  const results=[];const started=Date.now();
  const write=(status,extra={})=>report.textContent=JSON.stringify({status,results,...extra},null,2);
  const pass=name=>{results.push({name,passed:true});write("running");};
  const assert=(condition,message)=>{if(!condition) throw Error(message);};
  const wait=async(predicate,message,timeout=30000)=>{
    const start=Date.now();while(Date.now()-start<timeout) {const value=await predicate();if(value) return value;await new Promise(resolve=>setTimeout(resolve,100));}
    throw Error(message);
  };
  const client=new window.UltimatumEngineClient(()=>window.Module),store=new window.UltimatumSaveStore();
  const button=(action,slot)=>document.querySelector(`[data-adventure-action="${action}"][data-slot="${slot}"]`);
  const state=()=>window.Module?.ccall && document.querySelector('#engineStatus').hidden ? client.snapshot() : null;
  const option=value=>document.querySelector(`[data-prompt-value="${CSS.escape(value)}"]`);
  const confirmationReady=()=>!document.querySelector('#adventureConfirm').hidden && !document.querySelector('#adventureConfirmButton').disabled;
  const openMenu=async()=>{
    const selector=matchMedia('(max-width:650px), (max-width:980px) and (max-height:500px)').matches ? '#mobileMenuButton' : '#menuButton';
    await wait(()=>!document.querySelector(selector).disabled,"Menu did not become available");document.querySelector(selector).click();
  };
  try {
    const phase=sessionStorage.getItem("onboarding-phase");
    if(phase) results.push(...JSON.parse(sessionStorage.getItem("onboarding-results") || "[]"));
    if(!phase) {
    await wait(()=>button("new",2) && !button("new",2).disabled,"Title did not become actionable");
    assert(document.querySelector('#adventureDialog').open,"Title not visible");
    assert((await store.get(1))?.current,"Existing adventure was not migrated");
    assert(document.querySelector('#turnCount').textContent==="—","Game started before choosing an adventure");
    pass("Title waits for a choice and preserves legacy Slot 1");
    const seed=await store.get(1);
    sessionStorage.setItem("onboarding-seed",seed.current.fingerprint);
    // Only test-owned slots on this isolated origin are reset.
    await store.remove(2);await store.remove(3);
    button("new",2).click();
    if(!document.querySelector('#adventureConfirm').hidden) document.querySelector('#adventureConfirmButton').click();
    await wait(()=>state()?.prompt?.title==="Name your Avatar" && option("\u001b") && !option("\u001b").disabled,"Name prompt did not open");
    option("\u001b").click();
    await wait(()=>document.querySelector('#adventureDialog').open,"Cancelled creation did not return to title");
    assert(!(await store.get(2)),"Cancelled creation published a save");
    assert((await store.get(1)).current.fingerprint===seed.current.fingerprint,"Cancel damaged another slot");
    pass("Cancel creation returns to title without changing saves");
    // The next explicit choice reloads the existing engine rather than re-entering callMain.
      sessionStorage.setItem("onboarding-phase","create");
      sessionStorage.setItem("onboarding-results",JSON.stringify(results));
      button("new",2).click();
      await wait(confirmationReady,"New Game confirmation did not open");
      document.querySelector('#adventureConfirmButton').click();return;
    }
    if(phase==="create") {
      await wait(()=>state()?.prompt?.title==="Name your Avatar" || (document.querySelector('#adventureDialog').open && button("new",2) && !button("new",2).disabled),"Creation or title did not become ready");
      if(document.querySelector('#adventureDialog').open) {
        button("new",2).click();
        await wait(()=>confirmationReady() || state()?.prompt?.title==="Name your Avatar","New Game did not respond");
        if(!document.querySelector('#adventureConfirm').hidden) document.querySelector('#adventureConfirmButton').click();
      }
      await wait(()=>state()?.prompt?.title==="Name your Avatar" && option("\u001b") && !option("\u001b").disabled && !document.querySelector('#conversationForm').hidden,"Name prompt did not open after reload");
      await store.remove(3);
      const input=document.querySelector('#conversationInput');input.value="WebHero";
      document.querySelector('#conversationForm').requestSubmit(document.querySelector('#conversationSubmit'));
      await wait(()=>option("m") && !option("m").disabled,"Character selection did not open");option("m").click();
      let stories=0,questions=0,previousId=0;
      await wait(()=>{
        const s=state();
        if(s?.ready && s.inputMode==="command") return true;
        const prompt=s?.prompt;
        if(!prompt?.id || prompt.id===previousId || prompt.submitted) return false;
        const value=prompt.options.some(o=>o.value==="a") ? "a" : "\r";
        const choice=option(value);if(!choice || choice.disabled || Number(choice.dataset.promptId)!==prompt.id) return false;
        if(prompt.context.startsWith("The story begins")) stories++;
        if(value==="a") questions++;
        previousId=prompt.id;choice.click();return false;
      },"Original story and virtue questions did not finish",120000);
      assert(stories===24 && questions===7,`Expected 24 story pages and 7 virtue questions, got ${stories}/${questions}`);
      assert(state().party[0].name==="WebHero" && state().moves===0,"New game restored the seed instead of creating an Avatar");
      await wait(async()=> (await store.get(2))?.current?.summary?.name==="WebHero","New character was not published to Slot 2");
      assert((await store.get(1)).current.fingerprint===sessionStorage.getItem("onboarding-seed"),"New Game changed Slot 1");
      pass("Original 24-page story and seven virtue questions create an independent new adventure");
      for(let i=0;i<10;i++) {
        const before=state().moves;document.querySelector('[data-key="32"]').click();
        await wait(()=>state().inputMode==="command" && state().moves===before+1,"Wait did not finish");
      }
      await wait(async()=> (await store.get(2))?.current?.summary?.moves>=10,"Automatic checkpoint was not durably published");
      assert((await store.get(2)).previous.summary.moves===0,"Autosave did not retain the previous checkpoint");
      pass("Ten safe turns publish an automatic checkpoint and retain recovery");
      const before=state().moves;document.querySelector('[data-key="32"]').click();
      await wait(()=>state().moves===before+1 && state().canSave,"Manual save turn did not settle");
      await openMenu();await wait(()=>option("save") && !option("save").disabled,"Save Game was not available");option("save").click();
      await wait(async()=> (await store.get(2)).current.summary.moves===before+1,"Manual Save did not reach IndexedDB");
      pass("Manual save publishes actual party progress");
      await openMenu();
      await wait(()=>option("adventures") && !option("adventures").disabled,"Adventures menu entry missing");option("adventures").click();
      await wait(()=>document.querySelector('#adventureDialog').open,"Save manager did not open");
      const pausedMoves=state().moves;
      await new Promise(resolve=>setTimeout(resolve,1300));
      assert(state().interactionActive && state().moves===pausedMoves && !client.tapWorld(0,1),"Save manager did not pause game ownership");
      pass("Save manager pauses the engine and rejects gameplay underneath");
      button("rename",2).click();
      await wait(confirmationReady,"Rename confirmation missing");
      document.querySelector('#adventureName').value="Web journey";document.querySelector('#adventureConfirmButton').click();
      await wait(async()=> (await store.get(2)).label==="Web journey","Rename was not durable");
      await wait(()=>button("export",2) && !button("export",2).disabled,"Rename did not finish updating controls");
      pass("Rename updates the chosen slot without changing its adventure");
      let exported;
      const observe=event=>{if(event.target.closest('a')?.download?.endsWith('.u4save')) exported=event.target.closest('a').href;};
      document.addEventListener("click",observe);button("export",2).click();
      await wait(()=>exported,"Export did not trigger a backup download");document.removeEventListener("click",observe);
      const text=await (await fetch(exported)).text(),decoded=window.UltimatumSaveStore.decode(text);
      assert(client.validateAdventure(decoded.files).moves===pausedMoves,"Export did not contain the selected checkpoint");
      const chooser=document.querySelector('#adventureImport');
      const upload=(content,name)=>{
        const transfer=new DataTransfer();transfer.items.add(new File([content],name,{type:"application/json"}));
        chooser.files=transfer.files;chooser.dispatchEvent(new Event("change"));
      };
      button("import",3).click();upload(text,"backup.u4save");
      await wait(async()=> (await store.get(3))?.current?.fingerprint===window.UltimatumSaveStore.fingerprint(decoded.files),"Backup import did not finish");
      assert((await store.get(3)).current.fingerprint===(await store.get(2)).current.fingerprint,"Backup round trip changed bytes");
      await wait(()=>button("import",3) && !button("import",3).disabled,"Import did not finish updating controls");
      pass("UI export/import round-trips the real save into another slot");
      // Exercise Save against the metadata loaded with this active adventure.
      // Replacing files behind a running engine would not update its in-memory
      // journal and exploration models, and is not a supported user flow.
      const metadata=decoded.files;
      document.querySelector('#adventureSaveNow').click();
      const metadataNames=["topics.txt","map-pins.dat","map-discoveries.dat","explored-dungeons.dat","explored-map.dat"];
      const sameBytes=(a,b)=>a && b && a.length===b.length && a.every((byte,index)=>byte===b[index]);
      await wait(async()=>{
        const current=(await store.get(2)).current;
        return current.summary.moves===pausedMoves && metadataNames.every(name=>sameBytes(current.files[name],metadata[name]));
      },"Engine Save dropped optional adventure metadata");
      const metadataFingerprint=(await store.get(2)).current.fingerprint;
      await wait(()=>state().inputMode==="command","Save did not resume the engine");
      pass("Real engine saves preserve journal, maps, pins and discoveries in the slot checkpoint");
      await openMenu();await wait(()=>option("adventures") && !option("adventures").disabled,"Menu did not reopen");option("adventures").click();
      await wait(()=>document.querySelector('#adventureDialog').open && button("export",2) && !button("export",2).disabled,"Metadata manager did not open");
      exported=null;document.addEventListener("click",observe);button("export",2).click();await wait(()=>exported,"Metadata export did not download");document.removeEventListener("click",observe);
      const metadataText=await (await fetch(exported)).text(),metadataDecoded=window.UltimatumSaveStore.decode(metadataText);
      assert(window.UltimatumSaveStore.fingerprint(metadataDecoded.files)===metadataFingerprint,"Metadata export changed bytes");
      button("import",3).click();
      const classicTransfer=new DataTransfer();
      for(const [name,bytes] of Object.entries(metadataDecoded.files)) classicTransfer.items.add(new File([bytes],name.toUpperCase()));
      chooser.files=classicTransfer.files;chooser.dispatchEvent(new Event("change"));
      await wait(confirmationReady,"Classic replacement did not require confirmation");document.querySelector('#adventureConfirmButton').click();
      await wait(async()=> (await store.get(3)).current.fingerprint===metadataFingerprint,"Classic save import dropped metadata");
      await wait(()=>button("import",3) && !button("import",3).disabled,"Classic import did not finish updating controls");
      pass("Classic multi-file import validates and preserves every adventure-owned metadata file");
      const protectedFingerprint=(await store.get(3)).current.fingerprint;
      const damaged=JSON.parse(metadataText);damaged.files[0].crc32="deadbeef";
      button("import",3).click();upload(JSON.stringify(damaged),"damaged.u4save");
      await wait(()=>document.querySelector('#adventureStatus').textContent.includes("damaged"),"Damaged backup was not rejected");
      assert((await store.get(3)).current.fingerprint===protectedFingerprint,"Failed import replaced the slot");
      pass("Damaged backup is rejected without replacing a good checkpoint");
      // Real IndexedDB concurrency guard; scoped to a test-owned inactive slot.
      const first=await store.get(3),other=new window.UltimatumSaveStore();
      let aborted=false;
      try {await store.transact("readwrite",(objectStore,done,transaction)=>{objectStore.put({...first,label:"Must not publish"});transaction.abort();});} catch {aborted=true;}
      assert(aborted && (await store.get(3)).label===first.label,"Aborted storage transaction damaged the slot");
      pass("An aborted IndexedDB transaction leaves the published checkpoint intact");
      const changed={...first.current.files,"topics.txt":new TextEncoder().encode("invalid fixture metadata")};
      await other.commit(3,changed,first.current.summary,undefined,first.current.fingerprint);
      let rejected=false;
      try {await store.commit(3,decoded.files,first.current.summary,undefined,first.current.fingerprint);} catch {rejected=true;}
      assert(rejected,"Stale tab overwrite was accepted");
      pass("A stale tab cannot overwrite a newer slot checkpoint");
      // Return/reopen refreshes slot health; malformed metadata must disable Continue.
      document.querySelector('#adventureResume').click();
      await wait(()=>state().inputMode==="command","Resume did not return to controls");
      await openMenu();await wait(()=>option("adventures") && !option("adventures").disabled,"Menu did not reopen");option("adventures").click();
      await wait(()=>document.querySelector('#adventureDialog').open && button("recover",3),"Recovery UI missing");
      assert(button("continue",3).disabled && !button("recover",3).disabled,"Invalid metadata checkpoint was offered for Continue");
      button("recover",3).click();await wait(confirmationReady,"Recovery confirmation missing");document.querySelector('#adventureConfirmButton').click();
      await wait(async()=> (await store.get(3)).current.fingerprint===first.current.fingerprint,"Recovery did not restore the previous checkpoint");
      await wait(()=>button("delete",3) && !button("delete",3).disabled,"Recovery did not finish updating controls");
      pass("Shared native validation rejects corrupt metadata and recovery restores a validated checkpoint");
      button("delete",3).click();await wait(confirmationReady,"Delete confirmation missing");document.querySelector('#adventureCancelButton').click();
      assert(await store.get(3),"Cancel delete removed the slot");
      button("delete",3).click();await wait(confirmationReady,"Delete confirmation missing");document.querySelector('#adventureConfirmButton').click();
      await wait(async()=> !(await store.get(3)),"Confirmed delete did not clear the slot");
      await wait(()=>!document.querySelector('#adventureTitleButton').disabled,"Delete did not finish updating controls");
      pass("Delete requires confirmation and affects only the selected inactive slot");
      const beforeFailure=await store.get(2),originalCommit=window.UltimatumSaveStore.prototype.commit;
      let injectFailure=true;
      window.UltimatumSaveStore.prototype.commit=function(...args) {
        if(args[0]===2 && injectFailure) {injectFailure=false;return Promise.reject(new DOMException("Injected storage failure","QuotaExceededError"));}
        return originalCommit.apply(this,args);
      };
      document.querySelector('#adventureSaveNow').click();
      await wait(()=>document.querySelector('#toast').textContent.includes("Save storage failed"),"Storage failure was reported as a successful save");
      window.UltimatumSaveStore.prototype.commit=originalCommit;
      assert((await store.get(2)).current.fingerprint===beforeFailure.current.fingerprint,"Failed storage replaced the published checkpoint");
      await openMenu();await wait(()=>option("adventures") && !option("adventures").disabled,"Recovery menu did not open");option("adventures").click();
      await wait(()=>document.querySelector('#adventureDialog').open && !document.querySelector('#adventureExportPending').hidden && !document.querySelector('#adventureExportPending').disabled,"Unstored checkpoint recovery action missing");
      exported=null;document.addEventListener("click",observe);document.querySelector('#adventureExportPending').click();await wait(()=>exported,"Emergency backup did not download");document.removeEventListener("click",observe);
      const emergency=window.UltimatumSaveStore.decode(await (await fetch(exported)).text());
      assert(client.validateAdventure(emergency.files).name==="WebHero","Emergency export was not a valid engine checkpoint");
      pass("Injected storage failure keeps the good slot and offers a valid emergency export before reload");
      sessionStorage.setItem("onboarding-phase","reload");sessionStorage.setItem("onboarding-results",JSON.stringify(results));
      sessionStorage.setItem("onboarding-moves",String(pausedMoves));
      document.querySelector('#adventureTitleButton').click();await wait(confirmationReady,"Title confirmation missing");document.querySelector('#adventureConfirmButton').click();return;
    }
    if(phase==="reload") {
      await wait(()=>button("continue",2) && !button("continue",2).disabled,"Saved slot did not appear after reload");
      assert(document.querySelector('#adventureDialog').open,"Reload skipped the title screen");
      assert((await store.get(2)).label==="Web journey","Rename did not survive reload");
      button("continue",2).click();
      await wait(()=>state()?.ready && state().inputMode==="command","Continue did not restore the engine");
      assert(state().party[0].name==="WebHero" && state().moves===Number(sessionStorage.getItem("onboarding-moves")),"Continue restored the wrong adventure or moves");
      assert(window.UltimatumSaveStore.fingerprint(client.readAdventureFiles())===(await store.get(2)).current.fingerprint,"Continue dropped saved metadata");
      assert((await store.get(1)).current.fingerprint===sessionStorage.getItem("onboarding-seed"),"Full lifecycle changed Slot 1");
      pass("Reload returns to title; Continue restores the selected Avatar, moves and renamed slot");
      sessionStorage.removeItem("onboarding-phase");sessionStorage.removeItem("onboarding-results");
      write("passed",{elapsed:Date.now()-started});
    }
  } catch(error) {
    let diagnostic;
    try {diagnostic={state:state(),slot:await store.get(2),paths:client.module.FS.readdir('/home/web_user/.xu4')};} catch {}
    if(diagnostic?.slot) {diagnostic.slot.current.files=Object.keys(diagnostic.slot.current.files);if(diagnostic.slot.previous) diagnostic.slot.previous.files=Object.keys(diagnostic.slot.previous.files);}
    write("failed",{error:error.message,diagnostic,elapsed:Date.now()-started});return;
  }
})();
