const id=/^[a-z0-9]+(?:[.-][a-z0-9]+)*$/;
const owners=new Set(["platform","host","port","engine"]);
const scopes=new Set(["account","device-host","platform","game","installation-edition","player-profile","session"]);
const policies=new Set(["immediate","after-dismissal","safe-boundary","reload"]);
const hosts=new Set(["web","native"]);

function requireValue(condition,message){if(!condition)throw new TypeError(`Invalid settings manifest: ${message}`);}
function validValue(definition,value){
  if(definition.type==="boolean")return typeof value==="boolean";
  if(definition.type==="integer")return Number.isSafeInteger(value)&&(definition.minimum==null||value>=definition.minimum)&&(definition.maximum==null||value<=definition.maximum);
  if(definition.type==="number")return Number.isFinite(value)&&(definition.minimum==null||value>=definition.minimum)&&(definition.maximum==null||value<=definition.maximum);
  if(definition.type==="string")return typeof value==="string";
  if(definition.type==="enum")return typeof value==="string"&&definition.values.includes(value);
  return false;
}

export function validateSettingsManifest(value,{portDescriptor}={}){
  requireValue(value&&typeof value==="object"&&!Array.isArray(value),"expected an object");
  requireValue(value.schemaVersion===1,"unsupported schemaVersion");
  requireValue(Number.isSafeInteger(value.settingsVersion)&&value.settingsVersion>0,"settingsVersion is invalid");
  requireValue(Number.isSafeInteger(value.migrationVersion)&&value.migrationVersion>0,"migrationVersion is invalid");
  requireValue(id.test(value.gameId||"")&&id.test(value.portId||""),"gameId or portId is invalid");
  if(portDescriptor){
    requireValue(value.gameId===portDescriptor.gameId&&value.portId===portDescriptor.portId,"port identity disagrees with its descriptor");
    requireValue(value.settingsVersion===portDescriptor.settingsSchemaVersion,"settingsVersion disagrees with the port descriptor");
  }
  requireValue(Array.isArray(value.settings)&&value.settings.length>0,"settings are required");
  const definitions=new Map();
  for(const setting of value.settings){
    requireValue(setting&&typeof setting==="object"&&!Array.isArray(setting),"setting must be an object");
    requireValue(id.test(setting.id||"")&&!definitions.has(setting.id),`duplicate or invalid setting id ${setting.id||"(missing)"}`);
    requireValue(typeof setting.label==="string"&&setting.label.length>0&&setting.label.length<=80,`${setting.id} label is invalid`);
    requireValue(["boolean","integer","number","string","enum"].includes(setting.type),`${setting.id} type is invalid`);
    requireValue(owners.has(setting.owner)&&scopes.has(setting.scope)&&policies.has(setting.applyPolicy),`${setting.id} ownership, scope, or apply policy is invalid`);
    requireValue(typeof setting.portable==="boolean",`${setting.id} portable is required`);
    requireValue(Array.isArray(setting.hosts)&&setting.hosts.length>0&&new Set(setting.hosts).size===setting.hosts.length&&setting.hosts.every(host=>hosts.has(host)),`${setting.id} hosts are invalid`);
    requireValue(typeof setting.source==="string"&&/^[a-zA-Z0-9]+(?:\.[a-zA-Z0-9]+)*$/.test(setting.source),`${setting.id} source is invalid`);
    if(setting.type==="enum")requireValue(Array.isArray(setting.values)&&setting.values.length>0&&new Set(setting.values).size===setting.values.length&&setting.values.every(item=>typeof item==="string"),`${setting.id} enum values are invalid`);
    if(setting.minimum!=null||setting.maximum!=null)requireValue(["integer","number"].includes(setting.type)&&(!Number.isFinite(setting.minimum)||!Number.isFinite(setting.maximum)||setting.minimum<=setting.maximum),`${setting.id} range is invalid`);
    requireValue(validValue(setting,setting.default),`${setting.id} default is invalid`);
    if(setting.scope==="device-host")requireValue(setting.portable===false,`${setting.id} device/host settings must not be portable`);
    definitions.set(setting.id,setting);
  }
  requireValue(Array.isArray(value.profiles),"profiles must be an array");
  const profileIds=new Set();let recommended=0;
  for(const profile of value.profiles){
    requireValue(profile&&typeof profile==="object"&&!Array.isArray(profile),"profile must be an object");
    requireValue(id.test(profile.id||"")&&!profileIds.has(profile.id),`duplicate or invalid profile id ${profile.id||"(missing)"}`);profileIds.add(profile.id);
    requireValue(typeof profile.label==="string"&&profile.label.length>0&&typeof profile.description==="string"&&profile.description.length>0,`${profile.id} copy is invalid`);
    requireValue(typeof profile.recommended==="boolean",`${profile.id} recommended flag is required`);if(profile.recommended)recommended++;
    requireValue(profile.values&&typeof profile.values==="object"&&!Array.isArray(profile.values),`${profile.id} values are invalid`);
    for(const [key,settingValue] of Object.entries(profile.values)){
      const definition=definitions.get(key);
      requireValue(definition?.profileControlled===true,`${profile.id} references non-profile setting ${key}`);
      requireValue(validValue(definition,settingValue),`${profile.id} value for ${key} is invalid`);
    }
    for(const definition of definitions.values())if(definition.profileControlled)requireValue(Object.hasOwn(profile.values,definition.id),`${profile.id} is missing ${definition.id}`);
  }
  requireValue(!value.profiles.length||recommended===1,"exactly one settings profile must be recommended");
  return value;
}

export function validateSettingValue(definition,value){
  requireValue(definition&&typeof definition==="object","setting definition is required");
  requireValue(validValue(definition,value),`${definition.id||"setting"} value is invalid`);
  return value;
}
