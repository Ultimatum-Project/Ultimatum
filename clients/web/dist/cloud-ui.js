(function(global){
  'use strict';
  const Store=global.UltimatumAdventureStore;
  class CloudUI {
    constructor(root,host,cloud=new global.UltimatumCloudClient()){
      this.root=root;this.host=host;this.cloud=cloud;this.busy=false;this.background=false;this.confirmation=null;this.view='overview';
      root.innerHTML=`<header class="account-header"><div><p class="account-kicker">ULTIMATUM ACCOUNT</p><h2>Account</h2></div><button data-cloud="done" class="account-done">Done</button></header>
        <p class="cloud-status" role="status" aria-live="polite"></p>
        <section class="cloud-auth account-card"><h3>Continue your saved games anywhere</h3><p>Sign in by email to protect your saves and continue on iPhone or the web.</p>
          <form><label>Email address<input name="email" type="email" autocomplete="email" required></label><button type="submit">Send sign-in code</button>
          <section class="cloud-code" hidden><label>Email code<input name="code" type="text" inputmode="numeric" autocomplete="one-time-code" maxlength="10"></label>
          <button type="button" data-cloud="verify">Verify code</button><button type="button" data-cloud="changeemail" class="secondary">Use another email</button></section></form>
          <p class="account-help">No password needed. Your local saved games remain on this device.</p></section>
        <div class="cloud-signed" hidden>
          <section class="account-identity"><div><strong class="cloud-identity"></strong><span class="cloud-health">Cloud saves on</span></div>
            <button data-cloud="refresh" class="secondary">Sync now</button></section>
          <button class="storage-meter" data-cloud="storage"><span><strong class="storage-title">Storage</strong><span class="storage-copy"></span></span><span class="storage-track"><i></i></span></button>
          <nav class="account-tabs" aria-label="Account sections">
            <button data-cloud="overview" aria-current="page">Overview</button><button data-cloud="games">Games &amp; data</button><button data-cloud="storage">Storage</button><button data-cloud="account">Account</button>
          </nav>
          <main class="account-view"></main>
        </div>
        <section class="cloud-confirm account-card" hidden><h3>Review this action</h3><p></p><div><button data-cloud="confirm">Continue</button><button data-cloud="cancel" class="secondary">Cancel</button></div></section>`;
      cloud.client.auth.onAuthStateChange(event=>{if(event==='SIGNED_OUT')this.resetSignedOut();});
      root.querySelector('form').addEventListener('submit',event=>{event.preventDefault();this.run(()=>this.pendingEmail&&root.querySelector('input[name=code]').value.trim()?this.verifyCode():this.sendCode());});
      root.addEventListener('click',event=>{
        const button=event.target.closest('button[data-cloud]');if(!button)return;const action=button.dataset.cloud;
        if(action==='done'){if(!this.installing){this.cancel();host.close();}return;}
        if(action==='cancel'){this.cancel();return;}
        if(action==='confirm'){const callback=this.confirmation;this.cancel();if(callback)this.run(callback);return;}
        if(action==='verify'){this.run(()=>this.verifyCode());return;}
        if(action==='changeemail'){this.pendingEmail=null;root.querySelector('.cloud-code').hidden=true;root.querySelector('input[name=email]').readOnly=false;root.querySelector('input[name=code]').value='';return;}
        if(action==='refresh')this.run(()=>this.refresh());
        if(['overview','games','storage','account'].includes(action)){this.view=action;this.render();}
      });
    }
    status(text){this.root.querySelector('.cloud-status').textContent=text||'';}
    async run(callback){
      if(this.busy)return;this.busy=true;this.disable();
      try{await callback();}catch(error){this.status(error.message||'Account service is temporarily unavailable. Local saves are safe.');}
      finally{this.busy=false;this.disable();}
    }
    disable(){this.root.querySelectorAll('button,input,select').forEach(element=>{element.disabled=element.dataset.cloud==='done'?Boolean(this.installing):this.busy||element.dataset.unavailable==='true';});}
    resetSignedOut(){
      this.cancel();this.pendingEmail=null;this.data=null;
      this.root.querySelector('input[name=email]').readOnly=false;this.root.querySelector('input[name=code]').value='';
      this.root.querySelector('.cloud-code').hidden=true;this.root.querySelector('.cloud-signed').hidden=true;this.root.querySelector('.cloud-auth').hidden=false;
    }
    cancel(){this.confirmation=null;this.root.querySelector('.cloud-confirm').hidden=true;this.root.querySelector('.cloud-signed').inert=false;this.trigger?.focus();this.trigger=null;}
    confirm(text,callback,label='Continue'){
      this.trigger=document.activeElement;this.confirmation=callback;const panel=this.root.querySelector('.cloud-confirm');
      panel.querySelector('p').textContent=text;panel.querySelector('[data-cloud=confirm]').textContent=label;panel.hidden=false;
      this.root.querySelector('.cloud-signed').inert=true;panel.querySelector('button').focus();panel.scrollIntoView({block:'nearest'});
    }
    async sendCode(){
      if(this.lastCodeAt&&Date.now()-this.lastCodeAt<60000)throw Error('Please wait a minute before requesting another code.');
      const email=this.root.querySelector('input[name=email]').value.trim();await this.cloud.sendCode(email);this.pendingEmail=email;this.lastCodeAt=Date.now();
      this.root.querySelector('input[name=email]').readOnly=true;this.root.querySelector('.cloud-code').hidden=false;
      this.status('Check your email for a sign-in code.');this.root.querySelector('input[name=code]').focus();
    }
    async verifyCode(){
      const input=this.root.querySelector('input[name=code]'),token=input.value.trim();
      if(!this.pendingEmail||!/^[0-9]{6,10}$/.test(token))throw Error('Enter the sign-in code from your email.');
      try{await this.cloud.verifyCode(this.pendingEmail,token);await this.refresh();this.status('Signed in. Your saved games are protected when sync completes.');}
      finally{input.value='';}
    }
    deviceName(){
      if(this.host.deviceName)return this.host.deviceName;
      const platform=global.navigator?.userAgentData?.platform||global.navigator?.platform||'Web';
      return /iphone|ipad/i.test(platform)?'iPhone web':'Web · '+platform;
    }
    localInfo(local){
      if(!local?.text)return {...local,contentFingerprint:null};
      try{const decoded=Store.decode(local.text);return {...local,decoded,contentFingerprint:Store.fingerprint(decoded.files)};}
      catch{return {...local,contentFingerprint:null};}
    }
    link(resource,version,local){
      return {resourceId:resource.id,revisionId:version.id,fingerprint:version.content_fingerprint,label:resource.label,syncedAt:Date.now()};
    }
    async install(localSlot,resource,local){
      const text=await this.cloud.download(resource.current.id),decoded=Store.decode(text),fingerprint=Store.fingerprint(decoded.files);
      const cloudLink={resourceId:resource.id,revisionId:resource.current.id,fingerprint,label:resource.label,syncedAt:Date.now()};
      this.installing=true;this.disable();
      try{await this.host.install(localSlot,text,local?.fingerprint??null,cloudLink);}finally{this.installing=false;this.disable();}
    }
    async upload(local,resource,expected){
      const result=await this.cloud.upload(resource?.id||null,expected||null,local.text,this.deviceName());
      const link={resourceId:result.resource_id,revisionId:result.revision_id,fingerprint:result.content_fingerprint,label:local.label,syncedAt:Date.now()};
      await this.host.link(local.slot,link,local.fingerprint);return result;
    }
    async reconcile(resources,locals){
      const adventures=resources.filter(resource=>resource.kind==='adventure'&&resource.current);
      const localRows=locals.map(local=>this.localInfo(local));
      for(const local of localRows.filter(row=>row.text)){
        let resource=local.cloud?.resourceId?adventures.find(row=>row.id===local.cloud.resourceId):null;
        if(!resource){
          const exact=adventures.find(row=>row.current.content_fingerprint===local.contentFingerprint&&!localRows.some(other=>other!==local&&other.cloud?.resourceId===row.id));
          if(exact){await this.host.link(local.slot,this.link(exact,exact.current,local),local.fingerprint);return true;}
          const legacy=adventures.find(row=>row.legacy_slot===local.slot&&!localRows.some(other=>other.cloud?.resourceId===row.id));
          if(!legacy){await this.upload(local,null,null);return true;}
          continue;
        }
        const base=local.cloud,localChanged=local.contentFingerprint!==base.fingerprint||local.label!==base.label;
        const cloudChanged=resource.current_version!==base.revisionId;
        if(localChanged&&!cloudChanged){await this.upload(local,resource,resource.current_version);return true;}
        if(!localChanged&&cloudChanged&&!local.active){await this.install(local.slot,resource,local);return true;}
      }
      if(!localRows.some(row=>row.text)){
        const empty=[1,2,3];for(const resource of adventures.slice(0,3)){const slot=empty.shift();if(!slot)break;await this.install(slot,resource,null);return true;}
      }
      return false;
    }
    async refresh(){
      this.cancel();const session=await this.cloud.session();
      this.root.querySelector('.cloud-auth').hidden=Boolean(session);this.root.querySelector('.cloud-signed').hidden=!session;
      if(!session)return;
      const user=session.user.id;this.root.querySelector('.cloud-identity').textContent=session.user.email||'Ultimatum player';
      for(let pass=0;pass<7;pass++){
        const [resources,locals]=await Promise.all([this.cloud.resources(),this.host.list()]);
        if((await this.cloud.session())?.user?.id!==user)throw Error('The account changed. Open Account again.');
        if(!await this.reconcile(resources,locals))break;
      }
      if(this.host.gameData&&!global.ultimatumBackgroundSync)this.status('Checking this device’s game data…');
      const gameRequest=this.host.gameData&&!global.ultimatumBackgroundSync?this.host.gameData():Promise.resolve(this.data?.gameData||{available:false});
      const [resources,locals,summary,gameData]=await Promise.all([this.cloud.resources(),this.host.list(),this.cloud.summary(),gameRequest]);
      this.data={user,resources,locals:locals.map(local=>this.localInfo(local)),summary,gameData:gameData||{available:false}};this.render();
      const attention=this.data.locals.some(local=>{
        if(!local.text)return false;const resource=local.cloud?.resourceId?resources.find(row=>row.id===local.cloud.resourceId):resources.find(row=>row.legacy_slot===local.slot);
        return this.stateFor(local,resource)==='Needs your attention';
      });
      this.status(attention?'One saved game needs your attention. Choose which progress to keep.':'Cloud saves are up to date.');
    }
    async syncSavedAdventure(slot){
      if(this.busy||this.background)return;this.background=true;
      try{if(!await this.cloud.session())return;await this.refresh();if(this.root.closest('dialog')?.open)this.status('Saved to cloud.');}
      finally{this.background=false;}
    }
    formatBytes(value){if(value<1024)return value+' B';if(value<1048576)return (value/1024).toFixed(value<10240?1:0)+' KB';return (value/1048576).toFixed(value<10485760?1:0)+' MB';}
    setView(view){
      this.view=view;this.root.querySelectorAll('.account-tabs button').forEach(button=>button.setAttribute('aria-current',String(button.dataset.cloud===view)));
    }
    render(){
      if(!this.data)return;this.setView(this.view);const {summary}=this.data,percent=Math.min(100,summary.used_bytes/summary.quota_bytes*100);
      this.root.querySelector('.storage-copy').textContent=`${this.formatBytes(summary.used_bytes)} of ${this.formatBytes(summary.quota_bytes)} used`;
      this.root.querySelector('.storage-track i').style.width=percent+'%';
      const view=this.root.querySelector('.account-view');view.replaceChildren();
      if(this.view==='overview')this.renderOverview(view);else if(this.view==='games')this.renderGames(view);else if(this.view==='storage')this.renderStorage(view);else this.renderAccount(view);
      this.disable();
    }
    card(title,copy){
      const section=document.createElement('section');section.className='account-card';const h=document.createElement('h3');h.textContent=title;section.append(h);
      if(copy){const p=document.createElement('p');p.textContent=copy;section.append(p);}return section;
    }
    action(parent,label,callback,secondary=false,disabled=false){
      const button=document.createElement('button');button.type='button';button.textContent=label;if(secondary)button.classList.add('secondary');
      button.dataset.unavailable=String(disabled);button.disabled=disabled||this.busy;button.addEventListener('click',callback);parent.append(button);return button;
    }
    stateFor(local,resource){
      if(!local||!resource)return local?'Saved on this device':'Available from cloud';
      const base=local.cloud;if(!base)return resource.legacy_slot===local.slot?'Needs your attention':'Saved on this device';
      const localChanged=local.contentFingerprint!==base.fingerprint||local.label!==base.label,cloudChanged=resource.current_version!==base.revisionId;
      if(localChanged&&cloudChanged)return 'Needs your attention';
      if(cloudChanged)return local.active?'Updated on another device':'Syncing';
      if(localChanged)return 'Waiting to sync';return 'Saved to cloud';
    }
    detail(local,resource){
      const parts=[];if(local?.summary)parts.push(local.summary);if(resource?.current?.device_name)parts.push('Last saved on '+resource.current.device_name);
      if(resource?.current?.created_at)parts.push(new Date(resource.current.created_at).toLocaleString());return parts.join(' · ');
    }
    renderOverview(view){
      const heading=document.createElement('div');heading.className='view-heading';heading.innerHTML='<h3>Saved Games</h3><p>Continue on any signed-in device. Sync happens automatically.</p>';view.append(heading);
      const resources=this.data.resources.filter(row=>row.kind==='adventure'&&row.current),shown=new Set();
      for(const local of this.data.locals.filter(row=>row.text)){
        const resource=local.cloud?.resourceId?resources.find(row=>row.id===local.cloud.resourceId):resources.find(row=>row.legacy_slot===local.slot&&!shown.has(row.id));
        if(resource)shown.add(resource.id);const state=this.stateFor(local,resource),card=this.card(local.label||resource?.label||'Saved Game',this.detail(local,resource));
        const badge=document.createElement('p');badge.className='sync-state'+(state==='Needs your attention'?' needs-attention':'');badge.textContent=state;card.append(badge);
        if(state==='Needs your attention'&&resource){
          const explanation=document.createElement('p');explanation.textContent='This device and the cloud copy both changed. Choose the progress you want to continue with; the other version stays in history.';card.append(explanation);
          this.action(card,'Use this device',()=>this.confirm(`Use ${local.label} from this device as the current cloud save?`,async()=>{await this.upload(local,resource,resource.current_version);await this.refresh();},'Use this device'));
          this.action(card,'Use cloud copy',()=>this.confirm(`Replace the local checkpoint with the cloud copy from ${resource.current.device_name||'another device'}? The current local checkpoint remains available for recovery.`,async()=>{await this.install(local.slot,resource,local);await this.refresh();},'Use cloud copy'),true,local.active);
        }else if(!resource){
          this.action(card,'Protect in cloud',()=>this.run(async()=>{await this.upload(local,null,null);await this.refresh();}));
        }else if(resource.current_version!==local.cloud?.revisionId&&local.active){
          const note=document.createElement('p');note.textContent='Return to the title screen to apply the newer cloud checkpoint safely.';card.append(note);
        }
        this.action(card,'Backup history',()=>{this.view='storage';this.render();setTimeout(()=>document.querySelector(`[data-resource="${resource?.id||''}"]`)?.click(),0);},true,!resource);
        view.append(card);
      }
      for(const resource of resources.filter(row=>!shown.has(row.id))){
        const card=this.card(resource.label,this.detail(null,resource));const badge=document.createElement('p');badge.className='sync-state';badge.textContent='Available from cloud';card.append(badge);
        const empty=[1,2,3].find(slot=>!this.data.locals.some(local=>local.slot===slot&&local.text));
        this.action(card,empty?'Continue on this device':'Manage local saved games first',()=>this.confirm(`Add ${resource.label} to this device? Cloud history remains unchanged.`,async()=>{await this.install(empty,resource,null);await this.refresh();},'Add to this device'),false,!empty);
        this.action(card,'Backup history',()=>{this.view='storage';this.render();setTimeout(()=>this.root.querySelector(`[data-resource="${resource.id}"]`)?.click(),0);},true);
        view.append(card);
      }
      if(!this.data.locals.some(row=>row.text)&&!resources.length)view.append(this.card('No saved games yet','Start a new game. It will appear here after its first safe checkpoint.'));
    }
    renderGames(view){
      const adventures=this.data.resources.filter(row=>row.kind==='adventure'),bytes=adventures.reduce((sum,row)=>sum+(row.current?.stored_bytes||0),0);
      const resource=this.data.resources.find(row=>row.kind==='game_data'&&row.current),local=this.data.gameData;
      const card=this.card('Ultima IV · Quest of the Avatar',`${adventures.length} cloud save${adventures.length===1?'':'s'} · ${this.formatBytes(bytes)} current save data`);
      const list=document.createElement('dl');
      const localState=local.available?`${local.label||'Verified DOS/EGA files'} · ${this.formatBytes(local.logicalBytes||0)}`:'Not installed in this browser';
      const cloudState=resource?`${this.formatBytes(resource.current.stored_bytes)} · saved ${new Date(resource.current.created_at).toLocaleString()}`:'Not saved to your account';
      list.innerHTML=`<dt>This device</dt><dd></dd><dt>Private cloud copy</dt><dd></dd>`;list.children[1].textContent=localState;list.children[3].textContent=cloudState;card.append(list);
      if(local.available){
        this.action(card,resource?'Update private cloud copy':'Save game data to cloud',()=>this.confirm(resource?'Replace the private cloud copy with the verified game data on this device? The earlier copy remains in Storage history.':'Save these verified game files privately to your account? They count toward your 100 MB allowance.',async()=>{await this.cloud.uploadGameData(resource?.id||null,resource?.current_version||null,local.text,this.deviceName());await this.refresh();this.status('Game data saved privately to your account.');},resource?'Update cloud copy':'Save to cloud'));
      }
      if(resource&&this.host.installGameData){
        this.action(card,local.available?'Download cloud copy':'Download to this device',()=>this.confirm(local.available?'Replace the game data stored on this device with the private cloud copy? Saved games and save history are unaffected.':'Download and verify your private cloud game data on this device?',async()=>{const text=await this.cloud.downloadGameData(resource.current.id),result=await this.host.installGameData(text);await this.refresh();this.status(result?.reloadRequired?'Game data downloaded. Reopen the game to use this copy.':'Game data downloaded and verified on this device.');},'Download game data'),true);
      }
      if(!local.available&&!resource){const note=document.createElement('p');note.className='account-help';note.textContent='Install verified Ultima IV data on this device first. You can then keep a private copy in your account for your other devices.';card.append(note);}
      else {const note=document.createElement('p');note.className='account-help';note.textContent='Only you can access this package. It is kept separately from saved games and uses the same 100 MB allowance.';card.append(note);}
      view.append(card);
    }
    renderStorage(view){
      const s=this.data.summary,heading=document.createElement('div');heading.className='view-heading';heading.innerHTML=`<h3>Storage</h3><p>${this.formatBytes(s.used_bytes)} of ${this.formatBytes(s.quota_bytes)} used</p>`;view.append(heading);
      const totals=this.card('What is using space');totals.innerHTML+=`<dl><dt>Saved games</dt><dd>${this.formatBytes(s.adventures_bytes)}</dd><dt>Save history</dt><dd>${this.formatBytes(s.history_bytes)}</dd><dt>Game data</dt><dd>${this.formatBytes(s.game_data_bytes)}</dd></dl>`;view.append(totals);
      for(const resource of this.data.resources){
        const card=this.card(resource.label,`${resource.kind==='adventure'?'Saved game':'Game data'} · Current version ${this.formatBytes(resource.current?.stored_bytes||0)}`);
        const history=document.createElement('div');history.className='storage-history';history.hidden=true;card.append(history);
        this.action(card,'Show versions',()=>this.run(async()=>{if(history.hidden){await this.renderHistory(resource,history);history.hidden=false;}else history.hidden=true;}),true).dataset.resource=resource.id;
        this.action(card,'Remove from cloud',()=>this.confirm(`Remove ${resource.label} and all of its cloud history? Local copies on your devices are unaffected.`,async()=>{await this.cloud.deleteResource(resource.id,resource.current_version);await this.refresh();},'Remove from cloud'),true);
        view.append(card);
      }
    }
    async renderHistory(resource,container){
      const versions=await this.cloud.history(resource.id);container.replaceChildren();
      for(const version of versions){
        const row=document.createElement('div');row.className='storage-version';const copy=document.createElement('span');
        copy.textContent=`${version.id===resource.current_version?'Current':'Earlier checkpoint'} · ${new Date(version.created_at).toLocaleString()} · ${this.formatBytes(version.stored_bytes)}`;row.append(copy);
        if(version.id!==resource.current_version)this.action(row,'Delete',()=>this.confirm(`Delete this earlier checkpoint and free ${this.formatBytes(version.stored_bytes)}? The current saved game is unchanged.`,async()=>{await this.cloud.deleteVersion(version.id);await this.refresh();},'Delete checkpoint'),true);
        container.append(row);
      }
    }
    renderAccount(view){
      const identity=this.card('Email',this.root.querySelector('.cloud-identity').textContent);view.append(identity);
      const device=this.card('This device',this.deviceName()+' · Signed in');view.append(device);
      const support=this.card('Support','Questions, feedback, or correspondence are welcome.');
      const link=document.createElement('a');link.className='account-button';link.href='mailto:feedback@ultimatumproject.com?subject=Ultimatum%20feedback';link.textContent='Email feedback@ultimatumproject.com';support.append(link);view.append(support);
      const settings=this.card('Account settings','Signing out leaves local saved games on this device.');
      this.action(settings,'Sign out on this device',()=>this.confirm('Sign out on this device? Local saved games remain available.',async()=>{await this.cloud.signOut();this.resetSignedOut();this.status('Signed out. Local saved games are still on this device.');},'Sign out'),true);view.append(settings);
    }
  }
  global.UltimatumCloudUI=CloudUI;
})(window);
