(function installControlRegistry(global){
  class ControlRegistry{
    constructor({manifestUrl,fetch:fetcher=global.fetch}={}){
      if(!manifestUrl||typeof fetcher!=="function")throw new TypeError("A control manifest URL and fetch implementation are required.");
      this.manifestUrl=manifestUrl;this.fetcher=fetcher;this.manifest=null;this.loadPromise=null;
    }
    async load(){
      if(!this.loadPromise)this.loadPromise=this.fetcher.call(global,this.manifestUrl,{cache:"no-cache"}).then(response=>{if(!response.ok)throw Error(`Control manifest request failed (${response.status}).`);return response.json();}).then(manifest=>this.validate(manifest));
      return this.loadPromise;
    }
    validate(manifest){
      if(manifest?.schemaVersion!==1||!Number.isSafeInteger(manifest.actionsVersion)||!Array.isArray(manifest.actions)||!Array.isArray(manifest.profiles))throw new TypeError("Invalid semantic control manifest.");
      const actions=new Map();for(const action of manifest.actions){if(!action?.id||actions.has(action.id))throw new TypeError(`Invalid semantic action ${action?.id||"(missing)"}.`);actions.set(action.id,action);}
      for(const profile of manifest.profiles){
        const bound=new Set(),physical=new Set();
        for(const binding of profile.bindings||[]){
          if(!actions.has(binding.actionId))throw new TypeError(`${profile.id} references unknown action ${binding.actionId}.`);
          const signature=JSON.stringify([binding.input?.kind,binding.input?.id,[...(binding.input?.modifiers||[])].sort()]);
          if(physical.has(signature))throw new TypeError(`${profile.id} contains a duplicate physical binding.`);physical.add(signature);bound.add(binding.actionId);
        }
        for(const action of actions.values())if(action.required&&!bound.has(action.id))throw new TypeError(`${profile.id} is missing ${action.id}.`);
      }
      this.manifest=Object.freeze(manifest);return this.manifest;
    }
    async inspect(host="web"){
      const manifest=await this.load();
      const profiles=manifest.profiles.filter(profile=>profile.verifiedHosts.includes(host)).map(profile=>Object.freeze({id:profile.id,label:profile.label,device:profile.device,bindingCount:profile.bindings.length,portable:false}));
      return Object.freeze({schemaVersion:manifest.schemaVersion,actionsVersion:manifest.actionsVersion,gameId:manifest.gameId,portId:manifest.portId,host,actionCount:manifest.actions.length,profiles:Object.freeze(profiles),selectionPersistence:"library-device-host",migrationPerformed:false});
    }
  }
  if(typeof module!=="undefined"&&module.exports)module.exports={ControlRegistry};
  else global.UltimatumControlRegistry=ControlRegistry;
})(typeof window!=="undefined"?window:globalThis);
