# Installation-aware game-data imports

Verified 2026-09-15 against the actual public release WebAssembly engine,
with no original game data or development party preloaded and no debug tools.

## Audited user archive

`Ultima_IV_-_Quest_of_the_Avatar_1985.zip`:

- SHA-256: `6bbed280643fc40aeb99aeb0b8215476b6f5e9aeb1170974ff607dafb6534c98`.
- 408 file entries, 3,429,866 bytes total unpacked; all entries passed ZIP CRC.
- Base `ultima4/`: all 103 required files; 97 match the initial profile.
- Nested `ultima4/upgrade/`: 102 required filenames, missing AVATAR.EXE, with
  upgraded graphics; it is not merged into the base installation.
- Six changed base files contain seven changed bytes matching the included
  FIXES.TXT 1.01 corrections. These have an explicit complete-set profile.

The archive is read only by an explicitly configured loopback QA server. It
has not been committed, copied into build output, or published on the LAN site.

## Runtime result

All 44 checks passed in a real 393 × 700 portrait frame on a fresh test origin:

- The exact ZIP succeeds through the launcher ZIP file input and direct URL.
- The base directory and 1.01 profile are selected; original EGA COMPASSN.EGA
  has the expected 719 bytes, not the nested upgrade's 28,093-byte version.
- Embedded classic saves remain excluded; extraction staging is cleaned.
- The folder event path follows the same selection rules. This check uses
  synthesized browser-shaped File objects with webkitRelativePath properties,
  not a physical OS folder picker, which remains device-specific coverage.
- Real original character creation, two Wait turns, durable manual save,
  reload, revalidation of the 1.01 library, and Continue restore the Avatar/turns.
- Missing, unsupported, unsafe, same-directory duplicate, CRC-corrupt,
  oversized, excessive-entry, multiple-complete-installation and split-folder
  archives fail without publishing data or changing active bytes.
- Invalid replacements, injected durable-commit and staging-write failures
  preserve the working library; VGA validation and no-debug capability pass.

27 web tests and 3 public-package tests pass, plus syntax and whitespace checks.
The software SHA-256 fallback is independently compared with Node crypto;
the known local initial profile plus the exact seven-byte 1.01 corrections
also pass real hash validation. Mixed profiles are rejected.

Both normal LAN and separate public Release engines were rebuilt. The cache
revision is 20260915w; reload existing browser tabs before retrying the ZIP.
Physical phone browsers, complete playthrough and actual hosted Cloudflare
behavior were not tested in this slice. No native iOS code was changed.
