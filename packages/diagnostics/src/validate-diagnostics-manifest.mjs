const id=/^[a-z0-9]+(?:-[a-z0-9]+)*$/;
const code=/^[a-z0-9]+(?:[.-][a-z0-9]+)+$/;
const severities=new Set(["debug","info","warning","error"]);

function requireValue(condition,message){if(!condition)throw new TypeError(`Invalid diagnostics manifest: ${message}`);}

export function validateDiagnosticsManifest(value,{portDescriptor}={}){
  requireValue(value&&typeof value==="object"&&!Array.isArray(value),"expected an object");
  requireValue(value.schemaVersion===1,"unsupported schemaVersion");
  requireValue(Number.isSafeInteger(value.diagnosticsVersion)&&value.diagnosticsVersion>0,"diagnosticsVersion is invalid");
  requireValue(id.test(value.gameId||"")&&id.test(value.portId||""),"gameId or portId is invalid");
  if(portDescriptor){
    requireValue(value.gameId===portDescriptor.gameId&&value.portId===portDescriptor.portId,"port identity disagrees with its descriptor");
    requireValue(value.diagnosticsVersion===portDescriptor.diagnosticsSchemaVersion,"diagnosticsVersion disagrees with the port descriptor");
  }
  requireValue(Array.isArray(value.events)&&value.events.length>0,"events are required");
  const codes=new Set();
  for(const event of value.events){
    requireValue(event&&typeof event==="object"&&!Array.isArray(event),"event must be an object");
    requireValue(code.test(event.code||"")&&!codes.has(event.code),`duplicate or invalid event code ${event.code||"(missing)"}`);codes.add(event.code);
    requireValue(id.test(event.component||"")&&severities.has(event.severity),`${event.code} component or severity is invalid`);
    requireValue(Array.isArray(event.detailKeys)&&new Set(event.detailKeys).size===event.detailKeys.length&&event.detailKeys.every(key=>id.test(key)),`${event.code} detail keys are invalid`);
  }
  return value;
}
