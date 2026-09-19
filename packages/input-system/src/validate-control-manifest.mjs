const id=/^[a-z0-9]+(?:[.-][a-z0-9]+)*$/;
const kinds=new Set(["button","axis1d","axis2d","pointer","text"]);
const repeats=new Set(["none","key","continuous"]);
const devices=new Set(["keyboard","touch","controller"]);
const inputs=new Set(["key","touch-control","pointer","gamepad-button","gamepad-axis"]);
const hosts=new Set(["web","native"]);
function requireValue(condition,message){if(!condition)throw new TypeError(`Invalid control manifest: ${message}`);}

export function validateControlManifest(value,{portDescriptor}={}){
  requireValue(value&&typeof value==="object"&&!Array.isArray(value),"expected an object");
  requireValue(value.schemaVersion===1,"unsupported schemaVersion");
  requireValue(Number.isSafeInteger(value.actionsVersion)&&value.actionsVersion>0,"actionsVersion is invalid");
  requireValue(id.test(value.gameId||"")&&id.test(value.portId||""),"gameId or portId is invalid");
  if(portDescriptor){
    requireValue(value.gameId===portDescriptor.gameId&&value.portId===portDescriptor.portId,"port identity disagrees with its descriptor");
    requireValue(value.actionsVersion===portDescriptor.actionsSchemaVersion,"actionsVersion disagrees with the port descriptor");
  }
  requireValue(Array.isArray(value.actions)&&value.actions.length>0,"actions are required");
  const actions=new Map();
  for(const action of value.actions){
    requireValue(action&&typeof action==="object"&&!Array.isArray(action),"action must be an object");
    requireValue(id.test(action.id||"")&&!actions.has(action.id),`duplicate or invalid action id ${action.id||"(missing)"}`);
    requireValue(typeof action.label==="string"&&action.label.length>0&&id.test(action.category||""),`${action.id} label or category is invalid`);
    requireValue(kinds.has(action.kind)&&repeats.has(action.repeat),`${action.id} kind or repeat policy is invalid`);
    requireValue(typeof action.required==="boolean"&&typeof action.destructive==="boolean",`${action.id} flags are invalid`);
    requireValue(action.contexts==null||(Array.isArray(action.contexts)&&new Set(action.contexts).size===action.contexts.length&&action.contexts.every(context=>id.test(context))),`${action.id} contexts are invalid`);
    actions.set(action.id,action);
  }
  requireValue(Array.isArray(value.profiles)&&value.profiles.length>0,"profiles are required");
  const profileIds=new Set();let desktop=false;
  for(const profile of value.profiles){
    requireValue(profile&&typeof profile==="object"&&!Array.isArray(profile),"profile must be an object");
    requireValue(id.test(profile.id||"")&&!profileIds.has(profile.id),`duplicate or invalid profile id ${profile.id||"(missing)"}`);profileIds.add(profile.id);
    requireValue(typeof profile.label==="string"&&profile.label.length>0&&devices.has(profile.device),`${profile.id} label or device is invalid`);
    requireValue(profile.scope==="device-host"&&profile.portable===false,`${profile.id} must be a non-portable device/host preference`);
    requireValue(Array.isArray(profile.verifiedHosts)&&profile.verifiedHosts.length>0&&new Set(profile.verifiedHosts).size===profile.verifiedHosts.length&&profile.verifiedHosts.every(host=>hosts.has(host)),`${profile.id} verifiedHosts are invalid`);
    requireValue(Array.isArray(profile.bindings)&&profile.bindings.length>0,`${profile.id} bindings are required`);
    const bound=new Set(),physical=new Set();
    for(const binding of profile.bindings){
      requireValue(actions.has(binding?.actionId),`${profile.id} references unknown action ${binding?.actionId||"(missing)"}`);
      requireValue(binding.input&&inputs.has(binding.input.kind)&&typeof binding.input.id==="string"&&binding.input.id.length>0,`${profile.id} has an invalid input`);
      const signature=JSON.stringify([binding.input.kind,binding.input.id,[...(binding.input.modifiers||[])].sort()]);
      requireValue(!physical.has(signature),`${profile.id} has duplicate physical binding ${binding.input.id}`);physical.add(signature);bound.add(binding.actionId);
    }
    for(const action of actions.values())if(action.required)requireValue(bound.has(action.id),`${profile.id} is missing required action ${action.id}`);
    if(profile.device==="keyboard"&&profile.verifiedHosts.includes("web"))desktop=true;
  }
  requireValue(desktop,"a verified web desktop profile is required");
  return value;
}
