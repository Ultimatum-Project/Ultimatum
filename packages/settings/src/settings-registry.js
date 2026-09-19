(function installSettingsRegistry(global){
  function readPath(value,path){return path.split(".").reduce((current,key)=>current==null?undefined:current[key],value);}
  function valid(definition,value){
    if(definition.type==="boolean")return typeof value==="boolean";
    if(definition.type==="integer")return Number.isSafeInteger(value)&&(definition.minimum==null||value>=definition.minimum)&&(definition.maximum==null||value<=definition.maximum);
    if(definition.type==="number")return Number.isFinite(value)&&(definition.minimum==null||value>=definition.minimum)&&(definition.maximum==null||value<=definition.maximum);
    if(definition.type==="string")return typeof value==="string";
    if(definition.type==="enum")return typeof value==="string"&&definition.values?.includes(value);
    return false;
  }
  class SettingsRegistry{
    constructor({manifestUrl,fetch:fetcher=global.fetch}={}){
      if(!manifestUrl||typeof fetcher!=="function")throw new TypeError("A settings manifest URL and fetch implementation are required.");
      this.manifestUrl=manifestUrl;this.fetcher=fetcher;this.manifest=null;this.loadPromise=null;
    }
    async load(){
      if(!this.loadPromise)this.loadPromise=this.fetcher.call(global,this.manifestUrl,{cache:"no-cache"}).then(response=>{if(!response.ok)throw Error(`Settings manifest request failed (${response.status}).`);return response.json();}).then(manifest=>this.validate(manifest));
      return this.loadPromise;
    }
    validate(manifest){
      if(manifest?.schemaVersion!==1||!Number.isSafeInteger(manifest.settingsVersion)||!Array.isArray(manifest.settings)||!Array.isArray(manifest.profiles))throw new TypeError("Invalid typed settings manifest.");
      const ids=new Set();for(const definition of manifest.settings){if(!definition?.id||ids.has(definition.id)||!valid(definition,definition.default))throw new TypeError(`Invalid typed setting ${definition?.id||"(missing)"}.`);ids.add(definition.id);}
      this.manifest=Object.freeze(manifest);return this.manifest;
    }
    async project(snapshot,host="web"){
      const manifest=await this.load(),values={},unavailable=[];
      for(const definition of manifest.settings){
        if(!definition.hosts.includes(host))continue;
        const value=readPath(snapshot,definition.source);
        if(valid(definition,value))values[definition.id]=value;else unavailable.push(definition.id);
      }
      return Object.freeze({schemaVersion:manifest.schemaVersion,settingsVersion:manifest.settingsVersion,gameId:manifest.gameId,portId:manifest.portId,host,values:Object.freeze(values),unavailable:Object.freeze(unavailable),persistence:"engine-compatibility",migrationPerformed:false});
    }
  }
  if(typeof module!=="undefined"&&module.exports)module.exports={SettingsRegistry,readPath};
  else global.UltimatumSettingsRegistry=SettingsRegistry;
})(typeof window!=="undefined"?window:globalThis);
