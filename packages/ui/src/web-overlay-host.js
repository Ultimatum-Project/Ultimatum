(function installWebOverlayHost(global){
  const ID=/^[a-z0-9]+(?:-[a-z0-9]+)*$/;
  function requireValue(condition,message){if(!condition)throw new TypeError(`Invalid overlay host: ${message}`);}
  function focus(element){try{element?.focus?.({preventScroll:true});}catch{element?.focus?.();}}
  class WebOverlayHost{
    constructor({fallbackFocus=null,maxEvents=100,now=()=>new Date().toISOString(),manifestUrl=null,fetch:fetcher=global.fetch}={}){
      requireValue(Number.isSafeInteger(maxEvents)&&maxEvents>0&&maxEvents<=500,"maxEvents must be between 1 and 500");
      this.contractVersion=1;this.fallbackFocus=fallbackFocus;this.maxEvents=maxEvents;this.now=now;this.manifestUrl=manifestUrl;this.fetcher=fetcher;
      this.entries=new Map();this.states=new Map();this.events=[];this.manifest=null;this.loadPromise=null;this.managedHides=new WeakSet();
    }
    register(definition){
      requireValue(ID.test(definition?.id||"")&&!this.entries.has(definition.id),"overlay id is missing or duplicated");
      requireValue(definition.element&&typeof definition.element==="object","overlay element is required");
      requireValue(["modal","nonmodal"].includes(definition.modality),`${definition.id} modality is invalid`);
      const entry=Object.freeze({...definition});entry.element.dataset.ultimatumOverlay=entry.id;this.entries.set(entry.id,entry);
      if(!entry.closeElement&&typeof entry.element.close==="function"){
        const nativeClose=entry.element.close.bind(entry.element);
        entry.element.close=(...arguments_)=>{const managed=this.managedHides.has(entry.element);const result=nativeClose(...arguments_);if(!managed&&this.states.has(entry.id))this.close(entry.id);return result;};
      }
      entry.element.addEventListener?.("close",()=>{if(this.managedHides.delete(entry.element))return;if(this.states.has(entry.id))this.close(entry.id);});
      return entry;
    }
    entry(id){const entry=this.entries.get(id);requireValue(entry,`unknown overlay ${id}`);return entry;}
    async load(host="web"){
      requireValue(this.manifestUrl&&typeof this.fetcher==="function","manifest URL and fetch implementation are required");
      if(!this.loadPromise)this.loadPromise=this.fetcher.call(global,this.manifestUrl,{cache:"no-cache"}).then(response=>{if(!response.ok)throw Error(`Overlay manifest request failed (${response.status}).`);return response.json();}).then(manifest=>{
        requireValue(manifest?.schemaVersion===1&&Number.isSafeInteger(manifest.overlaysVersion)&&Array.isArray(manifest.overlays),"manifest is invalid");
        const declared=manifest.overlays.filter(overlay=>overlay.hosts?.includes(host));
        for(const overlay of declared){const entry=this.entries.get(overlay.id);requireValue(entry,`manifest overlay ${overlay.id} is not registered`);requireValue(entry.modality===overlay.modality,`${overlay.id} modality disagrees with its manifest`);}
        for(const entry of this.entries.values())requireValue(declared.some(overlay=>overlay.id===entry.id),`registered overlay ${entry.id} is not declared for ${host}`);
        this.manifest=Object.freeze(manifest);return this.manifest;
      });
      return this.loadPromise;
    }
    isOpen(entry){return entry.isOpen?Boolean(entry.isOpen(entry.element)):entry.element.open===true;}
    currentModal(){
      for(const entry of this.entries.values())if(entry.modality==="modal"&&this.isOpen(entry)){
        if(!this.states.has(entry.id))this.states.set(entry.id,{trigger:null,returnTo:null,suspended:false});return entry;
      }
      return null;
    }
    emit(type,entry,detail={}){
      const event=Object.freeze({contractVersion:1,at:this.now(),type,id:entry.id,modality:entry.modality,...detail});
      this.events.push(event);if(this.events.length>this.maxEvents)this.events.shift();return event;
    }
    show(entry){if(entry.openElement)entry.openElement(entry.element);else entry.element.showModal();}
    hide(entry){if(entry.closeElement)entry.closeElement(entry.element);else{this.managedHides.add(entry.element);entry.element.close();}}
    open(id,{trigger=null,initialFocus=null,replace=false,returnTo=null}={}){
      const entry=this.entry(id);
      if(this.isOpen(entry))return false;
      const current=entry.modality==="modal"?this.currentModal():null;
      if(current&&current.id!==id){
        requireValue(replace,`${id} cannot open while ${current.id} is active`);
        const currentState=this.states.get(current.id)||{trigger:null,returnTo:null};
        this.hide(current);currentState.suspended=true;this.states.set(current.id,currentState);this.emit("suspend",current,{replacement:id});
        if(returnTo===null)returnTo=current.id;
      }
      const prior=this.states.get(id);
      const state={trigger:trigger||prior?.trigger||global.document?.activeElement||null,returnTo:returnTo??prior?.returnTo??null,suspended:false};
      this.states.set(id,state);this.show(entry);this.emit(prior?.suspended?"resume":"open",entry,{returnTo:state.returnTo});
      focus(typeof initialFocus==="function"?initialFocus():initialFocus||entry.initialFocus?.());return true;
    }
    close(id,{restoreFocus=true,resumeReturn=true}={}){
      const entry=this.entry(id),state=this.states.get(id);
      if(!this.isOpen(entry)&&!state)return false;
      if(this.isOpen(entry))this.hide(entry);this.states.delete(id);this.emit("close",entry,{returnTo:state?.returnTo||null});
      const returned=resumeReturn&&state?.returnTo?this.entries.get(state.returnTo):null;
      const returnedState=returned&&this.states.get(returned.id);
      if(returned&&returnedState?.suspended){
        returnedState.suspended=false;this.show(returned);this.emit("resume",returned,{replacement:id});focus(state.trigger||returned.initialFocus?.());return true;
      }
      if(restoreFocus)focus(state?.trigger?.isConnected===false?this.fallbackFocus:state?.trigger||this.fallbackFocus);return true;
    }
    diagnostics(){
      const overlays=[...this.entries.values()].map(entry=>Object.freeze({id:entry.id,modality:entry.modality,open:this.isOpen(entry),suspended:Boolean(this.states.get(entry.id)?.suspended)}));
      return Object.freeze({contractVersion:1,manifestLoaded:Boolean(this.manifest),overlaysVersion:this.manifest?.overlaysVersion??null,registered:overlays.length,active:overlays.filter(item=>item.open).map(item=>item.id),overlays:Object.freeze(overlays),events:Object.freeze([...this.events])});
    }
  }
  if(typeof module!=="undefined"&&module.exports)module.exports={WebOverlayHost};else global.UltimatumWebOverlayHost=WebOverlayHost;
})(typeof window!=="undefined"?window:globalThis);
