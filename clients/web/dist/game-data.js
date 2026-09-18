(function(global) {
  const manifest = typeof module !== "undefined" && module.exports ? require("./game-data-manifest.js") : global.UltimatumGameDataManifest;
  const MAX_FILES = 2048;
  const MAX_TOTAL_BYTES = 32 * 1024 * 1024;
  const MAX_FILE_BYTES = 2 * 1024 * 1024;
  const VGA_ARCHIVE = {size:374307, sha256:"d319dee73a2045734d9be9087a7059d1a5a031e1dbbaf553f4c6f5c751f5d57c"};

  function basename(path) {
    if (typeof path !== "string" || path.length > 255 || /[\x00-\x1f\x7f]/.test(path)) throw Error("A file has an invalid name.");
    const normalized = path.replaceAll("\\", "/");
    if (normalized.startsWith("/") || /^[a-z]:/i.test(normalized) || normalized.split("/").includes("..")) throw Error("The selection contains an unsafe file path.");
    return normalized.split("/").at(-1).toUpperCase();
  }

  function selectInstallation(entries) {
    if (!Array.isArray(entries) || entries.length > MAX_FILES) throw Error("Select one game installation with at most 2,048 files.");
    const groups = new Map();
    let total = 0;
    for (const entry of entries) {
      const name = basename(entry.name);
      if (!Object.hasOwn(manifest.files,name)) continue;
      const directory = entry.name.replaceAll("\\", "/").split("/").slice(0,-1).filter(part=>part && part!==".").join("/").toUpperCase();
      if (!groups.has(directory)) groups.set(directory,new Map());
      const group = groups.get(directory);
      if (group.has(name)) throw Error(`More than one ${name} was found in ${directory || "the ZIP root"}. Select a single game installation.`);
      group.set(name,entry);
      const size = entry.size ?? entry.data?.byteLength;
      if (!Number.isSafeInteger(size) || size < 1 || size > MAX_FILE_BYTES) throw Error(`${name} is empty or too large for Ultima IV.`);
      total += size;
      if (total > MAX_TOTAL_BYTES) throw Error("The unpacked game data exceeds the 32 MB limit.");
    }
    const required = Object.keys(manifest.files);
    const complete = [...groups].filter(([,files])=>required.every(name=>files.has(name)));
    if (complete.length > 1) throw Error(`Multiple complete game installations were found (${complete.map(([dir])=>dir || "ZIP root").join(", ")}). Choose one folder or ZIP containing one installation.`);
    const ranked = [...groups].sort((a,b)=>b[1].size-a[1].size);
    if (!complete.length && ranked.length>1 && ranked[0][1].size===ranked[1][1].size) throw Error("More than one incomplete game installation was found. Choose one complete folder; files from separate folders are not combined.");
    const chosen = complete[0] || ranked[0];
    return {entries:chosen ? [...chosen[1].values()] : [],directory:chosen?.[0] || "",otherDirectories:Math.max(0,groups.size-1)};
  }

  function select(entries) {return selectInstallation(entries).entries;}

  async function digest(bytes) {
    if (global.crypto?.subtle) {
      const hash = await global.crypto.subtle.digest("SHA-256", bytes);
      return Array.from(new Uint8Array(hash), b => b.toString(16).padStart(2, "0")).join("");
    }
    // Compatibility fallback for the existing phone HTTP LAN preview only.
    // This hashes public file content; it is not used for secrets or signing.
    const initial = [], constants = [];
    for (let n = 2; constants.length < 64; n++) {
      let prime = true;
      for (let d = 2; d * d <= n; d++) if (n % d === 0) {prime = false; break;}
      if (prime) { if (initial.length < 8) initial.push((Math.sqrt(n) % 1 * 2 ** 32) >>> 0); constants.push((Math.cbrt(n) % 1 * 2 ** 32) >>> 0); }
    }
    const data = new Uint8Array(Math.ceil((bytes.length + 9) / 64) * 64);
    data.set(bytes); data[bytes.length] = 128;
    const view = new DataView(data.buffer);
    view.setUint32(data.length - 8, Math.floor(bytes.length / 2 ** 29));
    view.setUint32(data.length - 4, bytes.length * 8);
    const rotate = (n, bits) => (n >>> bits) | (n << (32 - bits));
    const words = new Uint32Array(64);
    for (let offset = 0; offset < data.length; offset += 64) {
      for (let i = 0; i < 16; i++) words[i] = view.getUint32(offset + i * 4);
      for (let i = 16; i < 64; i++) {
        const x = words[i - 15], y = words[i - 2];
        words[i] = words[i - 16] + (rotate(x, 7) ^ rotate(x, 18) ^ (x >>> 3)) + words[i - 7] + (rotate(y, 17) ^ rotate(y, 19) ^ (y >>> 10));
      }
      let [a,b,c,d,e,f,g,h] = initial;
      for (let i = 0; i < 64; i++) {
        const t1 = (h + (rotate(e, 6) ^ rotate(e, 11) ^ rotate(e, 25)) + ((e & f) ^ (~e & g)) + constants[i] + words[i]) >>> 0;
        const t2 = ((rotate(a, 2) ^ rotate(a, 13) ^ rotate(a, 22)) + ((a & b) ^ (a & c) ^ (b & c))) >>> 0;
        [a,b,c,d,e,f,g,h] = [(t1 + t2) >>> 0,a,b,c,(d + t1) >>> 0,e,f,g];
      }
      for (const [i, n] of [a,b,c,d,e,f,g,h].entries()) initial[i] = (initial[i] + n) >>> 0;
    }
    return initial.map(n => n.toString(16).padStart(8, "0")).join("");
  }

  function validateDialogue(name, bytes) {
    for (let start = 0; start < bytes.length; start += 288) {
      let cursor = start + 3;
      for (let i = 0; i < 12; i++) {
        const end = bytes.indexOf(0, cursor);
        if (end < cursor || end >= start + 288 || (i === 2 && end === cursor)) throw Error(`${name} contains a damaged conversation record.`);
        cursor = end + 1;
      }
    }
  }

  async function validate(entries, hash = digest) {
    const installation = selectInstallation(entries);
    const selected = installation.entries;
    const byName = new Map(selected.map(entry => [basename(entry.name), entry]));
    const missing = Object.keys(manifest.files).filter(name => !byName.has(name));
    if (missing.length) throw Error(`Missing ${missing.slice(0, 5).join(", ")}${missing.length > 5 ? ` and ${missing.length - 5} more files` : ""}. Choose the complete original DOS/EGA game folder or ZIP.`);
    const files = [];
    for (const [name, rule] of Object.entries(manifest.files)) {
      const bytes = new Uint8Array(byName.get(name).data);
      if (bytes.length !== rule.size) throw Error(`${name} has an unsupported size (${bytes.length}; expected ${rule.size} bytes). Your existing library has not been replaced.`);
      const sha256 = await hash(bytes);
      if (name.endsWith(".TLK")) validateDialogue(name, bytes);
      if (name.endsWith(".ULT")) {
        for (let i = 0; i < 32; i++) {
          // Out-of-range conversation IDs occur in the original data and are
          // safely unmatched by CityMapLoader; they are not array indexes.
          if (bytes[1056 + i] > 31 || bytes[1088 + i] > 31 || ![0, 1, 128, 255].includes(bytes[1216 + i])) throw Error(`${name} contains an invalid person record.`);
        }
      }
      files.push({name, data: bytes.slice().buffer, sha256});
    }
    const profiles = manifest.profiles || [{id:manifest.profile,label:"English DOS/EGA",files:manifest.files}];
    const profile = profiles.find(profile=>files.every(file=>profile.files[file.name]?.sha256===file.sha256));
    if (!profile) {
      const closest = [...profiles].sort((a,b)=>files.filter(file=>b.files[file.name]?.sha256===file.sha256).length-files.filter(file=>a.files[file.name]?.sha256===file.sha256).length)[0];
      const changed = files.find(file=>closest.files[file.name]?.sha256!==file.sha256);
      throw Error(`${changed.name} is not a supported version. The complete data set must match a verified English DOS/EGA release (including 1.01); mixed, modified, or translated files need a separate compatibility profile.`);
    }
    return {kind: "files", version: 1, game: manifest.game, profile: profile.id, files, verification: {label:profile.label,directory:installation.directory,otherDirectories:installation.otherDirectories,ignored: entries.length - files.length}};
  }

  async function validateVga(buffer) {
    const bytes = new Uint8Array(buffer);
    if (bytes.length !== VGA_ARCHIVE.size || await digest(bytes) !== VGA_ARCHIVE.sha256) throw Error("That VGA ZIP is not the verified U4UPGRAD.ZIP version supported by this build. Your existing overlay has not been replaced.");
    return buffer;
  }

  function bytesToBase64(buffer) {
    const bytes = new Uint8Array(buffer);
    let value = "";
    for (let offset = 0; offset < bytes.length; offset += 0x8000) {
      value += String.fromCharCode(...bytes.subarray(offset, offset + 0x8000));
    }
    return global.btoa(value);
  }

  function base64ToBytes(value) {
    if (typeof value !== "string" || value.length > MAX_FILE_BYTES * 2) throw Error("A cloud game-data file is invalid.");
    let decoded;
    try { decoded = global.atob(value); } catch { throw Error("A cloud game-data file is damaged."); }
    const bytes = new Uint8Array(decoded.length);
    for (let index = 0; index < decoded.length; index += 1) bytes[index] = decoded.charCodeAt(index);
    return bytes;
  }

  function encodePackage(verified, label = "Ultima IV game data") {
    if (!verified || verified.kind !== "files" || verified.game !== manifest.game || !Array.isArray(verified.files)) throw Error("Verified game data is required before cloud upload.");
    const byName = new Map(verified.files.map(file => [basename(file.name), file]));
    const files = Object.keys(manifest.files).map(name => {
      const file = byName.get(name);
      if (!file || typeof file.sha256 !== "string") throw Error(`Missing verified ${name}.`);
      const bytes = new Uint8Array(file.data);
      return {name, size: bytes.byteLength, sha256: file.sha256, data: bytesToBase64(bytes)};
    });
    return JSON.stringify({format:"ultimatum-game-data",version:1,game:manifest.game,profile:verified.profile,label:String(label).slice(0,80),files});
  }

  async function decodePackage(text) {
    if (typeof text !== "string" || new TextEncoder().encode(text).byteLength > 16 * 1024 * 1024) throw Error("The cloud game-data package is too large.");
    let value;
    try { value = JSON.parse(text); } catch { throw Error("The cloud game-data package is not valid JSON."); }
    if (value?.format !== "ultimatum-game-data" || value.version !== 1 || value.game !== manifest.game || typeof value.profile !== "string" || !Array.isArray(value.files)) throw Error("Unsupported cloud game-data package.");
    if (value.files.length !== Object.keys(manifest.files).length) throw Error("The cloud game-data package is incomplete.");
    const entries = [];
    for (const file of value.files) {
      if (!file || typeof file.name !== "string" || !Number.isSafeInteger(file.size) || typeof file.sha256 !== "string") throw Error("A cloud game-data file is invalid.");
      const bytes = base64ToBytes(file.data);
      if (bytes.byteLength !== file.size || await digest(bytes) !== file.sha256) throw Error(`${basename(file.name)} failed its cloud integrity check.`);
      entries.push({name:file.name,data:bytes.buffer});
    }
    const verified = await validate(entries);
    if (verified.profile !== value.profile) throw Error("The cloud game-data compatibility profile does not match its files.");
    return {...verified,label:typeof value.label === "string" ? value.label.slice(0,80) : "Ultima IV game data",text};
  }

  const api = {manifest, basename, select, validate, validateVga, digest, encodePackage, decodePackage, bytesToBase64, base64ToBytes, VGA_ARCHIVE, MAX_FILES, MAX_TOTAL_BYTES, MAX_FILE_BYTES};
  if (typeof module !== "undefined" && module.exports) module.exports = api;
  else global.UltimatumGameData = api;
})(typeof window !== "undefined" ? window : globalThis);
