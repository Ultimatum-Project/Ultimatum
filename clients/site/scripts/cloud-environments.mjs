import {existsSync,readFileSync} from "node:fs";
import path from "node:path";
import {fileURLToPath} from "node:url";

const repo=path.resolve(path.dirname(fileURLToPath(import.meta.url)),"../../..");
const localFile=path.join(repo,".env.local");

function parseLocalEnvironment(text) {
  const result={};
  for(const original of text.split(/\r?\n/)) {
    const line=original.trim();
    if(!line||line.startsWith("#"))continue;
    const match=line.match(/^(?:export\s+)?([A-Z][A-Z0-9_]*)\s*=\s*(.*)$/);
    if(!match)throw Error(`Invalid .env.local entry: ${original}`);
    let value=match[2].trim();
    if((value.startsWith('"')&&value.endsWith('"'))||(value.startsWith("'")&&value.endsWith("'")))value=value.slice(1,-1);
    result[match[1]]=value;
  }
  return result;
}

const local=existsSync(localFile)?parseLocalEnvironment(readFileSync(localFile,"utf8")):{};
const setting=name=>process.env[name]??local[name];
export const localSetting=setting;

export function cloudEnvironment(audience="public") {
  if(!["public","test"].includes(audience))throw Error(`Unknown cloud environment: ${audience}`);
  const prefix=`ULTIMATUM_SUPABASE_${audience==="public"?"PUBLIC":"TEST"}_`;
  const environment={url:setting(prefix+"URL"),key:setting(prefix+"KEY"),project:setting(prefix+"PROJECT")};
  const missing=Object.entries(environment).filter(([,value])=>!value).map(([name])=>name);
  if(missing.length)throw Error(`Missing ${audience} cloud configuration (${missing.join(", ")}). Copy .env.example to .env.local or set the matching environment variables.`);
  let url;
  try{url=new URL(environment.url);}catch{throw Error(`${audience} Supabase URL is invalid.`);}
  if(url.pathname!=="/"||url.search||url.hash||!(url.protocol==="https:"||(url.protocol==="http:"&&["127.0.0.1","localhost"].includes(url.hostname))))throw Error(`${audience} Supabase URL must be an HTTPS origin (or loopback HTTP for local development).`);
  if(!/^sb_publishable_[A-Za-z0-9_-]+$/.test(environment.key)&&!/^eyJ[A-Za-z0-9_-]+\.[A-Za-z0-9_-]+\.[A-Za-z0-9_-]+$/.test(environment.key))throw Error(`${audience} cloud client must use a Supabase publishable or legacy anon key.`);
  if(!/^[A-Za-z0-9][A-Za-z0-9._-]*$/.test(environment.project))throw Error(`${audience} cloud project label is invalid.`);
  return Object.freeze(environment);
}

export function renderCloudConfig(audience="public") {
  const environment=cloudEnvironment(audience);
  return `// Generated public-client configuration for ${environment.project}.\n// The publishable key is intentionally usable by clients; authorization remains enforced by auth + RLS.\nwindow.UltimatumCloudConfig = Object.freeze(${JSON.stringify(environment,null,2)});\n`;
}

export function renderCloudHtml(template,audience="public") {
  const origin=new URL(cloudEnvironment(audience).url).origin;
  if(!/connect-src [^;]+;/.test(template))throw Error("Account page is missing its connect-src policy.");
  return template.replace(/connect-src [^;]+;/,`connect-src ${origin};`);
}

export function cloudSessionAccount(audience="public") {
  return `ultimatum-account-${new URL(cloudEnvironment(audience).url).hostname}`;
}

export function parseCloudConfig(script) {
  const match=script.match(/window\.UltimatumCloudConfig\s*=\s*Object\.freeze\((\{[\s\S]*?\})\);/);
  if(!match)throw Error("Missing or unreadable cloud client configuration.");
  return JSON.parse(match[1]);
}

export function checkCloudConfig(script,audience="public") {
  const actual=parseCloudConfig(script),expected=cloudEnvironment(audience);
  for(const field of ["url","key","project"])if(actual[field]!==expected[field])throw Error(`${audience} package uses the wrong Supabase ${field}.`);
  if(Object.keys(actual).sort().join(",")!=="key,project,url")throw Error(`${audience} package contains unexpected cloud configuration fields.`);
  return actual;
}
