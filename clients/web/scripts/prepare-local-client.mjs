import {cp,mkdir,readFile,rm,writeFile} from "node:fs/promises";
import path from "node:path";
import {fileURLToPath} from "node:url";
import {cloudEnvironment,cloudSessionAccount,renderCloudConfig,renderCloudHtml} from "../../site/scripts/cloud-environments.mjs";

const web=path.resolve(path.dirname(fileURLToPath(import.meta.url)),"..");
const repo=path.resolve(web,"../..");
const output=path.join(web,".cache/local-dist");
const audience=process.argv.includes("--public")?"public":"test";

await rm(output,{recursive:true,force:true});
await mkdir(path.dirname(output),{recursive:true});
await cp(path.join(web,"dist"),output,{recursive:true});
await cp(path.join(repo,"packages/import-framework/src/installation-orchestrator.js"),path.join(output,"installation-orchestrator.js"));
await cp(path.join(repo,"packages/session-orchestrator/src/session-orchestrator.js"),path.join(output,"session-orchestrator.js"));
await cp(path.join(repo,"packages/storage/src/storage-provider-contract.js"),path.join(output,"storage-provider-contract.js"));
await cp(path.join(repo,"packages/storage/src/opfs-storage-provider.js"),path.join(output,"opfs-storage-provider.js"));
try {
  cloudEnvironment(audience);
  await writeFile(path.join(output,"cloud-config.js"),renderCloudConfig(audience));
  const html=await readFile(path.join(output,"cloud.html"),"utf8");
  await writeFile(path.join(output,"cloud.html"),renderCloudHtml(html,audience));
  await writeFile(path.join(output,"cloud-session-account.txt"),cloudSessionAccount(audience));
  console.log(`Prepared ${audience} local client in ${output}.`);
} catch(error) {
  await writeFile(path.join(output,"cloud-session-account.txt"),"ultimatum-account-unconfigured.invalid");
  if(process.argv.includes("--require-cloud"))throw error;
  console.warn(`${error.message} Accounts remain disabled; local play and saves are available.`);
}
