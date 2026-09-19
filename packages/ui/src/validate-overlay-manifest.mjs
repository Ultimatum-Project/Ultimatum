const id=/^[a-z0-9]+(?:-[a-z0-9]+)*$/;
const owners=new Set(["platform","port"]);
const surfaces=new Set(["dialog","drawer"]);
const presentations=new Set(["full-screen","partial-overlay","edge-drawer","adaptive"]);
const modalities=new Set(["modal","nonmodal"]);
const contexts=new Set(["hidden","visible","adaptive"]);
const gameplay=new Set(["unavailable","continues","input-captured","port-paused"]);
const dismissals=new Set(["dismissible","conditional","required"]);
const hosts=new Set(["web","native"]);
function requireValue(condition,message){if(!condition)throw new TypeError(`Invalid overlay manifest: ${message}`);}

export function validateOverlayManifest(value,{portDescriptor}={}){
  requireValue(value&&typeof value==="object"&&!Array.isArray(value),"expected an object");
  requireValue(value.schemaVersion===1,"unsupported schemaVersion");
  requireValue(Number.isSafeInteger(value.overlaysVersion)&&value.overlaysVersion>0,"overlaysVersion is invalid");
  requireValue(id.test(value.gameId||"")&&id.test(value.portId||""),"gameId or portId is invalid");
  if(portDescriptor){
    requireValue(value.gameId===portDescriptor.gameId&&value.portId===portDescriptor.portId,"port identity disagrees with its descriptor");
    requireValue(value.overlaysVersion===portDescriptor.overlaysSchemaVersion,"overlaysVersion disagrees with the port descriptor");
  }
  requireValue(Array.isArray(value.overlays)&&value.overlays.length>0,"overlays are required");
  const ids=new Set();
  for(const overlay of value.overlays){
    requireValue(overlay&&typeof overlay==="object"&&!Array.isArray(overlay),"overlay must be an object");
    requireValue(id.test(overlay.id||"")&&!ids.has(overlay.id),`duplicate or invalid overlay id ${overlay.id||"(missing)"}`);ids.add(overlay.id);
    requireValue(owners.has(overlay.owner)&&surfaces.has(overlay.surface)&&presentations.has(overlay.presentation),`${overlay.id} ownership or presentation is invalid`);
    requireValue(modalities.has(overlay.modality)&&contexts.has(overlay.worldContext)&&gameplay.has(overlay.gameplayPolicy)&&dismissals.has(overlay.dismissPolicy),`${overlay.id} lifecycle policy is invalid`);
    requireValue(Array.isArray(overlay.hosts)&&overlay.hosts.length>0&&new Set(overlay.hosts).size===overlay.hosts.length&&overlay.hosts.every(host=>hosts.has(host)),`${overlay.id} hosts are invalid`);
    requireValue(overlay.surface!=="drawer"||overlay.modality==="nonmodal",`${overlay.id} drawers must be nonmodal`);
    requireValue(overlay.gameplayPolicy!=="port-paused"||overlay.owner==="port",`${overlay.id} only a port can own engine pause behavior`);
  }
  return value;
}
