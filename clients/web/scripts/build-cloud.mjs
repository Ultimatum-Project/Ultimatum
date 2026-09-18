import {build} from 'esbuild';
import {readFile,writeFile} from 'node:fs/promises';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
const web=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
await build({entryPoints:[path.join(web,'src/cloud-sdk-entry.js')],bundle:true,minify:true,format:'iife',globalName:'UltimatumSupabase',target:'safari13',outfile:path.join(web,'dist/cloud-sdk.js')});
const dependencies=['@supabase/supabase-js','@supabase/auth-js','@supabase/functions-js','@supabase/postgrest-js','@supabase/realtime-js','@supabase/storage-js','iceberg-js','tslib','@supabase/phoenix'];
let licenses='Pinned Supabase client SDK bundled for Ultimatum Accounts.\n';
for(const name of dependencies) {
  let found=false;
  for(const file of ['LICENSE','LICENSE.md','LICENSE.txt','CopyrightNotice.txt']) {
    try {licenses+='\n'+name+'\n'+await readFile(path.join(web,'node_modules',name,file),'utf8');found=true;break;}catch {}
  }
  if(!found)throw Error('Missing dependency license: '+name);
}
await writeFile(path.join(web,'dist/cloud-sdk.js.LEGAL.txt'),licenses);
