// Disposable loopback origin + named test project only. Never shipped publicly.
(async function(){
  const report=document.createElement('pre');report.id='runtimeReport';report.style.cssText='position:fixed;left:0;top:0;z-index:10000;max-height:120px;overflow:auto;background:#000;color:#9f9;font:10px monospace;pointer-events:none';document.body.append(report);
  const results=[],write=(status,error)=>report.textContent=JSON.stringify({status,results,error},null,2);
  const assert=(value,message)=>{if(!value)throw Error(message);},pass=name=>{results.push(name);write('running');};
  const wait=async(fn,message)=>{const started=Date.now();while(Date.now()-started<30000){const value=await fn();if(value)return value;await new Promise(resolve=>setTimeout(resolve,100));}throw Error(message);};
  const button=(root,text)=>[...root.querySelectorAll('button')].find(item=>item.textContent===text);
  const tap=async item=>{await wait(()=>item&&!item.disabled,'Button unavailable');item.click();};
  try{
    assert(location.hostname==='127.0.0.1'&&window.UltimatumCloudConfig.project==='ultimatum-test','Isolated test origin/project required');
    const fixture=await(await fetch('/cloud-fixture.json')).json(),identity=fixture[0];
    await wait(()=>adventures.ready,'Title not ready');
    const store=adventures.store,Store=window.UltimatumAdventureStore;
    const imported=Store.decode(await(await fetch('/cloud-adventure.u4save')).text()),summary=engine.validateAdventure(imported.files);
    await store.commit(2,imported.files,summary,'Cloud test adventure');
    const original=await store.get(2);
    await tap(document.querySelector('#adventureAccount'));
    const panel=adventures.cloudUI,root=panel.root;
    await wait(()=>!panel.busy,'Account panel not ready');
    panel.cloud.sendCode=async email=>assert(email===identity.email,'Unexpected test email');
    root.querySelector('input[name=email]').value=identity.email;root.querySelector('form').requestSubmit();
    await wait(()=>!root.querySelector('.cloud-code').hidden&&!panel.busy,'Code input missing');
    root.querySelector('input[name=code]').value='000000';await tap(button(root,'Verify code'));
    await wait(()=>!panel.busy,'Invalid code still pending');assert(!root.querySelector('.cloud-auth').hidden,'Invalid code signed in');
    root.querySelector('input[name=code]').value=identity.code;await tap(button(root,'Verify code'));
    await wait(()=>panel.data&&!panel.busy,'Real code verification or automatic sync failed');
    assert(panel.data.summary.quota_bytes===104857600,'Account quota is not 100 MiB');
    const linked=(await store.get(2)).cloud,resource=panel.data.resources.find(row=>row.id===linked?.resourceId);
    assert(resource&&resource.current_version===linked.revisionId,'Automatic upload did not link the local adventure');
    pass('Real Supabase code verification opens the Account hub and automatic sync links the adventure');

    const cloudPackage=Store.encode(imported,'Competing device adventure');
    await panel.cloud.upload(resource.id,resource.current_version,cloudPackage,'Competing browser');
    await store.rename(2,'This device adventure');
    await panel.run(()=>panel.refresh());
    await wait(()=>/needs your attention/i.test(root.querySelector('.cloud-status').textContent),'Concurrent edits did not produce a conflict');
    const before=await store.get(2);
    await tap(button(root,'Use cloud copy'));await tap(root.querySelector('[data-cloud=cancel]'));
    assert((await store.get(2)).label===before.label,'Cancelled conflict choice changed the local adventure');
    await tap(button(root,'Use cloud copy'));await tap(root.querySelector('[data-cloud=confirm]'));
    await wait(()=>!panel.busy&&(store.get(2)).then(row=>row.label==='Competing device adventure'),'Chosen cloud copy was not installed');
    const installed=await store.get(2);
    assert(installed.previous?.fingerprint===before.current.fingerprint,'Cloud install did not preserve local recovery');
    pass('Concurrent progress requires a choice; cancellation is safe and cloud install retains recovery');

    await tap(root.querySelector('[data-cloud=storage]'));await wait(()=>root.querySelector('.storage-history'),'Storage view missing');
    await tap(button(root,'Show versions'));await wait(()=>root.querySelectorAll('.storage-version').length>=2,'Recovery history missing');
    assert(/100 MB/.test(root.querySelector('.storage-copy').textContent),'Storage allowance is not visible');
    pass('Storage shows the shared allowance and retained checkpoint history');

    await tap(root.querySelector('[data-cloud=account]'));await tap(button(root,'Sign out on this device'));await tap(root.querySelector('[data-cloud=confirm]'));
    await wait(()=>!panel.busy&&!root.querySelector('.cloud-auth').hidden,'Device-local sign-out failed');
    assert((await store.get(2)).current.fingerprint===installed.current.fingerprint,'Sign-out removed local progress');
    await tap(root.querySelector('[data-cloud=done]'));
    pass('Device-local sign-out leaves the local adventure intact and Done returns to the picker');
    write('passed');
  }catch(error){write('failed',error.message);console.error(error);}
})();
