import {existsSync,readdirSync} from "node:fs";
import {spawnSync} from "node:child_process";
import path from "node:path";
import {fileURLToPath} from "node:url";

const site = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const files = readdirSync(path.join(site, "tests"))
  .filter(name => name.endsWith(".test.mjs"))
  .sort()
  .map(name => path.join(site, "tests", name));

if (!files.length) throw Error("No site tests found.");
const environment={...process.env};
if(!existsSync(path.resolve(site,"../..",".env.local")))Object.assign(environment,{
  ULTIMATUM_SUPABASE_PUBLIC_URL:environment.ULTIMATUM_SUPABASE_PUBLIC_URL||"https://public-fixture.example.invalid",
  ULTIMATUM_SUPABASE_PUBLIC_KEY:environment.ULTIMATUM_SUPABASE_PUBLIC_KEY||"sb_publishable_fixture_public",
  ULTIMATUM_SUPABASE_PUBLIC_PROJECT:environment.ULTIMATUM_SUPABASE_PUBLIC_PROJECT||"ultimatum-prod",
  ULTIMATUM_SUPABASE_TEST_URL:environment.ULTIMATUM_SUPABASE_TEST_URL||"https://test-fixture.example.invalid",
  ULTIMATUM_SUPABASE_TEST_KEY:environment.ULTIMATUM_SUPABASE_TEST_KEY||"sb_publishable_fixture_test",
  ULTIMATUM_SUPABASE_TEST_PROJECT:environment.ULTIMATUM_SUPABASE_TEST_PROJECT||"ultimatum-test",
});
const result = spawnSync(process.execPath, ["--test", ...files], {
  cwd: site,
  stdio: "inherit",
  env:environment
});
if (result.error) throw result.error;
process.exit(result.status ?? 1);
