(async function() {
  const results=[];
  const panel=document.createElement("section"); panel.id="dataRuntimeResults";
  panel.style.cssText="position:fixed;z-index:10000;inset:0;overflow:auto;background:#101916;color:#f2eee4;padding:2rem;display:none";
  document.body.append(panel);
  const assert=(ok,label)=>{results.push({ok:Boolean(ok),label});if(!ok)throw Error(label);};
  const wait=async(predicate,label)=>{const deadline=Date.now()+30000;while(Date.now()<deadline){if(predicate())return;await new Promise(r=>setTimeout(r,60));}throw Error(`Timed out: ${label}`);};
  const submit=async(kind)=>{
    const before=ui.importStatus.textContent;
    ui.gameUrlInput.value=`${location.origin}/runtime/fixtures/${kind}.zip`;
    ui.gameUrlForm.requestSubmit();
    await wait(()=>ui.importStatus.dataset.state==="error" || (ui.importStatus.dataset.state==="success" && ui.importStatus.textContent!==before),kind);
  };
  try {
    const state=()=>engine.snapshot();
    const option=value=>document.querySelector(`[data-prompt-value="${CSS.escape(value)}"]`);
    const info=await (await fetch("/build-info.json")).json();
    const tap=async value=>{
      await wait(()=>option(value) && !option(value).disabled && Number(option(value).dataset.promptId)===state().prompt.id && !state().prompt.submitted,`current ${value} button`);
      const before=state().prompt.id; option(value).click();
      await wait(()=>state().inputMode==="command" || state().prompt?.id!==before,`reply ${value}`);
    };
    if (sessionStorage.getItem("public-data-phase")==="reload") {
      results.push(...JSON.parse(sessionStorage.getItem("public-data-results")));
      await wait(()=>ui.adventureDialog.open && document.querySelector('[data-adventure-action="continue"][data-slot="1"]:not(:disabled)'),"saved adventure title after reload");
      assert(engine.hasGameData(),"Verified game library restores on reload");
      const expectedProfile=sessionStorage.getItem("public-data-expected-profile");
      if(expectedProfile) assert((await libraryGet("game")).profile===expectedProfile,"The audited 1.01 library restores under its verified profile");
      document.querySelector('[data-adventure-action="continue"][data-slot="1"]').click();
      await wait(()=>state()?.ready && state().inputMode==="command","public Continue restores gameplay");
      assert(state().party[0].name==="PublicHero" && state().moves===2,"Public Continue restores the saved Avatar and turns");
      window.dataRuntimeResults={ok:true,results};
    } else {
    await wait(()=>runtimeReady && ui.dataDialog.open,"public engine needs game data");
    assert(!engine.hasGameData(),"Public engine starts without original game data");
    assert(!engine.hasSave(),"Public engine does not preload a development party");
    assert(window.UltimatumPublicLoader.script.startsWith("engine/engine-"),"Content-addressed engine loads with chunked assets");
    for(const kind of ["missing","wrong-version","unsafe","duplicate","corrupt","too-large","too-many","ambiguous","split"]) {
      await submit(kind);
      assert(ui.importStatus.dataset.state==="error",`${kind} archive rejected through the actual URL importer`);
      assert(!(await libraryGet("game")) && !engine.hasGameData(),`${kind} archive does not publish a library or active game files`);
    }
    await submit("valid");
    await wait(()=>ui.adventureDialog.open && document.querySelector("[data-adventure-action='new']"),"verified data reaches title");
    const stored=await libraryGet("game");
    assert(stored.files.length===103 && stored.profile==="u4-dos-english-ega-v1","Verified ZIP publishes exactly the 103 supported files");
    assert(!stored.files.some(file=>file.name.endsWith(".SAV") || file.name==="README.TXT"),"Embedded saves and unrelated files are excluded");
    assert(engine.hasGameData() && !engine.hasSave(),"Accepted data activates without importing embedded saves");
    assert(window.Module.FS.readFile("/u4upgrad.zip").length>0 && !(await libraryGet("vga")),"Verified graphics-only VGA overlay is already included without a separate upload");
    const map=window.Module.FS.readFile("/ultima4/WORLD.MAP").slice();
    openDataLibrary(); await submit("wrong-version");
    assert((await libraryGet("game")).files[0].sha256===stored.files[0].sha256,"Rejected replacement preserves the durable library");
    assert(window.Module.FS.readFile("/ultima4/WORLD.MAP").every((b,i)=>b===map[i]),"Rejected replacement preserves active game bytes");
    const persist=libraryPut;
    libraryPut=async()=>{throw new DOMException("Injected quota boundary failure","QuotaExceededError");};
    try {await submit("valid");} finally {libraryPut=persist;}
    assert(ui.importStatus.dataset.state==="error" && window.Module.FS.readFile("/ultima4/WORLD.MAP").every((b,i)=>b===map[i]),"Storage failure rolls active files back and reports no success");
    const write=window.Module.FS.writeFile;
    window.Module.FS.writeFile=function(path,...args) {
      if(path==="/game-data-install/AVATAR.EXE") throw Error("Injected staging write failure");
      return write.call(this,path,...args);
    };
    try {await submit("valid");} finally {window.Module.FS.writeFile=write;}
    assert(ui.importStatus.dataset.state==="error" && window.Module.FS.readFile("/ultima4/WORLD.MAP").every((b,i)=>b===map[i]),"Staging write failure leaves active game data untouched");
    ui.dataDialog.close();
    assert(ui.adventureDialog.open,"The adventure title remains available after a failed replacement");
    if(new URLSearchParams(location.search).get("audit")==="1") {
      const buffer=await (await fetch("/runtime/audit.zip")).arrayBuffer();
      openDataLibrary();
      const transfer=new DataTransfer();transfer.items.add(new File([buffer],"Ultima_IV_-_Quest_of_the_Avatar_1985.zip",{type:"application/zip"}));
      ui.gameZipInput.files=transfer.files;ui.gameZipInput.dispatchEvent(new Event("change"));
      await wait(()=>["success","error"].includes(ui.importStatus.dataset.state),"actual audited ZIP file input");
      let audited=await libraryGet("game");
      assert(ui.importStatus.dataset.state==="success" && audited.profile==="u4-dos-english-ega-1.01-v1","The user's exact Archive.org ZIP imports through the launcher file input as 1.01");
      assert(audited.verification.directory==="ULTIMA4" && audited.verification.otherDirectories===1,"Base installation is selected without mixing in the nested upgrade");
      assert(audited.files.find(file=>file.name==="COMPASSN.EGA").data.byteLength===719 && !engine.hasSave(),"Original EGA graphics are selected and embedded saves remain excluded");
      openDataLibrary();
      ui.gameUrlInput.value=`${location.origin}/runtime/audit.zip`;ui.gameUrlForm.requestSubmit();
      await wait(()=>["success","error"].includes(ui.importStatus.dataset.state),"audited direct ZIP URL");
      assert(ui.importStatus.dataset.state==="success" && (await libraryGet("game")).profile===audited.profile,"The same audited ZIP also imports by direct URL");
      // Exercise the folder event path with browser-shaped relative filenames.
      openDataLibrary();
      const folderTransfer=new DataTransfer();
      for(const entry of engine.extractGameZip(buffer,MAX_ZIP_BYTES)) {
        const file=new File([entry.data],entry.name.split("/").at(-1));
        Object.defineProperty(file,"webkitRelativePath",{value:entry.name});folderTransfer.items.add(file);
      }
      ui.gameFolderInput.files=folderTransfer.files;ui.gameFolderInput.dispatchEvent(new Event("change"));
      await wait(()=>["success","error"].includes(ui.importStatus.dataset.state),"audited folder input event");
      audited=await libraryGet("game");
      assert(ui.importStatus.dataset.state==="success" && audited.profile==="u4-dos-english-ega-1.01-v1","Folder import follows the same installation and complete-profile selection");
      sessionStorage.setItem("public-data-expected-profile",audited.profile);
      assert(window.Module.FS.readdir("/game-data-check").length===2,"Nested extraction staging is cleaned up after every import");
    }
    assert(!document.querySelector("#vgaZipInput") && document.querySelector(".vga-import").textContent.includes("NO PATCH UPLOAD NEEDED"),"Library explains automatic VGA without an upload option");
    document.querySelector('[data-adventure-action="new"][data-slot="1"]').click();
    await wait(()=>state()?.prompt?.title==="Name your Avatar" && !ui.conversationForm.hidden,"original new-game name prompt");
    ui.conversationInput.value="PublicHero";ui.conversationForm.requestSubmit(ui.conversationSubmit);
    await wait(()=>option("m") && !option("m").disabled,"character choice");option("m").click();
    let stories=0,questions=0,previous=0;
    await wait(()=>{
      const current=state();
      if(current?.ready && current.inputMode==="command") return true;
      const prompt=current?.prompt;
      if(!prompt?.id || prompt.id===previous || prompt.submitted) return false;
      const value=prompt.options.some(item=>item.value==="a") ? "a" : "\r";
      const choice=option(value);
      if(!choice || choice.disabled || Number(choice.dataset.promptId)!==prompt.id) return false;
      if(prompt.context.startsWith("The story begins"))stories++;
      if(value==="a")questions++;
      previous=prompt.id;choice.click();return false;
    },"story and virtue questions reach gameplay");
    assert(stories===24 && questions===7 && state().party[0].name==="PublicHero","Public BYOD build completes original character creation and enters the world");
    assert(state().vgaAvailable && state().video==="vga" && !(await libraryGet("vga")),"New adventure runs in VGA without a separate patch upload");
    for(let i=0;i<2;i++) {
      const before=state().moves;document.querySelector('[data-key="32"]').click();
      await wait(()=>state().inputMode==="command" && state().moves===before+1,"Wait advances one safe turn");
    }
    assert(state().moves===2,"Public game controls remain responsive after creation");
    document.querySelector(mobileLayout.matches ? '#mobileMenuButton' : '#menuButton').click();
    await tap("experience"); await tap("audio");
    assert(info.soundtrack==="xu4" && state().audio.soundtrack==="hurin","Public build does not identify the xu4 soundtrack");
    assert(!document.querySelector('[data-prompt-value="soundtrack"]'),"Public audio menu still offers alternate soundtrack packs");
    await tap("music"); await tap("0");
    assert(!state().audio.enabled,"Music volume Off did not mute the xu4 soundtrack");
    await tap("music"); await tap("6"); await tap("effects"); await tap("4");
    assert(state().audio.musicVolume===6 && state().audio.effectsVolume===4,"Music and effects have independent volumes");
    await tap("credits");
    assert(state().prompt.context.includes("Joshua Steele") && state().prompt.context.includes("Hurin"),"xu4 soundtrack and graphics credits are available in the web client");
    for(let depth=0;depth<4;depth++) await tap("\x1b");
    await wait(()=>state().inputMode==="command","audio cancellation returns to gameplay");
    assert(state().moves===2,"Audio menus and cancellation do not advance game turns");
    assert(!state().capabilities.debugTools,"Public runtime does not expose debug tools");
    const store=new window.UltimatumAdventureStore();
    ui.saveButton.click();
    const until=Date.now()+30000;
    while(Date.now()<until && (await store.get(1))?.current?.summary?.moves!==2) await new Promise(r=>setTimeout(r,100));
    assert((await store.get(1))?.current?.summary?.moves===2,"Public engine durably saves the new adventure");
    sessionStorage.setItem("public-data-phase","reload");
    sessionStorage.setItem("public-data-results",JSON.stringify(results));
    location.reload();return;
    }
  } catch(error) {
    results.push({ok:false,label:error.stack || error.message});
    window.dataRuntimeResults={ok:false,results};
  }
  panel.style.display="block";
  panel.replaceChildren();
  const heading=document.createElement("h1");heading.textContent=window.dataRuntimeResults.ok ? `PASS — ${results.length} public data/runtime checks` : "FAIL — public data/runtime checks";panel.append(heading);
  for(const result of results){const row=document.createElement("p");row.textContent=`${result.ok?"PASS":"FAIL"} ${result.label}`;panel.append(row);}
})();
