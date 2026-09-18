// Cold-start Account coverage: signed-in cloud refresh must wait for Wasm FS.
(async function(){
  const report=document.createElement('pre');report.id='runtimeReport';report.style.cssText='position:fixed;left:0;top:0;z-index:10000;max-height:140px;overflow:auto;background:#000;color:#9f9;font:11px monospace';document.body.append(report);
  const write=(status,error)=>report.textContent=JSON.stringify({status,error},null,2);
  const assert=(value,message)=>{if(!value)throw Error(message);};
  const wait=async(fn,message)=>{const started=Date.now();while(Date.now()-started<30000){const value=await fn();if(value)return value;await new Promise(resolve=>setTimeout(resolve,50));}throw Error(message);};
  try{
    const panel=adventures.cloudUI;
    let inspected=false;
    panel.cloud.session=async()=>({user:{id:'cold-account',email:'player@example.test'}});
    panel.cloud.resources=async()=>[];
    panel.cloud.summary=async()=>({used_bytes:0,quota_bytes:104857600,adventures_bytes:0,history_bytes:0,game_data_bytes:0});
    panel.host.list=async()=>[];
    panel.host.gameData=async()=>{inspected=true;await waitForRuntimeFilesystem();return {available:false};};
    const originalHasGameData=engine.hasGameData;
    engine.hasGameData=()=>false;
    const accountDialog=document.querySelector('#cloudDialog');
    const dataDialog=document.querySelector('#dataDialog');
    showDataDialog(true);
    assert(dataDialog.open,'Required Game Data dialog did not open');
    ui.dataAccount.click();
    assert(accountDialog.open,'Account dialog did not open');
    assert(!dataDialog.open,'Game Data stayed open underneath Account');
    await wait(()=>inspected,'Account did not begin its local game-data scan');
    assert(!/undefined is not an object/i.test(panel.root.textContent),'Account exposed a pre-runtime filesystem error');
    await wait(()=>panel.data&&!panel.busy,'Account did not finish after the runtime filesystem initialized');
    assert(accountDialog.open,'Game Data interrupted the Account dialog during startup');
    assert(!dataDialog.open,'Game Data opened over the Account dialog during startup');
    assert(panel.data.summary.quota_bytes===104857600,'Account summary did not render');
    assert(/Cloud saves are up to date/.test(panel.root.querySelector('.cloud-status').textContent),'Account did not finish cleanly');
    panel.root.querySelector('[data-cloud="done"]').click();
    await wait(()=>dataDialog.open,'Required Game Data did not open after Account closed');
    assert(!accountDialog.open,'Account dialog stayed open after Done');
    assert(dataDialog.classList.contains('required'),'Game Data was not marked required');
    engine.hasGameData=originalHasGameData;
    write('passed');
  }catch(error){write('failed',error.message);console.error(error);}
})();
