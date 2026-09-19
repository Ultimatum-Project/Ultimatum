(function installDiagnostics(global){
  const SEVERITIES=new Set(["debug","info","warning","error"]);
  const CODE=/^[a-z0-9]+(?:[.-][a-z0-9]+)+$/;
  const SAFE_KEY=/^[a-z][a-z0-9-]{0,63}$/;
  const SAFE_STRING=/^[a-z0-9][a-z0-9._-]{0,79}$/i;
  const SENSITIVE=/(?:^|-)(?:authorization|bytes|content|conversation|cookie|email|file|filename|hash|input|journal|key|message|password|path|save|screenshot|secret|text|token|username|user-agent)(?:-|$)/i;

  function requireValue(condition,message){if(!condition)throw new TypeError(`Invalid diagnostic event: ${message}`);}
  function redact(value,depth=0){
    if(depth>4)return undefined;
    if(value===null||typeof value==="boolean"||(typeof value==="number"&&Number.isFinite(value)))return value;
    if(typeof value==="string")return SAFE_STRING.test(value)?value:"[redacted]";
    if(Array.isArray(value))return Object.freeze(value.slice(0,32).map(item=>redact(item,depth+1)).filter(item=>item!==undefined));
    if(!value||typeof value!=="object")return undefined;
    const result={};
    for(const [key,item] of Object.entries(value).slice(0,64)){
      const normalized=key.replace(/([a-z])([A-Z])/g,"$1-$2").toLowerCase();
      if(!SAFE_KEY.test(normalized)||SENSITIVE.test(normalized))continue;
      const safe=redact(item,depth+1);if(safe!==undefined)result[normalized]=safe;
    }
    return Object.freeze(result);
  }
  function generatedSessionId(){
    const random=global.crypto?.randomUUID?.();
    return random&&SAFE_STRING.test(random)?random:`local-${Date.now().toString(36)}`;
  }
  class DiagnosticsCollector{
    constructor({product,port,maxEvents=100,now=()=>new Date().toISOString(),sessionId=generatedSessionId()}={}){
      requireValue(product?.id&&product?.version,"product id and version are required");
      requireValue(port?.gameId&&port?.portId&&port?.version&&port?.engineId&&port?.engineVersion,"port identity and versions are required");
      requireValue(Number.isSafeInteger(maxEvents)&&maxEvents>0&&maxEvents<=500,"maxEvents must be between 1 and 500");
      this.contractVersion=1;this.product=redact(product);this.port=redact(port);this.maxEvents=maxEvents;this.now=now;this.sessionId=sessionId;this.events=[];
    }
    record({component,severity="info",code,details={}}={}){
      requireValue(SAFE_KEY.test(component||""),"component is invalid");
      requireValue(SEVERITIES.has(severity),"severity is invalid");
      requireValue(CODE.test(code||""),"code is invalid");
      const event=Object.freeze({contractVersion:1,at:this.now(),sessionId:this.sessionId,component,severity,code,details:redact(details)});
      this.events.push(event);if(this.events.length>this.maxEvents)this.events.shift();return event;
    }
    bundle(sections={}){
      return Object.freeze({contractVersion:1,generatedAt:this.now(),sessionId:this.sessionId,product:this.product,port:this.port,sections:redact(sections),events:Object.freeze([...this.events]),privacy:Object.freeze({localOnly:true,sensitiveArtifactsIncluded:false})});
    }
  }
  const api={DiagnosticsCollector,redactDiagnosticValue:redact};
  if(typeof module!=="undefined"&&module.exports)module.exports=api;
  else Object.assign(global,{UltimatumDiagnosticsCollector:DiagnosticsCollector,ultimatumRedactDiagnosticValue:redact});
})(typeof window!=="undefined"?window:globalThis);
