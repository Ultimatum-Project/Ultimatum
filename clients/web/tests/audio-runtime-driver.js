// Disposable loopback origin only. Real engine and shipped prompt buttons.
(async function () {
  const report=document.createElement("pre"); report.id="audioRuntimeReport";
  report.style.cssText="position:fixed;left:0;top:0;z-index:10000;background:#000;color:#9f9;font:11px monospace;max-height:120px;overflow:auto";
  document.body.append(report);
  const results=[]; const write=status=>report.textContent=JSON.stringify({status,results},null,2);
  const pass=name=>{results.push(name);write("RUNNING");};
  const assert=(ok,name)=>{if(!ok)throw Error(name);};
  const client=new window.UltimatumEngineClient(()=>window.Module);
  const state=()=>window.Module?.ccall && document.querySelector('#engineStatus').hidden ? client.snapshot() : null;
  const wait=async(test,name)=>{for(let n=0;n<600;n++){if(test())return;await new Promise(r=>setTimeout(r,100));}throw Error(name);};
  const tap=async value=>{
    let target;
    await wait(()=>{
      target=document.querySelector(`[data-prompt-value="${CSS.escape(value)}"]:not(:disabled)`);
      return target && Number(target.dataset.promptId)===state().prompt.id && !state().prompt.submitted;
    },`Missing current-generation ${value}`);
    const before=state().prompt.id; target.click();
    await wait(()=>state().prompt.id!==before || state().inputMode==="command",`Reply ${value} did not settle`);
  };
  try {
    await wait(()=>state()?.ready && state().inputMode==="command","Engine did not start isolated seed");
    assert(state().vgaAvailable && state().video==="vga","Automatic graphics overlay is unavailable");
    pass("Verified graphics overlay activates automatically, with no separate patch upload");
    const before=state();
    document.querySelector('#menuButton').click(); await tap("experience"); await tap("audio");
    assert(state().audio.soundtrack==="hurin","The xu4 soundtrack is not active");
    assert(!document.querySelector('[data-prompt-value="soundtrack"]'),"Alternate soundtrack picker is still exposed");
    pass("Only the xu4 soundtrack is exposed");
    await tap("music"); await tap("0");
    assert(!state().audio.enabled,"Off did not mute");
    pass("Music volume Off mutes the xu4 soundtrack");
    await tap("music"); await tap("6"); await tap("effects"); await tap("4");
    assert(state().audio.enabled && state().audio.musicVolume===6 && state().audio.effectsVolume===4,"Independent volumes failed");
    await tap("credits");
    assert(state().prompt.context.includes("Hurin") && state().prompt.context.includes("Joshua Steele"),"Credits omit xu4 soundtrack or graphics attribution");
    await tap("\u001b"); await tap("\u001b"); await tap("\u001b"); await tap("\u001b");
    await wait(()=>state().inputMode==="command","Audio did not return to controls");
    assert(state().moves===before.moves && state().food===before.food,"Audio settings spent adventure resources");
    pass("Credits, cancellation and return to controls preserve adventure");
    await client.syncSaves(false);
    assert(window.Module.FS.readFile("/home/web_user/.xu4/xu4rc",{encoding:"utf8"}).includes("audio.soundtrack=hurin"),"xu4 soundtrack identity not persisted");
    pass("xu4 soundtrack and independent volumes persist"); write("PASS");
  }catch(error){results.push({error:String(error)});write("FAIL");}
})();
