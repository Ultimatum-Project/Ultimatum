// Native bridge uses a nonpersistent WKWebView; auth tokens live in Keychain.
(function() {
  'use strict';
  const bridge=window.webkit?.messageHandlers?.ultimatumCloud;
  if(!bridge){document.querySelector('#cloudPanel').textContent='Open Accounts from Saved Games.';return;}
  let sequence=0;const pending=new Map();
  const request=(action,data={})=>new Promise((resolve,reject)=>{
    const id=++sequence;pending.set(id,{resolve,reject});bridge.postMessage({id,action,...data});
  });
  window.ultimatumCloudReply=(id,value,error)=>{
    const job=pending.get(id);if(!job)return;pending.delete(id);error ? job.reject(Error(error)) : job.resolve(value);
  };
  const storage={getItem:key=>request('getSession',{key}),setItem:(key,value)=>request('setSession',{key,value}),removeItem:key=>request('removeSession',{key})};
  const cloud=new window.UltimatumCloudClient({storage});
  const panel=new window.UltimatumCloudUI(document.querySelector('#cloudPanel'),{
    deviceName:'iPhone',
    list:()=>request('list'),validate:text=>request('validate',{text}),
    install:(slot,text,expected,cloud)=>request('install',{slot,text,expected,cloud}),
    gameData:async()=>{
      const raw=await request('gameData');if(!raw?.available)return {available:false};
      const entries=raw.files.map(file=>({name:file.name,data:window.UltimatumGameData.base64ToBytes(file.data).buffer}));
      const verified=await window.UltimatumGameData.validate(entries);
      return {available:true,profile:verified.profile,label:verified.verification.label,logicalBytes:verified.files.reduce((sum,file)=>sum+file.data.byteLength,0),text:window.UltimatumGameData.encodePackage(verified)};
    },
    installGameData:async text=>{await window.UltimatumGameData.decodePackage(text);return request('installGameData',{text});},
    link:(slot,cloud,expected)=>request('setLink',{slot,cloud,expected}),close:()=>request('close'),
  },cloud);
  window.ultimatumCloudPanel=panel;
  panel.run(async()=>{
    try{await panel.refresh();}
    finally{if(window.ultimatumBackgroundSync)await request('syncComplete');}
  });
})();
