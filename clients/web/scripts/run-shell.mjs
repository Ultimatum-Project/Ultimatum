import {existsSync, unlinkSync, writeFileSync} from "node:fs";
import {spawnSync} from "node:child_process";
import path from "node:path";
import {fileURLToPath} from "node:url";
import {tmpdir} from "node:os";
import {randomUUID} from "node:crypto";

const scripts = path.dirname(fileURLToPath(import.meta.url));
const requested = process.argv[2];
if (!requested || path.basename(requested) !== requested || !requested.endsWith(".sh")) {
  console.error("Usage: node scripts/run-shell.mjs <script.sh> [arguments...]");
  process.exit(2);
}
const script = path.join(scripts, requested);
if (!existsSync(script)) throw Error(`Unknown web script: ${requested}`);

let shell = process.env.BASH || "/bin/bash";
let env = {...process.env};
if (process.platform === "win32") {
  let developerPath = "";
  const candidates = [
    process.env.PROGRAMFILES && path.join(process.env.PROGRAMFILES, "Git", "bin", "bash.exe"),
    process.env.LOCALAPPDATA && path.join(process.env.LOCALAPPDATA, "Programs", "Git", "bin", "bash.exe")
  ].filter(Boolean);
  shell = candidates.find(existsSync);
  if (!shell) throw Error("Git for Windows with Git Bash is required.");

  const vswhere = path.join(process.env["ProgramFiles(x86)"] || "", "Microsoft Visual Studio", "Installer", "vswhere.exe");
  if (existsSync(vswhere)) {
    const found = spawnSync(vswhere, ["-latest", "-products", "*", "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"], {encoding:"utf8"});
    const install = found.status === 0 ? found.stdout.trim() : "";
    const developer = install && path.join(install, "Common7", "Tools", "VsDevCmd.bat");
    if (developer && existsSync(developer)) {
      const environmentScript = path.join(tmpdir(), `ultimatum-vs-env-${randomUUID()}.cmd`);
      writeFileSync(environmentScript, `@call "${developer}" -no_logo -arch=x64 >nul\r\n@set\r\n`);
      const configured = spawnSync(process.env.ComSpec || "cmd.exe", ["/d", "/c", environmentScript], {encoding:"utf8"});
      unlinkSync(environmentScript);
      if (configured.status === 0) {
        for (const line of configured.stdout.split(/\r?\n/)) {
          const at = line.indexOf("=");
          if (at > 0) {
            const key = line.slice(0, at), value = line.slice(at + 1);
            env[key] = value;
            if (key.toLowerCase() === "path") developerPath = value;
          }
        }
      }
    }
  }
  const gitRoot = path.resolve(path.dirname(shell), "..");
  env.PATH = [path.join(gitRoot, "bin"), path.join(gitRoot, "usr", "bin"), path.join(gitRoot, "usr", "bin", "core_perl"), developerPath || env.Path || env.PATH || ""].join(path.delimiter);
  env.Path = env.PATH;
}

const result = spawnSync(shell, [script, ...process.argv.slice(3)], {cwd:path.resolve(scripts, ".."), env, stdio:"inherit"});
if (result.error) throw result.error;
process.exit(result.status ?? 1);
